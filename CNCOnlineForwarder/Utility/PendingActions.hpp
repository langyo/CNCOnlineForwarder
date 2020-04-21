#pragma once
#include "precompiled.h"

namespace CNCOnlineForwarder::Utility
{
    template<typename FutureData>
    class PendingActions
    {
    private:
        FutureData m_data;
        std::optional<std::vector<typename FutureData::ActionType>> m_pendingActions;

    public:
        PendingActions(FutureData data) :
            m_data{ data },
            m_pendingActions{ std::in_place }
        {}

        FutureData* operator->() noexcept
        {
            return &m_data;
        }

        void trySetReady()
        {
            setReadyIf(m_pendingActions.has_value() && m_data.isReady());
        }

        void setReadyIf(bool condition)
        {
            if (!condition)
            {
                return;
            }

            auto actions = std::move(m_pendingActions.value());
            m_pendingActions.reset();
            for (auto& action : actions)
            {
                m_data.apply(std::move(action));
            }
        }

        template<typename Action>
        void asyncDo(Action&& action)
        {
            if (m_pendingActions.has_value())
            {
                m_pendingActions->emplace_back(std::forward<Action>(action));
                return;
            }

            m_data.apply(std::forward<Action>(action));
        }
    };
}