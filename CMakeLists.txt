cmake_minimum_required(VERSION 3.16.5)
project(ProjectX)

enable_testing()
add_subdirectory(CNCOnlineForwarder)
add_subdirectory(CNCOnlineForwarder.Exe)
add_subdirectory(CNCOnlineForwarder.Tests)