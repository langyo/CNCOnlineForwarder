#include "precompiled.hpp"
#include "GameConnection.hpp"
#include "NatNegProxy.hpp"
#include <Logging/Logging.hpp>
#include <Utility/ProxyAddressTranslator.hpp>
#include <Utility/WeakRefHandler.hpp>


using UDP = boost::asio::ip::udp;
using ErrorCode = boost::system::error_code;
using LogLevel = CNCOnlineForwarder::Logging::Level;

using CNCOnlineForwarder::Utility::makeWeakHandler;

namespace CNCOnlineForwarder::NatNeg
{
    template<typename... Arguments>
    void logLine(LogLevel level, Arguments&&... arguments)
    {
        return Logging::logLine<GameConnection>(level, std::forward<Arguments>(arguments)...);
    }

    template<typename NextAction, typename Handler>
    class ReceiveHandler
    {
    private:
        std::size_t m_size;
        GameConnection::Buffer m_buffer;
        std::unique_ptr<GameConnection::EndPoint> m_from;
        NextAction m_nextAction;
        Handler m_handler;
    public:
        template<typename InputNextAction, typename InputNextHandler>
        ReceiveHandler(InputNextAction&& nextAction, InputNextHandler&& handler) :
            m_size{ 512 },
            m_buffer{ std::make_unique<char[]>(m_size) },
            m_from{ std::make_unique<GameConnection::EndPoint>() },
            m_nextAction{ std::forward<InputNextAction>(nextAction) },
            m_handler{ std::forward<InputNextHandler>(handler) }
        {}

        boost::asio::mutable_buffer getBuffer() const noexcept
        {
            return boost::asio::buffer(m_buffer.get(), m_size);
        }

        GameConnection::EndPoint& getFrom() const noexcept
        {
            return *m_from;
        }

        void operator()
        (
            GameConnection& self, 
            ErrorCode const& code, 
            std::size_t const bytesReceived
        )
        {
            m_nextAction(self);

            if (code.failed())
            {
                logLine(LogLevel::error, "Async receive failed: ", code);
                return;
            }

            if (bytesReceived >= m_size)
            {
                logLine(LogLevel::warning, "Received data may be truncated: ", bytesReceived, "/",  m_size);
            }

            return m_handler
            (
                self, 
                std::move(m_buffer), 
                bytesReceived, 
                getFrom()
            );
        }
    };

    template<typename NextAction, typename Handler>
    auto makeReceiveHandler
    (
        GameConnection* pointer, 
        NextAction&& nextAction, 
        Handler&& hanlder
    )
    {
        using NextActionValue = std::remove_reference_t<NextAction>;
        using HandlerValue = std::remove_reference_t<Handler>;

        return makeWeakHandler
        (
            pointer, 
            ReceiveHandler<NextActionValue, HandlerValue>
            {
                std::forward<NextAction>(nextAction),
                std::forward<Handler>(hanlder)
            }
        );
    }

    class SendHandler
    {
    private:
        GameConnection::Buffer m_buffer;
        std::size_t m_bytes;
    public:
        SendHandler
        (
            GameConnection::Buffer buffer,
            std::size_t const bytes
        ) :
            m_buffer{ std::move(buffer) },
            m_bytes{ bytes }
        {}

        boost::asio::const_buffer getBuffer() const noexcept
        {
            return boost::asio::buffer(m_buffer.get(), m_bytes);
        }

        void operator()(ErrorCode const& code, std::size_t const bytesSent) const
        {
            if (code.failed())
            {
                logLine(LogLevel::error, "Async write failed: ", code);
                return;
            }

            if (bytesSent != m_bytes)
            {
                logLine(LogLevel::error, "Only part of packet was sent: ", bytesSent, "/", m_bytes);
                return;
            }
        }
    };

    std::shared_ptr<GameConnection> GameConnection::create
    (
        IOManager::ObjectMaker const& objectMaker,
        std::weak_ptr<NatNegProxy> const& proxy,
        std::weak_ptr<ProxyAddressTranslator> const& addressTranslator,
        EndPoint const& server,
        EndPoint const& client
    )
    {
        auto const self = std::make_shared<GameConnection>
        (
            PrivateConstructor{},
            objectMaker, 
            proxy, 
            addressTranslator,
            server, 
            client
        );

        auto const action = [self]
        {
            logLine(LogLevel::info, "New Connection ", self, " created, client = ", self->m_clientPublicAddress);
            self->extendLife();
            self->prepareForNextPacketToClient();
        };
        boost::asio::defer(self->m_strand, action);

        return self;
    }

