#pragma once
#include <precompiled.hpp>

#ifndef PROJECT_NAME
static_assert(false, "PROJECT_NAME not defined");
#else
static_assert(std::is_convertible_v<decltype(PROJECT_NAME), std::string>, "PROJECT_NAME not convertible to string");
#endif