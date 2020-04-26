#include "TCPConnection.hpp"
#include <precompiled.hpp>
#include <Logging/Logging.hpp>
#include <Utility/WeakRefHandler.hpp>

using LogLevel = CNCOnlineForwarder::Logging::Level;

using CNCOnlineForwarder::Utility::makeWeakHandler;

namespace CNCOnlineForwarder::TCPProxy
{
    template<typename... Arguments>
    void logLine(LogLevel level, Arguments&&... arguments)
    {
        return CNCOnlineForwarder::Logging::logLine<TCPConnection>(level, std::forward<Arguments>(arguments)...);
    }

    std::shared_ptr<TCPConnection> TCPConnection::create
    (
        IOManager::ObjectMaker const& objectMaker,
        Socket::Type acceptedSocket,
        EndPoint const&/* serverEndPoint*/
    )
    {
        auto const self = std::make_shared<TCPConnection>
        (
            PrivateConstructor{},
            objectMaker,
            std::move(acceptedSocket)
        );

        /*auto const action = [serverEndPoint](TCPConnection& self)
        {
            logLine(LogLevel::info, "TCPConnection created, connecting to remote server...");
            auto const onConnect = [](TCPConnection& self)
            {
                self.readFromClient();
                self.readFromServer();
            };
            self.m_socketToServer->async_connect(serverEndPoint, makeWeakHandler(self, onConnect));
        };
        boost::asio::defer(self->m_strand, makeWeakHandler(self, action));*/

        return self;
    }

    TCPConnection::TCPConnection
    (
        PrivateConstructor,
        IOManager::ObjectMaker const& objectMaker,
        Socket::Type&& acceptedSocket
    ) :
        m_strand{ objectMaker.makeStrand() },
        m_socketToClient{ m_strand, acceptedSocket.remote_endpoint().protocol(), acceptedSocket.release() },
        m_socketToServer{ m_strand },
        m_lastClientReply{ SteadyClock::now() },
        m_lastServerReply{ SteadyClock::now() }
    {}

    void TCPConnection::readFromClient()
    {
        /*m_clientReceiveBuffer.resize(bufferSize);
        auto const buffer = boost::asio::buffer(m_clientReceiveBuffer);*/

        //m_socketToClient->async_read_some(buffer, makeWeakHandler(this, onRead));
    }

}