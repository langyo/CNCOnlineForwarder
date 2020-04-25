#include "InitialPhase.hpp"
#include <precompiled.hpp>
#include <NatNeg/GameConnection.hpp>
#include <NatNeg/NatNegProxy.hpp>
#include <Logging/Logging.hpp>
#include <Utility/SimpleWriteHandler.hpp>
#include <Utility/WeakRefHandler.hpp>

using AddressV4 = boost::asio::ip::address_v4;
using UDP = boost::asio::ip::udp;
using Resolved = boost::asio::ip::udp::resolver::results_type;
using ErrorCode = boost::system::error_code;
using LogLevel = CNCOnlineForwarder::Logging::Level;

using CNCOnlineForwarder::Utility::makeWeakHandler;
using CNCOnlineForwarder::Utility::makeWriteHandler;

namespace CNCOnlineForwarder::NatNeg
{
    template<typename... Arguments>
    void logLine(LogLevel level, Arguments&&... arguments)
    {
        return Logging::logLine<InitialPhase>(level, std::forward<Arguments>(arguments)...);
    }

    class InitialPhase::ReceiveHandler
    {
    private:
        std::unique_ptr<std::array<char, 1024>> m_buffer;
        std::unique_ptr<EndPoint> m_from;

    public:
        static auto create(InitialPhase* pointer)
        {
            return makeWeakHandler(pointer, ReceiveHandler{});
        }

        boost::asio::mutable_buffer getBuffer() 
        { 
            return boost::asio::buffer(*m_buffer); 
        }

        EndPoint& getFrom() 
        { 
            return *m_from; 
        }

        void operator()(InitialPhase& self, ErrorCode const& code, std::size_t const bytesReceived) const
        {
            self.prepareForNextPacketToCommunicationAddress();

            if (code.failed())
            {
                logLine(LogLevel::error, "Receive failed: ", code);
                return;
            }

            // When receiving, server is already resolved
            if (*m_from != self.m_server->getEndPoint())
            {
                logLine(LogLevel::warning, "Packet is not from server, but from ", *m_from,", discarded");
                return;
            }

            auto const packet = PacketView{ {m_buffer->data(), bytesReceived} };
            return self.handlePacketFromServer(packet);
        }

    private:
        ReceiveHandler() :
            m_buffer{ std::make_unique<std::array<char, 1024>>() },
            m_from{ std::make_unique<EndPoint>() }
        {}
    };

    std::shared_ptr<InitialPhase> InitialPhase::create
    (
        IOManager::ObjectMaker const& objectMaker,
        std::weak_ptr<NatNegProxy> const& proxy,
        PlayerID const id,
        std::string const& natNegServer,
        std::uint16_t const natNegPort
    )
    {
        auto const self = std::make_shared<InitialPhase>
        (
            PrivateConstructor{}, 
            objectMaker, 
            proxy, 
            id
        );

        auto const action = [self, natNegServer, natNegPort]
        {
            logLine(LogLevel::info, "InitialPhase creating, id = ", self->m_id);
            self->extendLife();

            auto const onResolved = []
            (
                InitialPhase& self,
                ErrorCode const& code,
                Resolved const resolved
            )
            {
                if (code.failed())
                {
                    logLine(LogLevel::error, "Failed to resolve server hostname: ", code);
                    return;
                }

                self.m_server->setEndPoint(*resolved);
                logLine(LogLevel::info, "server hostname resolved: ", self.m_server->getEndPoint());
                self.m_server.trySetReady();
            };
            logLine(LogLevel::info, "Resolving server hostname: ", natNegServer);
            self->m_resolver.asyncResolve
            (
                natNegServer,
                std::to_string(natNegPort),
                makeWeakHandler(self, onResolved)
            );

            self->m_server.asyncDo
            (
                [&self = *self](EndPoint const&)
                {
                    logLine(LogLevel::info, "Starting to receive comm packet on local endpoint ", self.m_communicationSocket->local_endpoint());
                    self.prepareForNextPacketToCommunicationAddress();
                }
            );
        };
        boost::asio::defer(self->m_strand, action);

        return self;
    }

    InitialPhase::InitialPhase
    (
        PrivateConstructor,
        IOManager::ObjectMaker const& objectMaker,
        std::weak_ptr<NatNegProxy> const& proxy,
        NatNegPlayerID const id
    ) :
        m_strand{ objectMaker.makeStrand() },
        m_resolver{ m_strand },
        m_communicationSocket{ m_strand, EndPoint{ UDP::v4(), 0 } },
        m_timeout{ m_strand },
        m_proxy{ proxy },
        m_connection{ {} },
        m_id{ id },
        m_server{ {} }, 
        m_clientCommunication{}/*,
        socketReadyToReceive{ {} }*/
    {}

