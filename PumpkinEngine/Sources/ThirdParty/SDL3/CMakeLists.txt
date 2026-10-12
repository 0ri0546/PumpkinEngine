add_library(SDL3Lib INTERFACE)

target_include_directories(SDL3Lib INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(SDL3_LIB_DIR "${CMAKE_SOURCE_DIR}/PumpkinEngine/Libraries/SDL3/lib/x64")
target_link_libraries(SDL3Lib INTERFACE "${SDL3_LIB_DIR}/SDL3.lib")
