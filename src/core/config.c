#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

/* ---------- State name ---------- */

const char *ozayn_config_state_name(ozayn_config_state_t state) {
    switch (state) {
        case OZAYN_CFG_NOT_LOADED:  return "NOT_LOADED";
        case OZAYN_CFG_LOADING:     return "LOADING";
        case OZAYN_CFG_LOADED:      return "LOADED";
        case OZAYN_CFG_VALIDATED:   return "VALIDATED";
        case OZAYN_CFG_ACTIVE:      return "ACTIVE";
        case OZAYN_CFG_LOAD_FAILED: return "LOAD_FAILED";
        case OZAYN_CFG_INVALID:     return "INVALID";
    }
    return "UNKNOWN";
}

/* ---------- Log level ---------- */

const char *ozayn_log_level_name(int level) {
    switch (level) {
        case 0: return "debug";
        case 1: return "info";
        case 2: return "warning";
        case 3: return "error";
        case 4: return "critical";
    }
    return "unknown";
}

int ozayn_log_level_from_name(const char *name) {
    if (!name) return 1;
    if (strcmp(name, "debug") == 0)    return 0;
    if (strcmp(name, "info") == 0)     return 1;
    if (strcmp(name, "warning") == 0)  return 2;
    if (strcmp(name, "error") == 0)    return 3;
    if (strcmp(name, "critical") == 0) return 4;
    return -1;
}

/* ---------- Defaults ---------- */

static void apply_defaults(ozayn_config_t *v) {
    v->runtime_interval = 1;
    v->log_level        = 1; /* info */
    v->log_console      = 1;
    v->log_file         = 0;
    v->log_directory[0] = '\0';
    v->config_version   = 1;
    v->module_max       = 16;
    v->plugin_dir[0]    = '\0';
}

/* ---------- Trim whitespace ---------- */

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1))) end--;
    *end = '\0';
    return s;
}

/* ---------- Parse one key=value line ---------- */

static int parse_line(char *line, ozayn_config_t *v) {
    char *eq = strchr(line, '=');
    if (!eq) return 0; /* skip lines without = */

    *eq = '\0';
    char *key = trim(line);
    char *val = trim(eq + 1);

    /* skip empty key */
    if (*key == '\0') return 0;

    if (strcmp(key, "runtime_interval") == 0) {
        v->runtime_interval = atoi(val);
    } else if (strcmp(key, "log_level") == 0) {
        int lv = ozayn_log_level_from_name(val);
        if (lv < 0) return -1;
        v->log_level = lv;
    } else if (strcmp(key, "log_console") == 0) {
        v->log_console = atoi(val);
    } else if (strcmp(key, "log_file") == 0) {
        v->log_file = atoi(val);
    } else if (strcmp(key, "log_directory") == 0) {
        snprintf(v->log_directory, sizeof(v->log_directory), "%s", val);
    } else if (strcmp(key, "config_version") == 0) {
        v->config_version = atoi(val);
    } else if (strcmp(key, "module_max") == 0) {
        v->module_max = atoi(val);
    } else if (strcmp(key, "plugin_dir") == 0) {
        snprintf(v->plugin_dir, sizeof(v->plugin_dir), "%s", val);
    }
    /* unknown keys are silently ignored — forward compatible */

    return 0;
}

/* ---------- Find config file ---------- */

static const char *find_config_file(char *buf, size_t buflen) {
    struct stat st;

    /* 1. Local: ./config/ozayn.conf */
    if (stat(OZAYN_CONFIG_PATH_LOCAL, &st) == 0) {
        snprintf(buf, buflen, "%s", OZAYN_CONFIG_PATH_LOCAL);
        return buf;
    }

    /* 2. User: ~/.config/ozayn/ozayn.conf */
    const char *home = getenv("HOME");
    if (home) {
        snprintf(buf, buflen, "%s/%s", home, OZAYN_CONFIG_PATH_USER);
        if (stat(buf, &st) == 0) return buf;
    }

    return NULL;
}

/* ---------- Load ---------- */

