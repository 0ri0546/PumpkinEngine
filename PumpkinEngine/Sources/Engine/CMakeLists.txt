add_library(PumpkinEngine SHARED)
add_executable(PumpkinRuntime main.cpp)

if(MSVC)
    set(PUMPKIN_COMPILE_FLAGS /arch:AVX2 /GL /Oi)

    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(APPEND PUMPKIN_COMPILE_FLAGS /O2)
    endif()

    set(PUMPKIN_LINK_FLAGS /LTCG)
else()
    set(PUMPKIN_COMPILE_FLAGS -mavx2 -mfma)

    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(APPEND PUMPKIN_COMPILE_FLAGS -O2)
    endif()

    set(PUMPKIN_LINK_FLAGS -flto)
endif()

add_compile_options(${PUMPKIN_COMPILE_FLAGS})
add_link_options(${PUMPKIN_LINK_FLAGS})

set_target_properties(PumpkinEngine PROPERTIES OUTPUT_NAME PumpkinEngine)
set_target_properties(PumpkinEngine PROPERTIES PREFIX "")
set_target_properties(PumpkinEngine PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_compile_definitions(PumpkinEngine PRIVATE PUMPKIN_ENGINE_BUILD)
set_target_properties(PumpkinEngine PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
target_include_directories(PumpkinEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Platform)

set_target_properties(PumpkinRuntime PROPERTIES OUTPUT_NAME PumpkinRuntime)
set_target_properties(PumpkinRuntime PROPERTIES PREFIX "")
set_target_properties(PumpkinRuntime PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

file(GLOB_RECURSE RENDER_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/*.cppm)

target_sources(PumpkinEngine PUBLIC
        FILE_SET engine_render_modules TYPE CXX_MODULES
        FILES ${RENDER_MODULES}
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

target_sources(PumpkinEngine PRIVATE ${SOURCES})

if(WIN32)
    add_custom_command(TARGET PumpkinEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/PumpkinEngine/Libraries/SDL3/lib/x64/SDL3.dll"
            $<TARGET_FILE_DIR:PumpkinEngine>
    )
    add_custom_command(TARGET PumpkinEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/PumpkinEngine/Libraries/SDL3/lib/x64/SDL3_ttf.dll"
            $<TARGET_FILE_DIR:PumpkinEngine>
    )
    target_sources(PumpkinRuntime PRIVATE
            ${CMAKE_SOURCE_DIR}/Assets/Windows/PumpkinEngine.rc
    )
endif()

add_custom_target(CopyAssets
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                "${CMAKE_SOURCE_DIR}/Assets/Copy"
                "$<TARGET_FILE_DIR:PumpkinEngine>/Assets"
        COMMENT "Copying assets"
)
add_dependencies(PumpkinEngine CopyAssets)

target_link_libraries(PumpkinRuntime PRIVATE PumpkinEngine)
