add_subdirectory(Engine)
add_subdirectory(ThirdParty)

target_link_libraries(PumpkinEngine PRIVATE PumpkinThirdParty)

