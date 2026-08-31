#include "config_validate.h"
#include "defense.h"
#include "logger.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ---------- Lifecycle ---------- */

int ozayn_cv_init(ozayn_config_validator_t *v) {
    if (!v) return -1;
    memset(v, 0, sizeof(ozayn_config_validator_t));
    v->current_version = 0;
    v->initialized = 1;
    LOG_INFO("CONFIG_VALIDATE", "Config validator initialized");
    return 0;
}

void ozayn_cv_shutdown(ozayn_config_validator_t *v) {
    if (!v) return;
    v->initialized = 0;
    LOG_INFO("CONFIG_VALIDATE", "Config validator shut down (version=%u)", v->current_version);
}

/* ---------- Constraint registration ---------- */

static ozayn_cv_constraint_t *find_constraint(ozayn_config_validator_t *v, const char *key) {
    for (uint32_t i = 0; i < v->constraint_count; i++) {
        if (v->constraints[i].active && strcmp(v->constraints[i].key, key) == 0)
            return &v->constraints[i];
    }
    return NULL;
}

static ozayn_cv_constraint_t *add_constraint(ozayn_config_validator_t *v, const char *key) {
    if (v->constraint_count >= OZAYN_CV_MAX_KEYS) return NULL;

    ozayn_cv_constraint_t *c = &v->constraints[v->constraint_count];
    memset(c, 0, sizeof(ozayn_cv_constraint_t));
    ozayn_defense_strlcpy(c->key, key, sizeof(c->key));
    c->active = 1;
    v->constraint_count++;
    return c;
}

int ozayn_cv_add_range_int(ozayn_config_validator_t *v, const char *key,
                             int64_t min, int64_t max, int required) {
    if (!v || !key) return -1;

    ozayn_cv_constraint_t *existing = find_constraint(v, key);
    if (existing) return -1;

    ozayn_cv_constraint_t *c = add_constraint(v, key);
    if (!c) return -1;

    c->type = OZAYN_CV_CONST_RANGE_INT;
    c->value_type = OZAYN_CV_TYPE_INT;
    c->constraint.range_i.min = min;
    c->constraint.range_i.max = max;
    c->required = required;
    return 0;
}

int ozayn_cv_add_range_uint(ozayn_config_validator_t *v, const char *key,
                              uint64_t min, uint64_t max, int required) {
    if (!v || !key) return -1;

    ozayn_cv_constraint_t *existing = find_constraint(v, key);
    if (existing) return -1;

    ozayn_cv_constraint_t *c = add_constraint(v, key);
    if (!c) return -1;

    c->type = OZAYN_CV_CONST_RANGE_UINT;
    c->value_type = OZAYN_CV_TYPE_UINT;
    c->constraint.range_u.min = min;
    c->constraint.range_u.max = max;
    c->required = required;
    return 0;
}

int ozayn_cv_add_range_float(ozayn_config_validator_t *v, const char *key,
                               double min, double max, int required) {
    if (!v || !key) return -1;

    ozayn_cv_constraint_t *existing = find_constraint(v, key);
    if (existing) return -1;

    ozayn_cv_constraint_t *c = add_constraint(v, key);
    if (!c) return -1;

    c->type = OZAYN_CV_CONST_RANGE_FLOAT;
    c->value_type = OZAYN_CV_TYPE_FLOAT;
    c->constraint.range_f.min = min;
    c->constraint.range_f.max = max;
    c->required = required;
    return 0;
}

int ozayn_cv_add_enum(ozayn_config_validator_t *v, const char *key,
                        const char **values, int count, int required) {
    if (!v || !key || !values || count <= 0 || count > 8) return -1;

    ozayn_cv_constraint_t *existing = find_constraint(v, key);
    if (existing) return -1;

    ozayn_cv_constraint_t *c = add_constraint(v, key);
    if (!c) return -1;

    c->type = OZAYN_CV_CONST_ENUM;
    c->value_type = OZAYN_CV_TYPE_STRING;
    c->constraint.enum_vals.count = count;
    for (int i = 0; i < count; i++) {
        ozayn_defense_strlcpy(c->constraint.enum_vals.values[i], values[i], 64);
    }
    c->required = required;
    return 0;
}

