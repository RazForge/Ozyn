#ifndef OZAYN_STATE_MANAGER_H
#define OZAYN_STATE_MANAGER_H

#include <stdint.h>
#include <time.h>
#include <stddef.h>

/*
 * state_manager.h — Persistence & State Management Engine.
 *
 * Centralized persistence for OZAYN Core. All state that must survive
 * restarts flows through the State Manager — no module writes files independently.
 *
 * Three categories of state:
 *   TRANSIENT    — exists only in memory, lost on stop
 *   PERSISTENT   — must survive restart
 *   RECOVERABLE  — runtime state that can be recreated from definitions
 *
 * Design principles:
 *   - Centralized storage layer (no per-module files)
 *   - Deterministic serialization
 *   - State versioning for migration
 *   - Atomic writes (write-then-rename)
 *   - Backup rotation (current + previous)
 *   - Integrity validation (CRC32)
 *   - Dirty tracking (avoid unnecessary writes)
 *   - Namespace isolation (core.*, security.*, plugins.*, etc.)
 */

/* ---- Constants ---- */

#define OZAYN_STATE_MAGIC          "OZST"
#define OZAYN_STATE_MAGIC_SIZE     4
#define OZAYN_STATE_FILE_VERSION   1
#define OZAYN_STATE_MAX_ENTRIES    64
#define OZAYN_STATE_MAX_KEY        64
#define OZAYN_STATE_MAX_OWNER      64
#define OZAYN_STATE_MAX_PATH       512
#define OZAYN_STATE_MAX_NAMESPACE  32
#define OZAYN_STATE_MAX_DATA       4096
#define OZAYN_STATE_MAX_BACKUPS    2

/* ---- State category ---- */

typedef enum {
    OZAYN_STATE_CAT_TRANSIENT   = 0,  /* lost on stop */
    OZAYN_STATE_CAT_PERSISTENT  = 1,  /* must survive restart */
    OZAYN_STATE_CAT_RECOVERABLE = 2,  /* can be recreated */
} ozayn_state_category_t;

/* ---- State flags ---- */

#define OZAYN_STATE_FLAG_NONE      0x00
#define OZAYN_STATE_FLAG_DIRTY     0x01  /* modified since last save */
#define OZAYN_STATE_FLAG_SEALED    0x02  /* read-only, cannot be modified */
#define OZAYN_STATE_FLAG_SENSITIVE 0x04  /* contains secrets, skip logging */

/* ---- State namespace ---- */

typedef enum {
    OZAYN_STATE_NS_CORE     = 0,
    OZAYN_STATE_NS_SECURITY = 1,
    OZAYN_STATE_NS_PLUGINS  = 2,
    OZAYN_STATE_NS_MODULES  = 3,
    OZAYN_STATE_NS_TASKS    = 4,
    OZAYN_STATE_NS_USER     = 5,
    OZAYN_STATE_NS_CUSTOM   = 6,
} ozayn_state_namespace_t;

/* ---- State recovery strategy ---- */

typedef enum {
    OZAYN_STATE_RECOVER_NEVER     = 0,  /* do not restore */
    OZAYN_STATE_RECOVER_ON_FAILURE = 1, /* restore if task failed */
    OZAYN_STATE_RECOVER_ON_RESTART = 2, /* always restore on restart */
    OZAYN_STATE_RECOVER_ALWAYS    = 3,  /* always restore */
} ozayn_state_recovery_t;

/* ---- Validation result ---- */

typedef enum {
    OZAYN_STATE_VALID          = 0,  /* state is valid */
    OZAYN_STATE_INVALID_FORMAT = 1,  /* unrecognized format */
    OZAYN_STATE_INVALID_VERSION = 2, /* version mismatch */
    OZAYN_STATE_INVALID_CHECKSUM = 3, /* integrity check failed */
    OZAYN_STATE_INVALID_DATA   = 4,  /* corrupt or truncated data */
    OZAYN_STATE_NOT_FOUND      = 5,  /* state file missing */
    OZAYN_STATE_IO_ERROR       = 6,  /* filesystem error */
} ozayn_state_validation_t;

/* ---- State entry (single persistent record) ---- */

typedef struct {
    int                          active;
    uint32_t                     id;
    ozayn_state_namespace_t      ns;
    ozayn_state_category_t       category;
    ozayn_state_recovery_t       recovery;
    uint32_t                     flags;
    uint32_t                     version;
    uint32_t                     data_size;
    time_t                       created;
    time_t                       updated;
    char                         key[OZAYN_STATE_MAX_KEY];
    char                         owner[OZAYN_STATE_MAX_OWNER];
    uint8_t                      data[OZAYN_STATE_MAX_DATA];
} ozayn_state_entry_t;

/* ---- State file header (on-disk) ---- */

typedef struct {
    char     magic[OZAYN_STATE_MAGIC_SIZE];
    uint32_t version;
    uint32_t entry_count;
    uint32_t checksum;   /* CRC32 of everything after this field */
    uint32_t reserved;
} ozayn_state_file_header_t;

/* ---- State file entry (on-disk) ---- */

typedef struct {
    uint32_t id;
    uint32_t ns;
    uint32_t category;
    uint32_t recovery;
    uint32_t flags;
    uint32_t version;
    uint32_t data_size;
    uint32_t key_len;
    uint32_t owner_len;
    time_t   created;
    time_t   updated;
    /* followed by key_len + owner_len + data_size bytes */
} ozayn_state_file_entry_t;

