add_executable(PumpkinTests main.cpp)

file(GLOB_RECURSE PRIVATE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
target_sources(PumpkinTests PRIVATE ${PRIVATE_SOURCES})

if (MSVC)
    set(PUMPKIN_SIMD_FLAGS /arch:AVX2)
else()
    set(PUMPKIN_SIMD_FLAGS -mavx2 -mfma)
endif()
target_compile_options(PumpkinTests PUBLIC ${PUMPKIN_SIMD_FLAGS})

target_link_libraries(PumpkinTests PRIVATE PumpkinEngine PumpkinThirdParty GTest::gtest_main)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(PumpkinTests PRIVATE "-lstdc++exp")
endif()

if(WIN32)
    add_custom_command(TARGET PumpkinTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:PumpkinEngine>
            $<TARGET_FILE_DIR:PumpkinTests>
    )
    add_custom_command(TARGET PumpkinTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/PumpkinEngine/Libraries/SDL3/lib/x64/SDL3.dll"
        $<TARGET_FILE_DIR:PumpkinTests>
    )
    add_custom_command(TARGET PumpkinTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/PumpkinEngine/Libraries/SDL3/lib/x64/SDL3_ttf.dll"
            $<TARGET_FILE_DIR:PumpkinTests>
    )
endif()
