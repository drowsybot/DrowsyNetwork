#pragma once

#include <print>

// Default logging implementation using std::println
// Users can define their own LOG_* macros before including this header
// to integrate with their preferred logging system

#ifndef LOG_DEBUG
#define LOG_DEBUG(fmt, ...) std::println("[DEBUG] " fmt, ##__VA_ARGS__)
#endif

#ifndef LOG_INFO
#define LOG_INFO(fmt, ...) std::println("[INFO] " fmt, ##__VA_ARGS__)
#endif

#ifndef LOG_WARN
#define LOG_WARN(fmt, ...) std::println("[WARN] " fmt, ##__VA_ARGS__)
#endif

#ifndef LOG_ERROR
#define LOG_ERROR(fmt, ...) std::println("[ERROR] " fmt, ##__VA_ARGS__)
#endif

// Disable debug logging in release builds unless explicitly enabled
#ifndef ENABLE_DEBUG_LOGGING
#ifdef NDEBUG
#undef LOG_DEBUG
#define LOG_DEBUG(fmt, ...) do {} while(0)
#endif
#endif