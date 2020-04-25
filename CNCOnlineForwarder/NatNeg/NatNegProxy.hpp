#pragma once
#include <precompiled.hpp>
#include <IOManager.hpp>
#include <NatNeg/NatNegPacket.hpp>
#include <Utility/ProxyAddressTranslator.hpp>
#include <Utility/WithStrand.hpp>

namespace CNCOnlineForwarder::NatNeg
{
    class InitialPhase;
    class Connection;

    class NatNegProxy : public std::enable_shared_from_this<NatNegProxy>
    {
    public:
        using Strand = IOManager::StrandType;
        using EndPoint = boost::asio::ip::udp::endpoint;
        using Socket = Utility::WithStrand<boost::asio::ip::udp::socket>;
        using ProxyAddressTranslator = Utility::ProxyAddressTranslator;
        using AddressV4 = boost::asio::ip::address_v4;
        using PlayerID = NatNegPlayerID;
        using PacketView = NatNegPacketView;
    private:
        struct PrivateConstructor {};
        class ReceiveHandler;

    private:
        IOManager::ObjectMaker m_objectMaker;
        Strand m_proxyStrand;
        Socket m_serverSocket;
        std::string m_serverHostName;
        std::uint16_t m_serverPort;
        std::unordered_map<NatNegPlayerID, std::weak_ptr<InitialPhase>, NatNegPlayerID::Hash> m_initialPhases;
        std::shared_ptr<ProxyAddressTranslator> m_addressTranslator;

    public:
        static constexpr auto description = "NatNegProxy";

        static std::shared_ptr<NatNegProxy> create
        (
            IOManager::ObjectMaker const& objectMaker,
            std::string_view const serverHostName,
            std::uint16_t const serverPort,
            std::weak_ptr<ProxyAddressTranslator> const& addressTranslator
        );

        NatNegProxy
        (
            PrivateConstructor,
            IOManager::ObjectMaker const& objectMaker,
            std::string_view const serverHostName,
            std::uint16_t const serverPort,
            std::weak_ptr<ProxyAddressTranslator> const& addressTranslator
        );

        void sendFromProxySocket(PacketView const packetView, EndPoint const& to);

        void removeConnection(PlayerID const id);

    private:
        void prepareForNextPacketToServer();

        void handlePacketToServer(PacketView const packetView, EndPoint const& from);
    };
}