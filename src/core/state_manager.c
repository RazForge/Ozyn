#include "state_manager.h"
#include "logger.h"
#include "events.h"
#include "recovery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*
 * State Manager — centralized persistence engine.
 *
 * File format:
 *   [file_header: magic + version + count + checksum + reserved]
 *   [entry_0: entry_header + key + owner + data]
 *   [entry_1: entry_header + key + owner + data]
 *   ...
 *
 * Atomic write: write to .tmp, validate, rename over current.
 * Backup rotation: keep up to OZAYN_STATE_MAX_BACKUPS previous copies.
 * Integrity: CRC32 on serialized data.
 */

/* ================================================================
 * CRC32 — standard polynomial 0xEDB88320
 * ================================================================ */

uint32_t ozayn_state_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(int32_t)(crc & 1)));
        }
    }
    return ~crc;
}

/* ================================================================
 * Name queries
 * ================================================================ */

const char *ozayn_state_category_name(ozayn_state_category_t cat) {
    switch (cat) {
        case OZAYN_STATE_CAT_TRANSIENT:   return "TRANSIENT";
        case OZAYN_STATE_CAT_PERSISTENT:  return "PERSISTENT";
        case OZAYN_STATE_CAT_RECOVERABLE: return "RECOVERABLE";
    }
    return "UNKNOWN";
}

const char *ozayn_state_namespace_name(ozayn_state_namespace_t ns) {
    switch (ns) {
        case OZAYN_STATE_NS_CORE:     return "core";
        case OZAYN_STATE_NS_SECURITY: return "security";
        case OZAYN_STATE_NS_PLUGINS:  return "plugins";
        case OZAYN_STATE_NS_MODULES:  return "modules";
        case OZAYN_STATE_NS_TASKS:    return "tasks";
        case OZAYN_STATE_NS_USER:     return "user";
        case OZAYN_STATE_NS_CUSTOM:   return "custom";
    }
    return "unknown";
}

const char *ozayn_state_recovery_name(ozayn_state_recovery_t rec) {
    switch (rec) {
        case OZAYN_STATE_RECOVER_NEVER:      return "NEVER";
        case OZAYN_STATE_RECOVER_ON_FAILURE:  return "ON_FAILURE";
        case OZAYN_STATE_RECOVER_ON_RESTART:  return "ON_RESTART";
        case OZAYN_STATE_RECOVER_ALWAYS:      return "ALWAYS";
    }
    return "UNKNOWN";
}

const char *ozayn_state_validation_name(ozayn_state_validation_t val) {
    switch (val) {
        case OZAYN_STATE_VALID:            return "VALID";
        case OZAYN_STATE_INVALID_FORMAT:   return "INVALID_FORMAT";
        case OZAYN_STATE_INVALID_VERSION:  return "INVALID_VERSION";
        case OZAYN_STATE_INVALID_CHECKSUM: return "INVALID_CHECKSUM";
        case OZAYN_STATE_INVALID_DATA:     return "INVALID_DATA";
        case OZAYN_STATE_NOT_FOUND:        return "NOT_FOUND";
        case OZAYN_STATE_IO_ERROR:         return "IO_ERROR";
    }
    return "UNKNOWN";
}

/* ================================================================
 * Internal: lookup by key
 * ================================================================ */

static ozayn_state_entry_t *find_entry(ozayn_state_manager_t *mgr, const char *key) {
    if (!mgr || !key) return NULL;
    for (int i = 0; i < OZAYN_STATE_MAX_ENTRIES; i++) {
        if (mgr->entries[i].active && strcmp(mgr->entries[i].key, key) == 0) {
            return &mgr->entries[i];
        }
    }
    return NULL;
}

static const ozayn_state_entry_t *find_entry_const(const ozayn_state_manager_t *mgr, const char *key) {
    if (!mgr || !key) return NULL;
    for (int i = 0; i < OZAYN_STATE_MAX_ENTRIES; i++) {
        if (mgr->entries[i].active && strcmp(mgr->entries[i].key, key) == 0) {
            return &mgr->entries[i];
        }
    }
    return NULL;
}

/* ================================================================
 * Internal: publish event (null-safe)
 * ================================================================ */

static void publish_event(ozayn_state_manager_t *mgr,
                           ozayn_event_type_t type,
                           void *payload) {
    if (!mgr || !mgr->events) return;
    ozayn_events_publish((ozayn_event_engine_t *)mgr->events,
                         type, OZAYN_SRC_STATE, payload);
}

/* ================================================================
 * CRC32 of file entry serialized bytes
 * ================================================================ */

/* (CRC32 computed inline during serialization for efficiency) */

/* ================================================================
 * Init / Shutdown
 * ================================================================ */

