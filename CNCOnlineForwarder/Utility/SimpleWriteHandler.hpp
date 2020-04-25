#pragma once
#include <precompiled.hpp>
#include <Logging/Logging.hpp>

namespace CNCOnlineForwarder::Utility
{
    template<typename Type>
    class SimpleWriteHandler
    {
    private:
        std::unique_ptr<std::string> m_data;

    public:
        template<typename String>
        SimpleWriteHandler(String&& data) :
            m_data{ std::make_unique<std::string>(std::forward<String>(data)) }
        {
        }

        boost::asio::const_buffer getData() const noexcept
        {
            return boost::asio::buffer(*m_data);
        }

        void operator()
        (
            boost::system::error_code const& code,
            std::size_t const bytesSent
        ) const
        {
            using namespace Logging;

            if (code.failed())
            {
                logLine<Type>(Level::error, "Async write failed: ", code);
                return;
            }

            if (bytesSent != m_data->size())
            {
                logLine<Type>(Level::error, "Only part of packet was sent: ", bytesSent, "/", m_data->size());
                return;
            }
        }
    };

    template<typename Type, typename String>
    auto makeWriteHandler(String&& data)
    {
        return SimpleWriteHandler<Type>{std::forward<String>(data)};
    }
}