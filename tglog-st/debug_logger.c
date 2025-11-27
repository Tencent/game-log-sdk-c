/* Standard library headers */
#include <time.h>

/* System headers */
#include <sys/time.h>

/* Third-party library headers */

/* Project headers */
#include "debug_logger.h"

tglog_debug_logger *tglog_debug_logger_create(can_log_func can_log, write_log_func write_log, void *user_data) {
    if (!can_log || !write_log) {
        return NULL;
    }

    tglog_debug_logger *logger = (tglog_debug_logger *) malloc(sizeof(tglog_debug_logger));
    if (!logger) {
        return NULL;
    }

    logger->can_log = can_log;
    logger->write_log = write_log;
    logger->user_data = user_data;

    return logger;
}

void tglog_debug_logger_destroy(tglog_debug_logger *logger) {
    if (logger) {
        free(logger);
    }
}

bool tglog_can_log_debug(const tglog_level level, void *user_data) {
    (void) user_data;
    return level >= TGLOG_LEVEL_DEBUG;
}

bool tglog_can_log_info(const tglog_level level, void *user_data) {
    (void) user_data;
    return level >= TGLOG_LEVEL_INFO;
}

bool tglog_can_log_warn(const tglog_level level, void *user_data) {
    (void) user_data;
    return level >= TGLOG_LEVEL_WARN;
}

bool tglog_can_log_error(const tglog_level level, void *user_data) {
    (void) user_data;
    return level >= TGLOG_LEVEL_ERROR;
}

bool tglog_can_log_fatal(const tglog_level level, void *user_data) {
    (void) user_data;
    return level >= TGLOG_LEVEL_FATAL;
}

bool tglog_can_log_none(const tglog_level level, void *user_data) {
    (void) level;
    (void) user_data;
    return false;
}

static const char *level_to_string(tglog_level level) {
    switch (level) {
        case TGLOG_LEVEL_DEBUG:
            return "DEBUG";
        case TGLOG_LEVEL_INFO:
            return "INFO";
        case TGLOG_LEVEL_WARN:
            return "WARN";
        case TGLOG_LEVEL_ERROR:
            return "ERROR";
        case TGLOG_LEVEL_FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
    }
}

void tglog_std_write_log(void *user_data, tglog_level level, const char *file, const int line, const char *func, const char *format, ...) {
    (void) user_data; /* Unused */
    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm *time_info;
    time_info = localtime(&tv.tv_sec);

    char time_buffer[64];
    char *time_format = "%Y-%m-%d %H:%M:%S";
    strftime(time_buffer, sizeof(time_buffer), time_format, time_info);
    snprintf(time_buffer + 19, sizeof(time_buffer) - 19, ".%06ld", (long) tv.tv_usec);

    fprintf(stdout, "[%s] [%s] [%s:%d] [%s()]: ",
            time_buffer,
            level_to_string(level),
            file, line, func);

    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);

    fprintf(stdout, "\n");
    fflush(stdout);
}