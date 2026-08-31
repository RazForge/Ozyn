#include "defense.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ---------- Clock ---------- */

static uint64_t clock_monotonic_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* ---------- NULL and pointer validation ---------- */

int ozayn_defense_not_null(const void *ptr, const char *name) {
    if (!ptr) {
        LOG_ERROR("DEFENSE", "NULL pointer: %s", name ? name : "(unknown)");
        return -1;
    }
    return 0;
}

int ozayn_defense_not_empty(const char *str, const char *name) {
    if (!str) {
        LOG_ERROR("DEFENSE", "NULL string: %s", name ? name : "(unknown)");
        return -1;
    }
    if (str[0] == '\0') {
        LOG_ERROR("DEFENSE", "Empty string: %s", name ? name : "(unknown)");
        return -1;
    }
    return 0;
}

int ozayn_defense_in_range_i32(int32_t val, int32_t min, int32_t max, const char *name) {
    if (val < min || val > max) {
        LOG_ERROR("DEFENSE", "Out of range: %s = %d (expected %d..%d)",
                  name ? name : "(unknown)", val, min, max);
        return -1;
    }
    return 0;
}

int ozayn_defense_in_range_u32(uint32_t val, uint32_t min, uint32_t max, const char *name) {
    if (val < min || val > max) {
        LOG_ERROR("DEFENSE", "Out of range: %s = %u (expected %u..%u)",
                  name ? name : "(unknown)", val, min, max);
        return -1;
    }
    return 0;
}

int ozayn_defense_in_range_u64(uint64_t val, uint64_t min, uint64_t max, const char *name) {
    if (val < min || val > max) {
        LOG_ERROR("DEFENSE", "Out of range: %s = %lu (expected %lu..%lu)",
                  name ? name : "(unknown)",
                  (unsigned long)val, (unsigned long)min, (unsigned long)max);
        return -1;
    }
    return 0;
}

/* ---------- Bounds-checked string operations ---------- */

size_t ozayn_defense_strlcpy(char *dst, const char *src, size_t dst_size) {
    if (!dst || !src || dst_size == 0) return 0;

    size_t src_len = strlen(src);
    if (src_len >= dst_size) {
        memcpy(dst, src, dst_size - 1);
        dst[dst_size - 1] = '\0';
        return src_len;
    }
    memcpy(dst, src, src_len + 1);
    return src_len;
}

size_t ozayn_defense_strlcat(char *dst, const char *src, size_t dst_size) {
    if (!dst || !src || dst_size == 0) return 0;

    size_t dst_len = strlen(dst);
    if (dst_len >= dst_size) return dst_size + strlen(src);

    size_t remaining = dst_size - dst_len - 1;
    size_t src_len = strlen(src);

    if (src_len <= remaining) {
        memcpy(dst + dst_len, src, src_len + 1);
    } else {
        memcpy(dst + dst_len, src, remaining);
        dst[dst_size - 1] = '\0';
    }
    return dst_len + src_len;
}

int ozayn_defense_str_check(const char *str, size_t max_len) {
    if (!str) return -1;
    if (strlen(str) > max_len) {
        LOG_ERROR("DEFENSE", "String too long: %zu > %zu", strlen(str), max_len);
        return -1;
    }
    return 0;
}

/* ---------- Safe integer parsing ---------- */

int ozayn_defense_parse_int32(const char *str, int32_t *out, int32_t min, int32_t max) {
    if (!str || !out) return -1;
    if (str[0] == '\0') return -1;

    char *end = NULL;
    long val = strtol(str, &end, 10);
    if (*end != '\0') {
        LOG_ERROR("DEFENSE", "Invalid integer: '%s'", str);
        return -1;
    }
    if (val < (long)min || val > (long)max) {
        LOG_ERROR("DEFENSE", "Integer out of range: %ld (expected %d..%d)", val, min, max);
        return -1;
    }
    *out = (int32_t)val;
    return 0;
}

int ozayn_defense_parse_uint32(const char *str, uint32_t *out, uint32_t min, uint32_t max) {
    if (!str || !out) return -1;
    if (str[0] == '\0') return -1;
    if (str[0] == '-') {
        LOG_ERROR("DEFENSE", "Negative value for unsigned: '%s'", str);
        return -1;
    }

    char *end = NULL;
    unsigned long val = strtoul(str, &end, 10);
    if (*end != '\0') {
        LOG_ERROR("DEFENSE", "Invalid unsigned integer: '%s'", str);
        return -1;
    }
    if (val < (unsigned long)min || val > (unsigned long)max) {
        LOG_ERROR("DEFENSE", "Unsigned out of range: %lu (expected %u..%u)",
                  val, min, max);
        return -1;
    }
    *out = (uint32_t)val;
    return 0;
}

