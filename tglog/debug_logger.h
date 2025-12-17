#ifndef TGLOG__DEBUG_LOGGER_H
#define TGLOG__DEBUG_LOGGER_H

/* Standard library headers */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

/* System headers */

/* Third-party library headers */

/* Project headers */

#ifdef __cplusplus
extern "C" {
#endif

/**
 log levels
 */
typedef enum tglog_level {
    TGLOG_LEVEL_DEBUG = 0,
    TGLOG_LEVEL_INFO,
    TGLOG_LEVEL_WARN,
    TGLOG_LEVEL_ERROR,
    TGLOG_LEVEL_FATAL
} tglog_level;

/**
 Callback function used to check if a log message of a given level should be written.

 @param level The log level.
 @param user_data The user data.
 @return true if the log message should be written, false otherwise.
 */
typedef bool (*can_log_func)(const tglog_level level, void *user_data);

/**
 Callback function used to write a log message of a given level.

 @param user_data The user data, usually NULL.
 @param level The log level.
 @param file The file name.
 @param line The line number.
 @param func The function name.
 @param format The format string.
 @param ... The format arguments.
 */
typedef void (*write_log_func)(void *user_data, tglog_level level, const char *file, const int line, const char *func, const char *format, ...);

typedef struct tglog_debug_logger {
    can_log_func can_log;
    write_log_func write_log;
    void *user_data;
} tglog_debug_logger;

/**
 Creates a debug logger
 @param can_log The callback function used to check if a log message of a given level should be written.
 @param write_log The callback function used to write a log message of a given level.
 @param user_data The user data.
 @return The logger or NULL on error.
 */
tglog_debug_logger *tglog_debug_logger_create(can_log_func can_log, write_log_func write_log, void *user_data);

/**
 Destroy a debug logger
 @param logger The logger to destroy.
 */
void tglog_debug_logger_destroy(tglog_debug_logger *logger);

/**
 Can write the logs whose level >= TGLOG_LEVEL_DEBUG
 */
bool tglog_can_log_debug(const tglog_level level, void *user_data);

/**
 Can write the logs whose level >= TGLOG_LEVEL_INFO
 */
bool tglog_can_log_info(const tglog_level level, void *user_data);

/**
 Can write the logs whose level >= TGLOG_LEVEL_WARN
 */
bool tglog_can_log_warn(const tglog_level level, void *user_data);

/**
 Can write the logs whose level >= TGLOG_LEVEL_ERROR
 */
bool tglog_can_log_error(const tglog_level level, void *user_data);

/**
 Can write the logs whose level >= TGLOG_LEVEL_FATAL
 */
bool tglog_can_log_fatal(const tglog_level level, void *user_data);

/**
 Can not write any logs
 */
bool tglog_can_log_none(const tglog_level level, void *user_data);

/**
 Write a log message to stdout

 @param user_data The user data, usually NULL.
 @param level The log level.
 @param file The file name.
 @param line The line number.
 @param func The function name.
 @param format The format string.
 @param ... The format arguments.
 */
void tglog_std_write_log(void *user_data, tglog_level level, const char *file, const int line, const char *func, const char *format, ...);

/**
 Write a log message with the given debug logger
 @param logger The logger.
 @param level The log level.
 @param fmt The format string.
 @param ... The format arguments.
 */
#define _tglog_log(logger, level, fmt, ...) do { \
    if (!(logger)) break; \
    if (!(logger)->can_log || !(logger)->write_log) break; \
    if (!(logger)->can_log(level, (logger)->user_data)) break; \
    (logger)->write_log((logger)->user_data, level, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__); \
} while (0)

/**
 Writes a debug log with the given logger
 @param logger The logger.
 @param fmt The format string.
 @param ... The format arguments.
 */
#define tglog_debug(logger, fmt, ...) \
    _tglog_log(logger, TGLOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)

/**
 Writes an info log with the given logger
 @param logger The logger.
 @param fmt The format string.
 @param ... The format arguments.
 */
#define tglog_info(logger, fmt, ...) \
    _tglog_log(logger, TGLOG_LEVEL_INFO, fmt, ##__VA_ARGS__)

/**
 Writes a warn log with the given logger
 @param logger The logger.
 @param fmt The format string.
 @param ... The format arguments.
 */
#define tglog_warn(logger, fmt, ...) \
    _tglog_log(logger, TGLOG_LEVEL_WARN, fmt, ##__VA_ARGS__)

/**
 Writes an error log with the given logger
 @param logger The logger.
 @param fmt The format string.
 @param ... The format arguments.
 */
#define tglog_error(logger, fmt, ...) \
    _tglog_log(logger, TGLOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

/**
 Writes a fatal log with the given logger
 @param logger The logger.
 @param fmt The format string.
 @param ... The format arguments.
 */
#define tglog_fatal(logger, fmt, ...) \
    _tglog_log(logger, TGLOG_LEVEL_FATAL, fmt, ##__VA_ARGS__)


#ifdef __cplusplus
}
#endif

#endif /* TGLOG__DEBUG_LOGGER_H */