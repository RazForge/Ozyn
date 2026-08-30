#ifndef OZAYN_REGISTRY_H
#define OZAYN_REGISTRY_H

#include "ozayn.h"
#include <stdint.h>
#include <time.h>

/*
 * ozayn_registry.h — Service Registry & Component Discovery.
 *
 * Centralized registry for runtime service discovery.
 * Answers: "Who is available and how can they be reached?"
 */

#define OZAYN_REGISTRY_MAX_SERVICES    32
#define OZAYN_REGISTRY_MAX_ID          64
#define OZAYN_REGISTRY_MAX_NAME        128
#define OZAYN_REGISTRY_MAX_VERSION     32
#define OZAYN_REGISTRY_MAX_ENDPOINT    256
#define OZAYN_REGISTRY_MAX_CAPABILITIES 8
#define OZAYN_REGISTRY_MAX_CAP_LEN     64
#define OZAYN_REGISTRY_MAX_PROVIDER    64

/* ---- Service states ---- */
typedef enum {
    OZAYN_SVC_REGISTERING = 0,
    OZAYN_SVC_READY       = 1,
    OZAYN_SVC_DEGRADED    = 2,
    OZAYN_SVC_FAILED      = 3,
    OZAYN_SVC_OFFLINE     = 4,
    OZAYN_SVC_STOPPING    = 5,
} ozayn_service_state_t;

/* ---- Service record ---- */
typedef struct {
    int                      active;
    char                     id[OZAYN_REGISTRY_MAX_ID];
    char                     name[OZAYN_REGISTRY_MAX_NAME];
    char                     version[OZAYN_REGISTRY_MAX_VERSION];
    uint8_t                  protocol_version;
    ozayn_service_state_t    state;
    char                     endpoint[OZAYN_REGISTRY_MAX_ENDPOINT];
    char                     provider[OZAYN_REGISTRY_MAX_PROVIDER];
    char                     capabilities[OZAYN_REGISTRY_MAX_CAPABILITIES][OZAYN_REGISTRY_MAX_CAP_LEN];
    int                      capability_count;
    time_t                   registered_at;
    time_t                   updated_at;
    int                      conn_fd;          /* IPC connection fd that owns this registration */
} ozayn_service_record_t;

/* ---- Registry manager ---- */
typedef struct {
    ozayn_service_record_t services[OZAYN_REGISTRY_MAX_SERVICES];
    int                    service_count;
    int                    enabled;
    void                  *events;    /* event engine (void* to avoid circular include) */
    void                  *recovery;  /* recovery context */
    int                    initialized;
} ozayn_registry_manager_t;

/* ---- Registration request (input) ---- */
typedef struct {
    char      id[OZAYN_REGISTRY_MAX_ID];
    char      name[OZAYN_REGISTRY_MAX_NAME];
    char      version[OZAYN_REGISTRY_MAX_VERSION];
    uint8_t   protocol_version;
    char      endpoint[OZAYN_REGISTRY_MAX_ENDPOINT];
    char      provider[OZAYN_REGISTRY_MAX_PROVIDER];
    char      capabilities[OZAYN_REGISTRY_MAX_CAPABILITIES][OZAYN_REGISTRY_MAX_CAP_LEN];
    int       capability_count;
} ozayn_service_registration_t;

/* ---- Lifecycle ---- */
ozayn_result_t ozayn_registry_init(ozayn_registry_manager_t *mgr, int enabled);
void           ozayn_registry_shutdown(ozayn_registry_manager_t *mgr);

/* ---- Bindings ---- */
void ozayn_registry_set_events(ozayn_registry_manager_t *mgr, void *events);
void ozayn_registry_set_recovery(ozayn_registry_manager_t *mgr, void *recovery);

/* ---- Operations ---- */
ozayn_result_t ozayn_registry_register(ozayn_registry_manager_t *mgr,
                                        const ozayn_service_registration_t *reg,
                                        int conn_fd);
ozayn_result_t ozayn_registry_unregister(ozayn_registry_manager_t *mgr,
                                          const char *service_id);
const ozayn_service_record_t *ozayn_registry_lookup(const ozayn_registry_manager_t *mgr,
                                                     const char *service_id);
const ozayn_service_record_t *ozayn_registry_find_by_capability(const ozayn_registry_manager_t *mgr,
                                                                 const char *capability);
int            ozayn_registry_list(const ozayn_registry_manager_t *mgr,
                                    const ozayn_service_record_t **out,
                                    int max_out);
ozayn_result_t ozayn_registry_update_state(ozayn_registry_manager_t *mgr,
                                            const char *service_id,
                                            ozayn_service_state_t state);

/* ---- Connection lost ---- */
ozayn_result_t ozayn_registry_on_disconnect(ozayn_registry_manager_t *mgr, int conn_fd);

/* ---- Query ---- */
int            ozayn_registry_count(const ozayn_registry_manager_t *mgr);
int            ozayn_registry_is_enabled(const ozayn_registry_manager_t *mgr);

/* ---- Names ---- */
const char *ozayn_service_state_name(ozayn_service_state_t state);

#endif
