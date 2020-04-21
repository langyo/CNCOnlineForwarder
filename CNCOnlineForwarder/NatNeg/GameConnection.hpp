#pragma once
#include "precompiled.hpp"
#include <NatNeg/NatNegPacket.hpp>
#include <IOManager.hpp>
#include <Utility/ProxyAddressTranslator.hpp>
#include <Utility/WithStrand.hpp>

namespace CNCOnlineForwarder::NatNeg
{
    class NatNegProxy;
    class InitialPhase;

    class GameConnection : public std::enable_shared_from_this<GameConnection>
    {
    public:
        using Strand = IOManager::StrandType;
        using EndPoint = boost::asio::ip::udp::endpoint;
        using Socket = Utility::WithStrand<boost::asio::ip::udp::socket>;
        using Timer = Utility::WithStrand<boost::asio::steady_timer>;
        using ProxyAddressTranslator = Utility::ProxyAddressTranslator;
        using NatNegPlayerID = NatNegPlayerID;
        using PacketView = NatNegPacketView;
        using Buffer = std::unique_ptr<char[]>;
    private:
        struct PrivateConstructor {};
    private:
        Strand m_strand;
        std::weak_ptr<NatNegProxy> m_proxy;
        std::weak_ptr<ProxyAddressTranslator> m_addressTranslator;
        EndPoint m_server;
        EndPoint m_clientPublicAddress;
        EndPoint m_clientRealAddress;
        EndPoint m_remotePlayer;
        Socket m_publicSocketForClient;
        Socket m_fakeRemotePlayerSocket;
        Timer m_timeout;
    public:

        static constexpr auto description = "GameConnection";

        static std::shared_ptr<GameConnection> create
        (
            IOManager::ObjectMaker const& objectMaker,
            std::weak_ptr<NatNegProxy> const& proxy,
            std::weak_ptr<ProxyAddressTranslator> const& addressTranslator,
            EndPoint const& server,
            EndPoint const& clientPublicAddress
        );

        GameConnection
        (
            PrivateConstructor,
            IOManager::ObjectMaker const& objectMaker,
            std::weak_ptr<NatNegProxy> const& proxy,
            std::weak_ptr<ProxyAddressTranslator> const& addressTranslator,
            EndPoint const& server,
            EndPoint const& clientPublicAddress
        );

        EndPoint const& getClientPublicAddress() const noexcept;

        void handlePacketToServer(PacketView const packet);

        void handleCommunicationPacketFromServer
        (
            PacketView const packet,
            EndPoint const& communicationAddress
        );

    private:

        void extendLife();

        void prepareForNextPacketFromClient();

        void prepareForNextPacketToClient();

        void handlePacketFromServer(Buffer buffer, std::size_t const size);

        void handleCommunicationPacketFromServerInternal
        (
            PacketView const packet,
            EndPoint const& communicationAddress
        );

        void handlePacketFromRemotePlayer
        (
            Buffer buffer, 
            std::size_t const size, 
            EndPoint const& from
        );

        void handlePacketToRemotePlayer
        (
            Buffer buffer, 
            std::size_t const size, 
            EndPoint const& from
        );
    };
}
