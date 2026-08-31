#ifndef OZAYN_RELEASE_MGR_H
#define OZAYN_RELEASE_MGR_H

#include "version.h"
#include <stdint.h>
#include <stddef.h>

/*
 * release_mgr.h — Release Engineering & Production Integration (Stage 30).
 *
 * Self-contained header — no circular includes.
 * Manages release lifecycle: manifest, dependency/integrity verification,
 * packaging, installation, backup, rollback, smoke testing, deployment validation.
 *
 * Principles:
 *   - Every release is uniquely identifiable
 *   - Every build is traceable to source
 *   - Upgrades are safe with backup/rollback
 *   - Integrity of release artifacts is verified
 *   - Deployment is validated with smoke tests
 */

/* ---- Constants ---- */

#define OZAYN_REL_MAX_DEPS      32
#define OZAYN_REL_MAX_FILES     64
#define OZAYN_REL_MAX_PATH     256
#define OZAYN_REL_MAX_NAME      64
#define OZAYN_REL_MAX_ENTRIES   32
#define OZAYN_REL_MAX_LOG       16
#define OZAYN_REL_MANIFEST_FILE "release.manifest"
#define OZAYN_REL_BACKUP_DIR    "backups"

/* ---- Dependency requirement ---- */

typedef struct {
    char     name[OZAYN_REL_MAX_NAME];
    char     version_min[32];   /* minimum compatible version */
    char     version_max[32];   /* maximum compatible version (""=any) */
    int      required;          /* 1=required, 0=optional */
    int      present;           /* filled during verification */
    char     actual_version[32]; /* filled during verification */
} ozayn_rel_dependency_t;

/* ---- Integrity record ---- */

typedef struct {
    char     path[OZAYN_REL_MAX_PATH];
    uint32_t expected_checksum;
    uint32_t actual_checksum;
    int      verified;          /* 1=match, 0=mismatch, -1=not checked */
} ozayn_rel_integrity_t;

/* ---- Release manifest ---- */

typedef struct {
    ozayn_build_identity_t build;

    ozayn_rel_dependency_t deps[OZAYN_REL_MAX_DEPS];
    uint32_t               dep_count;

    ozayn_rel_integrity_t  files[OZAYN_REL_MAX_FILES];
    uint32_t               file_count;

    char     state_format[32];   /* persistent state format version */
    char     min_upgrade_from[32]; /* minimum version that can upgrade to this */
    uint32_t manifest_version;   /* manifest format version */
} ozayn_rel_manifest_t;

/* ---- Installation status ---- */

typedef enum {
    OZAYN_REL_INSTALL_FRESH    = 0,
    OZAYN_REL_INSTALL_UPGRADE  = 1,
    OZAYN_REL_INSTALL_ROLLBACK = 2,
} ozayn_rel_install_type_t;

typedef struct {
    ozayn_rel_install_type_t install_type;
    char     previous_version[32];
    char     backup_path[OZAYN_REL_MAX_PATH];
    int      backup_created;
    int      state_migrated;
    char     migration_from[32];
    char     migration_to[32];
} ozayn_rel_install_result_t;

/* ---- State migration step ---- */

typedef struct {
    char     from_version[32];
    char     to_version[32];
    int      (*migrate)(void *state, size_t state_size);
    int      (*validate)(const void *state, size_t state_size);
    int      (*rollback)(void *state, size_t state_size);
} ozayn_rel_migration_t;

/* ---- Smoke test ---- */

typedef enum {
    OZAYN_REL_SMOKE_BINARY     = 0,
    OZAYN_REL_SMOKE_STARTUP    = 1,
    OZAYN_REL_SMOKE_HEALTH     = 2,
    OZAYN_REL_SMOKE_MODULES    = 3,
    OZAYN_REL_SMOKE_PLUGINS    = 4,
    OZAYN_REL_SMOKE_IPC        = 5,
    OZAYN_REL_SMOKE_CONFIG     = 6,
    OZAYN_REL_SMOKE_RECOVERY   = 7,
} ozayn_rel_smoke_type_t;

typedef struct {
    ozayn_rel_smoke_type_t type;
    int      passed;
    char     message[128];
    uint64_t duration_us;
} ozayn_rel_smoke_result_t;

/* ---- Deployment gate ---- */

typedef enum {
    OZAYN_REL_GATE_BUILD       = 0,
    OZAYN_REL_GATE_UNIT_TESTS  = 1,
    OZAYN_REL_GATE_INTEGRATION = 2,
    OZAYN_REL_GATE_SECURITY    = 3,
    OZAYN_REL_GATE_PERFORMANCE = 4,
    OZAYN_REL_GATE_PACKAGING   = 5,
    OZAYN_REL_GATE_SMOKE       = 6,
    OZAYN_REL_GATE_REVIEW      = 7,
} ozayn_rel_gate_type_t;

typedef struct {
    ozayn_rel_gate_type_t type;
    int      passed;
    char     message[128];
    uint64_t timestamp;
} ozayn_rel_gate_result_t;

/* ---- Backup record ---- */

typedef struct {
    char     version[32];
    char     path[OZAYN_REL_MAX_PATH];
    uint64_t timestamp;
    uint32_t size_bytes;
} ozayn_rel_backup_t;

/* ---- Deployment log entry ---- */

typedef struct {
    char     action[32];
    char     version[32];
    char     result[32];
    char     message[128];
    uint64_t timestamp;
} ozayn_rel_deploy_log_t;

/* ---- Release manager ---- */

