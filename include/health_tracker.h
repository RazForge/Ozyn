#ifndef OZAYN_HEALTH_TRACKER_H
#define OZAYN_HEALTH_TRACKER_H

#include <stdint.h>
#include <time.h>

/*
 * health_tracker.h — Health States, Watchdog & Heartbeats (Stage 29).
 *
 * Self-contained header — no circular includes.
 * Tracks component health with nuanced states, watchdog monitoring,
 * and heartbeat-based liveness detection.
 */

/* ---- Constants ---- */

#define OZAYN_HT_MAX_COMPONENTS   64
#define OZAYN_HT_MAX_NAME         64

/* ---- Health states ---- */

typedef enum {
    OZAYN_HT_UNKNOWN       = 0,
    OZAYN_HT_STARTING      = 1,
    OZAYN_HT_HEALTHY       = 2,
    OZAYN_HT_DEGRADED      = 3,
    OZAYN_HT_UNRESPONSIVE  = 4,
    OZAYN_HT_FAILED        = 5,
    OZAYN_HT_RECOVERING    = 6,
    OZAYN_HT_STOPPING      = 7,
    OZAYN_HT_STOPPED       = 8,
    OZAYN_HT_QUARANTINED   = 9,
} ozayn_ht_state_t;

/* ---- Component criticality ---- */

typedef enum {
    OZAYN_HT_CRITICAL  = 0,  /* core cannot function without this */
    OZAYN_HT_IMPORTANT = 1,  /* degraded operation without this */
    OZAYN_HT_OPTIONAL  = 2,  /* safe to lose */
} ozayn_ht_criticality_t;

/* ---- Component health record ---- */

typedef struct {
    char                    name[OZAYN_HT_MAX_NAME];
    ozayn_ht_state_t        state;
    ozayn_ht_criticality_t  criticality;
    uint32_t                heartbeat_interval_ms; /* expected heartbeat period */
    uint32_t                heartbeat_timeout_ms;  /* missing heartbeat threshold */
    struct timespec         last_heartbeat;
    struct timespec         last_state_change;
    uint32_t                miss_count;
    uint32_t                total_heartbeats;
    uint32_t                total_misses;
    int                     watchdog_enabled;
    int                     active;
} ozayn_ht_component_t;

/* ---- Watchdog configuration ---- */

typedef struct {
    uint32_t check_interval_ms; /* how often to check heartbeats */
    int      auto_degrade;      /* auto-degrade on missed heartbeats */
    int      auto_fail;         /* auto-fail after N missed heartbeats */
    uint32_t max_misses;        /* misses before fail */
} ozayn_ht_watchdog_config_t;

/* ---- Health tracker ---- */

typedef struct {
    ozayn_ht_component_t      components[OZAYN_HT_MAX_COMPONENTS];
    uint32_t                  component_count;
    ozayn_ht_watchdog_config_t watchdog;
    struct timespec           last_watchdog_check;
    uint32_t                  total_degraded;
    uint32_t                  total_failed;
    uint32_t                  total_recovered;
    uint32_t                  total_quarantined;
    int                       initialized;
} ozayn_health_tracker_t;

/* ---- Lifecycle ---- */

int  ozayn_ht_init(ozayn_health_tracker_t *ht, const ozayn_ht_watchdog_config_t *wd_cfg);
void ozayn_ht_shutdown(ozayn_health_tracker_t *ht);

/* ---- Component registration ---- */

int  ozayn_ht_register(ozayn_health_tracker_t *ht, const char *name,
                        ozayn_ht_criticality_t criticality,
                        uint32_t heartbeat_interval_ms,
                        uint32_t heartbeat_timeout_ms);
int  ozayn_ht_unregister(ozayn_health_tracker_t *ht, const char *name);

/* ---- Heartbeat ---- */

void ozayn_ht_heartbeat(ozayn_health_tracker_t *ht, const char *name);

/* ---- State management ---- */

void ozayn_ht_set_state(ozayn_health_tracker_t *ht, const char *name,
                         ozayn_ht_state_t state);
void ozayn_ht_quarantine(ozayn_health_tracker_t *ht, const char *name);
void ozayn_ht_unquarantine(ozayn_health_tracker_t *ht, const char *name);

/* ---- Watchdog tick ---- */

void ozayn_ht_watchdog_tick(ozayn_health_tracker_t *ht);

/* ---- Query ---- */

ozayn_ht_state_t ozayn_ht_get_state(const ozayn_health_tracker_t *ht, const char *name);
const ozayn_ht_component_t *ozayn_ht_find(const ozayn_health_tracker_t *ht,
                                           const char *name);
int  ozayn_ht_is_healthy(const ozayn_health_tracker_t *ht, const char *name);
int  ozayn_ht_any_failed(const ozayn_health_tracker_t *ht);
int  ozayn_ht_any_degraded(const ozayn_health_tracker_t *ht);
int  ozayn_ht_count_by_state(const ozayn_health_tracker_t *ht, ozayn_ht_state_t state);
int  ozayn_ht_count_by_criticality(const ozayn_health_tracker_t *ht,
                                    ozayn_ht_criticality_t crit);

/* ---- Stats ---- */

typedef struct {
    uint32_t component_count;
    uint32_t healthy_count;
    uint32_t degraded_count;
    uint32_t failed_count;
    uint32_t quarantined_count;
    uint32_t total_degraded;
    uint32_t total_failed;
    uint32_t total_recovered;
    uint32_t total_quarantined;
} ozayn_ht_stats_t;

ozayn_ht_stats_t ozayn_ht_stats(const ozayn_health_tracker_t *ht);

/* ---- Names ---- */

const char *ozayn_ht_state_name(ozayn_ht_state_t state);
const char *ozayn_ht_criticality_name(ozayn_ht_criticality_t crit);

/* ---- Print ---- */

void ozayn_ht_print(const ozayn_health_tracker_t *ht);
void ozayn_ht_print_component(const ozayn_health_tracker_t *ht, const char *name);

#endif /* OZAYN_HEALTH_TRACKER_H */
