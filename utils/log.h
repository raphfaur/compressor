#pragma once
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

#define LOGLEVEL_INFO 0
#define LOGLEVEL_DEBUG 1

#define LOGLEVEL 0

std::mutex log_mutex;

#define RESET "\033[0m"
#define BLACK "\033[30m"              /* Black */
#define RED "\033[31m"                /* Red */
#define GREEN "\033[32m"              /* Green */
#define YELLOW "\033[33m"             /* Yellow */
#define BLUE "\033[34m"               /* Blue */
#define MAGENTA "\033[35m"            /* Magenta */
#define CYAN "\033[36m"               /* Cyan */
#define WHITE "\033[37m"              /* White */
#define BOLDBLACK "\033[1m\033[30m"   /* Bold Black */
#define BOLDRED "\033[1m\033[31m"     /* Bold Red */
#define BOLDGREEN "\033[1m\033[32m"   /* Bold Green */
#define BOLDYELLOW "\033[1m\033[33m"  /* Bold Yellow */
#define BOLDBLUE "\033[1m\033[34m"    /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m" /* Bold Magenta */
#define BOLDCYAN "\033[1m\033[36m"    /* Bold Cyan */
#define BOLDWHITE "\033[1m\033[37m"   /* Bold White */

uint64_t micros() {
  uint64_t us =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch())
          .count();
  return us;
}

#if LOGLEVEL >= LOGLEVEL_INFO
#define INFO(expr)                                                             \
  log_mutex.lock();                                                            \
  std::cout << YELLOW << "[INFO] " << RESET << " [" << micros() << "] "        \
            << expr << std::endl;                                              \
  log_mutex.unlock();
#else
#define INFO(expr)
#endif

#if LOGLEVEL >= LOGLEVEL_DEBUG
#define DEBUG(expr)                                                            \
  log_mutex.lock();                                                            \
  std::cout << MAGENTA << "[DEBUG]" << RESET << " [" << micros() << "] "       \
            << expr << std::endl;                                              \
  log_mutex.unlock();
#else
#define DEBUG(expr)
#endif