int ozayn_state_manager_init(ozayn_state_manager_t *mgr, int enabled) {
    if (!mgr) return -1;

    memset(mgr, 0, sizeof(ozayn_state_manager_t));
    mgr->enabled   = enabled;
    mgr->initialized = 1;
    mgr->next_id   = 1;

    /* Default storage path */
    snprintf(mgr->storage_path, sizeof(mgr->storage_path), "data/ozayn.state");

    /* Initialize backups */
    for (int i = 0; i < OZAYN_STATE_MAX_BACKUPS; i++) {
        mgr->backups[i].active = 0;
    }

    LOG_INFO("STATE_MGR", "State manager initialized (enabled=%s)",
             enabled ? "yes" : "no");

    return 0;
}

void ozayn_state_manager_shutdown(ozayn_state_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    /* Auto-save dirty entries on shutdown */
    int dirty = ozayn_state_dirty_count(mgr);
    if (dirty > 0) {
        LOG_INFO("STATE_MGR", "Saving %d dirty entries before shutdown", dirty);
        ozayn_state_save(mgr);
    }

    mgr->initialized = 0;
    LOG_INFO("STATE_MGR", "State manager shut down (saves=%d, loads=%d)",
             mgr->total_saves, mgr->total_loads);
}

/* ================================================================
 * Binding
 * ================================================================ */

void ozayn_state_manager_set_events(ozayn_state_manager_t *mgr, void *events) {
    if (mgr) mgr->events = events;
}

void ozayn_state_manager_set_recovery(ozayn_state_manager_t *mgr, void *recovery) {
    if (mgr) mgr->recovery = recovery;
}

void ozayn_state_manager_set_storage_path(ozayn_state_manager_t *mgr, const char *path) {
    if (mgr && path) {
        snprintf(mgr->storage_path, sizeof(mgr->storage_path), "%s", path);
    }
}

/* ================================================================
 * Create
 * ================================================================ */

uint32_t ozayn_state_create(ozayn_state_manager_t *mgr,
                            const char *key,
                            const char *owner,
                            ozayn_state_namespace_t ns,
                            ozayn_state_category_t category,
                            ozayn_state_recovery_t recovery,
                            const void *data,
                            uint32_t data_size)
{
    if (!mgr || !mgr->initialized) return 0;
    if (!key || key[0] == '\0') return 0;
    if (data_size > OZAYN_STATE_MAX_DATA) {
        LOG_WARN("STATE_MGR", "Data too large for key '%s': %u > %u",
                 key, data_size, OZAYN_STATE_MAX_DATA);
        return 0;
    }

    /* Check duplicate */
    if (find_entry(mgr, key)) {
        LOG_WARN("STATE_MGR", "State key '%s' already exists", key);
        return 0;
    }

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < OZAYN_STATE_MAX_ENTRIES; i++) {
        if (!mgr->entries[i].active) { slot = i; break; }
    }
    if (slot < 0) {
        LOG_WARN("STATE_MGR", "State entry limit reached (%d)", OZAYN_STATE_MAX_ENTRIES);
        return 0;
    }

    ozayn_state_entry_t *e = &mgr->entries[slot];
    memset(e, 0, sizeof(ozayn_state_entry_t));
    e->active   = 1;
    e->id       = mgr->next_id++;
    e->ns       = ns;
    e->category = category;
    e->recovery = recovery;
    e->flags    = OZAYN_STATE_FLAG_DIRTY;
    e->version  = 1;
    e->data_size = data_size;
    e->created  = time(NULL);
    e->updated  = e->created;

    snprintf(e->key, sizeof(e->key), "%s", key);
    if (owner) snprintf(e->owner, sizeof(e->owner), "%s", owner);

    if (data && data_size > 0) {
        memcpy(e->data, data, data_size);
    }

    mgr->entry_count++;
    mgr->dirty_count++;

    LOG_INFO("STATE_MGR", "Created state '%s' (id=%u, ns=%s, cat=%s, recovery=%s)",
             key, e->id,
             ozayn_state_namespace_name(ns),
             ozayn_state_category_name(category),
             ozayn_state_recovery_name(recovery));

    publish_event(mgr, OZAYN_EVENT_STATE_CREATED, NULL);

    return e->id;
}

/* ================================================================
 * Delete
 * ================================================================ */

int ozayn_state_delete(ozayn_state_manager_t *mgr, const char *key) {
    if (!mgr || !mgr->initialized || !key) return -1;

    ozayn_state_entry_t *e = find_entry(mgr, key);
    if (!e) {
        LOG_WARN("STATE_MGR", "State key '%s' not found", key);
        return -1;
    }

    LOG_INFO("STATE_MGR", "Deleted state '%s' (id=%u)", key, e->id);

    e->active = 0;
    mgr->entry_count--;
    if (e->flags & OZAYN_STATE_FLAG_DIRTY) mgr->dirty_count--;

    publish_event(mgr, OZAYN_EVENT_STATE_DELETED, NULL);

    return 0;
}

