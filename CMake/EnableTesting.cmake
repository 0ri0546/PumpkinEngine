# SPDX-License-Identifier: MIT

include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.18.0
)

# See: https://github.com/google/googletest/blob/main/docs/quickstart-cmake.md
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

message(STATUS "Googletest: Version 1.18.0")