int ozayn_defense_parse_uint64(const char *str, uint64_t *out, uint64_t min, uint64_t max) {
    if (!str || !out) return -1;
    if (str[0] == '\0') return -1;
    if (str[0] == '-') {
        LOG_ERROR("DEFENSE", "Negative value for unsigned: '%s'", str);
        return -1;
    }

    char *end = NULL;
    unsigned long long val = strtoull(str, &end, 10);
    if (*end != '\0') {
        LOG_ERROR("DEFENSE", "Invalid uint64: '%s'", str);
        return -1;
    }
    if (val < min || val > max) {
        LOG_ERROR("DEFENSE", "uint64 out of range");
        return -1;
    }
    *out = (uint64_t)val;
    return 0;
}

/* ---------- Format validation ---------- */

int ozayn_defense_is_alphanumeric(const char *str) {
    if (!str || str[0] == '\0') return 0;
    for (size_t i = 0; str[i]; i++) {
        if (!isalnum((unsigned char)str[i])) return 0;
    }
    return 1;
}

int ozayn_defense_is_identifier(const char *str) {
    if (!str || str[0] == '\0') return 0;
    if (!isalpha((unsigned char)str[0]) && str[0] != '_') return 0;
    for (size_t i = 1; str[i]; i++) {
        if (!isalnum((unsigned char)str[i]) && str[i] != '_' && str[i] != '.') return 0;
    }
    return 1;
}

int ozayn_defense_is_path_safe(const char *path) {
    if (!path || path[0] == '\0') return 0;
    /* Reject paths with ".." (directory traversal) */
    if (strstr(path, "..")) return 0;
    /* Reject paths starting with tilde (home expansion) */
    if (path[0] == '~') return 0;
    return 1;
}

int ozayn_defense_no_control_chars(const char *str) {
    if (!str) return 1;
    for (size_t i = 0; str[i]; i++) {
        if ((unsigned char)str[i] < 32 && str[i] != '\t' && str[i] != '\n') return 0;
    }
    return 1;
}

/* ---------- Violation log ---------- */

void ozayn_defense_log_init(ozayn_defense_log_t *log) {
    if (!log) return;
    memset(log, 0, sizeof(ozayn_defense_log_t));
}

void ozayn_defense_log_record(ozayn_defense_log_t *log, const char *func,
                               const char *msg, int32_t code) {
    if (!log) return;

    ozayn_defense_violation_t *v = &log->violations[log->head];
    memset(v, 0, sizeof(ozayn_defense_violation_t));
    ozayn_defense_strlcpy(v->function, func ? func : "?", sizeof(v->function));
    ozayn_defense_strlcpy(v->message, msg ? msg : "?", sizeof(v->message));
    v->code = code;
    v->timestamp_us = clock_monotonic_us();

    log->head = (log->head + 1) % OZAYN_DEFENSE_MAX_VIOLATIONS;
    if (log->total_count < OZAYN_DEFENSE_MAX_VIOLATIONS)
        log->total_count++;
}

uint32_t ozayn_defense_log_count(const ozayn_defense_log_t *log) {
    return log ? log->total_count : 0;
}

const ozayn_defense_violation_t *ozayn_defense_log_get(const ozayn_defense_log_t *log,
                                                        uint32_t index) {
    if (!log || index >= log->total_count) return NULL;
    uint32_t actual = (log->head + OZAYN_DEFENSE_MAX_VIOLATIONS - 1 - index)
                      % OZAYN_DEFENSE_MAX_VIOLATIONS;
    return &log->violations[actual];
}

void ozayn_defense_log_print(const ozayn_defense_log_t *log) {
    if (!log) return;

    LOG_INFO("DEFENSE", "=== Violation Log (%u entries) ===", log->total_count);
    uint32_t count = log->total_count < OZAYN_DEFENSE_MAX_VIOLATIONS
                     ? log->total_count : OZAYN_DEFENSE_MAX_VIOLATIONS;
    for (uint32_t i = 0; i < count; i++) {
        const ozayn_defense_violation_t *v = &log->violations[
            (log->head + OZAYN_DEFENSE_MAX_VIOLATIONS - 1 - i) %
            OZAYN_DEFENSE_MAX_VIOLATIONS];
        LOG_INFO("DEFENSE", "  [%d] %s: %s", v->code, v->function, v->message);
    }
}

void ozayn_defense_log_clear(ozayn_defense_log_t *log) {
    if (log) {
        log->head = 0;
        log->total_count = 0;
    }
}
