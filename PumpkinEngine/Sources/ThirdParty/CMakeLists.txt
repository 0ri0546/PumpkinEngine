add_library(PumpkinThirdParty INTERFACE)

add_subdirectory(SDL3)
add_subdirectory(Math)
add_subdirectory(STB)

if (MSVC)
    set(PUMPKIN_SIMD_FLAGS /arch:AVX2)
else()
    set(PUMPKIN_SIMD_FLAGS -mavx2)
endif()

target_compile_options(MathLib PUBLIC ${PUMPKIN_SIMD_FLAGS})

target_link_libraries(PumpkinThirdParty INTERFACE SDL3Lib MathLib)
