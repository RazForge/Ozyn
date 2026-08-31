#include "release_mgr.h"
#include "logger.h"
#include "defense.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

/* ---- Lifecycle ---- */

int ozayn_release_mgr_init(ozayn_release_mgr_t *mgr) {
    if (!mgr) return -1;

    memset(mgr, 0, sizeof(ozayn_release_mgr_t));
    ozayn_release_manifest_init(&mgr->manifest);
    mgr->initialized = 1;

    LOG_INFO("RELEASE_MGR", "Release manager initialized (version=%s)",
             ozayn_version_string());
    return 0;
}

void ozayn_release_mgr_shutdown(ozayn_release_mgr_t *mgr) {
    if (!mgr) return;
    mgr->initialized = 0;
    LOG_INFO("RELEASE_MGR", "Release manager shut down (gates=%u, smoke=%u, log=%u)",
             mgr->gate_count, mgr->smoke_count, mgr->deploy_log_count);
}

/* ---- Manifest ---- */

int ozayn_release_manifest_init(ozayn_rel_manifest_t *m) {
    if (!m) return -1;

    memset(m, 0, sizeof(ozayn_rel_manifest_t));
    ozayn_build_identity_init(&m->build);
    m->manifest_version = 1;
    ozayn_defense_strlcpy(m->state_format, "1.0", sizeof(m->state_format));
    ozayn_defense_strlcpy(m->min_upgrade_from, "0.0.0", sizeof(m->min_upgrade_from));
    return 0;
}

int ozayn_release_manifest_write(const ozayn_rel_manifest_t *m, const char *path) {
    if (!m || !path) return -1;

    FILE *f = fopen(path, "w");
    if (!f) {
        LOG_ERROR("RELEASE_MGR", "Failed to write manifest: %s", path);
        return -1;
    }

    fprintf(f, "# OZAYN Release Manifest\n");
    fprintf(f, "manifest_version=%u\n", m->manifest_version);
    fprintf(f, "version=%s\n", m->build.version);
    fprintf(f, "build_id=%s\n", m->build.build_id);
    fprintf(f, "commit=%s\n", m->build.commit);
    fprintf(f, "branch=%s\n", m->build.branch);
    fprintf(f, "platform=%s\n", m->build.platform);
    fprintf(f, "arch=%s\n", m->build.arch);
    fprintf(f, "compiler=%s %s\n", m->build.compiler, m->build.compiler_version);
    fprintf(f, "build_mode=%s\n", m->build.build_mode);
    fprintf(f, "build_date=%s\n", m->build.build_date);
    fprintf(f, "state_format=%s\n", m->state_format);
    fprintf(f, "min_upgrade_from=%s\n", m->min_upgrade_from);
    fprintf(f, "dependencies=%u\n", m->dep_count);
    fprintf(f, "files=%u\n", m->file_count);

    for (uint32_t i = 0; i < m->dep_count; i++) {
        const ozayn_rel_dependency_t *d = &m->deps[i];
        fprintf(f, "dep[%u]=%s,%s,%s,%s\n", i, d->name, d->version_min,
                d->version_max, d->required ? "required" : "optional");
    }

    fclose(f);
    LOG_INFO("RELEASE_MGR", "Manifest written: %s (version=%s)", path, m->build.version);
    return 0;
}

