#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

/* Global logger pointer — used by convenience macros */
ozayn_logger_t *g_logger = NULL;

/* ---------- Level name ---------- */

const char *ozayn_log_level_str(int level) {
    switch (level) {
        case OZAYN_LOG_DEBUG:    return "DEBUG";
        case OZAYN_LOG_INFO:     return "INFO ";
        case OZAYN_LOG_WARNING:  return "WARN ";
        case OZAYN_LOG_ERROR:    return "ERROR";
        case OZAYN_LOG_CRITICAL: return "CRIT ";
    }
    return "?????";
}

/* ---------- Timestamp ---------- */

static void format_timestamp(char *buf, size_t buflen) {
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(buf, buflen, "%Y-%m-%d %H:%M:%S", &tm);
}

/* ---------- Init ---------- */

ozayn_result_t ozayn_logger_init(ozayn_logger_t *log, const ozayn_log_config_t *cfg) {
    if (!log || !cfg) return OZAYN_ERR_NULL;

    memset(log, 0, sizeof(ozayn_logger_t));
    log->config = *cfg;
    log->file = NULL;

    /* Create log directory if file logging enabled */
    if (cfg->file_enabled && cfg->directory[0] != '\0') {
        mkdir(cfg->directory, 0755);

        char path[512];
        snprintf(path, sizeof(path), "%s/ozayn.log", cfg->directory);

        log->file = fopen(path, "a");
        if (!log->file) {
            fprintf(stderr, "[%s] Logger: cannot open %s — continuing with console only\n",
                    OZAYN_NAME, path);
            log->config.file_enabled = 0;
        }
    }

    log->state = OZAYN_LOG_ACTIVE;

    /* Set global pointer */
    g_logger = log;

    return OZAYN_OK;
}

/* ---------- Shutdown ---------- */

void ozayn_logger_shutdown(ozayn_logger_t *log) {
    if (!log) return;

    if (log->file) {
        fclose(log->file);
        log->file = NULL;
    }

    log->state = OZAYN_LOG_STOPPED;
    g_logger = NULL;
}

/* ---------- Log ---------- */

void ozayn_log(ozayn_logger_t *log, int level, const char *component,
               const char *fmt, ...) {
    /* Before logger init, fall back to stderr */
    if (!log || log->state != OZAYN_LOG_ACTIVE) {
        if (level >= OZAYN_LOG_WARNING) {
            va_list ap;
            va_start(ap, fmt);
            fprintf(stderr, "[%s] ", OZAYN_NAME);
            vfprintf(stderr, fmt, ap);
            fprintf(stderr, "\n");
            va_end(ap);
        }
        return;
    }

    /* Filter by level */
    if (level < log->config.min_level) return;

    /* Format message */
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    /* Format record */
    char ts[64];
    format_timestamp(ts, sizeof(ts));

    const char *comp = component ? component : "???";

    /* Console */
    if (log->config.console_enabled) {
        fprintf(stderr, "[%s] [%s] [%s] %s\n", ts, ozayn_log_level_str(level), comp, msg);
    }

    /* File */
    if (log->config.file_enabled && log->file) {
        fprintf(log->file, "[%s] [%s] [%s] %s\n", ts, ozayn_log_level_str(level), comp, msg);
        fflush(log->file);
    }
}
