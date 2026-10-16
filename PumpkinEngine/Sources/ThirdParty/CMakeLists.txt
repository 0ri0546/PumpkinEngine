add_library(PumpkinThirdParty INTERFACE)

add_subdirectory(SDL3)
add_subdirectory(Math)
add_subdirectory(STB)

target_link_libraries(PumpkinThirdParty INTERFACE SDL3Lib MathLib)
