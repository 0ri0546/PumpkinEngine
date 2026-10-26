add_library(MathLib)

file(GLOB_RECURSE MATH_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/*.cppm)

target_sources(MathLib PUBLIC
        FILE_SET engine_math_modules TYPE CXX_MODULES
        FILES ${MATH_MODULES}
)
