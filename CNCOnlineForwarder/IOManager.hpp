#pragma once
#include "precompiled.hpp"

namespace CNCOnlineForwarder
{
    class IOManager : public std::enable_shared_from_this<IOManager>
    {
    public:
        using ContextType = boost::asio::io_context;
        using StrandType = decltype(boost::asio::make_strand(std::declval<ContextType&>()));
        class ObjectMaker;
    private:
        struct PrivateConstructor{};
        ContextType m_context;
    public:

        static constexpr auto description = "IOManager";

        static std::shared_ptr<IOManager> create()
        {
            return std::make_shared<IOManager>(PrivateConstructor{});
        }

        IOManager(PrivateConstructor) {}

        auto stop() { return m_context.stop(); }

        auto run() 
        { 
            try
            {
                return m_context.run();
            }
            catch (...)
            {
                stop();
                throw;
            }
        }
    };

    class IOManager::ObjectMaker
    {
    private:
        std::weak_ptr<IOManager> m_ioManager;
    public:
        ObjectMaker(std::weak_ptr<IOManager> ioManager) : 
            m_ioManager{ std::move(ioManager) } {}

        template<typename T, typename... Arguments>
        T make(Arguments&&... arguments) const
        {
            auto const ioManager = std::shared_ptr{ m_ioManager };
            return T{ ioManager->m_context, std::forward<Arguments>(arguments)... };
        }

        StrandType makeStrand() const
        {
            auto const ioManager = std::shared_ptr{ m_ioManager };
            return boost::asio::make_strand(ioManager->m_context);
        }
    };
}