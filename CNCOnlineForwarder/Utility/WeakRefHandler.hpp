#pragma once
#include "precompiled.hpp"
#include <Logging/Logging.hpp>

namespace CNCOnlineForwarder::Utility
{
    template<typename Type, typename Handler>
    class WeakRefHandler
    {
    private:
        std::weak_ptr<Type> m_ref;
        Handler m_handler;

    public:
        template<typename InputHandler>
        WeakRefHandler(std::weak_ptr<Type> const& ref, InputHandler&& handler) :
            m_ref{ ref },
            m_handler{ std::forward<InputHandler>(handler) } 
        {}

        template<typename... Arguments>
        void operator()(Arguments&&... arguments)
        {
            using namespace Logging;

            auto const self = m_ref.lock();
            if (!self)
            {
                logLine<Type>(Level::error, "Tried to execute deferred action after self is died");
                return;
            }

            std::invoke(m_handler, *self, std::forward<Arguments>(arguments)...);
        }

        // Allow accessing handler members
        Handler* operator->()
        {
            return &m_handler;
        }
    };

    namespace Details
    {
        template<typename T>
        struct IsTemplate : std::false_type
        {
            using HeadType = void;
        };

        template<typename Head, typename... Tail, template<typename...> class T>
        struct IsTemplate<T<Head, Tail...>> : std::true_type
        {
            using HeadType = Head;
        };
    }

    template<typename T, typename Handler>
    auto makeWeakHandler
    (
        T const& pointer, 
        Handler&& handler
    )
    {
        using TemplateCheck = Details::IsTemplate<T>;
        using HandlerValue = std::remove_reference_t<Handler>;
        if constexpr (std::conjunction_v<TemplateCheck, std::is_convertible<T const&, std::weak_ptr<typename TemplateCheck::HeadType>>>)
        {
            return WeakRefHandler<typename TemplateCheck::HeadType, HandlerValue>
            { 
                pointer, 
                std::forward<Handler>(handler)
            };
        }
        else
        {
            static_assert(std::is_pointer_v<T>);
            return WeakRefHandler<std::remove_pointer_t<T>, HandlerValue>
            { 
                pointer->weak_from_this(), 
                std::forward<Handler>(handler)
            };
        }
    }
}