#include "NatNegProxy.hpp"
#include <precompiled.hpp>
#include <NatNeg/InitialPhase.hpp>
#include <Logging/Logging.hpp>
#include <Utility/SimpleWriteHandler.hpp>
#include <Utility/WeakRefHandler.hpp>

using UDP = boost::asio::ip::udp;
using ErrorCode = boost::system::error_code;
using LogLevel = CNCOnlineForwarder::Logging::Level;
using WriteHandler = CNCOnlineForwarder::Utility::SimpleWriteHandler<CNCOnlineForwarder::NatNeg::NatNegProxy>;
using CNCOnlineForwarder::Utility::makeWeakHandler;

namespace CNCOnlineForwarder::NatNeg
{
    template<typename... Arguments>
    void logLine(LogLevel level, Arguments&&... arguments)
    {
        return Logging::logLine<NatNegProxy>(level, std::forward<Arguments>(arguments)...);
    }

    class NatNegProxy::ReceiveHandler
    {
    private:
        std::unique_ptr<std::array<char, 1024>> m_buffer;
        std::unique_ptr<EndPoint> m_from;

    public:
        static auto create(NatNegProxy* pointer)
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

        void operator()(NatNegProxy& self, ErrorCode const& code, std::size_t const bytesReceived) const
        {
            self.prepareForNextPacketToServer();

            if (code.failed())
            {
                logLine(LogLevel::error, "Async receive failed: ", code);
                return;
            }

            auto const view = PacketView{ {m_buffer->data(), bytesReceived} };
            self.handlePacketToServer(view, *m_from);
        }

    private:
        ReceiveHandler() :
            m_buffer{ std::make_unique<std::array<char, 1024>>() },
            m_from{ std::make_unique<EndPoint>() }
        {}
    };

    std::shared_ptr<NatNegProxy> NatNegProxy::create
    (
        IOManager::ObjectMaker const& objectMaker,
        std::string_view const serverHostName,
        std::uint16_t const serverPort,
        std::weak_ptr<ProxyAddressTranslator> const& addressTranslator
    )
    {
        auto const self = std::make_shared<NatNegProxy>
        (
            PrivateConstructor{},
            objectMaker, 
            serverHostName,
            serverPort,
            addressTranslator
        );

        auto const action = [](NatNegProxy& self)
        {
            logLine(LogLevel::info, "NatNegProxy created.");
            self.prepareForNextPacketToServer();
        };
        boost::asio::defer(self->m_proxyStrand, makeWeakHandler(self, action));

        return self;
    }

    NatNegProxy::NatNegProxy
    (
        PrivateConstructor,
        IOManager::ObjectMaker const& objectMaker,
        std::string_view const serverHostName,
        std::uint16_t const serverPort,
        std::weak_ptr<ProxyAddressTranslator> const& addressTranslator
    ) :
        m_objectMaker{ objectMaker },
        m_proxyStrand{ objectMaker.makeStrand() },
        m_serverSocket{ m_proxyStrand, EndPoint{ UDP::v4(), serverPort } },
        m_serverHostName{ serverHostName },
        m_serverPort{ serverPort },
        m_addressTranslator{ addressTranslator }
    {}

    void NatNegProxy::sendFromProxySocket(PacketView const packetView, EndPoint const& to)
    {
        auto action = [data = packetView.copyBuffer(), to](NatNegProxy& self)
        {
            logLine(LogLevel::info, "Sending data to ", to);
            auto writeHandler = WriteHandler{ data };
            self.m_serverSocket.asyncSendTo
            (
                writeHandler.getData(), 
                to, 
                std::move(writeHandler)
            );
        };

        boost::asio::defer
        (
            m_proxyStrand,
            makeWeakHandler(this, std::move(action))
        );
    }

    void NatNegProxy::removeConnection(PlayerID const id)
    {
        auto action = [id](NatNegProxy& self)
        {
            logLine(LogLevel::error, "Removing InitaialPhase ", id);
            self.m_initialPhases.erase(id);
        };

        boost::asio::defer
        (
            m_proxyStrand, 
            makeWeakHandler(this, std::move(action))
        );
    }

    void NatNegProxy::prepareForNextPacketToServer()
    {
        auto handler = ReceiveHandler::create(this);
        m_serverSocket.asyncReceiveFrom
        (
            handler->getBuffer(),
            handler->getFrom(),
            std::move(handler)
        );
    }

    void NatNegProxy::handlePacketToServer(PacketView const packet, EndPoint const& from)
    {
        if (!packet.isNatNeg())
        {
            logLine(LogLevel::warning, "Packet is not natneg, discarded.");
            return;
        }

        auto const step = packet.getStep();
        auto const playerIDHolder = packet.getNatNegPlayerID();
        if (!playerIDHolder.has_value())
        {
            logLine(LogLevel::info, "Packet of step ", step, " does not have NatNegPlayerID, discarded.");
            return;
        }
        auto const playerID = playerIDHolder.value();

        auto& initialPhaseRef = m_initialPhases[playerID];
        if (initialPhaseRef.expired())
        {
            logLine(LogLevel::info, "New NatNegPlayerID, creating InitialPhase: ", playerID);
            initialPhaseRef = InitialPhase::create
            (
                m_objectMaker,
                weak_from_this(),
                playerID,
                m_serverHostName,
                m_serverPort
            );
        }

        auto const initialPhase = initialPhaseRef.lock();
        if (!initialPhase)
        {
            logLine(LogLevel::error, "InitialPhase already expired: ", playerID);
            removeConnection(playerID);
            return;
        }

        logLine(LogLevel::info, "Processing packet (step ", step, ") from ", from);
        if (step == NatNegStep::init)
        {
            constexpr auto sequenceNumberOffset = 12;
            auto const sequenceNumber = 
                static_cast<int>(packet.getView().at(sequenceNumberOffset));

            logLine(LogLevel::info, "Init packet, seq num = ", sequenceNumber);

            if (sequenceNumber == 0)
            {
                // Packet is from client public address
                logLine(LogLevel::info, "Preparing GameConnection, client = ", from);
                initialPhase->prepareGameConnection
                (
                    m_objectMaker, 
                    m_addressTranslator, 
                    from
                );
            }
        }

        initialPhase->handlePacketToServer(packet, from);
    }
}