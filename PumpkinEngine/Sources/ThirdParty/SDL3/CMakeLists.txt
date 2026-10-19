add_library(SDL3Lib INTERFACE)

target_include_directories(SDL3Lib INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(WIN32)
    set(SDL3_LIB_DIR "${CMAKE_SOURCE_DIR}/PumpkinEngine/Libraries/SDL3/lib/x64")
    target_link_libraries(SDL3Lib INTERFACE "${SDL3_LIB_DIR}/SDL3.lib" "${SDL3_LIB_DIR}/SDL3_ttf.dll.a")
elseif(UNIX)
    # On Unix, easiest way is just to install libsdl3-devel package
    find_package(SDL3 REQUIRED)
    target_link_libraries(SDL3Lib INTERFACE SDL3_ttf::SDL3_ttf-static SDL3::SDL3)
else()
    message(FATAL_ERROR "Unsupported platform!")
endif()