    GameConnection::GameConnection
    (
        PrivateConstructor,
        IOManager::ObjectMaker const& objectMaker,
        std::weak_ptr<NatNegProxy> const& proxy,
        std::weak_ptr<ProxyAddressTranslator> const& addressTranslator,
        EndPoint const& server,
        EndPoint const& clientPublicAddress
    ) :
        m_strand{ objectMaker.makeStrand() },
        m_proxy{ proxy },
        m_addressTranslator{ addressTranslator },
        m_server{ server },
        m_clientPublicAddress{ clientPublicAddress },
        m_clientRealAddress{ clientPublicAddress },
        m_remotePlayer{},
        m_publicSocketForClient{ m_strand, EndPoint{ UDP::v4(), 0 } },
        m_fakeRemotePlayerSocket{ m_strand, EndPoint{ UDP::v4(), 0 } },
        m_timeout{ m_strand }
    {}

    GameConnection::EndPoint const& GameConnection::getClientPublicAddress() const noexcept
    {
        return m_clientPublicAddress;
    }

    void GameConnection::handlePacketToServer(PacketView const packet)
    {
        auto action = [data = packet.copyBuffer()](GameConnection& self)
        {
            auto const packet = PacketView{ data };
            if (!packet.isNatNeg())
            {
                logLine(LogLevel::warning, "Packet to server is not NatNeg, discarded.");
                return;
            }

            logLine(LogLevel::info, "Packet to server handler: NatNeg step ", packet.getStep());
            logLine(LogLevel::info, "Sending data to server through client public socket...");

            auto const& packetContent = packet.getView();
            auto copy = std::make_unique<char[]>(packetContent.size());
            std::copy_n(packetContent.begin(), packetContent.size(), copy.get());
            auto handler = SendHandler{ std::move(copy), packetContent.size() };
            self.m_publicSocketForClient.asyncSendTo
            (
                handler.getBuffer(),
                self.m_server,
                std::move(handler)
            );

            self.extendLife();
        };

        boost::asio::defer(m_strand, makeWeakHandler(this, std::move(action)));
    }

    void GameConnection::handleCommunicationPacketFromServer
    (
        PacketView const packet,
        EndPoint const& communicationAddress
    )
    {
        auto action = [data = packet.copyBuffer(), communicationAddress](GameConnection& self)
        {
            return self.handleCommunicationPacketFromServerInternal
            (
                PacketView{ data },
                communicationAddress
            );
        };

        boost::asio::defer(m_strand, makeWeakHandler(this, std::move(action)));
    }

    void GameConnection::extendLife()
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

