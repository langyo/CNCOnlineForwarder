#include "precompiled.h"
#include <IOManager.hpp>

namespace CNCOnlineForwarder::TCPProxy 
{
    class TCPProxy : std::enable_shared_from_this<TCPProxy> 
    {
    private:
        struct PrivateConstructor {};

    private:
        using Strand = IOManager::StrandType;
        using EndPoint = boost::asio::ip::tcp::endpoint;
        using Socket = Utility::WithStrand<boost::asio::ip::tcp::socket>;
        using AddressV4 = boost::asio::ip::address_v4;

    public:
        static constexpr auto description = "TCPProxy";

        static std::shared_ptr<TCPProxy> create
        (
            IOManager::ObjectMaker const& objectMaker,
            std::string_view const serverHostName,
            std::uint16_t const serverPort
        );

        TCPProxy
        (
            PrivateConstructor,
            IOManager::ObjectMaker const& objectMaker,
            std::string_view const serverHostName,
            std::uint16_t const serverPort
        );
    };
}