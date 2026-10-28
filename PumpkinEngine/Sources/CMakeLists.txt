add_subdirectory(Engine)
add_subdirectory(ThirdParty)

target_link_libraries(PumpkinEngine PRIVATE PumpkinThirdParty)
target_link_libraries(PumpkinRuntime PRIVATE PumpkinThirdParty)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(PumpkinEngine PRIVATE stdc++exp)
    target_link_libraries(PumpkinRuntime PRIVATE stdc++exp)
endif()
