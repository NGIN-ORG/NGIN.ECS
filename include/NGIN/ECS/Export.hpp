#pragma once

#if defined(_WIN32) || defined(_WIN64)
  #if defined(NGIN_ECS_STATIC)
    #define NGIN_ECS_API
  #else
    #if defined(NGIN_ECS_EXPORTS)
      #define NGIN_ECS_API __declspec(dllexport)
    #else
      #define NGIN_ECS_API __declspec(dllimport)
    #endif
  #endif
#else
  #define NGIN_ECS_API
#endif

