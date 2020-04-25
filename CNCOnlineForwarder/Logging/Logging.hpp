#pragma once
#include <precompiled.hpp>

namespace CNCOnlineForwarder::Logging
{
    using Level = boost::log::trivial::severity_level;

    class Logging
    {
    public:
        using SeverityLogger = boost::log::sources::severity_logger_mt<Level>;
        using LogRecord = boost::log::record;
        using LogStream = boost::log::record_ostream;
        class LogProxy;

        Logging();
    };

    class Logging::LogProxy
    {
    private:
        SeverityLogger& m_logger;
        LogRecord m_record;
        LogStream m_stream;
    public:
        LogProxy(Logging::SeverityLogger& logger, Level level);
        LogProxy(LogProxy const&) = delete;
        LogProxy& operator=(LogProxy const&) = delete;
        ~LogProxy();

        template<typename T>
        LogStream& operator<<(T&& argument);
    };

    Logging::LogProxy log(Level level);

    void setFilterLevel(Level level);

    template<typename T>
    Logging::LogStream& Logging::LogProxy::operator<<(T&& argument)
    {
        if (m_record)
        {
            m_stream << std::forward<T>(argument);
        }
        return m_stream;
    }

    template<typename Type, typename... Arguments>
    void logLine(Level const level, Arguments&&... arguments)
    {
        auto logProxy = log(level);
        logProxy << Type::description << ": ";
        (logProxy << ... << std::forward<Arguments>(arguments));
    }
}
