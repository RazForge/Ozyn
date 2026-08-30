#ifndef OZAYN_RESOURCE_H
#define OZAYN_RESOURCE_H

#include "ozayn.h"
#include <stdint.h>
#include <time.h>

/*
 * resource.h — Resource Manager (Stage 16).
 *
 * Answers: "What resources exist, who owns them, what state are they in,
 *           who may use them, and when should they be released?"
 *
 * Concepts:
 *   Resource  — managed entity with unique ID, type, owner, state
 *   Ownership — which identity controls a resource
 *   Lifecycle — CREATED -> AVAILABLE -> ALLOCATED -> ACTIVE -> RELEASED -> DESTROYED
 *
 * Resource Manager is NOT a replacement for specialized managers.
 * It provides the common resource lifecycle, ownership, and accounting layer.
 */

/* ---- Constants ---- */
#define OZAYN_RESOURCE_MAX            128
#define OZAYN_RESOURCE_MAX_ID_LEN     64
#define OZAYN_RESOURCE_MAX_NAME_LEN   128
#define OZAYN_RESOURCE_MAX_OWNER_LEN  64

/* ---- Resource types ---- */
typedef enum {
    OZAYN_RESOURCE_TYPE_UNKNOWN  = 0,
    OZAYN_RESOURCE_TYPE_PROCESS  = 1,
    OZAYN_RESOURCE_TYPE_TASK     = 2,
    OZAYN_RESOURCE_TYPE_IPC      = 3,
    OZAYN_RESOURCE_TYPE_MODULE   = 4,
    OZAYN_RESOURCE_TYPE_PLUGIN   = 5,
    OZAYN_RESOURCE_TYPE_SERVICE  = 6,
    OZAYN_RESOURCE_TYPE_BUFFER   = 7,
    OZAYN_RESOURCE_TYPE_DEVICE   = 8,
    OZAYN_RESOURCE_TYPE_HANDLE   = 9,
} ozayn_resource_type_t;

/* ---- Resource lifecycle states ---- */
typedef enum {
    OZAYN_RESOURCE_STATE_CREATED    = 0,
    OZAYN_RESOURCE_STATE_AVAILABLE  = 1,
    OZAYN_RESOURCE_STATE_ALLOCATED  = 2,
    OZAYN_RESOURCE_STATE_ACTIVE     = 3,
    OZAYN_RESOURCE_STATE_RELEASING  = 4,
    OZAYN_RESOURCE_STATE_DESTROYING = 5,
    OZAYN_RESOURCE_STATE_DESTROYED  = 6,
    OZAYN_RESOURCE_STATE_FAILED     = 7,
} ozayn_resource_state_t;

/* ---- Resource allocation result ---- */
typedef enum {
    OZAYN_RESOURCE_OK               =  0,
    OZAYN_RESOURCE_ERR              = -1,
    OZAYN_RESOURCE_NOT_FOUND        = -2,
    OZAYN_RESOURCE_UNAVAILABLE      = -3,
    OZAYN_RESOURCE_UNAUTHORIZED     = -4,
    OZAYN_RESOURCE_INVALID_STATE    = -5,
    OZAYN_RESOURCE_INVALID_TYPE     = -6,
    OZAYN_RESOURCE_ALREADY_OWNED    = -7,
    OZAYN_RESOURCE_NOT_OWNER        = -8,
    OZAYN_RESOURCE_ALREADY_RELEASED = -9,
    OZAYN_RESOURCE_EXHAUSTED        = -10,
} ozayn_resource_result_t;

/* ---- Resource record ---- */
typedef struct ozayn_resource_record_s {
    int                        active;
    uint32_t                   id;          /* internal slot ID */
    uint32_t                   generation;  /* generation counter for handle validation */
    char                       resource_id[OZAYN_RESOURCE_MAX_ID_LEN];
    char                       name[OZAYN_RESOURCE_MAX_NAME_LEN];
    ozayn_resource_type_t      type;
    ozayn_resource_state_t     state;
    char                       owner[OZAYN_RESOURCE_MAX_OWNER_LEN];
    int                        ref_count;
    int                        exclusive;
    time_t                     created_at;
    time_t                     allocated_at;
    time_t                     released_at;
    void                      *domain_data;
    void                     (*cleanup)(struct ozayn_resource_record_s *res);
} ozayn_resource_record_t;

