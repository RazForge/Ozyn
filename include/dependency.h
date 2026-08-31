#ifndef OZAYN_DEPENDENCY_H
#define OZAYN_DEPENDENCY_H

#include <stdint.h>
#include <time.h>
#include <stddef.h>

/*
 * dependency.h — Dependency Management (Stage 23).
 *
 * Self-contained header — no circular includes.
 * Manages the dependency graph between Core components.
 *
 * Responsibilities:
 *   - Register component dependencies (A requires B)
 *   - Represent dependencies as a directed graph
 *   - Topological sort for startup order
 *   - Reverse topological sort for shutdown order
 *   - Circular dependency detection (DFS)
 *   - Missing dependency detection
 *   - Propagate failures / blocking
 *   - Forward lookup: "what does A depend on?"
 *   - Reverse lookup: "who depends on A?"
 *   - Distinguish BLOCKED vs FAILED
 */

/* ---- Constants ---- */

#define OZAYN_DEP_MAX_NODES       32
#define OZAYN_DEP_MAX_EDGES       64   /* max total dependency edges */
#define OZAYN_DEP_MAX_PER_NODE    8    /* max dependencies per component */
#define OZAYN_DEP_MAX_NAME        64

/* ---- Dependency edge type ---- */

typedef enum {
    OZAYN_DEP_TYPE_REQUIRED  = 0,  /* cannot operate without it */
    OZAYN_DEP_TYPE_OPTIONAL  = 1,  /* can work without it (reduced functionality) */
} ozayn_dep_type_t;

/* ---- Dependency node state ---- */

typedef enum {
    OZAYN_DEP_STATE_UNKNOWN      = 0,
    OZAYN_DEP_STATE_DISCOVERED   = 1,  /* registered but not checked */
    OZAYN_DEP_STATE_RESOLVING    = 2,  /* dependency check in progress */
    OZAYN_DEP_STATE_READY        = 3,  /* all required deps satisfied */
    OZAYN_DEP_STATE_BLOCKED      = 4,  /* waiting for a dependency */
    OZAYN_DEP_STATE_FAILED       = 5,  /* a required dependency failed */
    OZAYN_DEP_STATE_INCOMPATIBLE = 6,  /* version mismatch */
    OZAYN_DEP_STATE_UNAVAILABLE  = 7,  /* dependency does not exist */
    OZAYN_DEP_STATE_STOPPING     = 8,  /* shutting down */
    OZAYN_DEP_STATE_STOPPED      = 9,  /* fully stopped */
} ozayn_dep_state_t;

/* ---- Dependency node ---- */

typedef struct {
    int                  active;
    char                 name[OZAYN_DEP_MAX_NAME];
    ozayn_dep_state_t    state;

    /* Edges: this node depends on these nodes */
    int                  deps[OZAYN_DEP_MAX_PER_NODE];
    ozayn_dep_type_t     dep_types[OZAYN_DEP_MAX_PER_NODE];
    int                  dep_count;

    /* Metadata */
    int                  dep_version_min;  /* minimum version required (-1 = any) */
    int                  dep_version_max;  /* maximum version required (-1 = any) */
    int                  version;          /* this node's own version */
    time_t               resolved_time;
    time_t               failed_time;

    /* External context */
    void                *context;
} ozayn_dep_node_t;

/* ---- Dependency manager configuration ---- */

typedef struct {
    int   resolve_timeout_ms;  /* max time to wait for resolution */
    int   fail_on_cycle;       /* 1 = abort on cycle, 0 = log warning */
    int   fail_on_missing;     /* 1 = abort on missing required dep, 0 = log warning */
} ozayn_dep_config_t;

/* ---- Dependency manager ---- */

typedef struct {
    ozayn_dep_node_t    nodes[OZAYN_DEP_MAX_NODES];
    int                 node_count;

    int                 edge_count;

    /* Startup/shutdown order (resolved via topological sort) */
    int                 startup_order[OZAYN_DEP_MAX_NODES];
    int                 startup_order_count;
    int                 shutdown_order[OZAYN_DEP_MAX_NODES];
    int                 shutdown_order_count;

    /* Statistics */
    int                 total_edges;
    int                 cycles_detected;
    int                 missing_detected;
    int                 blocked_count;
    int                 ready_count;

    /* Configuration */
    ozayn_dep_config_t  config;

    /* External dependencies (void* to avoid circular includes) */
    void               *events;
    void               *recovery;

    int                 initialized;
} ozayn_dep_manager_t;

