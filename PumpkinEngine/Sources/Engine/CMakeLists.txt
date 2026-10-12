add_executable(PumpkinEngine main.cpp)

set_target_properties(PumpkinEngine PROPERTIES OUTPUT_NAME PumpkinEngine)
set_target_properties(PumpkinEngine PROPERTIES PREFIX "")
set_target_properties(PumpkinEngine PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(WIN32)
    add_custom_command(TARGET PumpkinEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/PumpkinEngine/Libraries/SDL3/lib/x64/SDL3.dll"
            $<TARGET_FILE_DIR:PumpkinEngine>
    )
endif()
