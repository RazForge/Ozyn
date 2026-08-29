#ifndef OZAYN_CONFIG_H
#define OZAYN_CONFIG_H

#include "ozayn.h"

/*
 * ozayn_config.h — Configuration system.
 *
 * Loads settings from a file or applies defaults.
 * Read once at startup, validated, frozen. Immutable after activation.
 */

#define OZAYN_CONFIG_MAX_LINE   256
#define OZAYN_CONFIG_PATH_LOCAL  "config/ozayn.conf"
#define OZAYN_CONFIG_PATH_USER  ".config/ozayn/ozayn.conf"

/* Config states */
typedef enum {
    OZAYN_CFG_NOT_LOADED  = 0,
    OZAYN_CFG_LOADING     = 1,
    OZAYN_CFG_LOADED      = 2,
    OZAYN_CFG_VALIDATED   = 3,
    OZAYN_CFG_ACTIVE      = 4,
    OZAYN_CFG_LOAD_FAILED = 5,
    OZAYN_CFG_INVALID     = 6,
} ozayn_config_state_t;

/* Config values */
typedef struct ozayn_config_s {
    int         runtime_interval;   /* seconds between loop iterations */
    int         log_level;          /* 0=debug 1=info 2=warn 3=error 4=critical */
    int         log_console;        /* 1=enabled 0=disabled */
    int         log_file;           /* 1=enabled 0=disabled */
    char        log_directory[256]; /* log file directory */
    int         config_version;     /* config format version */
    int         module_max;         /* max registered modules */
} ozayn_config_t;

/* Config object (owns loaded data) */
typedef struct {
    ozayn_config_state_t state;
    ozayn_config_t       values;
    char                *file_data;   /* raw file content, freed on destroy */
    char                 path[512];   /* path that was loaded */
} ozayn_config_object_t;

/* Lifecycle */
ozayn_result_t ozayn_config_load(ozayn_config_object_t *cfg);
ozayn_result_t ozayn_config_validate(const ozayn_config_object_t *cfg);
void           ozayn_config_destroy(ozayn_config_object_t *cfg);

/* Query */
const char    *ozayn_config_state_name(ozayn_config_state_t state);
const char    *ozayn_log_level_name(int level);
int            ozayn_log_level_from_name(const char *name);

#endif