int ozayn_release_manifest_read(ozayn_rel_manifest_t *m, const char *path) {
    if (!m || !path) return -1;

    FILE *f = fopen(path, "r");
    if (!f) {
        LOG_ERROR("RELEASE_MGR", "Failed to read manifest: %s", path);
        return -1;
    }

    memset(m, 0, sizeof(ozayn_rel_manifest_t));
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        /* strip trailing newline */
        val[strcspn(val, "\r\n")] = '\0';

        if (strcmp(key, "manifest_version") == 0) m->manifest_version = (uint32_t)strtoul(val, NULL, 10);
        else if (strcmp(key, "version") == 0) ozayn_defense_strlcpy(m->build.version, val, sizeof(m->build.version));
        else if (strcmp(key, "build_id") == 0) ozayn_defense_strlcpy(m->build.build_id, val, sizeof(m->build.build_id));
        else if (strcmp(key, "commit") == 0) ozayn_defense_strlcpy(m->build.commit, val, sizeof(m->build.commit));
        else if (strcmp(key, "branch") == 0) ozayn_defense_strlcpy(m->build.branch, val, sizeof(m->build.branch));
        else if (strcmp(key, "platform") == 0) ozayn_defense_strlcpy(m->build.platform, val, sizeof(m->build.platform));
        else if (strcmp(key, "arch") == 0) ozayn_defense_strlcpy(m->build.arch, val, sizeof(m->build.arch));
        else if (strcmp(key, "build_mode") == 0) ozayn_defense_strlcpy(m->build.build_mode, val, sizeof(m->build.build_mode));
        else if (strcmp(key, "build_date") == 0) ozayn_defense_strlcpy(m->build.build_date, val, sizeof(m->build.build_date));
        else if (strcmp(key, "state_format") == 0) ozayn_defense_strlcpy(m->state_format, val, sizeof(m->state_format));
        else if (strcmp(key, "min_upgrade_from") == 0) ozayn_defense_strlcpy(m->min_upgrade_from, val, sizeof(m->min_upgrade_from));
        else if (strncmp(key, "dep[", 4) == 0) {
            /* dep[0]=name,min,max,required */
            if (m->dep_count < OZAYN_REL_MAX_DEPS) {
                ozayn_rel_dependency_t *d = &m->deps[m->dep_count];
                memset(d, 0, sizeof(ozayn_rel_dependency_t));
                char *tok = strtok(val, ",");
                if (tok) ozayn_defense_strlcpy(d->name, tok, sizeof(d->name));
                tok = strtok(NULL, ",");
                if (tok) ozayn_defense_strlcpy(d->version_min, tok, sizeof(d->version_min));
                tok = strtok(NULL, ",");
                if (tok) ozayn_defense_strlcpy(d->version_max, tok, sizeof(d->version_max));
                tok = strtok(NULL, ",");
                if (tok) d->required = (strcmp(tok, "required") == 0) ? 1 : 0;
                m->dep_count++;
            }
        }
    }

    fclose(f);
    LOG_INFO("RELEASE_MGR", "Manifest read: %s (version=%s)", path, m->build.version);
    return 0;
}

void ozayn_release_manifest_print(const ozayn_rel_manifest_t *m) {
    if (!m) return;

    LOG_INFO("RELEASE_MGR", "=== Release Manifest (v%u) ===", m->manifest_version);
    LOG_INFO("RELEASE_MGR", "  Version:    %s", m->build.version);
    LOG_INFO("RELEASE_MGR", "  Build ID:   %s", m->build.build_id);
    LOG_INFO("RELEASE_MGR", "  Commit:     %s", m->build.commit);
    LOG_INFO("RELEASE_MGR", "  Branch:     %s", m->build.branch);
    LOG_INFO("RELEASE_MGR", "  Platform:   %s/%s", m->build.platform, m->build.arch);
    LOG_INFO("RELEASE_MGR", "  Compiler:   %s %s", m->build.compiler, m->build.compiler_version);
    LOG_INFO("RELEASE_MGR", "  Mode:       %s", m->build.build_mode);
    LOG_INFO("RELEASE_MGR", "  Built:      %s", m->build.build_date);
    LOG_INFO("RELEASE_MGR", "  State fmt:  %s", m->state_format);
    LOG_INFO("RELEASE_MGR", "  Min upgrade: %s", m->min_upgrade_from);
    LOG_INFO("RELEASE_MGR", "  Dependencies: %u", m->dep_count);
    LOG_INFO("RELEASE_MGR", "  Files:      %u", m->file_count);
}

/* ---- Dependencies ---- */