/* ================================================================
 * Read
 * ================================================================ */

const ozayn_state_entry_t *ozayn_state_get(ozayn_state_manager_t *mgr, const char *key) {
    if (!mgr || !mgr->initialized || !key) return NULL;
    return find_entry_const(mgr, key);
}

const ozayn_state_entry_t *ozayn_state_get_by_id(ozayn_state_manager_t *mgr, uint32_t id) {
    if (!mgr || !mgr->initialized) return NULL;
    for (int i = 0; i < OZAYN_STATE_MAX_ENTRIES; i++) {
        if (mgr->entries[i].active && mgr->entries[i].id == id) {
            return &mgr->entries[i];
        }
    }
    return NULL;
}

/* ================================================================
 * Update
 * ================================================================ */

int ozayn_state_update(ozayn_state_manager_t *mgr, const char *key,
                       const void *data, uint32_t data_size) {
    if (!mgr || !mgr->initialized || !key) return -1;

    ozayn_state_entry_t *e = find_entry(mgr, key);
    if (!e) {
        LOG_WARN("STATE_MGR", "State key '%s' not found for update", key);
        return -1;
    }

    if (e->flags & OZAYN_STATE_FLAG_SEALED) {
        LOG_WARN("STATE_MGR", "State '%s' is sealed — cannot update", key);
        return -1;
    }

    if (data_size > OZAYN_STATE_MAX_DATA) {
        LOG_WARN("STATE_MGR", "Data too large for key '%s'", key);
        return -1;
    }

    memcpy(e->data, data, data_size);
    e->data_size = data_size;
    e->version++;
    e->updated = time(NULL);
    e->flags |= OZAYN_STATE_FLAG_DIRTY;
    mgr->dirty_count++;

    LOG_DEBUG("STATE_MGR", "Updated state '%s' (version=%u)", key, e->version);
    publish_event(mgr, OZAYN_EVENT_STATE_CHANGED, NULL);

    return 0;
}

int ozayn_state_update_sealed(ozayn_state_manager_t *mgr, const char *key, int sealed) {
    if (!mgr || !mgr->initialized || !key) return -1;

    ozayn_state_entry_t *e = find_entry(mgr, key);
    if (!e) return -1;

    if (sealed)
        e->flags |= OZAYN_STATE_FLAG_SEALED;
    else
        e->flags &= ~OZAYN_STATE_FLAG_SEALED;

    return 0;
}

/* ================================================================
 * Dirty tracking
 * ================================================================ */

int ozayn_state_mark_dirty(ozayn_state_manager_t *mgr, const char *key) {
    if (!mgr || !key) return -1;
    ozayn_state_entry_t *e = find_entry(mgr, key);
    if (!e) return -1;
    if (!(e->flags & OZAYN_STATE_FLAG_DIRTY)) {
        e->flags |= OZAYN_STATE_FLAG_DIRTY;
        mgr->dirty_count++;
    }
    publish_event(mgr, OZAYN_EVENT_STATE_DIRTY, NULL);
    return 0;
}

int ozayn_state_mark_clean(ozayn_state_manager_t *mgr, const char *key) {
    if (!mgr || !key) return -1;
    ozayn_state_entry_t *e = find_entry(mgr, key);
    if (!e) return -1;
    if (e->flags & OZAYN_STATE_FLAG_DIRTY) {
        e->flags &= ~OZAYN_STATE_FLAG_DIRTY;
        mgr->dirty_count--;
    }
    return 0;
}

int ozayn_state_is_dirty(ozayn_state_manager_t *mgr, const char *key) {
    if (!mgr || !key) return 0;
    ozayn_state_entry_t *e = find_entry(mgr, key);
    return e ? (e->flags & OZAYN_STATE_FLAG_DIRTY) != 0 : 0;
}

int ozayn_state_dirty_count(const ozayn_state_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->dirty_count;
}

/* ================================================================
 * Query
 * ================================================================ */

int ozayn_state_count(const ozayn_state_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->entry_count;
}

int ozayn_state_count_by_category(const ozayn_state_manager_t *mgr, ozayn_state_category_t cat) {
    if (!mgr) return 0;
    int count = 0;
    for (int i = 0; i < OZAYN_STATE_MAX_ENTRIES; i++) {
        if (mgr->entries[i].active && mgr->entries[i].category == cat) count++;
    }
    return count;
}

