#pragma once

// Note: Some compilers like GCC and Clang have redefinitions issues from SIMD intrinsics

#include <cstdlib>
#include <cstdio>
#if defined(_WIN32)
#if !defined(_MSC_VER)
#include <x86intrin.h>
#endif
#include <intrin.h>
#endif
#include <SDL3/SDL.h>
