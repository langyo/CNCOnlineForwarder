cmake_minimum_required(VERSION 3.16.5)
project(CNCOnlineForwarder.Tests)

include(BoostTest.cmake)

find_package(Boost 1.70 COMPONENTS unit_test_framework REQUIRED)
add_executable(TCPTests TCPTests.cpp asioFutureHelper.hpp)
target_link_libraries(TCPTests PRIVATE CNCOnlineForwarder Boost::unit_test_framework)
boost_test_discover_tests(TCPTests EXTRA_ARGS --log_level=message)

#add_boost_test(TCPTests.cpp CNCOnlineForwarder)