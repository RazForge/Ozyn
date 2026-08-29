#ifndef OZAYN_LOGGER_H
#define OZAYN_LOGGER_H

#include "ozayn.h"
#include <stdio.h>
#include <stdarg.h>

/*
 * ozayn_logger.h — Centralized logging system.
 *
 * Single global logger. Two destinations: console + file.
 * Format: [TIMESTAMP] [LEVEL] [COMPONENT] message
 */

/* Log levels */
#define OZAYN_LOG_DEBUG     0
#define OZAYN_LOG_INFO      1
#define OZAYN_LOG_WARNING   2
#define OZAYN_LOG_ERROR     3
#define OZAYN_LOG_CRITICAL  4

/* Logger states */
typedef enum {
    OZAYN_LOG_UNINITIALIZED = 0,
    OZAYN_LOG_ACTIVE        = 1,
    OZAYN_LOG_STOPPED       = 2,
} ozayn_log_state_t;

/* Logger configuration */
typedef struct {
    int   console_enabled;     /* write to stderr */
    int   file_enabled;        /* write to log file */
    char  directory[256];      /* log directory path */
    int   min_level;           /* minimum level to output */
} ozayn_log_config_t;

/* Logger object */
typedef struct {
    ozayn_log_state_t state;
    ozayn_log_config_t config;
    FILE *file;                /* log file handle */
} ozayn_logger_t;

/* Global logger — set by ozayn_logger_init, cleared by shutdown */
extern ozayn_logger_t *g_logger;

/* Lifecycle */
ozayn_result_t ozayn_logger_init(ozayn_logger_t *log, const ozayn_log_config_t *cfg);
void           ozayn_logger_shutdown(ozayn_logger_t *log);

/* Logging */
void ozayn_log(ozayn_logger_t *log, int level, const char *component,
               const char *fmt, ...) __attribute__((format(printf, 4, 5)));

/* Convenience macros — require a global 'g_logger' pointer */
#define LOG_DEBUG(comp, ...)    ozayn_log(g_logger, OZAYN_LOG_DEBUG,    comp, __VA_ARGS__)
#define LOG_INFO(comp, ...)     ozayn_log(g_logger, OZAYN_LOG_INFO,     comp, __VA_ARGS__)
#define LOG_WARN(comp, ...)     ozayn_log(g_logger, OZAYN_LOG_WARNING,  comp, __VA_ARGS__)
#define LOG_ERROR(comp, ...)    ozayn_log(g_logger, OZAYN_LOG_ERROR,    comp, __VA_ARGS__)
#define LOG_CRITICAL(comp, ...) ozayn_log(g_logger, OZAYN_LOG_CRITICAL, comp, __VA_ARGS__)

/* Query */
const char *ozayn_log_level_str(int level);

#endif