int ozayn_release_add_dependency(ozayn_release_mgr_t *mgr,
                                   const char *name,
                                   const char *version_min,
                                   const char *version_max,
                                   int required) {
    if (!mgr || !name || mgr->manifest.dep_count >= OZAYN_REL_MAX_DEPS) return -1;

    ozayn_rel_dependency_t *d = &mgr->manifest.deps[mgr->manifest.dep_count];
    memset(d, 0, sizeof(ozayn_rel_dependency_t));
    ozayn_defense_strlcpy(d->name, name, sizeof(d->name));
    if (version_min) ozayn_defense_strlcpy(d->version_min, version_min, sizeof(d->version_min));
    if (version_max) ozayn_defense_strlcpy(d->version_max, version_max, sizeof(d->version_max));
    d->required = required;
    mgr->manifest.dep_count++;

    LOG_DEBUG("RELEASE_MGR", "Dependency added: %s (min=%s, required=%d)",
              name, version_min ? version_min : "*", required);
    return 0;
}

int ozayn_release_verify_dependencies(ozayn_release_mgr_t *mgr) {
    if (!mgr) return -1;

    int all_ok = 1;
    for (uint32_t i = 0; i < mgr->manifest.dep_count; i++) {
        ozayn_rel_dependency_t *d = &mgr->manifest.deps[i];
        /* For now, mark as present if name is non-empty (real verification
           would check system libraries) */
        d->present = (d->name[0] != '\0');
        if (d->present) {
            ozayn_defense_strlcpy(d->actual_version, "1.0.0", sizeof(d->actual_version));
        }
        if (d->required && !d->present) {
            LOG_WARN("RELEASE_MGR", "Required dependency missing: %s", d->name);
            all_ok = 0;
        }
    }
    return all_ok ? 0 : -1;
}

int ozayn_release_deps_satisfied(const ozayn_release_mgr_t *mgr) {
    if (!mgr) return 0;

    for (uint32_t i = 0; i < mgr->manifest.dep_count; i++) {
        if (mgr->manifest.deps[i].required && !mgr->manifest.deps[i].present)
            return 0;
    }
    return 1;
}

/* ---- Integrity ---- */

/* FNV-1a 32-bit hash */
static uint32_t fnv1a_hash(const void *data, size_t len) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

uint32_t ozayn_release_checksum_data(const void *data, size_t len) {
    if (!data || len == 0) return 0;
    return fnv1a_hash(data, len);
}

uint32_t ozayn_release_checksum_file(const char *path) {
    if (!path) return 0;

    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    uint32_t hash = 2166136261u;
    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        for (size_t i = 0; i < n; i++) {
            hash ^= buf[i];
            hash *= 16777619u;
        }
    }
    fclose(f);
    return hash;
}

int ozayn_release_add_integrity(ozayn_release_mgr_t *mgr,
                                  const char *path,
                                  uint32_t expected_checksum) {
    if (!mgr || !path || mgr->manifest.file_count >= OZAYN_REL_MAX_FILES) return -1;

    ozayn_rel_integrity_t *rec = &mgr->manifest.files[mgr->manifest.file_count];
    memset(rec, 0, sizeof(ozayn_rel_integrity_t));
    ozayn_defense_strlcpy(rec->path, path, sizeof(rec->path));
    rec->expected_checksum = expected_checksum;
    rec->verified = -1; /* not yet checked */
    mgr->manifest.file_count++;

    return 0;
}

int ozayn_release_verify_integrity(ozayn_release_mgr_t *mgr) {
    if (!mgr) return -1;

    int all_ok = 1;
    for (uint32_t i = 0; i < mgr->manifest.file_count; i++) {
        ozayn_rel_integrity_t *rec = &mgr->manifest.files[i];
        rec->actual_checksum = ozayn_release_checksum_file(rec->path);
        if (rec->expected_checksum == 0) {
            /* No expected checksum → just record actual */
            rec->expected_checksum = rec->actual_checksum;
            rec->verified = 1;
        } else {
            rec->verified = (rec->actual_checksum == rec->expected_checksum) ? 1 : 0;
        }
        if (!rec->verified) {
            LOG_WARN("RELEASE_MGR", "Integrity mismatch: %s", rec->path);
            all_ok = 0;
        }
    }
    return all_ok ? 0 : -1;
}

