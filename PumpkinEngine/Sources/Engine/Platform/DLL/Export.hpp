#pragma once

#if defined(_WIN32) && defined(_MSC_VER)
#    if defined(PUMPKIN_ENGINE_BUILD)
#        define PUMPKIN_API __declspec(dllexport)
#    else
#        define PUMPKIN_API __declspec(dllimport)
#    endif
#else
#    define PUMPKIN_API __attribute__((visibility("default")))
#endif