/* ---- Backup record ---- */

typedef struct {
    int      active;
    uint32_t checksum;
    time_t   timestamp;
    uint32_t entry_count;
} ozayn_state_backup_t;

/* ---- Statistics ---- */

typedef struct {
    int total_entries;
    int persistent_entries;
    int transient_entries;
    int recoverable_entries;
    int dirty_entries;
    int total_saves;
    int total_loads;
    int total_backups;
    int validation_failures;
    int recoveries_attempted;
    int recoveries_succeeded;
    int recoveries_failed;
} ozayn_state_stats_t;

/* ---- State Manager ---- */

typedef struct {
    int                     enabled;
    int                     initialized;
    uint32_t                next_id;
    int                     entry_count;
    int                     dirty_count;
    int                     total_saves;
    int                     total_loads;
    int                     total_backups;
    int                     validation_failures;
    int                     recoveries_attempted;
    int                     recoveries_succeeded;
    int                     recoveries_failed;
    ozayn_state_entry_t     entries[OZAYN_STATE_MAX_ENTRIES];
    ozayn_state_backup_t    backups[OZAYN_STATE_MAX_BACKUPS];
    char                    storage_path[OZAYN_STATE_MAX_PATH];
    void                   *events;   /* ozayn_event_engine_t* */
    void                   *recovery; /* ozayn_recovery_t* */
} ozayn_state_manager_t;

/* ---- Lifecycle ---- */

int  ozayn_state_manager_init(ozayn_state_manager_t *mgr, int enabled);
void ozayn_state_manager_shutdown(ozayn_state_manager_t *mgr);

/* ---- Binding ---- */

void ozayn_state_manager_set_events(ozayn_state_manager_t *mgr, void *events);
void ozayn_state_manager_set_recovery(ozayn_state_manager_t *mgr, void *recovery);
void ozayn_state_manager_set_storage_path(ozayn_state_manager_t *mgr, const char *path);

/* ---- Create / Delete ---- */

uint32_t ozayn_state_create(ozayn_state_manager_t *mgr,
                            const char *key,
                            const char *owner,
                            ozayn_state_namespace_t ns,
                            ozayn_state_category_t category,
                            ozayn_state_recovery_t recovery,
                            const void *data,
                            uint32_t data_size);

int  ozayn_state_delete(ozayn_state_manager_t *mgr, const char *key);

/* ---- Read / Update ---- */

const ozayn_state_entry_t *ozayn_state_get(ozayn_state_manager_t *mgr, const char *key);
const ozayn_state_entry_t *ozayn_state_get_by_id(ozayn_state_manager_t *mgr, uint32_t id);
int  ozayn_state_update(ozayn_state_manager_t *mgr, const char *key,
                        const void *data, uint32_t data_size);
int  ozayn_state_update_sealed(ozayn_state_manager_t *mgr, const char *key, int sealed);

/* ---- Dirty tracking ---- */

int  ozayn_state_mark_dirty(ozayn_state_manager_t *mgr, const char *key);
int  ozayn_state_mark_clean(ozayn_state_manager_t *mgr, const char *key);
int  ozayn_state_is_dirty(ozayn_state_manager_t *mgr, const char *key);
int  ozayn_state_dirty_count(const ozayn_state_manager_t *mgr);

/* ---- Query ---- */

int  ozayn_state_count(const ozayn_state_manager_t *mgr);
int  ozayn_state_count_by_category(const ozayn_state_manager_t *mgr, ozayn_state_category_t cat);
int  ozayn_state_count_by_namespace(const ozayn_state_manager_t *mgr, ozayn_state_namespace_t ns);
int  ozayn_state_exists(ozayn_state_manager_t *mgr, const char *key);
const ozayn_state_entry_t *ozayn_state_find_by_owner(ozayn_state_manager_t *mgr,
                                                      const char *owner, int *index);

/* ---- Persistence ---- */

int  ozayn_state_save(ozayn_state_manager_t *mgr);
int  ozayn_state_save_entry(ozayn_state_manager_t *mgr, const char *key);
int  ozayn_state_load(ozayn_state_manager_t *mgr);
int  ozayn_state_load_defaults(ozayn_state_manager_t *mgr);
int  ozayn_state_save_backup(ozayn_state_manager_t *mgr);

/* ---- Validation ---- */

ozayn_state_validation_t ozayn_state_validate_file(const char *path);
ozayn_state_validation_t ozayn_state_validate(const ozayn_state_manager_t *mgr);

/* ---- Recovery ---- */

int  ozayn_state_recover(ozayn_state_manager_t *mgr);
int  ozayn_state_recovery_count(const ozayn_state_manager_t *mgr);

/* ---- Statistics ---- */

ozayn_state_stats_t ozayn_state_manager_stats(const ozayn_state_manager_t *mgr);

/* ---- Names ---- */

const char *ozayn_state_category_name(ozayn_state_category_t cat);
const char *ozayn_state_namespace_name(ozayn_state_namespace_t ns);
const char *ozayn_state_recovery_name(ozayn_state_recovery_t rec);
const char *ozayn_state_validation_name(ozayn_state_validation_t val);

/* ---- Print ---- */

void ozayn_state_manager_print_entries(const ozayn_state_manager_t *mgr);
void ozayn_state_manager_print_stats(const ozayn_state_manager_t *mgr);

/* ---- CRC32 (internal, exposed for testing) ---- */

uint32_t ozayn_state_crc32(const uint8_t *data, size_t len);

#endif