/* ---- Lifecycle ---- */

int  ozayn_dep_init(ozayn_dep_manager_t *mgr, const ozayn_dep_config_t *cfg);
void ozayn_dep_shutdown(ozayn_dep_manager_t *mgr);

/* ---- Binding ---- */

void ozayn_dep_set_events(ozayn_dep_manager_t *mgr, void *events);
void ozayn_dep_set_recovery(ozayn_dep_manager_t *mgr, void *recovery);

/* ---- Node registration ---- */

int  ozayn_dep_register_node(ozayn_dep_manager_t *mgr,
                             const char *name,
                             void *context);

int  ozayn_dep_register_simple(ozayn_dep_manager_t *mgr,
                               const char *name);

/* ---- Edge registration (dependency declaration) ---- */

int  ozayn_dep_add_dependency(ozayn_dep_manager_t *mgr,
                              const char *dependent,
                              const char *dependency,
                              ozayn_dep_type_t type);

int  ozayn_dep_add_required(ozayn_dep_manager_t *mgr,
                            const char *dependent,
                            const char *dependency);

int  ozayn_dep_add_optional(ozayn_dep_manager_t *mgr,
                            const char *dependent,
                            const char *dependency);

/* ---- Graph resolution ---- */

int  ozayn_dep_resolve(ozayn_dep_manager_t *mgr);

/* ---- Cycle detection ---- */

int  ozayn_dep_has_cycles(ozayn_dep_manager_t *mgr);
int  ozayn_dep_detect_cycles(ozayn_dep_manager_t *mgr,
                             char cycle_buf[][OZAYN_DEP_MAX_NAME],
                             int max_names);

/* ---- State management ---- */

int  ozayn_dep_set_state(ozayn_dep_manager_t *mgr,
                         const char *name,
                         ozayn_dep_state_t state);

int  ozayn_dep_propagate_failure(ozayn_dep_manager_t *mgr,
                                 const char *failed_name);

/* ---- Query ---- */

ozayn_dep_state_t  ozayn_dep_get_state(const ozayn_dep_manager_t *mgr,
                                        const char *name);

const ozayn_dep_node_t *ozayn_dep_get_node(const ozayn_dep_manager_t *mgr,
                                           const char *name);

int  ozayn_dep_node_count(const ozayn_dep_manager_t *mgr);
int  ozayn_dep_edge_count(const ozayn_dep_manager_t *mgr);

/* ---- Dependency check ---- */

int  ozayn_dep_can_start(const ozayn_dep_manager_t *mgr, const char *name);
int  ozayn_dep_all_ready(const ozayn_dep_manager_t *mgr);

/* ---- Forward lookup: what does A depend on? ---- */

int  ozayn_dep_get_dependencies(const ozayn_dep_manager_t *mgr,
                                const char *name,
                                char deps[][OZAYN_DEP_MAX_NAME],
                                int max_deps);

/* ---- Reverse lookup: who depends on A? ---- */

int  ozayn_dep_get_dependents(const ozayn_dep_manager_t *mgr,
                              const char *name,
                              char dependents[][OZAYN_DEP_MAX_NAME],
                              int max_dependents);

/* ---- Startup order query ---- */

int  ozayn_dep_get_startup_order(const ozayn_dep_manager_t *mgr,
                                 char order[][OZAYN_DEP_MAX_NAME],
                                 int max_entries);

int  ozayn_dep_get_shutdown_order(const ozayn_dep_manager_t *mgr,
                                  char order[][OZAYN_DEP_MAX_NAME],
                                  int max_entries);

/* ---- Statistics ---- */

int  ozayn_dep_blocked_count(const ozayn_dep_manager_t *mgr);
int  ozayn_dep_ready_count(const ozayn_dep_manager_t *mgr);
int  ozayn_dep_failed_count(const ozayn_dep_manager_t *mgr);

/* ---- Name helpers ---- */

const char *ozayn_dep_state_name(ozayn_dep_state_t state);
const char *ozayn_dep_type_name(ozayn_dep_type_t type);

/* ---- Print ---- */

void ozayn_dep_print_graph(const ozayn_dep_manager_t *mgr);
void ozayn_dep_print_status(const ozayn_dep_manager_t *mgr);
void ozayn_dep_print_startup_order(const ozayn_dep_manager_t *mgr);

#endif
