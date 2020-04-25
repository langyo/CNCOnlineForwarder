#include "SimpleHTTPClient.hpp"
#include <precompiled.hpp>
#include <Logging/Logging.hpp>
#include <Utility/WithStrand.hpp>

namespace CNCOnlineForwarder::Utility
{
    // From https://www.boost.org/doc/libs/1_70_0/libs/beast/example/http/client/async/http_client_async.cpp
    // Performs an HTTP GET and prints the response
    class SimpleHTTPClient : public std::enable_shared_from_this<SimpleHTTPClient>
    {
    private:
        using Strand = IOManager::StrandType;
        using Resolver = Utility::WithStrand<boost::asio::ip::tcp::resolver>;
        using TCPStream = boost::beast::tcp_stream;
        using FlatBuffer = boost::beast::flat_buffer;
        using HTTPRequest = boost::beast::http::request<boost::beast::http::empty_body>;
        using HTTPResponse = boost::beast::http::response<boost::beast::http::string_body>;

        using ErrorCode = boost::beast::error_code;
        using TCPEndPoint = boost::asio::ip::tcp::resolver::endpoint_type;
        using ResolvedHostName = boost::asio::ip::tcp::resolver::results_type;

        using LogLevel = Logging::Level;

        struct PrivateConstructor {};

    private:
        Strand m_strand;
        Resolver m_resolver;
        TCPStream m_stream;
        FlatBuffer m_buffer; // (Must persist between reads)
        HTTPRequest m_request;
        HTTPResponse m_response;
        std::function<void(std::string)> m_onGet;

    public:
        static constexpr auto description = "SimpleHTTPClient";

        static void startGet
        (
            IOManager::ObjectMaker const& objectMaker,
            std::string_view const hostName,
            std::string_view const target,
            std::function<void(std::string)> onGet
        )
        {
            auto const session = std::make_shared<SimpleHTTPClient>
            (
                PrivateConstructor{},
                objectMaker, 
                std::move(onGet)
            );
            session->run(hostName, "80", target, 11);
        }

        // Objects are constructed with a strand to
        // ensure that handlers do not execute concurrently.
        SimpleHTTPClient
        (
            PrivateConstructor,
            IOManager::ObjectMaker const& objectMaker,
            std::function<void(std::string)>&& onGet
        ) :
            m_strand{ objectMaker.makeStrand() },
            m_resolver{ m_strand },
            m_stream{ m_strand },
            m_buffer{},
            m_request{},
            m_response{},
            m_onGet{ std::move(onGet) }
        {}

    private:
        template<typename... Arguments>
        static void log(LogLevel const level, Arguments&&... arguments)
        {
            return Logging::logLine<SimpleHTTPClient>(level, std::forward<Arguments>(arguments)...);
        }

        // Start the asynchronous operation
        void run
        (
            std::string_view const host,
            std::string_view const port,
            std::string_view const target,
            int version
        )
        {
            namespace Http = boost::beast::http;
            // Set up an HTTP GET request message
            m_request.version(version);
            m_request.method(Http::verb::get);
            m_request.target({ target.data(), target.size() });
            m_request.set(Http::field::host, host);
            m_request.set(Http::field::user_agent, BOOST_BEAST_VERSION_STRING);

            log(LogLevel::info, "Starting HTTP Get on ", host, "/", target);

            // Look up the domain name
            m_resolver.asyncResolve
            (
                host,
                port,
                boost::beast::bind_front_handler
                (
                    &SimpleHTTPClient::onResolve,
                    shared_from_this()
                )
            );
        }

        void onResolve(ErrorCode const& code, ResolvedHostName const& results)
        {
            if (code.failed())
            {
                log(LogLevel::error, "Cannot resolve hostname: ", code);
                return;
            }

            // Set a timeout on the operation
            m_stream.expires_after(std::chrono::seconds(30));

            log(LogLevel::info, "Hostname resolved. Connecting...");

            // Make the connection on the IP address we get from a lookup
            m_stream.async_connect
            (
                results,
                boost::beast::bind_front_handler
                (
                    &SimpleHTTPClient::onConnect,
                    shared_from_this()
                )
            );
        }

        void onConnect(ErrorCode const& code, TCPEndPoint const& endPoint)
        {
            if (code.failed())
            {
                log(LogLevel::error, "Connect failed: ", code);
                return;
            }

            // Set a timeout on the operation
            m_stream.expires_after(std::chrono::seconds(30));

            log(LogLevel::info, "Connected to ", endPoint, "; Writing header.");

            // Send the HTTP request to the remote host
            boost::beast::http::async_write
            (
                m_stream,
                m_request,
                boost::beast::bind_front_handler
                (
                    &SimpleHTTPClient::onWrite,
                    shared_from_this()
                )
            );
        }

        void onWrite(ErrorCode const& code, std::size_t const /* bytesTransferred */)
        {
            if (code.failed())
            {
                log(LogLevel::error, "Async write failed: ", code);
                return;
            }

            log(LogLevel::info, "Start receiving response.");

            // Receive the HTTP response
            boost::beast::http::async_read
            (
                m_stream,
                m_buffer,
                m_response,
                boost::beast::bind_front_handler
                (
                    &SimpleHTTPClient::onRead,
                    shared_from_this()
                )
            );
        }

        void onRead(ErrorCode const& code, std::size_t const /* bytesTransferred */)
        {
            if (code.failed())
            {
                log(LogLevel::error, "Async read failed: ", code);
                return;
            }

            // Write the message to standard out
            auto stringStream = std::stringstream{};
            stringStream << m_response.body();
            
            log(LogLevel::info, "Response read.");
            m_onGet(stringStream.str());

            // Gracefully close the socket
            try
            {
                m_stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
            }
            catch (std::exception const& error)
            {
                log(LogLevel::warning, "Socket shutdown failed: ", error.what());
            }
        }
    };

    void asyncHttpGet
    (
        IOManager::ObjectMaker const& objectMaker,
        std::string_view const hostName,
        std::string_view const target,
        std::function<void(std::string)> onGet
    )
    {
        SimpleHTTPClient::startGet(objectMaker, hostName, target, std::move(onGet));
    }
}