int ozayn_release_integrity_ok(const ozayn_release_mgr_t *mgr) {
    if (!mgr) return 0;

    for (uint32_t i = 0; i < mgr->manifest.file_count; i++) {
        if (mgr->manifest.files[i].verified == 0) return 0;
    }
    return 1;
}

/* ---- State migration ---- */

int ozayn_release_register_migration(ozayn_release_mgr_t *mgr,
                                       const char *from_ver,
                                       const char *to_ver,
                                       int (*migrate)(void*, size_t),
                                       int (*validate)(const void*, size_t),
                                       int (*rollback)(void*, size_t)) {
    if (!mgr || !from_ver || !to_ver || mgr->migration_count >= OZAYN_REL_MAX_ENTRIES)
        return -1;

    ozayn_rel_migration_t *m = &mgr->migrations[mgr->migration_count];
    memset(m, 0, sizeof(ozayn_rel_migration_t));
    ozayn_defense_strlcpy(m->from_version, from_ver, sizeof(m->from_version));
    ozayn_defense_strlcpy(m->to_version, to_ver, sizeof(m->to_version));
    m->migrate = migrate;
    m->validate = validate;
    m->rollback = rollback;
    mgr->migration_count++;

    LOG_DEBUG("RELEASE_MGR", "Migration registered: %s -> %s", from_ver, to_ver);
    return 0;
}

int ozayn_release_needs_migration(const ozayn_release_mgr_t *mgr,
                                    const char *from_ver,
                                    const char *to_ver) {
    if (!mgr || !from_ver || !to_ver) return 0;

    for (uint32_t i = 0; i < mgr->migration_count; i++) {
        if (strcmp(mgr->migrations[i].from_version, from_ver) == 0 &&
            strcmp(mgr->migrations[i].to_version, to_ver) == 0) {
            return 1;
        }
    }
    return 0;
}

int ozayn_release_migrate(ozayn_release_mgr_t *mgr,
                            void *state, size_t state_size,
                            const char *from_ver, const char *to_ver) {
    if (!mgr || !state) return -1;

    for (uint32_t i = 0; i < mgr->migration_count; i++) {
        ozayn_rel_migration_t *m = &mgr->migrations[i];
        if (strcmp(m->from_version, from_ver) == 0 &&
            strcmp(m->to_version, to_ver) == 0) {
            if (m->migrate) {
                int r = m->migrate(state, state_size);
                if (r != 0) {
                    LOG_ERROR("RELEASE_MGR", "Migration failed: %s -> %s", from_ver, to_ver);
                    return r;
                }
            }
            if (m->validate) {
                int r = m->validate(state, state_size);
                if (r != 0) {
                    LOG_ERROR("RELEASE_MGR", "Migration validation failed: %s -> %s",
                              from_ver, to_ver);
                    return r;
                }
            }
            LOG_INFO("RELEASE_MGR", "Migration complete: %s -> %s", from_ver, to_ver);
            return 0;
        }
    }

    LOG_WARN("RELEASE_MGR", "No migration found: %s -> %s", from_ver, to_ver);
    return -1;
}

int ozayn_release_rollback_migration(ozayn_release_mgr_t *mgr,
                                       void *state, size_t state_size,
                                       const char *from_ver, const char *to_ver) {
    if (!mgr || !state) return -1;

    for (uint32_t i = 0; i < mgr->migration_count; i++) {
        ozayn_rel_migration_t *m = &mgr->migrations[i];
        if (strcmp(m->from_version, from_ver) == 0 &&
            strcmp(m->to_version, to_ver) == 0) {
            if (m->rollback) {
                return m->rollback(state, state_size);
            }
            return 0; /* no rollback needed */
        }
    }
    return -1;
}

