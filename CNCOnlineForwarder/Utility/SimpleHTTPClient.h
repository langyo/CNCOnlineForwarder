#pragma once
#include "precompiled.h"
#include <IOManager.hpp>

namespace CNCOnlineForwarder::Utility
{
    void asyncHttpGet
    (
        IOManager::ObjectMaker const& objectMaker,
        std::string_view const hostName,
        std::string_view const target,
        std::function<void(std::string)> onGet
    );
}
