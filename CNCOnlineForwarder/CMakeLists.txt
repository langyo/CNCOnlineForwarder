cmake_minimum_required(VERSION 3.16.5)
project(CNCOnlineForwarder)
find_package(Boost 1.70 REQUIRED log system)

add_executable(${PROJECT_NAME})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_compile_definitions(${PROJECT_NAME} PRIVATE PROJECT_NAME="${PROJECT_NAME}")
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/W4" "$<$<CONFIG:RELEASE>:/O2>")
else()
    target_compile_options(${PROJECT_NAME} PRIVATE "-Wall" "-Wextra" "-Werror" "$<$<CONFIG:RELEASE>:-O3>")
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        target_compile_options(${PROJECT_NAME} PRIVATE "-stdlib=libc++")
    else()
        # nothing special for gcc at the moment
    endif()
endif()

target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${Boost_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR})
target_precompile_headers(${PROJECT_NAME} PRIVATE precompiled.h)
target_link_libraries(${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES})
target_sources(${PROJECT_NAME} PRIVATE
    main.cpp
    IOManager.hpp
    "Utility/ProxyAddressTranslator.cpp"
    "NatNeg/NatNegProxy.cpp"
    "NatNeg/NatNegProxy.h"
    "NatNeg/GameConnection.cpp"
    "NatNeg/GameConnection.h"
    "NatNeg/InitialPhase.cpp"
    "NatNeg/InitialPhase.h"
    "NatNeg/NatNegPacket.hpp"
    "Logging/Logging.cpp"
    "Logging/Logging.h"
    "Utility/PendingActions.hpp"
    "Utility/SimpleHTTPClient.cpp"
    "Utility/SimpleHTTPClient.h"
    "Utility/SimpleWriteHandler.hpp"
    "Utility/WeakRefHandler.hpp"
)