int ozayn_state_count_by_namespace(const ozayn_state_manager_t *mgr, ozayn_state_namespace_t ns) {
    if (!mgr) return 0;
    int count = 0;
    for (int i = 0; i < OZAYN_STATE_MAX_ENTRIES; i++) {
        if (mgr->entries[i].active && mgr->entries[i].ns == ns) count++;
    }
    return count;
}

int ozayn_state_exists(ozayn_state_manager_t *mgr, const char *key) {
    if (!mgr || !key) return 0;
    return find_entry(mgr, key) != NULL;
}

const ozayn_state_entry_t *ozayn_state_find_by_owner(ozayn_state_manager_t *mgr,
                                                      const char *owner, int *index) {
    if (!mgr || !owner) return NULL;
    int start = (index && *index >= 0) ? *index : 0;
    for (int i = start; i < OZAYN_STATE_MAX_ENTRIES; i++) {
        if (mgr->entries[i].active && strcmp(mgr->entries[i].owner, owner) == 0) {
            if (index) *index = i + 1;
            return &mgr->entries[i];
        }
    }
    return NULL;
}

/* ================================================================
 * Serialization — write state to file
 * ================================================================ */

static int write_state_file(ozayn_state_manager_t *mgr, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        LOG_ERROR("STATE_MGR", "Failed to open '%s' for writing: %s",
                  path, strerror(errno));
        return -1;
    }

    /* Count active entries */
    int count = 0;
    for (int i = 0; i < OZAYN_STATE_MAX_ENTRIES; i++) {
        if (mgr->entries[i].active) count++;
    }

    /* Write header (placeholder checksum) */
    ozayn_state_file_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, OZAYN_STATE_MAGIC, OZAYN_STATE_MAGIC_SIZE);
    hdr.version     = OZAYN_STATE_FILE_VERSION;
    hdr.entry_count = (uint32_t)count;
    hdr.checksum    = 0; /* computed after */
    hdr.reserved    = 0;

    /* Serialize entries, collecting data for CRC */
    size_t serialize_buf_size = 64 * 1024;
    uint8_t *serialize_buf = (uint8_t *)malloc(serialize_buf_size);
    if (!serialize_buf) {
        fclose(f);
        return -1;
    }
    size_t serialize_len = 0;

    for (int i = 0; i < OZAYN_STATE_MAX_ENTRIES; i++) {
        const ozayn_state_entry_t *e = &mgr->entries[i];
        if (!e->active) continue;

        ozayn_state_file_entry_t fe;
        memset(&fe, 0, sizeof(fe));
        fe.id         = e->id;
        fe.ns         = (uint32_t)e->ns;
        fe.category   = (uint32_t)e->category;
        fe.recovery   = (uint32_t)e->recovery;
        fe.flags      = e->flags;
        fe.version    = e->version;
        fe.data_size  = e->data_size;
        fe.key_len    = (uint32_t)strlen(e->key);
        fe.owner_len  = (uint32_t)strlen(e->owner);
        fe.created    = e->created;
        fe.updated    = e->updated;

        /* Append to serialize buffer */
        size_t entry_size = sizeof(ozayn_state_file_entry_t) +
                            fe.key_len + fe.owner_len + fe.data_size;
        if (serialize_len + entry_size > serialize_buf_size) {
            LOG_WARN("STATE_MGR", "Serialize buffer overflow");
            break;
        }
        memcpy(serialize_buf + serialize_len, &fe, sizeof(ozayn_state_file_entry_t));
        serialize_len += sizeof(ozayn_state_file_entry_t);
        memcpy(serialize_buf + serialize_len, e->key, fe.key_len);
        serialize_len += fe.key_len;
        memcpy(serialize_buf + serialize_len, e->owner, fe.owner_len);
        serialize_len += fe.owner_len;
        memcpy(serialize_buf + serialize_len, e->data, fe.data_size);
        serialize_len += fe.data_size;
    }

    /* Compute CRC over serialized entries */
    hdr.checksum = ozayn_state_crc32(serialize_buf, serialize_len);

    /* Write header */
    fwrite(&hdr, sizeof(hdr), 1, f);

    /* Write serialized entries */
    fwrite(serialize_buf, serialize_len, 1, f);

    free(serialize_buf);
    fclose(f);

    return 0;
}

/* ================================================================
 * Deserialization — read state from file
 * ================================================================ */