typedef struct {
    ozayn_rel_manifest_t    manifest;
    ozayn_rel_migration_t   migrations[OZAYN_REL_MAX_ENTRIES];
    uint32_t                migration_count;
    ozayn_rel_backup_t      backups[OZAYN_REL_MAX_ENTRIES];
    uint32_t                backup_count;
    ozayn_rel_smoke_result_t smoke_results[OZAYN_REL_MAX_ENTRIES];
    uint32_t                smoke_count;
    ozayn_rel_gate_result_t gates[OZAYN_REL_MAX_ENTRIES];
    uint32_t                gate_count;
    ozayn_rel_deploy_log_t  deploy_log[OZAYN_REL_MAX_LOG];
    uint32_t                deploy_log_count;
    int                     initialized;
} ozayn_release_mgr_t;

/* ---- Lifecycle ---- */

int  ozayn_release_mgr_init(ozayn_release_mgr_t *mgr);
void ozayn_release_mgr_shutdown(ozayn_release_mgr_t *mgr);

/* ---- Manifest management ---- */

int  ozayn_release_manifest_init(ozayn_rel_manifest_t *m);
int  ozayn_release_manifest_write(const ozayn_rel_manifest_t *m, const char *path);
int  ozayn_release_manifest_read(ozayn_rel_manifest_t *m, const char *path);
void ozayn_release_manifest_print(const ozayn_rel_manifest_t *m);

/* ---- Dependency management ---- */

int  ozayn_release_add_dependency(ozayn_release_mgr_t *mgr,
                                   const char *name,
                                   const char *version_min,
                                   const char *version_max,
                                   int required);
int  ozayn_release_verify_dependencies(ozayn_release_mgr_t *mgr);
int  ozayn_release_deps_satisfied(const ozayn_release_mgr_t *mgr);

/* ---- Integrity verification ---- */

int  ozayn_release_add_integrity(ozayn_release_mgr_t *mgr,
                                  const char *path,
                                  uint32_t expected_checksum);
int  ozayn_release_verify_integrity(ozayn_release_mgr_t *mgr);
int  ozayn_release_integrity_ok(const ozayn_release_mgr_t *mgr);

/* Checksum utility: FNV-1a hash of file contents */
uint32_t ozayn_release_checksum_file(const char *path);
uint32_t ozayn_release_checksum_data(const void *data, size_t len);

/* ---- State migration ---- */

int  ozayn_release_register_migration(ozayn_release_mgr_t *mgr,
                                       const char *from_ver,
                                       const char *to_ver,
                                       int (*migrate)(void*, size_t),
                                       int (*validate)(const void*, size_t),
                                       int (*rollback)(void*, size_t));
int  ozayn_release_needs_migration(const ozayn_release_mgr_t *mgr,
                                    const char *from_ver,
                                    const char *to_ver);
int  ozayn_release_migrate(ozayn_release_mgr_t *mgr,
                            void *state, size_t state_size,
                            const char *from_ver, const char *to_ver);
int  ozayn_release_rollback_migration(ozayn_release_mgr_t *mgr,
                                       void *state, size_t state_size,
                                       const char *from_ver, const char *to_ver);

/* ---- Backup & restore ---- */

int  ozayn_release_backup_create(ozayn_release_mgr_t *mgr,
                                  const char *version,
                                  const char *source_dir);
int  ozayn_release_backup_restore(ozayn_release_mgr_t *mgr,
                                   const char *version,
                                   const char *target_dir);
int  ozayn_release_backup_list(const ozayn_release_mgr_t *mgr,
                                ozayn_rel_backup_t *out, uint32_t max);

/* ---- Installation ---- */

int  ozayn_release_install(ozayn_release_mgr_t *mgr,
                            const ozayn_rel_manifest_t *manifest,
                            const char *install_dir,
                            const char *previous_version,
                            ozayn_rel_install_result_t *result);

/* ---- Rollback ---- */

int  ozayn_release_rollback(ozayn_release_mgr_t *mgr,
                             const char *target_version,
                             const char *install_dir);

/* ---- Smoke testing ---- */

int  ozayn_release_smoke_add(ozayn_release_mgr_t *mgr,
                              ozayn_rel_smoke_type_t type,
                              int passed,
                              const char *message,
                              uint64_t duration_us);
int  ozayn_release_smoke_all_passed(const ozayn_release_mgr_t *mgr);
uint32_t ozayn_release_smoke_passed_count(const ozayn_release_mgr_t *mgr);
uint32_t ozayn_release_smoke_failed_count(const ozayn_release_mgr_t *mgr);

/* ---- Deployment gates ---- */

int  ozayn_release_gate_add(ozayn_release_mgr_t *mgr,
                             ozayn_rel_gate_type_t type,
                             int passed,
                             const char *message);
int  ozayn_release_gates_all_passed(const ozayn_release_mgr_t *mgr);
uint32_t ozayn_release_gate_passed_count(const ozayn_release_mgr_t *mgr);
uint32_t ozayn_release_gate_failed_count(const ozayn_release_mgr_t *mgr);

/* ---- Deployment log ---- */

int  ozayn_release_deploy_log(ozayn_release_mgr_t *mgr,
                               const char *action,
                               const char *version,
                               const char *result,
                               const char *message);
const ozayn_rel_deploy_log_t *ozayn_release_deploy_log_get(
                                const ozayn_release_mgr_t *mgr,
                                uint32_t index);

/* ---- Release readiness ---- */

int  ozayn_release_is_ready(const ozayn_release_mgr_t *mgr);
void ozayn_release_print_status(const ozayn_release_mgr_t *mgr);
void ozayn_release_print_gates(const ozayn_release_mgr_t *mgr);
void ozayn_release_print_smoke(const ozayn_release_mgr_t *mgr);
void ozayn_release_print_log(const ozayn_release_mgr_t *mgr);

#endif /* OZAYN_RELEASE_MGR_H */
