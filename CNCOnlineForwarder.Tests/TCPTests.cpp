#define BOOST_TEST_MODULE TCPTests
#include <boost/test/included/unit_test.hpp>
#include "asioFutureHelper.hpp"
#include <IOManager.hpp>
#include <TCPProxy/TCPProxy.hpp>
#include <boost/format.hpp>
#include <mbctype.h>

using CNCOnlineForwarder::IOManager;
using CNCOnlineForwarder::TCPProxy::TCPProxy;

using boost::asio::buffer;
using boost::asio::ip::make_address_v4;

using Timer = boost::asio::steady_timer;
using ErrorCode = boost::system::error_code;

struct IOManagerFixture
{
    std::shared_ptr<IOManager> manager;
    IOManager::ObjectMaker maker;
    std::vector<std::future<void>> futures;
    std::shared_ptr<CancellableFutureHelper> helper;

    IOManagerFixture() :
        manager{ IOManager::create() },
        maker{ IOManager::ObjectMaker{ manager } },
        helper{ std::make_shared<CancellableFutureHelper>(maker.make<Timer>(std::chrono::milliseconds{200})) }
    {
        helper->addHandler([m = manager->weak_from_this()](ErrorCode const& code)
        {
            auto const manager = m.lock();
            if (manager == nullptr) { return; }
            if (not manager->stopped())
            {
                manager->stop();
                BOOST_TEST_FAIL("Time expired!");
            }
        });
    }

    void setup()
    {
        auto const launch = [this]
        {
            return std::async(std::launch::async, [this]
            {
                while (not manager->stopped())
                {
                    manager->run();
                    std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
                }
            });
        };

        futures.emplace_back(launch());
        futures.emplace_back(launch());
        futures.emplace_back(launch());
        futures.emplace_back(launch());
    }

    void teardown()
    {
        manager->stop();
        futures.clear();
    }

    std::shared_ptr<CancellableFutureHelper> useFuture() { return helper; }
};

struct TCPTestFixture
{
    IOManagerFixture fixture;
    std::shared_ptr<TCPProxy> proxy;
    std::uint16_t proxyPort = 6799;
    std::string realHostName = "127.16.38.9";
    std::uint16_t realPort = 6667;

    void setup()
    {
        fixture.setup();
        auto const maker = IOManager::ObjectMaker{ fixture.manager };
        proxy = TCPProxy::create(maker, proxyPort, realHostName, realPort);

    }

    void teardown()
    {
        fixture.teardown();
    }
};

BOOST_FIXTURE_TEST_CASE(TestTCPProxyWithFutures, TCPTestFixture)
{
    auto constexpr n = 10;
    auto const useFuture = fixture.useFuture();
    auto const messages = std::vector<std::pair<std::string, std::string>>
    {
        {"HELLO", "HI"},
        {"WORLD", "SEE"},
        {"TCPPROXY", "REPLY"}
    };

    auto const serverCode = [this, n, useFuture, &messages]
    {
        auto serverStrand = fixture.maker.makeStrand();
        auto server = TCPProxy::Acceptor{ serverStrand, TCPProxy::EndPoint{ make_address_v4(realHostName), realPort } };
        auto threads = std::vector<std::future<void>>{};

        auto lambda = [useFuture, &messages](std::future<std::optional<TCPProxy::Socket::Type>> future)
        {
            BOOST_TEST_CHECKPOINT("Server connection thread");
            auto socket = get(std::move(future));
            BOOST_TEST_CHECKPOINT("Server connection created");
            auto previousDigit = std::optional<char>{};

            for (auto const& [send, reply] : messages)
            {
                auto string = std::string{};
                string.resize(send.size() + 2, '\xFF');

                auto const bytesRead = get(async_read(socket, buffer(string), useFuture));
                auto const message = "Server received message `" + string + "`";
                BOOST_TEST_CHECKPOINT(message);
                BOOST_TEST_MESSAGE(message);
                BOOST_TEST(bytesRead == string.size());
                BOOST_TEST(std::string_view{ string }.substr(send.size()) == send);
                BOOST_TEST(string.at(send.size()) == '\0');

                auto const digit = string.at(send.size() + 1);
                BOOST_TEST(std::isdigit(digit));
                if (not previousDigit.has_value())
                {
                    previousDigit = digit;
                }
                BOOST_TEST(previousDigit.value() == digit);

                auto const actualReply = reply + '\0' + digit;
                async_write(socket, buffer(actualReply), useFuture);
            }

            auto const message = "Server connection " + std::string{ previousDigit.value() } +" waiting for close";
            BOOST_TEST_CHECKPOINT(message);
            BOOST_TEST_MESSAGE(message);
            auto string = std::string{};
            try
            {
                socket.async_receive(buffer(string), useFuture);
                BOOST_TEST_ERROR("Expeccted EOF not thrown");
            }
            catch (boost::system::system_error exception)
            {
                BOOST_TEST(exception.code() == boost::asio::error::eof);
            }
        };

        for (auto i = 0; i < n; ++i)
        {
            threads.emplace_back(std::async(lambda, server->async_accept(useFuture)));
        }
    };

    auto const clientCode = [this, useFuture, &messages](int n)
    {
        auto client = fixture.maker.make<TCPProxy::Socket::Type>();
        client.connect(TCPProxy::EndPoint{ make_address_v4("127.0.0.1"), proxyPort });
        for (auto const& [send, reply] : messages)
        {
            async_write(client, buffer(send + '\0' + std::to_string(n)), useFuture);

            auto string = std::string{};
            string.resize(reply.size() + 2, '\xFF');

            auto const bytesRead = get(async_read(client, buffer(string), useFuture));
            auto const message = "Client received message `" + string + "`";
            BOOST_TEST_CHECKPOINT(message);
            BOOST_TEST_MESSAGE(message);
            BOOST_TEST(bytesRead == string.size());
            BOOST_TEST(std::string_view{ string }.substr(reply.size()) == reply);
            BOOST_TEST(string.at(reply.size()) == '\0');
            auto const digit = std::string{ string.at(reply.size() + 1) };

            BOOST_TEST(digit == std::to_string(n));
        }
    };

    auto serverTask = std::async(serverCode);
    auto clientTasks = std::vector<std::future<void>>{};
    serverTask.get();
    for (auto& future : clientTasks) { future.get(); }
}
