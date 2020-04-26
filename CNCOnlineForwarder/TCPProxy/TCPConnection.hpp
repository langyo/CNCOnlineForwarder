#include <precompiled.hpp>
#include <IOManager.hpp>
#include <Utility/WithStrand.hpp>

namespace CNCOnlineForwarder::TCPProxy
{
    class TCPConnection : public std::enable_shared_from_this<TCPConnection>
    {
    public:
        using Strand = IOManager::StrandType;
        using EndPoint = boost::asio::ip::tcp::endpoint;
        using Socket = Utility::WithStrand<boost::asio::ip::tcp::socket>;
        using AddressV4 = boost::asio::ip::address_v4;
        using SteadyClock = std::chrono::steady_clock;
        using TimePoint = SteadyClock::time_point;
        using Buffer = std::vector<std::byte>;
    private:
        struct PrivateConstructor {};

    public:
        static constexpr auto description = "TCPConnection";
        static constexpr auto bufferSize = 512;
    private:
        Strand m_strand;
        Socket m_socketToClient;
        Socket m_socketToServer;
        TimePoint m_lastClientReply;
        TimePoint m_lastServerReply;
        Buffer m_clientReceiveBuffer;
        Buffer m_serverReceiveBuffer;
        std::vector<Buffer> m_clientSendBuffer;
        std::vector<Buffer> m_serverSendBuffer;

    public:
        static std::shared_ptr<TCPConnection> create
        (
            IOManager::ObjectMaker const& objectMaker,
            Socket::Type acceptedSocket,
            EndPoint const& serverEndPoint
        );

        TCPConnection
        (
            PrivateConstructor,
            IOManager::ObjectMaker const& objectMaker,
            Socket::Type&& acceptedSocket
        );

    private:
        void readFromClient();
        void readFromServer();
        
    };
}