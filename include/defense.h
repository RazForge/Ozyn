#ifndef OZAYN_DEFENSE_H
#define OZAYN_DEFENSE_H

#include <stdint.h>
#include <stddef.h>

/*
 * defense.h — Input Validation & Defensive Programming (Stage 29).
 *
 * Self-contained header — no circular includes.
 * Provides safe utilities for input validation, bounds checking,
 * NULL protection, and defensive string operations.
 *
 * Principles:
 *   - Never trust input blindly
 *   - Validate before processing
 *   - Fail safely, not silently
 *   - Log violations for audit
 */

/* ---- Validation result ---- */

typedef enum {
    OZAYN_DEFENSE_OK           =  0,
    OZAYN_DEFENSE_ERR_NULL     = -1,
    OZAYN_DEFENSE_ERR_RANGE    = -2,
    OZAYN_DEFENSE_ERR_LENGTH   = -3,
    OZAYN_DEFENSE_ERR_FORMAT   = -4,
    OZAYN_DEFENSE_ERR_PERM     = -5,
    OZAYN_DEFENSE_ERR_RESOURCE = -6,
} ozayn_defense_result_t;

/* ---- Constants ---- */

#define OZAYN_DEFENSE_MAX_NAME       256
#define OZAYN_DEFENSE_MAX_PATH       1024
#define OZAYN_DEFENSE_MAX_MSG        2048
#define OZAYN_DEFENSE_MAX_LABEL      128

/* ---- NULL and pointer validation ---- */

int ozayn_defense_not_null(const void *ptr, const char *name);
int ozayn_defense_not_empty(const char *str, const char *name);
int ozayn_defense_in_range_i32(int32_t val, int32_t min, int32_t max, const char *name);
int ozayn_defense_in_range_u32(uint32_t val, uint32_t min, uint32_t max, const char *name);
int ozayn_defense_in_range_u64(uint64_t val, uint64_t min, uint64_t max, const char *name);

/* ---- Bounds-checked string operations ---- */

size_t ozayn_defense_strlcpy(char *dst, const char *src, size_t dst_size);
size_t ozayn_defense_strlcat(char *dst, const char *src, size_t dst_size);
int    ozayn_defense_str_check(const char *str, size_t max_len);

/* ---- Safe integer parsing ---- */

int ozayn_defense_parse_int32(const char *str, int32_t *out, int32_t min, int32_t max);
int ozayn_defense_parse_uint32(const char *str, uint32_t *out, uint32_t min, uint32_t max);
int ozayn_defense_parse_uint64(const char *str, uint64_t *out, uint64_t min, uint64_t max);

/* ---- Format validation ---- */

int ozayn_defense_is_alphanumeric(const char *str);
int ozayn_defense_is_identifier(const char *str);
int ozayn_defense_is_path_safe(const char *path);
int ozayn_defense_no_control_chars(const char *str);

/* ---- Violation tracking ---- */

#define OZAYN_DEFENSE_MAX_VIOLATIONS  256
#define OZAYN_DEFENSE_MAX_VIOLATION_MSG 128

typedef struct {
    char     function[OZAYN_DEFENSE_MAX_LABEL];
    char     message[OZAYN_DEFENSE_MAX_VIOLATION_MSG];
    int32_t  code;
    uint64_t timestamp_us;
} ozayn_defense_violation_t;

typedef struct {
    ozayn_defense_violation_t violations[OZAYN_DEFENSE_MAX_VIOLATIONS];
    uint32_t                 head;
    uint32_t                 total_count;
} ozayn_defense_log_t;

void     ozayn_defense_log_init(ozayn_defense_log_t *log);
void     ozayn_defense_log_record(ozayn_defense_log_t *log, const char *func,
                                   const char *msg, int32_t code);
uint32_t ozayn_defense_log_count(const ozayn_defense_log_t *log);
const ozayn_defense_violation_t *ozayn_defense_log_get(const ozayn_defense_log_t *log,
                                                        uint32_t index);
void     ozayn_defense_log_print(const ozayn_defense_log_t *log);
void     ozayn_defense_log_clear(ozayn_defense_log_t *log);

#endif /* OZAYN_DEFENSE_H */