            logLine(LogLevel::error, "Timeout reached, closing self: ", self.get());
        };

        m_timeout.asyncWait(std::chrono::minutes{ 1 }, std::move(waitHandler));
    }

    void GameConnection::prepareForNextPacketFromClient()
    {
        auto const then = [](GameConnection& self)
        {
            return self.prepareForNextPacketFromClient();
        };

        auto const dispatcher = []
        (
            GameConnection& self, 
            Buffer&& data, 
            std::size_t const size,
            EndPoint const& from
        )
        {
            return self.handlePacketToRemotePlayer(std::move(data), size, from);
        };

        auto handler = makeReceiveHandler(this, then, dispatcher);
        m_fakeRemotePlayerSocket.asyncReceiveFrom
        (
            handler->getBuffer(),
            handler->getFrom(),
            std::move(handler)
        );
    }

    void GameConnection::prepareForNextPacketToClient()
    {
        auto const then = [](GameConnection& self)
        {
            return self.prepareForNextPacketToClient();
        };

        auto const dispatcher = []
        (
            GameConnection& self, 
            Buffer&& data,
            std::size_t const size,
            EndPoint const& from
        )
        {
            if (from == self.m_server)
            {
                return self.handlePacketFromServer(std::move(data), size);
            }

            return self.handlePacketFromRemotePlayer(std::move(data), size, from);
        };
        auto handler = makeReceiveHandler(this, then, dispatcher);
        m_publicSocketForClient.asyncReceiveFrom
        (
            handler->getBuffer(),
            handler->getFrom(),
            std::move(handler)
        );
    }

    void GameConnection::handlePacketFromServer(Buffer buffer, std::size_t const size)
    {
        auto const proxy = m_proxy.lock();
        if (!proxy)
        {
            logLine(LogLevel::warning, "Proxy already died when handling packet from server");
            return;
        }

        auto const packet = PacketView{ { buffer.get(), size } };

        if (!packet.isNatNeg())
        {
            logLine(LogLevel::warning, "Packet from server is not NatNeg, discarded.");
            return;
        }

        logLine(LogLevel::info, "Packet from server handler: NatNeg step ", packet.getStep());
        logLine(LogLevel::info, "Packet from server will be send to client from proxy.");
        proxy->sendFromProxySocket(packet, m_clientPublicAddress);

        extendLife();
    }

    void GameConnection::handleCommunicationPacketFromServerInternal
    (
        PacketView const packet,
        EndPoint const& communicationAddress
    )
    {
        auto const proxy = m_proxy.lock();
        if (!proxy)
        {
            logLine(LogLevel::error, "Proxy already died when handling CommPacket from server");
            return;
        }

        logLine(LogLevel::info, "CommPacket handler: NatNeg step ", packet.getStep());

        auto outputBuffer = std::string{ packet.getView() };
        auto const addressOffset = PacketView::getAddressOffset(packet.getStep());
        if (addressOffset.has_value())
        {
            logLine(LogLevel::info, "CommPacket contains address, will try to rewrite it");

            {
                auto const[ip, port] = parseAddress(packet.getView(), addressOffset.value());
                m_remotePlayer.address(boost::asio::ip::address_v4{ ip });
                m_remotePlayer.port(boost::endian::big_to_native(port));
                logLine(LogLevel::info, "CommPacket's address stored in m_remotePlayer: ", m_remotePlayer);
            }

            auto const fakeRemotePlayerAddress = m_fakeRemotePlayerSocket->local_endpoint();
            logLine(LogLevel::info, "FakeRemote local endpoint:", fakeRemotePlayerAddress);
            auto const addressTranslator = m_addressTranslator.lock();
            if (!addressTranslator)
            {
                logLine(LogLevel::error, "AddressTranslator already died when rewriting CommPacket");
                return;
            }
            auto const publicRemoteFakeAddress = 
                addressTranslator->localToPublic(fakeRemotePlayerAddress);
            auto const ip = publicRemoteFakeAddress.address().to_v4().to_bytes();
            auto const port = boost::endian::native_to_big(publicRemoteFakeAddress.port());
            rewriteAddress(outputBuffer, addressOffset.value(), ip, port);

            logLine(LogLevel::info, "Address rewritten as ", publicRemoteFakeAddress);
            logLine(LogLevel::info, "Preparing to receive packet from player to fakeRemote");
            prepareForNextPacketFromClient();
        }
        logLine(LogLevel::info, "CommPacket from server will be send to client from proxy.");
        proxy->sendFromProxySocket(PacketView{ outputBuffer }, communicationAddress);

        extendLife();
    }

    void GameConnection::handlePacketFromRemotePlayer
    (
        Buffer buffer,
        std::size_t const size,
        EndPoint const& from
    )
    {
        if (m_remotePlayer != from)
        {
            logLine(LogLevel::warning, "Updating remote player address from ", m_remotePlayer, " to ", from);
            m_remotePlayer = from;
        }

        if (PacketView{ { buffer.get(), size } }.isNatNeg())
        {
            logLine(LogLevel::info, "Forwarding NatNeg Packet from remote ", m_remotePlayer, " to ", m_clientRealAddress);
        }

        auto handler = SendHandler{ std::move(buffer), size };
        m_fakeRemotePlayerSocket.asyncSendTo
        (
            handler.getBuffer(),
            m_clientRealAddress,
            std::move(handler)
        );

        extendLife();
    }

    void GameConnection::handlePacketToRemotePlayer
    (
        Buffer buffer,
        std::size_t const size,
        EndPoint const& from
    )
    {
        if (from != m_clientRealAddress)
        {
            logLine(LogLevel::warning, "Updating client address from ", m_clientRealAddress, " to ", from);
            m_clientRealAddress = from;
        }

        if (PacketView{ { buffer.get(), size } }.isNatNeg())
        {
            logLine(LogLevel::info, "Forwarding NatNeg Packet from client ", m_remotePlayer, " to ", m_clientRealAddress);
        }

        auto handler = SendHandler{ std::move(buffer), size };
        m_publicSocketForClient.asyncSendTo
        (
            handler.getBuffer(),
            m_remotePlayer,
            std::move(handler)
        );

        extendLife();
    }
}