static int read_state_file(ozayn_state_manager_t *mgr, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    /* Read header */
    ozayn_state_file_header_t hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fclose(f);
        return -1;
    }

    /* Validate magic */
    if (memcmp(hdr.magic, OZAYN_STATE_MAGIC, OZAYN_STATE_MAGIC_SIZE) != 0) {
        LOG_WARN("STATE_MGR", "Invalid magic in state file '%s'", path);
        fclose(f);
        return -1;
    }

    /* Validate version */
    if (hdr.version != OZAYN_STATE_FILE_VERSION) {
        LOG_WARN("STATE_MGR", "State file version mismatch: got %u, expected %u",
                 hdr.version, OZAYN_STATE_FILE_VERSION);
        fclose(f);
        return -1;
    }

    /* Read serialized data */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, sizeof(ozayn_state_file_header_t), SEEK_SET);
    long data_size = file_size - (long)sizeof(ozayn_state_file_header_t);
    if (data_size <= 0) {
        fclose(f);
        return -1;
    }

    uint8_t *buf = (uint8_t *)malloc((size_t)data_size);
    if (!buf) { fclose(f); return -1; }
    if ((long)fread(buf, 1, (size_t)data_size, f) != data_size) {
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);

    /* Verify CRC */
    uint32_t actual_crc = ozayn_state_crc32(buf, (size_t)data_size);
    if (actual_crc != hdr.checksum) {
        LOG_WARN("STATE_MGR", "State file checksum mismatch: got 0x%08X, expected 0x%08X",
                 actual_crc, hdr.checksum);
        free(buf);
        return -1;
    }

    /* Parse entries */
    size_t offset = 0;
    for (uint32_t i = 0; i < hdr.entry_count && offset < (size_t)data_size; i++) {
        if (offset + sizeof(ozayn_state_file_entry_t) > (size_t)data_size) break;

        ozayn_state_file_entry_t fe;
        memcpy(&fe, buf + offset, sizeof(ozayn_state_file_entry_t));
        offset += sizeof(ozayn_state_file_entry_t);

        if (offset + fe.key_len + fe.owner_len + fe.data_size > (size_t)data_size) break;
        if (fe.data_size > OZAYN_STATE_MAX_DATA) break;

        /* Find or create entry in manager */
        char key_buf[OZAYN_STATE_MAX_KEY] = {0};
        char owner_buf[OZAYN_STATE_MAX_OWNER] = {0};
        memcpy(key_buf, buf + offset, fe.key_len);
        offset += fe.key_len;
        memcpy(owner_buf, buf + offset, fe.owner_len);
        offset += fe.owner_len;

        ozayn_state_entry_t *e = find_entry(mgr, key_buf);
        if (!e) {
            /* Find free slot */
            int slot = -1;
            for (int s = 0; s < OZAYN_STATE_MAX_ENTRIES; s++) {
                if (!mgr->entries[s].active) { slot = s; break; }
            }
            if (slot < 0) {
                LOG_WARN("STATE_MGR", "No free slot for loaded key '%s'", key_buf);
                offset += fe.data_size;
                continue;
            }
            e = &mgr->entries[slot];
            memset(e, 0, sizeof(ozayn_state_entry_t));
            e->active = 1;
            snprintf(e->key, sizeof(e->key), "%s", key_buf);
            mgr->entry_count++;
        }

        snprintf(e->owner, sizeof(e->owner), "%s", owner_buf);
        e->id         = fe.id;
        e->ns         = (ozayn_state_namespace_t)fe.ns;
        e->category   = (ozayn_state_category_t)fe.category;
        e->recovery   = (ozayn_state_recovery_t)fe.recovery;
        e->flags      = fe.flags & ~OZAYN_STATE_FLAG_DIRTY; /* clear dirty on load */
        e->version    = fe.version;
        e->data_size  = fe.data_size;
        e->created    = fe.created;
        e->updated    = fe.updated;
        memcpy(e->data, buf + offset, fe.data_size);
        offset += fe.data_size;

        if (e->id >= mgr->next_id) mgr->next_id = e->id + 1;
    }

    free(buf);
    return 0;
}

/* ================================================================
 * Save / Load
 * ================================================================ */

int ozayn_state_save(ozayn_state_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return -1;

    const char *path = mgr->storage_path;

    /* Write to temp file first */
    char tmp_path[OZAYN_STATE_MAX_PATH + 16];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    if (write_state_file(mgr, tmp_path) != 0) {
        LOG_ERROR("STATE_MGR", "Failed to write temporary state file");
        return -1;
    }

    /* Validate what we just wrote */
    ozayn_state_validation_t val = ozayn_state_validate_file(tmp_path);
    if (val != OZAYN_STATE_VALID) {
        LOG_ERROR("STATE_MGR", "Written state file failed validation: %s",
                  ozayn_state_validation_name(val));
        unlink(tmp_path);
        mgr->validation_failures++;
        return -1;
    }

    /* Rotate backup */
    ozayn_state_save_backup(mgr);

    /* Atomic rename: tmp -> current */
    if (rename(tmp_path, path) != 0) {
        LOG_ERROR("STATE_MGR", "Failed to rename temp state file: %s", strerror(errno));
        unlink(tmp_path);
        return -1;
    }

    /* Mark all entries clean */
    for (int i = 0; i < OZAYN_STATE_MAX_ENTRIES; i++) {
        if (mgr->entries[i].active && (mgr->entries[i].flags & OZAYN_STATE_FLAG_DIRTY)) {
            mgr->entries[i].flags &= ~OZAYN_STATE_FLAG_DIRTY;
            mgr->dirty_count--;
        }
    }

    mgr->total_saves++;

    LOG_INFO("STATE_MGR", "State saved to '%s' (%d entries)", path, mgr->entry_count);
    publish_event(mgr, OZAYN_EVENT_STATE_SAVED, NULL);

    return 0;
}

