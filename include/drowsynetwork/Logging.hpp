#pragma once

#include <print>

/**
 * @file Logging.hpp
 * @brief Lightweight logging system with customization support
 *
 * This logging system is designed to be simple but flexible. By default,
 * it uses std::println for output, but you can define your own LOG_* macros
 * before including this header to integrate with your favorite logging library.
 *
 * Example custom integration:
 * @code
 * #define LOG_INFO(fmt, ...) spdlog::info(fmt, ##__VA_ARGS__)
 * #include <drowsynetwork/Logging.hpp>
 * @endcode
 */

// Default logging implementation using std::println
// Users can define their own LOG_* macros before including this header
// to integrate with their preferred logging system

#ifndef LOG_DEBUG
/**
 * @brief Debug level logging - for detailed troubleshooting info
 * @param fmt Format string (printf-style)
 * @param ... Format arguments
 *
 * Debug logs are automatically disabled in release builds unless
 * ENABLE_DEBUG_LOGGING is defined. Use for verbose information that
 * helps during development but would clutter production logs.
 */
#define LOG_DEBUG(fmt, ...) std::println("[DEBUG] " fmt, ##__VA_ARGS__)
#endif

#ifndef LOG_INFO
/**
 * @brief Info level logging - for general information
 * @param fmt Format string (printf-style)
 * @param ... Format arguments
 *
 * Use for normal operational messages like "Server started on port 8080"
 * or "Client connected". Should be meaningful but not overwhelming.
 */
#define LOG_INFO(fmt, ...) std::println("[INFO] " fmt, ##__VA_ARGS__)
#endif

#ifndef LOG_WARN
/**
 * @brief Warning level logging - for recoverable problems
 * @param fmt Format string (printf-style)
 * @param ... Format arguments
 *
 * Use for situations that are unusual but not fatal, like connection
 * timeouts, retry attempts, or configuration fallbacks.
 */
#define LOG_WARN(fmt, ...) std::println("[WARN] " fmt, ##__VA_ARGS__)
#endif

#ifndef LOG_ERROR
/**
 * @brief Error level logging - for serious problems
 * @param fmt Format string (printf-style)
 * @param ... Format arguments
 *
 * Use for errors that prevent normal operation but don't crash the
 * application, like failed connections, invalid data, or resource exhaustion.
 */
#define LOG_ERROR(fmt, ...) std::println("[ERROR] " fmt, ##__VA_ARGS__)
#endif

// Disable debug logging in release builds unless explicitly enabled
#ifndef ENABLE_DEBUG_LOGGING
#ifdef NDEBUG
#undef LOG_DEBUG
/// In release builds, debug logging becomes a no-op for performance
#define LOG_DEBUG(fmt, ...) do {} while(0)
#endif
#endif