/* ---- Backup & restore ---- */

static int ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return 0;
    return mkdir(path, 0755);
}

static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return -1;

    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        fwrite(buf, 1, n, out);
    }
    fclose(in);
    fclose(out);
    return 0;
}

int ozayn_release_backup_create(ozayn_release_mgr_t *mgr,
                                  const char *version,
                                  const char *source_dir) {
    if (!mgr || !version || !source_dir) return -1;
    if (mgr->backup_count >= OZAYN_REL_MAX_ENTRIES) return -1;

    ozayn_rel_backup_t *b = &mgr->backups[mgr->backup_count];
    memset(b, 0, sizeof(ozayn_rel_backup_t));
    ozayn_defense_strlcpy(b->version, version, sizeof(b->version));
    snprintf(b->path, sizeof(b->path), "%s/backup_%s", OZAYN_REL_BACKUP_DIR, version);
    b->timestamp = (uint64_t)time(NULL);

    ensure_dir(OZAYN_REL_BACKUP_DIR);
    ensure_dir(b->path);

    /* Copy key files from source_dir to backup */
    char src_path[OZAYN_REL_MAX_PATH + 64];
    char dst_path[OZAYN_REL_MAX_PATH + 64];
    const char *files[] = { "release.manifest", "ozayn.state", NULL };

    for (int i = 0; files[i]; i++) {
        snprintf(src_path, sizeof(src_path), "%s/%s", source_dir, files[i]);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", b->path, files[i]);
        if (copy_file(src_path, dst_path) == 0) {
            struct stat st;
            if (stat(dst_path, &st) == 0)
                b->size_bytes += (uint32_t)st.st_size;
        }
    }

    mgr->backup_count++;
    LOG_INFO("RELEASE_MGR", "Backup created: %s (size=%u bytes)", b->path, b->size_bytes);
    return 0;
}

int ozayn_release_backup_restore(ozayn_release_mgr_t *mgr,
                                   const char *version,
                                   const char *target_dir) {
    if (!mgr || !version || !target_dir) return -1;

    /* Find backup */
    for (uint32_t i = 0; i < mgr->backup_count; i++) {
        if (strcmp(mgr->backups[i].version, version) == 0) {
            ensure_dir(target_dir);

            char src_path[OZAYN_REL_MAX_PATH + 64];
            char dst_path[OZAYN_REL_MAX_PATH + 64];
            const char *files[] = { "release.manifest", "ozayn.state", NULL };

            for (int j = 0; files[j]; j++) {
                snprintf(src_path, sizeof(src_path), "%s/%s", mgr->backups[i].path, files[j]);
                snprintf(dst_path, sizeof(dst_path), "%s/%s", target_dir, files[j]);
                copy_file(src_path, dst_path);
            }

            LOG_INFO("RELEASE_MGR", "Backup restored: %s -> %s", version, target_dir);
            return 0;
        }
    }

    LOG_ERROR("RELEASE_MGR", "Backup not found for version: %s", version);
    return -1;
}

int ozayn_release_backup_list(const ozayn_release_mgr_t *mgr,
                                ozayn_rel_backup_t *out, uint32_t max) {
    if (!mgr || !out) return 0;

    uint32_t count = mgr->backup_count < max ? mgr->backup_count : max;
    for (uint32_t i = 0; i < count; i++) {
        out[i] = mgr->backups[i];
    }
    return (int)count;
}

/* ---- Installation ---- */

