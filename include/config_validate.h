#ifndef OZAYN_CONFIG_VALIDATE_H
#define OZAYN_CONFIG_VALIDATE_H

#include <stdint.h>
#include <stddef.h>

/*
 * config_validate.h — Configuration Validation & Rollback (Stage 29).
 *
 * Self-contained header — no circular includes.
 * Validates configuration before applying, supports rollback to
 * last known-good configuration, and provides atomic config changes.
 */

/* ---- Constants ---- */

#define OZAYN_CV_MAX_KEYS       128
#define OZAYN_CV_MAX_KEY_NAME   64
#define OZAYN_CV_MAX_VALUE      256
#define OZAYN_CV_MAX_SNAPSHOTS  8
#define OZAYN_CV_MAX_ERRORS     32

/* ---- Value types ---- */

typedef enum {
    OZAYN_CV_TYPE_INT    = 0,
    OZAYN_CV_TYPE_UINT   = 1,
    OZAYN_CV_TYPE_FLOAT  = 2,
    OZAYN_CV_TYPE_BOOL   = 3,
    OZAYN_CV_TYPE_STRING = 4,
} ozayn_cv_value_type_t;

/* ---- Validation severity ---- */

typedef enum {
    OZAYN_CV_SEV_INFO    = 0,
    OZAYN_CV_SEV_WARNING = 1,
    OZAYN_CV_SEV_ERROR   = 2,
    OZAYN_CV_SEV_FATAL   = 3,
} ozayn_cv_severity_t;

/* ---- Key constraint types ---- */

typedef enum {
    OZAYN_CV_CONST_RANGE_INT  = 0,
    OZAYN_CV_CONST_RANGE_UINT = 1,
    OZAYN_CV_CONST_RANGE_FLOAT = 2,
    OZAYN_CV_CONST_ENUM       = 3,
    OZAYN_CV_CONST_NOT_EMPTY  = 4,
    OZAYN_CV_CONST_MAX_LEN    = 5,
    OZAYN_CV_CONST_ONE_OF     = 6,
} ozayn_cv_constraint_type_t;

/* ---- Key value (for snapshots) ---- */

typedef struct {
    char                      key[OZAYN_CV_MAX_KEY_NAME];
    ozayn_cv_value_type_t     type;
    union {
        int64_t               i;
        uint64_t              u;
        double                f;
        int                   b;
        char                  s[OZAYN_CV_MAX_VALUE];
    } value;
} ozayn_cv_key_value_t;

/* ---- Configuration snapshot (for rollback) ---- */

typedef struct {
    ozayn_cv_key_value_t  keys[OZAYN_CV_MAX_KEYS];
    uint32_t              key_count;
    uint32_t              version;
    int                   valid;
} ozayn_cv_snapshot_t;

/* ---- Validation error ---- */

typedef struct {
    char                 key[OZAYN_CV_MAX_KEY_NAME];
    char                 message[OZAYN_CV_MAX_VALUE];
    ozayn_cv_severity_t  severity;
} ozayn_cv_error_t;

/* ---- Validation constraint ---- */

typedef struct {
    char                      key[OZAYN_CV_MAX_KEY_NAME];
    ozayn_cv_constraint_type_t type;
    ozayn_cv_value_type_t     value_type;
    union {
        struct { int64_t min; int64_t max; } range_i;
        struct { uint64_t min; uint64_t max; } range_u;
        struct { double min; double max; } range_f;
        struct { char values[8][64]; int count; } enum_vals;
        struct { size_t max_len; } str;
        struct { char values[16][64]; int count; } one_of;
    } constraint;
    int required;
    int active;
} ozayn_cv_constraint_t;

/* ---- Config validator ---- */

typedef struct {
    ozayn_cv_snapshot_t    snapshots[OZAYN_CV_MAX_SNAPSHOTS];
    uint32_t               snapshot_head;
    uint32_t               snapshot_count;
    uint32_t               current_version;
    ozayn_cv_constraint_t  constraints[OZAYN_CV_MAX_KEYS];
    uint32_t               constraint_count;
    ozayn_cv_error_t       errors[OZAYN_CV_MAX_ERRORS];
    uint32_t               error_count;
    int                    initialized;
} ozayn_config_validator_t;

/* ---- Lifecycle ---- */

int  ozayn_cv_init(ozayn_config_validator_t *v);
void ozayn_cv_shutdown(ozayn_config_validator_t *v);

/* ---- Constraint registration ---- */

int  ozayn_cv_add_range_int(ozayn_config_validator_t *v, const char *key,
                              int64_t min, int64_t max, int required);
int  ozayn_cv_add_range_uint(ozayn_config_validator_t *v, const char *key,
                               uint64_t min, uint64_t max, int required);
int  ozayn_cv_add_range_float(ozayn_config_validator_t *v, const char *key,
                                double min, double max, int required);
int  ozayn_cv_add_enum(ozayn_config_validator_t *v, const char *key,
                         const char **values, int count, int required);
int  ozayn_cv_add_max_length(ozayn_config_validator_t *v, const char *key,
                               size_t max_len, int required);
int  ozayn_cv_add_one_of(ozayn_config_validator_t *v, const char *key,
                           const char **values, int count, int required);

/* ---- Snapshot management ---- */

int  ozayn_cv_snapshot_save(ozayn_config_validator_t *v,
                              const ozayn_cv_key_value_t *keys, uint32_t count);
int  ozayn_cv_snapshot_restore(ozayn_config_validator_t *v, uint32_t version);
const ozayn_cv_snapshot_t *ozayn_cv_snapshot_current(const ozayn_config_validator_t *v);
uint32_t ozayn_cv_version(const ozayn_config_validator_t *v);

/* ---- Validation ---- */

int  ozayn_cv_validate(ozayn_config_validator_t *v,
                         const ozayn_cv_key_value_t *keys, uint32_t count);
int  ozayn_cv_has_errors(const ozayn_config_validator_t *v);
uint32_t ozayn_cv_error_count(const ozayn_config_validator_t *v);
const ozayn_cv_error_t *ozayn_cv_get_error(const ozayn_config_validator_t *v,
                                             uint32_t index);
void ozayn_cv_clear_errors(ozayn_config_validator_t *v);

/* ---- Print ---- */

void ozayn_cv_print_errors(const ozayn_config_validator_t *v);
void ozayn_cv_print_constraints(const ozayn_config_validator_t *v);
void ozayn_cv_print_snapshot(const ozayn_cv_snapshot_t *snap);

#endif /* OZAYN_CONFIG_VALIDATE_H */
