#include <IOManager.hpp>
#include <NatNeg/NatNegProxy.hpp>
#include <Logging/Logging.hpp>
#include <Utility/WeakRefHandler.hpp>

using CNCOnlineForwarder::IOManager;
using CNCOnlineForwarder::Logging::logLine;
using CNCOnlineForwarder::Logging::Level;
using CNCOnlineForwarder::NatNeg::NatNegProxy;
using CNCOnlineForwarder::Utility::makeWeakHandler;
using CNCOnlineForwarder::Utility::ProxyAddressTranslator;

using ErrorCode = boost::system::error_code;
using SignalSet = boost::asio::signal_set;

void signalHandler(IOManager& manager, ErrorCode const& errorCode, int const signal)
{
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

struct Main 
{
    static constexpr auto description = "Main";

    template<typename... Arguments>
    static void logLine(Level const level, Arguments&&... arguments)
    {
        return ::logLine<Main>(level, std::forward<Arguments>(arguments)...);
    }

    static void run()
    {
        logLine(Level::info, "Begin!");
        try
        {
            auto const ioManager = IOManager::create();
            auto objectMaker = IOManager::ObjectMaker{ ioManager };

            auto signals = objectMaker.make<SignalSet>(SIGINT, SIGTERM);
            signals.async_wait(makeWeakHandler(ioManager.get(), &signalHandler));

            auto const addressTranslator = ProxyAddressTranslator::create(objectMaker);

            auto const natNegProxy = NatNegProxy::create
            (
                objectMaker,
                "natneg.server.cnc-online.net",
                27901,
                addressTranslator
            );

            {
                auto const runner = [ioManager] 
                { 
                    try
                    {
                        ioManager->run();
                    }
                    catch (...)
                    {
                        ioManager->stop();
                        throw;
                    }
                };
                auto f1 = std::async(std::launch::async, runner);
                auto f2 = std::async(std::launch::async, runner);

                f1.get();
                f2.get();
            }
        }
        catch (std::exception const& error)
        {
            logLine(Level::fatal, "Unhandled exception: ", error.what());
        }
        logLine(Level::info, "End");
    }
};



int main()
{
    try
    {
        Main::run();
    }
    catch (...)
    {
        logLine<Main>(Level::fatal, "Unknown exception");
        return 1;
    }
    return 0;
}