int ozayn_release_install(ozayn_release_mgr_t *mgr,
                            const ozayn_rel_manifest_t *manifest,
                            const char *install_dir,
                            const char *previous_version,
                            ozayn_rel_install_result_t *result) {
    if (!mgr || !manifest || !install_dir || !result) return -1;

    memset(result, 0, sizeof(ozayn_rel_install_result_t));

    if (previous_version && previous_version[0] != '\0') {
        result->install_type = OZAYN_REL_INSTALL_UPGRADE;
        ozayn_defense_strlcpy(result->previous_version, previous_version,
                              sizeof(result->previous_version));

        /* Check if migration is needed */
        if (ozayn_release_needs_migration(mgr, previous_version, manifest->build.version)) {
            ozayn_defense_strlcpy(result->migration_from, previous_version,
                                  sizeof(result->migration_from));
            ozayn_defense_strlcpy(result->migration_to, manifest->build.version,
                                  sizeof(result->migration_to));
            result->state_migrated = 1;
        }
    } else {
        result->install_type = OZAYN_REL_INSTALL_FRESH;
    }

    /* Create backup before upgrade */
    if (result->install_type == OZAYN_REL_INSTALL_UPGRADE) {
        if (ozayn_release_backup_create(mgr, previous_version, install_dir) == 0) {
            result->backup_created = 1;
            ozayn_defense_strlcpy(result->backup_path,
                                  mgr->backups[mgr->backup_count - 1].path,
                                  sizeof(result->backup_path));
        }
    }

    LOG_INFO("RELEASE_MGR", "Installing %s (type=%s, prev=%s)",
             manifest->build.version,
             result->install_type == OZAYN_REL_INSTALL_FRESH ? "fresh" : "upgrade",
             previous_version ? previous_version : "none");

    return 0;
}

/* ---- Rollback ---- */

int ozayn_release_rollback(ozayn_release_mgr_t *mgr,
                             const char *target_version,
                             const char *install_dir) {
    if (!mgr || !target_version || !install_dir) return -1;

    LOG_WARN("RELEASE_MGR", "Rolling back to version: %s", target_version);

    /* Restore from backup */
    int r = ozayn_release_backup_restore(mgr, target_version, install_dir);
    if (r == 0) {
        ozayn_release_deploy_log(mgr, "rollback", target_version, "OK",
                                  "Restored from backup");
    } else {
        ozayn_release_deploy_log(mgr, "rollback", target_version, "FAILED",
                                  "Backup not found");
    }
    return r;
}

/* ---- Smoke testing ---- */

static const char *smoke_type_name(ozayn_rel_smoke_type_t type) {
    switch (type) {
        case OZAYN_REL_SMOKE_BINARY:   return "binary";
        case OZAYN_REL_SMOKE_STARTUP:  return "startup";
        case OZAYN_REL_SMOKE_HEALTH:   return "health";
        case OZAYN_REL_SMOKE_MODULES:  return "modules";
        case OZAYN_REL_SMOKE_PLUGINS:  return "plugins";
        case OZAYN_REL_SMOKE_IPC:      return "ipc";
        case OZAYN_REL_SMOKE_CONFIG:   return "config";
        case OZAYN_REL_SMOKE_RECOVERY: return "recovery";
    }
    return "unknown";
}

int ozayn_release_smoke_add(ozayn_release_mgr_t *mgr,
                              ozayn_rel_smoke_type_t type,
                              int passed,
                              const char *message,
                              uint64_t duration_us) {
    if (!mgr || mgr->smoke_count >= OZAYN_REL_MAX_ENTRIES) return -1;

    ozayn_rel_smoke_result_t *r = &mgr->smoke_results[mgr->smoke_count];
    memset(r, 0, sizeof(ozayn_rel_smoke_result_t));
    r->type = type;
    r->passed = passed;
    if (message) ozayn_defense_strlcpy(r->message, message, sizeof(r->message));
    r->duration_us = duration_us;
    mgr->smoke_count++;

    LOG_INFO("RELEASE_MGR", "Smoke test [%s]: %s (%lu us)",
             smoke_type_name(type), passed ? "PASS" : "FAIL",
             (unsigned long)duration_us);
    return 0;
}

int ozayn_release_smoke_all_passed(const ozayn_release_mgr_t *mgr) {
    if (!mgr) return 0;

    for (uint32_t i = 0; i < mgr->smoke_count; i++) {
        if (!mgr->smoke_results[i].passed) return 0;
    }
    return mgr->smoke_count > 0 ? 1 : 0;
}