ozayn_result_t ozayn_config_load(ozayn_config_object_t *cfg) {
    if (!cfg) return OZAYN_ERR_NULL;

    memset(cfg, 0, sizeof(ozayn_config_object_t));
    cfg->state = OZAYN_CFG_LOADING;

    /* Apply defaults first */
    apply_defaults(&cfg->values);

    /* Find config file */
    char path[512];
    const char *found = find_config_file(path, sizeof(path));

    if (!found) {
        /* No file — use defaults, this is not an error */
        cfg->state = OZAYN_CFG_VALIDATED;
        return OZAYN_OK;
    }

    snprintf(cfg->path, sizeof(cfg->path), "%s", found);

    /* Read file */
    FILE *fp = fopen(found, "r");
    if (!fp) {
        cfg->state = OZAYN_CFG_LOAD_FAILED;
        fprintf(stderr, "[%s] Config: cannot read %s\n", OZAYN_NAME, found);
        return OZAYN_ERR;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 65536) {
        fclose(fp);
        cfg->state = OZAYN_CFG_LOAD_FAILED;
        fprintf(stderr, "[%s] Config: file too large or empty: %s\n", OZAYN_NAME, found);
        return OZAYN_ERR;
    }

    cfg->file_data = malloc((size_t)fsize + 1);
    if (!cfg->file_data) {
        fclose(fp);
        cfg->state = OZAYN_CFG_LOAD_FAILED;
        return OZAYN_ERR;
    }

    size_t read = fread(cfg->file_data, 1, (size_t)fsize, fp);
    fclose(fp);
    cfg->file_data[read] = '\0';

    /* Parse lines */
    int line_num = 0;
    char *saveptr = NULL;
    char *line = strtok_r(cfg->file_data, "\n", &saveptr);

    while (line) {
        line_num++;
        char *trimmed = trim(line);

        /* skip empty lines and comments */
        if (*trimmed == '\0' || *trimmed == '#') {
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        /* restore newline for next strtok_r (not needed, but safe) */
        if (parse_line(trimmed, &cfg->values) != 0) {
            fprintf(stderr, "[%s] Config: invalid value at line %d\n", OZAYN_NAME, line_num);
            cfg->state = OZAYN_CFG_INVALID;
            return OZAYN_ERR;
        }

        line = strtok_r(NULL, "\n", &saveptr);
    }

    cfg->state = OZAYN_CFG_LOADED;
    return OZAYN_OK;
}

/* ---------- Validate ---------- */

ozayn_result_t ozayn_config_validate(const ozayn_config_object_t *cfg) {
    if (!cfg) return OZAYN_ERR_NULL;
    if (cfg->state != OZAYN_CFG_LOADED && cfg->state != OZAYN_CFG_VALIDATED)
        return OZAYN_ERR_STATE;

    const ozayn_config_t *v = &cfg->values;

    if (v->runtime_interval < 1 || v->runtime_interval > 60) {
        fprintf(stderr, "[%s] Config: runtime_interval must be 1-60, got %d\n",
                OZAYN_NAME, v->runtime_interval);
        return OZAYN_ERR;
    }

    if (v->log_level < 0 || v->log_level > 4) {
        fprintf(stderr, "[%s] Config: log_level must be 0-4, got %d\n",
                OZAYN_NAME, v->log_level);
        return OZAYN_ERR;
    }

    if (v->config_version < 1) {
        fprintf(stderr, "[%s] Config: config_version must be >= 1, got %d\n",
                OZAYN_NAME, v->config_version);
        return OZAYN_ERR;
    }

    if (v->log_console != 0 && v->log_console != 1) {
        fprintf(stderr, "[%s] Config: log_console must be 0 or 1, got %d\n",
                OZAYN_NAME, v->log_console);
        return OZAYN_ERR;
    }

    if (v->log_file != 0 && v->log_file != 1) {
        fprintf(stderr, "[%s] Config: log_file must be 0 or 1, got %d\n",
                OZAYN_NAME, v->log_file);
        return OZAYN_ERR;
    }

    return OZAYN_OK;
}

/* ---------- Destroy ---------- */

void ozayn_config_destroy(ozayn_config_object_t *cfg) {
    if (!cfg) return;
    free(cfg->file_data);
    cfg->file_data = NULL;
    cfg->state = OZAYN_CFG_NOT_LOADED;
}