int ozayn_state_save_entry(ozayn_state_manager_t *mgr, const char *key) {
    if (!mgr || !key) return -1;
    /* For now, saving a single entry triggers a full save (simpler, safer) */
    return ozayn_state_save(mgr);
}

int ozayn_state_load(ozayn_state_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return -1;

    const char *path = mgr->storage_path;

    /* Check file exists */
    FILE *f = fopen(path, "rb");
    if (!f) {
        LOG_INFO("STATE_MGR", "No state file at '%s' — starting fresh", path);
        return ozayn_state_load_defaults(mgr);
    }
    fclose(f);

    /* Validate */
    ozayn_state_validation_t val = ozayn_state_validate_file(path);
    if (val != OZAYN_STATE_VALID) {
        LOG_WARN("STATE_MGR", "State file invalid: %s — attempting recovery",
                 ozayn_state_validation_name(val));
        mgr->validation_failures++;
        publish_event(mgr, OZAYN_EVENT_STATE_INVALID, NULL);

        /* Try recovery from backup */
        if (ozayn_state_recover(mgr) == 0) {
            return 0;
        }

        /* Unrecoverable — load defaults */
        LOG_WARN("STATE_MGR", "Recovery failed — loading defaults");
        return ozayn_state_load_defaults(mgr);
    }

    /* Read file */
    if (read_state_file(mgr, path) != 0) {
        LOG_ERROR("STATE_MGR", "Failed to read state file");
        return -1;
    }

    mgr->total_loads++;

    LOG_INFO("STATE_MGR", "State loaded from '%s' (%d entries)", path, mgr->entry_count);
    publish_event(mgr, OZAYN_EVENT_STATE_LOADED, NULL);
    publish_event(mgr, OZAYN_EVENT_STATE_VALIDATED, NULL);

    return 0;
}

int ozayn_state_load_defaults(ozayn_state_manager_t *mgr) {
    if (!mgr) return -1;

    LOG_INFO("STATE_MGR", "Loading default state");

    /* Create a minimal default configuration state */
    ozayn_state_create(mgr, "core.log_level", "core",
                       OZAYN_STATE_NS_CORE, OZAYN_STATE_CAT_PERSISTENT,
                       OZAYN_STATE_RECOVER_ON_RESTART,
                       "normal", 7);

    ozayn_state_create(mgr, "core.runtime_interval", "core",
                       OZAYN_STATE_NS_CORE, OZAYN_STATE_CAT_PERSISTENT,
                       OZAYN_STATE_RECOVER_ON_RESTART,
                       "1", 2);

    ozayn_state_create(mgr, "core.version", "core",
                       OZAYN_STATE_NS_CORE, OZAYN_STATE_CAT_PERSISTENT,
                       OZAYN_STATE_RECOVER_NEVER,
                       OZAYN_VERSION, strlen(OZAYN_VERSION));

    /* Try to save defaults */
    ozayn_state_save(mgr);

    mgr->total_loads++;

    LOG_INFO("STATE_MGR", "Default state loaded and saved (%d entries)", mgr->entry_count);
    publish_event(mgr, OZAYN_EVENT_STATE_LOADED, NULL);

    return 0;
}

