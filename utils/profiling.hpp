#pragma once
#include "log.h"
#include <chrono>

#define PROFILING

#ifdef PROFILING
#define PROFILE(expr)                                                          \
  {                                                                            \
    std::chrono::steady_clock::time_point _start =                             \
        std::chrono::steady_clock::now();                                      \
    expr;                                                                      \
    std::chrono::steady_clock::time_point _end =                               \
        std::chrono::steady_clock::now();                                      \
    std::cout << GREEN << "[PERF] " << RESET << __FILE__ << ":" << __LINE__    \
              << " -> " << #expr " | " << GREEN                                \
              << std::chrono::duration_cast<std::chrono::microseconds>(_end -  \
                                                                       _start) \
                     .count()                                                  \
              << " [µs]" << RESET << std::endl;                                \
  }
#else
#define PROFILE(expr) expr;
#endif
