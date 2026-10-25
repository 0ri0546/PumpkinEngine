add_executable(PumpkinTests main.cpp)

file(GLOB_RECURSE PRIVATE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE PRIVATE_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/*.cppm)

target_sources(PumpkinTests PRIVATE ${PRIVATE_SOURCES})
target_sources(PumpkinTests PRIVATE
        FILE_SET tests_modules TYPE CXX_MODULES
        FILES ${PRIVATE_MODULES}
)

target_link_libraries(PumpkinTests PRIVATE GTest::gtest_main)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(PumpkinTests PRIVATE "-lstdc++exp")
endif()