/* ---- Resource handle (for external callers) ---- */
typedef struct {
    uint32_t slot;
    uint32_t generation;
} ozayn_resource_handle_t;

/* ---- Resource statistics ---- */
typedef struct {
    int total;
    int available;
    int allocated;
    int active;
    int failed;
    int released;
} ozayn_resource_stats_t;

/* ---- Resource manager ---- */
typedef struct {
    int                        enabled;
    int                        initialized;
    ozayn_resource_record_t    resources[OZAYN_RESOURCE_MAX];
    int                        resource_count;
    uint32_t                   next_id;
    ozayn_resource_stats_t     stats;
    void                      *events;
    void                      *recovery;
    void                      *authorization;
} ozayn_resource_manager_t;

/* ---- Lifecycle ---- */
ozayn_result_t ozayn_resource_manager_init(ozayn_resource_manager_t *mgr, int enabled);
void           ozayn_resource_manager_shutdown(ozayn_resource_manager_t *mgr);

/* ---- Bindings ---- */
void ozayn_resource_manager_set_events(ozayn_resource_manager_t *mgr, void *events);
void ozayn_resource_manager_set_recovery(ozayn_resource_manager_t *mgr, void *recovery);
void ozayn_resource_manager_set_authorization(ozayn_resource_manager_t *mgr, void *authorization);

/* ---- Resource lifecycle ---- */
ozayn_resource_result_t ozayn_resource_create(ozayn_resource_manager_t *mgr,
                                               const char *resource_id,
                                               const char *name,
                                               ozayn_resource_type_t type,
                                               int exclusive);

ozayn_resource_result_t ozayn_resource_destroy(ozayn_resource_manager_t *mgr,
                                                const char *resource_id,
                                                const char *identity_id);

ozayn_resource_result_t ozayn_resource_allocate(ozayn_resource_manager_t *mgr,
                                                 const char *resource_id,
                                                 const char *identity_id);

ozayn_resource_result_t ozayn_resource_release(ozayn_resource_manager_t *mgr,
                                                const char *resource_id,
                                                const char *identity_id);

ozayn_resource_result_t ozayn_resource_activate(ozayn_resource_manager_t *mgr,
                                                 const char *resource_id,
                                                 const char *identity_id);

ozayn_resource_result_t ozayn_resource_transfer(ozayn_resource_manager_t *mgr,
                                                 const char *resource_id,
                                                 const char *from_identity,
                                                 const char *to_identity);

/* ---- Query ---- */
const ozayn_resource_record_t *ozayn_resource_find(const ozayn_resource_manager_t *mgr,
                                                    const char *resource_id);

int ozayn_resource_is_available(const ozayn_resource_manager_t *mgr, const char *resource_id);
int ozayn_resource_exists(const ozayn_resource_manager_t *mgr, const char *resource_id);
const char *ozayn_resource_owner(const ozayn_resource_manager_t *mgr, const char *resource_id);

/* ---- Handle-based API ---- */
ozayn_resource_handle_t ozayn_resource_get_handle(const ozayn_resource_record_t *rec);
const ozayn_resource_record_t *ozayn_resource_from_handle(const ozayn_resource_manager_t *mgr,
                                                          ozayn_resource_handle_t handle);

/* ---- Statistics ---- */
ozayn_resource_stats_t ozayn_resource_manager_stats(const ozayn_resource_manager_t *mgr);
int ozayn_resource_manager_count(const ozayn_resource_manager_t *mgr);

/* ---- Query by type/owner ---- */
int ozayn_resource_count_by_type(const ozayn_resource_manager_t *mgr, ozayn_resource_type_t type);
int ozayn_resource_count_by_owner(const ozayn_resource_manager_t *mgr, const char *identity_id);

/* ---- Orphan detection ---- */
int ozayn_resource_detect_orphans(ozayn_resource_manager_t *mgr,
                                   const char *dead_identity,
                                   ozayn_resource_record_t *orphan_list,
                                   int max_orphans);

/* ---- Names ---- */
const char *ozayn_resource_type_name(ozayn_resource_type_t type);
const char *ozayn_resource_state_name(ozayn_resource_state_t state);
const char *ozayn_resource_result_name(ozayn_resource_result_t result);

#endif
