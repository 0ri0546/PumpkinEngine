# SPDX-License-Identifier: MIT

# See: https://discourse.cmake.org/t/cmake-experimental-cxx-import-std-value-history-and-state/15712/3
function(SetModuleID)
    set(CMAKE_CXX_SCAN_FOR_MODULES ON CACHE BOOL "" FORCE)
    set(CMAKE_EXPERIMENTAL_CXX_MODULE_STD ON CACHE BOOL "" FORCE)

    if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.4.0")
        set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
            "f35a9ac6-8463-4d38-8eec-5d6008153e7d"
            CACHE STRING "" FORCE
        )
    elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "4.3.0")
        set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
            "451f2fe2-a8a2-47c3-bc32-94786d8fc91b"
            CACHE STRING "" FORCE
        )
    elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30.0")
        if(CMAKE_VERSION VERSION_LESS "3.31.8")
            set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
                "0e5b6991-d74f-4b3d-a41c-cf096e0b2508"
                CACHE STRING "" FORCE
            )
        elseif(CMAKE_VERSION VERSION_LESS "4.0.0")
            set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
                "d0edc3af-4c50-42ea-a356-e2862fe7a444"
                CACHE STRING "" FORCE
            )
        elseif(CMAKE_VERSION VERSION_LESS "4.0.3")
            set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
                "a9e1cf81-9932-4810-974b-6eccaf14e457"
                CACHE STRING "" FORCE
            )
        elseif(CMAKE_VERSION VERSION_LESS "4.3.0")
            set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
                "d0edc3af-4c50-42ea-a356-e2862fe7a444"
                CACHE STRING "" FORCE
            )
        else()
            message(FATAL_ERROR "Unsupported CMake version ${CMAKE_VERSION}")
        endif()
    else()
        message(FATAL_ERROR "CMake CXX_MODULES not supported for version ${CMAKE_VERSION}")
    endif()

    set(CMAKE_CXX_MODULE_STD ON CACHE BOOL "" FORCE)

    message(STATUS "C++ modules enabled (CMake ${CMAKE_VERSION}, UUID: ${CMAKE_EXPERIMENTAL_CXX_IMPORT_STD})")
endfunction()