    void InitialPhase::prepareGameConnection
    (
        IOManager::ObjectMaker const& objectMaker,
        std::weak_ptr<ProxyAddressTranslator> const& addressTranslator,
        EndPoint const& client
    )
    {
        auto const maker = [this, objectMaker, addressTranslator, client]
        (
            EndPoint const& server
        )
        {
            if (!m_connection->isReady())
            {
                m_connection->ref() = GameConnection::create
                (
                    objectMaker,
                    m_proxy,
                    addressTranslator,
                    server,
                    client
                );
            }
            m_connection.trySetReady();
        };
        auto const action = [maker](InitialPhase& self)
        {
            self.m_server.asyncDo(maker);
        };
        boost::asio::defer(m_strand, makeWeakHandler(this, action));
    }

    void InitialPhase::handlePacketToServer
    (
        PacketView const packet, 
        EndPoint const& from
    )
    {
        auto action = [data = packet.copyBuffer(), from](InitialPhase& self) mutable
        {
            if (auto const packet = PacketView{ data }; !packet.isNatNeg())
            {
                logLine(LogLevel::warning, "Packet to server dispatcher: Not NatNeg, discarded.");
                return;
            }

            // Handle packet "locally" if it's from communication address,
            // otherwise, dispatch it to GameConnection
            auto dispatcher = [data = std::move(data), from, &self]
            (
                std::weak_ptr<GameConnection> const& connectionRef
            )
            {
                auto const connection = connectionRef.lock();
                if (!connection)
                {
                    logLine(LogLevel::warning, "Packet to server dispatcher: aborting because connection expired");
                    self.close();
                    return;
                }
                
                auto const packet = PacketView{ data };

                if (connection->getClientPublicAddress() == from)
                {
                    logLine(LogLevel::info, "Packet to server dispatcher: source ", from, " is client public address, dispatching to GameConnection");
                    connection->handlePacketToServer(packet);
                    return;
                }

                logLine(LogLevel::info, "Packet to server dispatcher: dispatching to self (InitialPhase)");
                self.handlePacketToServerInternal
                (
                    packet,
                    from, 
                    self.m_server->getEndPoint()
                    // When connection is ready, server is certainly ready as well
                );
            };
            self.m_connection.asyncDo(std::move(dispatcher));
        };

        boost::asio::defer(m_strand, makeWeakHandler(this, std::move(action)));
    }

    void InitialPhase::close()
    {
        auto const proxy = m_proxy.lock();
        if (!proxy)
        {
            logLine(LogLevel::warning, "Proxy already died when closing InitialPhase");
            return;
        }
        proxy->removeConnection(m_id);
    }

    void InitialPhase::extendLife()
    {
        auto waitHandler = [self = shared_from_this()](ErrorCode const& code)
        {
            if (code == boost::asio::error::operation_aborted)
            {
                return;
            }

            if (code.failed())
            {
                logLine(LogLevel::error, "Async wait failed: ", code);
            }

            logLine(LogLevel::info, "Closing self (natNegId ", self->m_id, ")");
            self->close();
        };
        m_timeout.asyncWait(std::chrono::minutes{ 1 }, std::move(waitHandler));
    }

    void InitialPhase::prepareForNextPacketToCommunicationAddress()
    {
        /*const auto action = [this]
        {*/
        auto handler = ReceiveHandler::create(this);
        m_communicationSocket.asyncReceiveFrom
        (
            handler->getBuffer(),
            handler->getFrom(),
            std::move(handler)
        );
        /*};
        socketReadyToReceive.asyncDo(action);*/
    }

    void InitialPhase::handlePacketFromServer(PacketView const packet)
    {
        auto const proxy = m_proxy.lock();
        if (!proxy)
        {
            logLine(LogLevel::warning, "Proxy already died when handling packet from server");
            close();
            return;
        }

        if (!packet.isNatNeg())
        {
            logLine(LogLevel::warning, "Packet from server is not NatNeg, discarded.");
            return;
        }

        logLine(LogLevel::info, "Packet from server will be processed by GameConnection.");
        // When handlePacketFromServer is called, connection should already be ready.
        auto const connection = m_connection->ref().lock();
        if (!connection)
        {
            logLine(LogLevel::warning, "Packet from server handler: aborting because connection expired");
            close();
            return;
        }

        connection->handleCommunicationPacketFromServer
        (
            packet, 
            m_clientCommunication
        );

        extendLife();
    }

    void InitialPhase::handlePacketToServerInternal
    (
        PacketView const packet, 
        EndPoint const& from,
        EndPoint const& server
    )
    {
        // TODO: Don't update address if packet is init and seqnum is not 1
        logLine(LogLevel::info, "Packet to server handler: NatNeg step ", packet.getStep());
        logLine(LogLevel::info, "Updating clientCommunication endpoint to ", from);
        m_clientCommunication = from;
        /*auto writeHandler = makeWeakWriteHandler
        (
            packet.copyBuffer(), 
            this, 
            [](InitialPhase& self) { return self.socketReadyToReceive; }
        );*/
        auto writeHandler = makeWriteHandler<InitialPhase>(packet.copyBuffer());
        m_communicationSocket.asyncSendTo
        (
            writeHandler.getData(),
            server,
            std::move(writeHandler)
        );

        extendLife();
    }
}
