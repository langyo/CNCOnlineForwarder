#include "precompiled.hpp"
#include <IOManager.hpp>
#include <NatNeg/NatNegProxy.hpp>
#include <Logging/Logging.hpp>
#include <Utility/WeakRefHandler.hpp>

using AddressV4 = boost::asio::ip::address_v4;
using UDP = boost::asio::ip::udp;
using UDPEndPoint = boost::asio::ip::udp::endpoint;
using ErrorCode = boost::system::error_code;

namespace CNCOnlineForwarder
{
    void signalHandler(IOManager& manager, ErrorCode const& errorCode, int const signal)
    {
        using namespace Logging;
        if (errorCode.failed())
        {
            logLine<IOManager>(Level::error, "Signal async wait failed: ", errorCode);
        }
        else
        {
            logLine<IOManager>(Level::info, "Received signal ", signal);
        }

        logLine<IOManager>(Level::info, "Shutting down.");
        manager.stop();
    }

    void run()
    {
        using namespace Logging;
        using namespace Utility;

        log(Level::info) << "Begin!";
        try
        {
            auto const ioManager = IOManager::create();
            auto objectMaker = IOManager::ObjectMaker{ ioManager };

            auto signals = objectMaker.make<boost::asio::signal_set>(SIGINT, SIGTERM);
            signals.async_wait(makeWeakHandler(ioManager.get(), &signalHandler));

            auto const addressTranslator = ProxyAddressTranslator::create(objectMaker);

            auto const natNegProxy = NatNeg::NatNegProxy::create
            (
                objectMaker,
                "natneg.server.cnc-online.net",
                27901,
                addressTranslator
            );

            {
                auto const runner = [ioManager] { ioManager->run(); };
                auto f1 = std::async(std::launch::async, runner);
                auto f2 = std::async(std::launch::async, runner);

                f1.get();
                f2.get();
            }
        }
        catch (std::exception const& error)
        {
            log(Level::fatal) << "Unhandled exception: " << error.what();
        }
        log(Level::info) << "End";
    }
}


int main()
{
    try
    {
        CNCOnlineForwarder::run();

    }
    catch (...)
    {
        using namespace CNCOnlineForwarder::Logging;
        log(Level::fatal) << "Unknown exception";
        return 1;
    }
    return 0;
}