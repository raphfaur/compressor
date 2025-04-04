#pragma once
#include<thread>
#include<mutex>
#include<chrono>

#define LOGLEVEL_INFO 0
#define LOGLEVEL_DEBUG 1

#define LOGLEVEL -1

std::mutex log_mutex;


uint64_t micros()
{
    uint64_t us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
    return us; 
}

#if LOGLEVEL >= LOGLEVEL_INFO
#define INFO(expr) \
log_mutex.lock(); \
std::cout << "[INFO]" << " [" << micros() << "] " << expr << std::endl; \
log_mutex.unlock(); 
#else
#define INFO(expr)
#endif

#if LOGLEVEL >= LOGLEVEL_DEBUG
#define DEBUG(expr) \
log_mutex.lock(); \
std::cout << "[DEBUG]" << " [" << micros() << "] " << expr << std::endl; \
log_mutex.unlock(); 
#else
#define DEBUG(expr)
#endif