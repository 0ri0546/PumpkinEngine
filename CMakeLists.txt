cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# C++20 modules
include(CMake/SetModuleUUID.cmake)
SetModuleID()

project(PumpkinEngine VERSION 1.0 LANGUAGES C CXX)

add_subdirectory(PumpkinEngine)

option(PUMPKIN_BUILD_TEST "Enable Pumpkin Engine tests" OFF)
if(PUMPKIN_BUILD_TEST)
    include(CMake/EnableTesting.cmake)
    enable_testing()
    add_subdirectory(PumpkinEngine/Tests)
endif()