int ozayn_cv_add_max_length(ozayn_config_validator_t *v, const char *key,
                              size_t max_len, int required) {
    if (!v || !key) return -1;

    ozayn_cv_constraint_t *existing = find_constraint(v, key);
    if (existing) return -1;

    ozayn_cv_constraint_t *c = add_constraint(v, key);
    if (!c) return -1;

    c->type = OZAYN_CV_CONST_MAX_LEN;
    c->value_type = OZAYN_CV_TYPE_STRING;
    c->constraint.str.max_len = max_len;
    c->required = required;
    return 0;
}

int ozayn_cv_add_one_of(ozayn_config_validator_t *v, const char *key,
                          const char **values, int count, int required) {
    if (!v || !key || !values || count <= 0 || count > 16) return -1;

    ozayn_cv_constraint_t *existing = find_constraint(v, key);
    if (existing) return -1;

    ozayn_cv_constraint_t *c = add_constraint(v, key);
    if (!c) return -1;

    c->type = OZAYN_CV_CONST_ONE_OF;
    c->value_type = OZAYN_CV_TYPE_STRING;
    c->constraint.one_of.count = count;
    for (int i = 0; i < count; i++) {
        ozayn_defense_strlcpy(c->constraint.one_of.values[i], values[i], 64);
    }
    c->required = required;
    return 0;
}

/* ---------- Snapshot management ---------- */

int ozayn_cv_snapshot_save(ozayn_config_validator_t *v,
                             const ozayn_cv_key_value_t *keys, uint32_t count) {
    if (!v || !keys) return -1;
    if (count > OZAYN_CV_MAX_KEYS) return -1;

    ozayn_cv_snapshot_t *snap = &v->snapshots[v->snapshot_head];
    memset(snap, 0, sizeof(ozayn_cv_snapshot_t));

    for (uint32_t i = 0; i < count; i++) {
        snap->keys[i] = keys[i];
    }
    snap->key_count = count;
    snap->version = v->current_version;
    snap->valid = 1;

    v->snapshot_head = (v->snapshot_head + 1) % OZAYN_CV_MAX_SNAPSHOTS;
    if (v->snapshot_count < OZAYN_CV_MAX_SNAPSHOTS)
        v->snapshot_count++;
    v->current_version++;

    LOG_DEBUG("CONFIG_VALIDATE", "Snapshot saved: version=%u, keys=%u",
              snap->version, count);
    return 0;
}

int ozayn_cv_snapshot_restore(ozayn_config_validator_t *v, uint32_t version) {
    if (!v) return -1;

    for (uint32_t i = 0; i < v->snapshot_count; i++) {
        uint32_t idx = (v->snapshot_head + OZAYN_CV_MAX_SNAPSHOTS - 1 - i)
                       % OZAYN_CV_MAX_SNAPSHOTS;
        if (v->snapshots[idx].valid && v->snapshots[idx].version == version) {
            LOG_INFO("CONFIG_VALIDATE", "Snapshot restored: version=%u", version);
            return 0;
        }
    }

    LOG_ERROR("CONFIG_VALIDATE", "Snapshot not found: version=%u", version);
    return -1;
}

const ozayn_cv_snapshot_t *ozayn_cv_snapshot_current(const ozayn_config_validator_t *v) {
    if (!v || v->snapshot_count == 0) return NULL;
    uint32_t idx = (v->snapshot_head + OZAYN_CV_MAX_SNAPSHOTS - 1) % OZAYN_CV_MAX_SNAPSHOTS;
    return &v->snapshots[idx];
}

uint32_t ozayn_cv_version(const ozayn_config_validator_t *v) {
    return v ? v->current_version : 0;
}

/* ---------- Validation ---------- */

void ozayn_cv_clear_errors(ozayn_config_validator_t *v) {
    if (v) v->error_count = 0;
}

static void add_error(ozayn_config_validator_t *v, const char *key,
                       const char *msg, ozayn_cv_severity_t sev) {
    if (v->error_count >= OZAYN_CV_MAX_ERRORS) return;

    ozayn_cv_error_t *e = &v->errors[v->error_count];
    ozayn_defense_strlcpy(e->key, key, sizeof(e->key));
    ozayn_defense_strlcpy(e->message, msg, sizeof(e->message));
    e->severity = sev;
    v->error_count++;
}