int ozayn_state_save_backup(ozayn_state_manager_t *mgr) {
    if (!mgr) return -1;

    const char *path = mgr->storage_path;

    /* Check if current file exists */
    FILE *f = fopen(path, "rb");
    if (!f) return 0; /* nothing to backup */
    fclose(f);

    /* Rotate backups: shift older ones */
    for (int i = OZAYN_STATE_MAX_BACKUPS - 1; i > 0; i--) {
        if (mgr->backups[i - 1].active) {
            char old_path[OZAYN_STATE_MAX_PATH + 16];
            char new_path[OZAYN_STATE_MAX_PATH + 16];
            snprintf(old_path, sizeof(old_path), "%s.bak%d", path, i - 1);
            snprintf(new_path, sizeof(new_path), "%s.bak%d", path, i);
            rename(old_path, new_path);
            mgr->backups[i] = mgr->backups[i - 1];
        }
    }

    /* Copy current to .bak0 */
    char bak_path[OZAYN_STATE_MAX_PATH + 16];
    snprintf(bak_path, sizeof(bak_path), "%s.bak0", path);

    FILE *src = fopen(path, "rb");
    FILE *dst = fopen(bak_path, "wb");
    if (src && dst) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
            fwrite(buf, 1, n, dst);
        }
    }
    if (src) fclose(src);
    if (dst) fclose(dst);

    /* Record backup */
    mgr->backups[0].active = 1;
    mgr->backups[0].timestamp = time(NULL);
    mgr->backups[0].entry_count = mgr->entry_count;
    mgr->backups[0].checksum = 0; /* computed on load if needed */

    mgr->total_backups++;

    LOG_DEBUG("STATE_MGR", "Backup created: %s", bak_path);
    publish_event(mgr, OZAYN_EVENT_STATE_BACKUP_CREATED, NULL);

    return 0;
}

/* ================================================================
 * Validation
 * ================================================================ */

ozayn_state_validation_t ozayn_state_validate_file(const char *path) {
    if (!path) return OZAYN_STATE_IO_ERROR;

    FILE *f = fopen(path, "rb");
    if (!f) return OZAYN_STATE_NOT_FOUND;

    /* Read header */
    ozayn_state_file_header_t hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fclose(f);
        return OZAYN_STATE_INVALID_FORMAT;
    }

    /* Check magic */
    if (memcmp(hdr.magic, OZAYN_STATE_MAGIC, OZAYN_STATE_MAGIC_SIZE) != 0) {
        fclose(f);
        return OZAYN_STATE_INVALID_FORMAT;
    }

    /* Check version */
    if (hdr.version != OZAYN_STATE_FILE_VERSION) {
        fclose(f);
        return OZAYN_STATE_INVALID_VERSION;
    }

    /* Read data and check CRC */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, sizeof(ozayn_state_file_header_t), SEEK_SET);
    long data_size = file_size - (long)sizeof(ozayn_state_file_header_t);

    if (data_size <= 0) {
        fclose(f);
        return (hdr.entry_count == 0) ? OZAYN_STATE_VALID : OZAYN_STATE_INVALID_DATA;
    }

    uint8_t *buf = (uint8_t *)malloc((size_t)data_size);
    if (!buf) { fclose(f); return OZAYN_STATE_IO_ERROR; }
    if ((long)fread(buf, 1, (size_t)data_size, f) != data_size) {
        free(buf);
        fclose(f);
        return OZAYN_STATE_INVALID_DATA;
    }
    fclose(f);

    uint32_t actual_crc = ozayn_state_crc32(buf, (size_t)data_size);
    free(buf);

    if (actual_crc != hdr.checksum) {
        return OZAYN_STATE_INVALID_CHECKSUM;
    }

    return OZAYN_STATE_VALID;
}

ozayn_state_validation_t ozayn_state_validate(const ozayn_state_manager_t *mgr) {
    if (!mgr) return OZAYN_STATE_IO_ERROR;
    return ozayn_state_validate_file(mgr->storage_path);
}

/* ================================================================
 * Recovery
 * ================================================================ */

int ozayn_state_recover(ozayn_state_manager_t *mgr) {
    if (!mgr) return -1;

    mgr->recoveries_attempted++;
    publish_event(mgr, OZAYN_EVENT_STATE_RECOVERY_STARTED, NULL);

    LOG_INFO("STATE_MGR", "Attempting state recovery from backup");

    for (int i = 0; i < OZAYN_STATE_MAX_BACKUPS; i++) {
        if (!mgr->backups[i].active) continue;

        char bak_path[OZAYN_STATE_MAX_PATH + 16];
        snprintf(bak_path, sizeof(bak_path), "%s.bak%d", mgr->storage_path, i);

        ozayn_state_validation_t val = ozayn_state_validate_file(bak_path);
        if (val != OZAYN_STATE_VALID) {
            LOG_DEBUG("STATE_MGR", "Backup %d invalid: %s", i, ozayn_state_validation_name(val));
            continue;
        }

        /* Try to load from this backup */
        ozayn_state_manager_t tmp;
        memset(&tmp, 0, sizeof(tmp));
        tmp.initialized = 1;
        snprintf(tmp.storage_path, sizeof(tmp.storage_path), "%.*s",
                 (int)(sizeof(tmp.storage_path) - 1), bak_path);

        if (read_state_file(&tmp, bak_path) == 0) {
            /* Copy recovered entries to manager */
            mgr->entry_count = 0;
            mgr->dirty_count = 0;
            mgr->next_id = 1;
            for (int j = 0; j < OZAYN_STATE_MAX_ENTRIES; j++) {
                mgr->entries[j] = tmp.entries[j];
                if (mgr->entries[j].active) {
                    mgr->entry_count++;
                    if (mgr->entries[j].id >= mgr->next_id)
                        mgr->next_id = mgr->entries[j].id + 1;
                }
            }

            mgr->recoveries_succeeded++;
            LOG_INFO("STATE_MGR", "Recovery succeeded from backup %d (%d entries)",
                     i, mgr->entry_count);
            publish_event(mgr, OZAYN_EVENT_STATE_RECOVERY_COMPLETED, NULL);

            /* Save recovered state as current */
            ozayn_state_save(mgr);
            return 0;
        }
    }

    mgr->recoveries_failed++;
    LOG_WARN("STATE_MGR", "Recovery failed — no valid backup found");
    publish_event(mgr, OZAYN_EVENT_STATE_CORRUPTED, NULL);

    return -1;
}

