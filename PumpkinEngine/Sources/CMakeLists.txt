add_subdirectory(Engine)
add_subdirectory(ThirdParty)

target_link_libraries(PumpkinEngine PRIVATE PumpkinThirdParty)

if (MSVC)
    set(PUMPKIN_SIMD_FLAGS /arch:AVX2)
else()
    set(PUMPKIN_SIMD_FLAGS -mavx2)
endif()

target_compile_options(PumpkinEngine PRIVATE ${PUMPKIN_SIMD_FLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(PumpkinEngine PRIVATE stdc++exp)
endif()
