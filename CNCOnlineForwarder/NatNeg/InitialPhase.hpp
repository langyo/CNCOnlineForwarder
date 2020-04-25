#pragma once
#include <precompiled.hpp>
#include <NatNeg/NatNegPacket.hpp>
#include <IOManager.hpp>
#include <Utility/PendingActions.hpp>
#include <Utility/ProxyAddressTranslator.hpp>
#include <Utility/WithStrand.hpp>

namespace CNCOnlineForwarder::NatNeg
{
    class NatNegProxy;
    class GameConnection;

    class InitialPhase : public std::enable_shared_from_this<InitialPhase>
    {
    public:
        using Strand = IOManager::StrandType;
        using EndPoint = boost::asio::ip::udp::endpoint;
        using Socket = Utility::WithStrand<boost::asio::ip::udp::socket>;
        using Timer = Utility::WithStrand<boost::asio::steady_timer>;
        using Resolver = Utility::WithStrand<boost::asio::ip::udp::resolver>;
        using ProxyAddressTranslator = Utility::ProxyAddressTranslator;
        using NatNegPlayerID = NatNegPlayerID;
        using PacketView = NatNegPacketView;
        
    private:
        class ReceiveHandler;

        struct PrivateConstructor {};

        class PromisedEndPoint
        {
        public:
            using ActionType = std::function<void(EndPoint const&)>;

        private:
            std::optional<EndPoint> m_endPoint;

        public:

            EndPoint const& getEndPoint() const { return m_endPoint.value(); }
            void setEndPoint(EndPoint&& value) { m_endPoint = std::move(value); }

            template<typename Action>
            void apply(Action&& action) { action(m_endPoint.value()); }

            bool isReady() const noexcept { return m_endPoint.has_value(); }
        };

        class PromisedConnection
        {
        public:
            using ActionType = std::function<void(std::weak_ptr<GameConnection>)>;

        private:
            std::weak_ptr<GameConnection> m_ref;

        public:

            std::weak_ptr<GameConnection>& ref() noexcept { return m_ref; }

            template<typename Action>
            void apply(Action&& action) { action(m_ref); }

            bool isReady() const noexcept
            {
                auto const defaultValue = decltype(m_ref){};
                // from https://stackoverflow.com/a/45507610/4399840
                auto const isNotAssignedYet =
                    (!m_ref.owner_before(defaultValue))
                    &&
                    (!defaultValue.owner_before(m_ref));
                return !isNotAssignedYet;
            }
        };

        using FutureEndPoint = Utility::PendingActions<PromisedEndPoint>;
        using FutureConnection = Utility::PendingActions<PromisedConnection>;

    private:
        Strand m_strand;
        Resolver m_resolver;
        Socket m_communicationSocket;
        Timer m_timeout;

        std::weak_ptr<NatNegProxy> m_proxy;
        FutureConnection m_connection;

        NatNegPlayerID m_id;
        FutureEndPoint m_server;
        EndPoint m_clientCommunication;

    public:
        static constexpr auto description = "InitialPhase";

        static std::shared_ptr<InitialPhase> create
        (
            IOManager::ObjectMaker const& objectMaker,
            std::weak_ptr<NatNegProxy> const& proxy,
            NatNegPlayerID const id,
            std::string const& natNegServer,
            std::uint16_t const natNegPort
        );

        InitialPhase
        (
            PrivateConstructor,
            IOManager::ObjectMaker const& objectMaker,
            std::weak_ptr<NatNegProxy> const& proxy,
            NatNegPlayerID const id
        );

        InitialPhase(InitialPhase const&) = delete;
        InitialPhase& operator=(InitialPhase const&) = delete;

        void prepareGameConnection
        (
            IOManager::ObjectMaker const& objectMaker,
            std::weak_ptr<ProxyAddressTranslator> const& addressTranslator,
            EndPoint const& client
        );

        void handlePacketToServer(PacketView const packet, EndPoint const& from);

    private:
        void close();

        void extendLife();

        void prepareForNextPacketToCommunicationAddress();

        void handlePacketFromServer(PacketView const packet);

        void handlePacketToServerInternal
        (
            PacketView const packet,
            EndPoint const& from,
            EndPoint const& server
        );
    };
}