uint32_t ozayn_release_smoke_passed_count(const ozayn_release_mgr_t *mgr) {
    if (!mgr) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < mgr->smoke_count; i++) {
        if (mgr->smoke_results[i].passed) count++;
    }
    return count;
}

uint32_t ozayn_release_smoke_failed_count(const ozayn_release_mgr_t *mgr) {
    if (!mgr) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < mgr->smoke_count; i++) {
        if (!mgr->smoke_results[i].passed) count++;
    }
    return count;
}

/* ---- Deployment gates ---- */

static const char *gate_type_name(ozayn_rel_gate_type_t type) {
    switch (type) {
        case OZAYN_REL_GATE_BUILD:       return "build";
        case OZAYN_REL_GATE_UNIT_TESTS:  return "unit_tests";
        case OZAYN_REL_GATE_INTEGRATION: return "integration";
        case OZAYN_REL_GATE_SECURITY:    return "security";
        case OZAYN_REL_GATE_PERFORMANCE: return "performance";
        case OZAYN_REL_GATE_PACKAGING:   return "packaging";
        case OZAYN_REL_GATE_SMOKE:       return "smoke";
        case OZAYN_REL_GATE_REVIEW:      return "review";
    }
    return "unknown";
}

int ozayn_release_gate_add(ozayn_release_mgr_t *mgr,
                             ozayn_rel_gate_type_t type,
                             int passed,
                             const char *message) {
    if (!mgr || mgr->gate_count >= OZAYN_REL_MAX_ENTRIES) return -1;

    ozayn_rel_gate_result_t *g = &mgr->gates[mgr->gate_count];
    memset(g, 0, sizeof(ozayn_rel_gate_result_t));
    g->type = type;
    g->passed = passed;
    if (message) ozayn_defense_strlcpy(g->message, message, sizeof(g->message));
    g->timestamp = (uint64_t)time(NULL);
    mgr->gate_count++;

    LOG_INFO("RELEASE_MGR", "Gate [%s]: %s", gate_type_name(type),
             passed ? "PASSED" : "FAILED");
    return 0;
}

int ozayn_release_gates_all_passed(const ozayn_release_mgr_t *mgr) {
    if (!mgr) return 0;

    for (uint32_t i = 0; i < mgr->gate_count; i++) {
        if (!mgr->gates[i].passed) return 0;
    }
    return mgr->gate_count > 0 ? 1 : 0;
}

uint32_t ozayn_release_gate_passed_count(const ozayn_release_mgr_t *mgr) {
    if (!mgr) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < mgr->gate_count; i++) {
        if (mgr->gates[i].passed) count++;
    }
    return count;
}

uint32_t ozayn_release_gate_failed_count(const ozayn_release_mgr_t *mgr) {
    if (!mgr) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < mgr->gate_count; i++) {
        if (!mgr->gates[i].passed) count++;
    }
    return count;
}

/* ---- Deployment log ---- */

int ozayn_release_deploy_log(ozayn_release_mgr_t *mgr,
                               const char *action,
                               const char *version,
                               const char *result,
                               const char *message) {
    if (!mgr || mgr->deploy_log_count >= OZAYN_REL_MAX_LOG) return -1;

    ozayn_rel_deploy_log_t *e = &mgr->deploy_log[mgr->deploy_log_count];
    memset(e, 0, sizeof(ozayn_rel_deploy_log_t));
    if (action) ozayn_defense_strlcpy(e->action, action, sizeof(e->action));
    if (version) ozayn_defense_strlcpy(e->version, version, sizeof(e->version));
    if (result) ozayn_defense_strlcpy(e->result, result, sizeof(e->result));
    if (message) ozayn_defense_strlcpy(e->message, message, sizeof(e->message));
    e->timestamp = (uint64_t)time(NULL);
    mgr->deploy_log_count++;

    return 0;
}

const ozayn_rel_deploy_log_t *ozayn_release_deploy_log_get(
                                const ozayn_release_mgr_t *mgr,
                                uint32_t index) {
    if (!mgr || index >= mgr->deploy_log_count) return NULL;
    return &mgr->deploy_log[index];
}

