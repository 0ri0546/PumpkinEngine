add_executable(PumpkinEngine main.cpp)

set_target_properties(PumpkinEngine PROPERTIES OUTPUT_NAME PumpkinEngine)
set_target_properties(PumpkinEngine PROPERTIES PREFIX "")
set_target_properties(PumpkinEngine PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

file(GLOB_RECURSE RENDER_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/*.cppm)

target_sources(PumpkinEngine PRIVATE
        FILE_SET engine_render_modules TYPE CXX_MODULES
        FILES ${RENDER_MODULES}
)

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

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
    target_sources(PumpkinEngine PRIVATE
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
