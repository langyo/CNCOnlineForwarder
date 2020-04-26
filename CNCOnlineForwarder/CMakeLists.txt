cmake_minimum_required(VERSION 3.16.5)
project(CNCOnlineForwarder)
find_package(Boost 1.70 REQUIRED log system)

add_library(${PROJECT_NAME})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    PROJECT_NAME="${PROJECT_NAME}"
)
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/W4" "$<$<CONFIG:RELEASE>:/O2>")
    if (MSVC_VERSION GREATER_EQUAL 1910)
        target_compile_options(${PROJECT_NAME} PUBLIC "/permissive-")
    endif()
else()
    target_compile_options(${PROJECT_NAME} PRIVATE "-Wall" "-Wextra" "-Werror" "$<$<CONFIG:RELEASE>:-O3>")
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        target_compile_options(${PROJECT_NAME} PUBLIC "-stdlib=libc++")
    else()
        # nothing special for gcc at the moment
    endif()
endif()

target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR})
target_precompile_headers(${PROJECT_NAME} PUBLIC precompiled.hpp)
target_link_libraries(${PROJECT_NAME} PUBLIC 
    ${Boost_LOG_SETUP_LIBRARY} # https://gitlab.kitware.com/cmake/cmake/-/issues/20638
    Boost::headers 
    Boost::log 
    Boost::system)
target_sources(${PROJECT_NAME} PRIVATE
    "IOManager.hpp"
    "NatNeg/NatNegProxy.cpp"
    "NatNeg/NatNegProxy.hpp"
    "NatNeg/GameConnection.cpp"
    "NatNeg/GameConnection.hpp"
    "NatNeg/InitialPhase.cpp"
    "NatNeg/InitialPhase.hpp"
    "NatNeg/NatNegPacket.hpp"
    "Logging/Logging.cpp"
    "Logging/Logging.hpp"
    "TCPProxy/TCPProxy.cpp"
    "TCPProxy/TCPProxy.hpp"
    "TCPProxy/TCPConnection.cpp"
    "TCPProxy/TCPConnection.hpp"
    "Utility/PendingActions.hpp"
    "Utility/ProxyAddressTranslator.cpp"
    "Utility/ProxyAddressTranslator.hpp"
    # "Utility/ReadHandler.hpp"
    "Utility/SimpleHTTPClient.cpp"
    "Utility/SimpleHTTPClient.hpp"
    "Utility/SimpleWriteHandler.hpp"
    "Utility/WeakRefHandler.hpp"
  )