#include <precompiled.hpp>
#include <IOManager.hpp>

namespace CNCOnlineForwarder::TCPProxy
{
    class TCPConnection : public std::enable_shared_from_this<TCPConnection>
    {
    public:
        using Strand = IOManager::StrandType;
        using EndPoint = boost::asio::ip::tcp::endpoint;
        using Socket = Utility::WithStrand<boost::asio::ip::tcp::socket>;
        using AddressV4 = boost::asio::ip::address_v4;

    private:
        Strand m_strand;
        Socket m_socketToClient;
        Socket m_socketToServer;

    public:
        static std::shared_ptr<TCPConnection> create
        (

        );
    };
}