#include <precompiled.hpp>
#include "Logging.hpp"
#include <BuildConfiguration.hpp>

namespace CNCOnlineForwarder::Logging
{

    Logging::Logging()
    {
        namespace logging = boost::log;
        namespace keywords = boost::log::keywords;
        using namespace std::string_literals;

        logging::add_file_log
        (
            keywords::file_name = (PROJECT_NAME + "_%N.log"s),
            keywords::open_mode = (std::ios::out | std::ios::app),
            keywords::rotation_size = 1024 * 1024,
            keywords::format = "[%TimeStamp%]: %Message%"
        );

        setFilterLevel(Level::info);

        logging::add_common_attributes();
    }


    Logging::LogProxy::LogProxy(Logging::SeverityLogger& logger, Level level) :
        m_logger{ logger },
        m_record{ logger.open_record(boost::log::keywords::severity = level) }
    {
        if (m_record)
        {
            m_stream.attach_record(m_record);
            static constexpr auto levels = std::array<std::string_view, 6>
            {
                "[trace] ",
                "[debug] ",
                "[info] ",
                "[warning] ",
                "[error] ",
                "[fatal] "
            };
            m_stream << levels.at(level);
        }
    }

    Logging::LogProxy::~LogProxy()
    {
        if (m_record)
        {
            m_stream.flush();
            m_logger.push_record(std::move(m_record));
        }
    }

    Logging::LogProxy log(Level level)
    {
        static auto loggingSetup = Logging{};
        static auto logger = Logging::SeverityLogger{};
        return Logging::LogProxy{ logger, level };
    }

    void setFilterLevel(Level level)
    {
        static auto firstTime = true;
        if (!firstTime)
        {
            throw std::runtime_error{ "Not implemented yet" };
        }
        firstTime = false;
        boost::log::core::get()->set_filter(boost::log::trivial::severity >= level);
    }
    
}