int ozayn_cv_validate(ozayn_config_validator_t *v,
                        const ozayn_cv_key_value_t *keys, uint32_t count) {
    if (!v || !keys) return -1;

    v->error_count = 0;

    /* Check required keys */
    for (uint32_t ci = 0; ci < v->constraint_count; ci++) {
        ozayn_cv_constraint_t *con = &v->constraints[ci];
        if (!con->active || !con->required) continue;

        int found = 0;
        for (uint32_t ki = 0; ki < count; ki++) {
            if (strcmp(keys[ki].key, con->key) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            add_error(v, con->key, "Required key missing", OZAYN_CV_SEV_FATAL);
        }
    }

    /* Validate each provided key against constraints */
    for (uint32_t ki = 0; ki < count; ki++) {
        const ozayn_cv_key_value_t *kv = &keys[ki];
        ozayn_cv_constraint_t *con = find_constraint(v, kv->key);
        if (!con) continue; /* no constraint → accept any value */

        char msg[256];
        switch (con->type) {
            case OZAYN_CV_CONST_RANGE_INT:
                if (kv->type != OZAYN_CV_TYPE_INT) {
                    add_error(v, kv->key, "Type mismatch (expected int)", OZAYN_CV_SEV_ERROR);
                } else if (kv->value.i < con->constraint.range_i.min ||
                           kv->value.i > con->constraint.range_i.max) {
                    snprintf(msg, sizeof(msg), "Out of range (%ld..%ld)",
                             (long)con->constraint.range_i.min,
                             (long)con->constraint.range_i.max);
                    add_error(v, kv->key, msg, OZAYN_CV_SEV_ERROR);
                }
                break;

            case OZAYN_CV_CONST_RANGE_UINT:
                if (kv->type != OZAYN_CV_TYPE_UINT) {
                    add_error(v, kv->key, "Type mismatch (expected uint)", OZAYN_CV_SEV_ERROR);
                } else if (kv->value.u < con->constraint.range_u.min ||
                           kv->value.u > con->constraint.range_u.max) {
                    snprintf(msg, sizeof(msg), "Out of range (%lu..%lu)",
                             (unsigned long)con->constraint.range_u.min,
                             (unsigned long)con->constraint.range_u.max);
                    add_error(v, kv->key, msg, OZAYN_CV_SEV_ERROR);
                }
                break;

            case OZAYN_CV_CONST_RANGE_FLOAT:
                if (kv->type != OZAYN_CV_TYPE_FLOAT) {
                    add_error(v, kv->key, "Type mismatch (expected float)", OZAYN_CV_SEV_ERROR);
                } else if (kv->value.f < con->constraint.range_f.min ||
                           kv->value.f > con->constraint.range_f.max) {
                    snprintf(msg, sizeof(msg), "Out of range (%.2f..%.2f)",
                             con->constraint.range_f.min, con->constraint.range_f.max);
                    add_error(v, kv->key, msg, OZAYN_CV_SEV_ERROR);
                }
                break;

            case OZAYN_CV_CONST_ENUM:
            case OZAYN_CV_CONST_ONE_OF: {
                if (kv->type != OZAYN_CV_TYPE_STRING) {
                    add_error(v, kv->key, "Type mismatch (expected string)", OZAYN_CV_SEV_ERROR);
                    break;
                }
                int match = 0;
                int cnt = con->type == OZAYN_CV_CONST_ENUM
                          ? con->constraint.enum_vals.count
                          : con->constraint.one_of.count;
                const char (*vals)[64] = con->type == OZAYN_CV_CONST_ENUM
                          ? con->constraint.enum_vals.values
                          : con->constraint.one_of.values;
                for (int i = 0; i < cnt; i++) {
                    if (strcmp(kv->value.s, vals[i]) == 0) { match = 1; break; }
                }
                if (!match) {
                    snprintf(msg, sizeof(msg), "Invalid value '%.200s'", kv->value.s);
                    add_error(v, kv->key, msg, OZAYN_CV_SEV_ERROR);
                }
                break;
            }

            case OZAYN_CV_CONST_MAX_LEN:
                if (kv->type != OZAYN_CV_TYPE_STRING) {
                    add_error(v, kv->key, "Type mismatch (expected string)", OZAYN_CV_SEV_ERROR);
                } else if (strlen(kv->value.s) > con->constraint.str.max_len) {
                    snprintf(msg, sizeof(msg), "String too long (%zu > %zu)",
                             strlen(kv->value.s), con->constraint.str.max_len);
                    add_error(v, kv->key, msg, OZAYN_CV_SEV_ERROR);
                }
                break;

            case OZAYN_CV_CONST_NOT_EMPTY:
                if (kv->type == OZAYN_CV_TYPE_STRING && kv->value.s[0] == '\0') {
                    add_error(v, kv->key, "Empty string", OZAYN_CV_SEV_WARNING);
                }
                break;
        }
    }

    if (v->error_count > 0) {
        LOG_WARN("CONFIG_VALIDATE", "Validation failed: %u errors", v->error_count);
    }
    return v->error_count > 0 ? -1 : 0;
}

int ozayn_cv_has_errors(const ozayn_config_validator_t *v) {
    return v && v->error_count > 0;
}

uint32_t ozayn_cv_error_count(const ozayn_config_validator_t *v) {
    return v ? v->error_count : 0;
}

const ozayn_cv_error_t *ozayn_cv_get_error(const ozayn_config_validator_t *v,
                                            uint32_t index) {
    if (!v || index >= v->error_count) return NULL;
    return &v->errors[index];
}

/* ---------- Print ---------- */

void ozayn_cv_print_errors(const ozayn_config_validator_t *v) {
    if (!v) return;

    LOG_INFO("CONFIG_VALIDATE", "=== Validation Errors (%u) ===", v->error_count);
    for (uint32_t i = 0; i < v->error_count; i++) {
        const ozayn_cv_error_t *e = &v->errors[i];
        LOG_INFO("CONFIG_VALIDATE", "  [%s] %s: %s",
                 e->severity == OZAYN_CV_SEV_FATAL ? "FATAL" :
                 e->severity == OZAYN_CV_SEV_ERROR ? "ERROR" :
                 e->severity == OZAYN_CV_SEV_WARNING ? "WARN" : "INFO",
                 e->key, e->message);
    }
}

void ozayn_cv_print_constraints(const ozayn_config_validator_t *v) {
    if (!v) return;

    LOG_INFO("CONFIG_VALIDATE", "=== Constraints (%u) ===", v->constraint_count);
    for (uint32_t i = 0; i < v->constraint_count; i++) {
        const ozayn_cv_constraint_t *c = &v->constraints[i];
        if (!c->active) continue;

        LOG_INFO("CONFIG_VALIDATE", "  [%s] type=%d required=%d",
                 c->key, c->type, c->required);
    }
}

void ozayn_cv_print_snapshot(const ozayn_cv_snapshot_t *snap) {
    if (!snap) return;

    LOG_INFO("CONFIG_VALIDATE", "=== Snapshot v%u (%u keys) ===",
             snap->version, snap->key_count);
    for (uint32_t i = 0; i < snap->key_count; i++) {
        const ozayn_cv_key_value_t *kv = &snap->keys[i];
        switch (kv->type) {
            case OZAYN_CV_TYPE_INT:    LOG_INFO("CONFIG_VALIDATE", "  %s = %ld", kv->key, (long)kv->value.i); break;
            case OZAYN_CV_TYPE_UINT:   LOG_INFO("CONFIG_VALIDATE", "  %s = %lu", kv->key, (unsigned long)kv->value.u); break;
            case OZAYN_CV_TYPE_FLOAT:  LOG_INFO("CONFIG_VALIDATE", "  %s = %.2f", kv->key, kv->value.f); break;
            case OZAYN_CV_TYPE_BOOL:   LOG_INFO("CONFIG_VALIDATE", "  %s = %s", kv->key, kv->value.b ? "true" : "false"); break;
            case OZAYN_CV_TYPE_STRING: LOG_INFO("CONFIG_VALIDATE", "  %s = '%s'", kv->key, kv->value.s); break;
        }
    }
}
