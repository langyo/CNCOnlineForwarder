#pragma once
#include <precompiled.hpp>
#include <IOManager.hpp>

namespace CNCOnlineForwarder::Utility
{
    class ProxyAddressTranslator : public std::enable_shared_from_this<ProxyAddressTranslator>
    {
    public:
        using AddressV4 = boost::asio::ip::address_v4;
        using UDPEndPoint = boost::asio::ip::udp::endpoint;
    private:
        struct PrivateConstructor {};
    private:
        IOManager::ObjectMaker m_objectMaker;
        std::mutex mutable m_mutex;
        AddressV4 m_publicAddress;
    public:

        static constexpr auto description = "ProxyAddressTranslator";

        static std::shared_ptr<ProxyAddressTranslator> create
        (
            IOManager::ObjectMaker const& objectMaker
        );

        ProxyAddressTranslator
        (
            PrivateConstructor,
            IOManager::ObjectMaker const& objectMaker
        );

        AddressV4 getUntranslated() const;

        void setPublicAddress(AddressV4 const& newPublicAddress);

        UDPEndPoint localToPublic(UDPEndPoint const& endPoint) const;

    private:
        static void periodicallySetPublicAddress
        (
            std::weak_ptr<ProxyAddressTranslator> const& ref
        );
    };
}



