#include <precompiled.hpp>

namespace CNCOnlineForwarder::Utility
{
    template<typename NextAction, typename Handler>
    class ReceiveHandler
    {
    
    private:
        std::size_t m_size;
        std::unique_ptr<std::byte[]> m_buffer;
        NextAction m_nextAction;
        Handler m_handler;
    public:
        template<typename InputNextAction, typename InputNextHandler>
        ReceiveHandler(InputNextAction&& nextAction, InputNextHandler&& handler) :
            m_size{ 512 },
            m_buffer{ std::make_unique<char[]>(m_size) },
            m_from{ std::make_unique<GameConnection::EndPoint>() },
            m_nextAction{ std::forward<InputNextAction>(nextAction) },
            m_handler{ std::forward<InputNextHandler>(handler) }
        {}

        boost::asio::mutable_buffer getBuffer() const noexcept
        {
            return boost::asio::buffer(m_buffer.get(), m_size);
        }

        void operator()
        (
            GameConnection& self,
            ErrorCode const& code,
            std::size_t const bytesReceived
        )
        {
            m_nextAction(self);

            if (code.failed())
            {
                logLine(LogLevel::error, "Async receive failed: ", code);
                return;
            }

            if (bytesReceived >= m_size)
            {
                logLine(LogLevel::warning, "Received data may be truncated: ", bytesReceived, "/", m_size);
            }

            return m_handler
            (
                self,
                std::move(m_buffer),
                bytesReceived
            );
        }
    };
}