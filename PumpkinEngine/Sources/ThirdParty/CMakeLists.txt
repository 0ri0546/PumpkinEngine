add_library(PumpkinThirdParty INTERFACE)

add_subdirectory(SDL3)

target_link_libraries(PumpkinThirdParty INTERFACE SDL3Lib)
