#include <precompiled.hpp>
#include <IOManager.hpp>
#include <Utility/WithStrand.hpp>

namespace CNCOnlineForwarder::TCPProxy 
{
    class TCPProxy : public std::enable_shared_from_this<TCPProxy> 
    {
    public:
        using Strand = IOManager::StrandType;
        using EndPoint = boost::asio::ip::tcp::endpoint;
        using Acceptor = Utility::WithStrand<boost::asio::ip::tcp::acceptor>;
        using Socket = Utility::WithStrand<boost::asio::ip::tcp::socket>;
        using AddressV4 = boost::asio::ip::address_v4;
    private:
        struct PrivateConstructor {};

    public:
        static constexpr auto description = "TCPProxy";
    private:
        Strand m_strand;
        Acceptor m_acceptor;

    public:
        static std::shared_ptr<TCPProxy> create
        (
            IOManager::ObjectMaker const& objectMaker,
            std::uint16_t const localPort,
            std::string_view const serverHostName,
            std::uint16_t const serverPort
        );

        TCPProxy
        (
            PrivateConstructor,
            IOManager::ObjectMaker const& objectMaker,
            std::uint16_t const localPort,
            std::string_view const serverHostName,
            std::uint16_t const serverPort
        );

    private:
        void prepareForNextConnection();

    };
}