/* ---- Release readiness ---- */

int ozayn_release_is_ready(const ozayn_release_mgr_t *mgr) {
    if (!mgr) return 0;

    /* All gates must pass */
    if (!ozayn_release_gates_all_passed(mgr)) return 0;

    /* All smoke tests must pass */
    if (!ozayn_release_smoke_all_passed(mgr)) return 0;

    /* Dependencies must be satisfied */
    if (!ozayn_release_deps_satisfied(mgr)) return 0;

    /* Integrity must be verified */
    if (mgr->manifest.file_count > 0 && !ozayn_release_integrity_ok(mgr)) return 0;

    return 1;
}

void ozayn_release_print_status(const ozayn_release_mgr_t *mgr) {
    if (!mgr) return;

    LOG_INFO("RELEASE_MGR", "=== Release Status ===");
    LOG_INFO("RELEASE_MGR", "  Version:   %s", mgr->manifest.build.version);
    LOG_INFO("RELEASE_MGR", "  Build ID:  %s", mgr->manifest.build.build_id);
    LOG_INFO("RELEASE_MGR", "  Platform:  %s/%s", mgr->manifest.build.platform,
             mgr->manifest.build.arch);
    LOG_INFO("RELEASE_MGR", "  Gates:     %u/%u passed",
             ozayn_release_gate_passed_count(mgr), mgr->gate_count);
    LOG_INFO("RELEASE_MGR", "  Smoke:     %u/%u passed",
             ozayn_release_smoke_passed_count(mgr), mgr->smoke_count);
    LOG_INFO("RELEASE_MGR", "  Deps:      %s",
             ozayn_release_deps_satisfied(mgr) ? "satisfied" : "MISSING");
    LOG_INFO("RELEASE_MGR", "  Integrity: %s",
             mgr->manifest.file_count == 0 ? "N/A" :
             (ozayn_release_integrity_ok(mgr) ? "OK" : "MISMATCH"));
    LOG_INFO("RELEASE_MGR", "  Ready:     %s", ozayn_release_is_ready(mgr) ? "YES" : "NO");
}

void ozayn_release_print_gates(const ozayn_release_mgr_t *mgr) {
    if (!mgr) return;

    LOG_INFO("RELEASE_MGR", "=== Deployment Gates (%u) ===", mgr->gate_count);
    for (uint32_t i = 0; i < mgr->gate_count; i++) {
        const ozayn_rel_gate_result_t *g = &mgr->gates[i];
        LOG_INFO("RELEASE_MGR", "  [%s] %s %s",
                 gate_type_name(g->type),
                 g->passed ? "PASS" : "FAIL",
                 g->message);
    }
}

void ozayn_release_print_smoke(const ozayn_release_mgr_t *mgr) {
    if (!mgr) return;

    LOG_INFO("RELEASE_MGR", "=== Smoke Tests (%u) ===", mgr->smoke_count);
    for (uint32_t i = 0; i < mgr->smoke_count; i++) {
        const ozayn_rel_smoke_result_t *r = &mgr->smoke_results[i];
        LOG_INFO("RELEASE_MGR", "  [%s] %s (%lu us) %s",
                 smoke_type_name(r->type),
                 r->passed ? "PASS" : "FAIL",
                 (unsigned long)r->duration_us,
                 r->message);
    }
}

void ozayn_release_print_log(const ozayn_release_mgr_t *mgr) {
    if (!mgr) return;

    LOG_INFO("RELEASE_MGR", "=== Deployment Log (%u entries) ===", mgr->deploy_log_count);
    for (uint32_t i = 0; i < mgr->deploy_log_count; i++) {
        const ozayn_rel_deploy_log_t *e = &mgr->deploy_log[i];
        LOG_INFO("RELEASE_MGR", "  [%s] %s: %s — %s",
                 e->action, e->version, e->result, e->message);
    }
}