int ozayn_state_recovery_count(const ozayn_state_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->recoveries_attempted;
}

/* ================================================================
 * Statistics
 * ================================================================ */

ozayn_state_stats_t ozayn_state_manager_stats(const ozayn_state_manager_t *mgr) {
    ozayn_state_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    if (!mgr) return stats;

    stats.total_entries     = mgr->entry_count;
    stats.dirty_entries     = mgr->dirty_count;
    stats.total_saves       = mgr->total_saves;
    stats.total_loads       = mgr->total_loads;
    stats.total_backups     = mgr->total_backups;
    stats.validation_failures = mgr->validation_failures;
    stats.recoveries_attempted  = mgr->recoveries_attempted;
    stats.recoveries_succeeded  = mgr->recoveries_succeeded;
    stats.recoveries_failed     = mgr->recoveries_failed;

    for (int i = 0; i < OZAYN_STATE_MAX_ENTRIES; i++) {
        if (!mgr->entries[i].active) continue;
        switch (mgr->entries[i].category) {
            case OZAYN_STATE_CAT_TRANSIENT:   stats.transient_entries++; break;
            case OZAYN_STATE_CAT_PERSISTENT:  stats.persistent_entries++; break;
            case OZAYN_STATE_CAT_RECOVERABLE: stats.recoverable_entries++; break;
        }
    }

    return stats;
}

/* ================================================================
 * Print
 * ================================================================ */

void ozayn_state_manager_print_entries(const ozayn_state_manager_t *mgr) {
    if (!mgr) return;

    LOG_INFO("STATE_MGR", "--- State Entries (%d) ---", mgr->entry_count);

    for (int i = 0; i < OZAYN_STATE_MAX_ENTRIES; i++) {
        const ozayn_state_entry_t *e = &mgr->entries[i];
        if (!e->active) continue;

        struct tm tm_created, tm_updated;
        localtime_r(&e->created, &tm_created);
        localtime_r(&e->updated, &tm_updated);
        char created_buf[32], updated_buf[32];
        strftime(created_buf, sizeof(created_buf), "%H:%M:%S", &tm_created);
        strftime(updated_buf, sizeof(updated_buf), "%H:%M:%S", &tm_updated);

        LOG_INFO("STATE_MGR", "  #%u [%s.%s] owner=%s v%u %s%s%s (created=%s updated=%s)",
                 e->id,
                 ozayn_state_namespace_name(e->ns),
                 e->key,
                 e->owner,
                 e->version,
                 ozayn_state_category_name(e->category),
                 (e->flags & OZAYN_STATE_FLAG_DIRTY) ? " DIRTY" : "",
                 (e->flags & OZAYN_STATE_FLAG_SEALED) ? " SEALED" : "",
                 created_buf,
                 updated_buf);
    }
}

void ozayn_state_manager_print_stats(const ozayn_state_manager_t *mgr) {
    if (!mgr) return;

    ozayn_state_stats_t s = ozayn_state_manager_stats(mgr);
    LOG_INFO("STATE_MGR", "--- State Manager Statistics ---");
    LOG_INFO("STATE_MGR", "  Entries: %d (persistent=%d, transient=%d, recoverable=%d)",
             s.total_entries, s.persistent_entries, s.transient_entries, s.recoverable_entries);
    LOG_INFO("STATE_MGR", "  Dirty: %d", s.dirty_entries);
    LOG_INFO("STATE_MGR", "  Saves: %d, Loads: %d", s.total_saves, s.total_loads);
    LOG_INFO("STATE_MGR", "  Backups: %d", s.total_backups);
    LOG_INFO("STATE_MGR", "  Validation failures: %d", s.validation_failures);
    LOG_INFO("STATE_MGR", "  Recoveries: %d attempted, %d succeeded, %d failed",
             s.recoveries_attempted, s.recoveries_succeeded, s.recoveries_failed);
}
