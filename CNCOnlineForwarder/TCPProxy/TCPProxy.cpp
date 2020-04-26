#include "TCPProxy.hpp"
#include <precompiled.hpp>
#include <Utility/WeakRefHandler.hpp>

using TCP = boost::asio::ip::tcp;
using ErrorCode = boost::system::error_code;
using LogLevel = CNCOnlineForwarder::Logging::Level;

using CNCOnlineForwarder::Utility::makeWeakHandler;

namespace CNCOnlineForwarder::TCPProxy
{
    template<typename... Arguments>
    void logLine(LogLevel level, Arguments&&... arguments)
    {
        return CNCOnlineForwarder::Logging::logLine<TCPProxy>(level, std::forward<Arguments>(arguments)...);
    }

    std::shared_ptr<TCPProxy> TCPProxy::create
    (
        IOManager::ObjectMaker const& objectMaker,
        std::uint16_t const localPort,
        std::string_view const serverHostName,
        std::uint16_t const serverPort
    )
    {
        auto const self = std::make_shared<TCPProxy>
        (
            PrivateConstructor{},
            objectMaker,
            localPort,
            serverHostName,
            serverPort
        );

        auto const action = [](TCPProxy& self)
        {
            logLine(LogLevel::info, "TCPProxy created.");
            self.prepareForNextConnection();
        };
        boost::asio::defer(self->m_strand, makeWeakHandler(self, action));

        return self;
    }

    TCPProxy::TCPProxy
    (
        PrivateConstructor,
        IOManager::ObjectMaker const& objectMaker,
        std::uint16_t const localPort,
        std::string_view const/* serverHostName*/,
        std::uint16_t const/* serverPort*/
    ) :
        m_strand{ objectMaker.makeStrand() },
        m_acceptor{ m_strand, EndPoint{TCP::v4(), localPort} }
    {}

    void TCPProxy::prepareForNextConnection()
    {
        auto const handler = [](TCPProxy& self, ErrorCode const& code, Socket::Type socket)
        {
            self.prepareForNextConnection();
            if (code.failed())
            {
                logLine(LogLevel::error, "Accept failed: ", code);
                return;
            }
        };
        m_acceptor->async_accept(makeWeakHandler(this, handler));
    }
}