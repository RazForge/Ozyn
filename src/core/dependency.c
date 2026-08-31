#include "dependency.h"
#include "logger.h"
#include "events.h"
#include <stdio.h>
#include <string.h>

/*
 * dependency.c — Dependency Management (Stage 23).
 *
 * Implements a directed acyclic graph (DAG) of component dependencies.
 * Uses Kahn's algorithm for topological sorting and DFS for cycle detection.
 */

/* ================================================================
 * NAME HELPERS
 * ================================================================ */

const char *ozayn_dep_state_name(ozayn_dep_state_t state) {
    switch (state) {
        case OZAYN_DEP_STATE_UNKNOWN:      return "UNKNOWN";
        case OZAYN_DEP_STATE_DISCOVERED:   return "DISCOVERED";
        case OZAYN_DEP_STATE_RESOLVING:    return "RESOLVING";
        case OZAYN_DEP_STATE_READY:        return "READY";
        case OZAYN_DEP_STATE_BLOCKED:      return "BLOCKED";
        case OZAYN_DEP_STATE_FAILED:       return "FAILED";
        case OZAYN_DEP_STATE_INCOMPATIBLE: return "INCOMPATIBLE";
        case OZAYN_DEP_STATE_UNAVAILABLE:  return "UNAVAILABLE";
        case OZAYN_DEP_STATE_STOPPING:     return "STOPPING";
        case OZAYN_DEP_STATE_STOPPED:      return "STOPPED";
    }
    return "UNKNOWN";
}

const char *ozayn_dep_type_name(ozayn_dep_type_t type) {
    switch (type) {
        case OZAYN_DEP_TYPE_REQUIRED: return "REQUIRED";
        case OZAYN_DEP_TYPE_OPTIONAL: return "OPTIONAL";
    }
    return "UNKNOWN";
}

/* ================================================================
 * INTERNAL: find node by name
 * ================================================================ */

static ozayn_dep_node_t *find_node(ozayn_dep_manager_t *mgr, const char *name) {
    if (!mgr || !name) return NULL;
    for (int i = 0; i < mgr->node_count; i++) {
        if (mgr->nodes[i].active && strcmp(mgr->nodes[i].name, name) == 0) {
            return &mgr->nodes[i];
        }
    }
    return NULL;
}

static const ozayn_dep_node_t *find_node_const(const ozayn_dep_manager_t *mgr, const char *name) {
    if (!mgr || !name) return NULL;
    for (int i = 0; i < mgr->node_count; i++) {
        if (mgr->nodes[i].active && strcmp(mgr->nodes[i].name, name) == 0) {
            return &mgr->nodes[i];
        }
    }
    return NULL;
}

static int find_node_index(const ozayn_dep_manager_t *mgr, const char *name) {
    if (!mgr || !name) return -1;
    for (int i = 0; i < mgr->node_count; i++) {
        if (mgr->nodes[i].active && strcmp(mgr->nodes[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/* ================================================================
 * LIFECYCLE
 * ================================================================ */

int ozayn_dep_init(ozayn_dep_manager_t *mgr, const ozayn_dep_config_t *cfg) {
    if (!mgr) return -1;

    memset(mgr, 0, sizeof(ozayn_dep_manager_t));

    if (cfg) {
        mgr->config = *cfg;
    } else {
        mgr->config.resolve_timeout_ms = 5000;
        mgr->config.fail_on_cycle      = 1;
        mgr->config.fail_on_missing    = 0;
    }

    mgr->initialized = 1;
    LOG_INFO("DEPENDENCY", "Dependency manager initialized (max_nodes=%d, max_edges=%d)",
             OZAYN_DEP_MAX_NODES, OZAYN_DEP_MAX_EDGES);
    return 0;
}

void ozayn_dep_shutdown(ozayn_dep_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    LOG_INFO("DEPENDENCY", "Dependency manager shut down (nodes=%d, edges=%d, cycles=%d, missing=%d)",
             mgr->node_count, mgr->total_edges, mgr->cycles_detected, mgr->missing_detected);

    mgr->initialized = 0;
}

/* ================================================================
 * BINDING
 * ================================================================ */

void ozayn_dep_set_events(ozayn_dep_manager_t *mgr, void *events) {
    if (mgr) mgr->events = events;
}

void ozayn_dep_set_recovery(ozayn_dep_manager_t *mgr, void *recovery) {
    if (mgr) mgr->recovery = recovery;
}

/* ================================================================
 * NODE REGISTRATION
 * ================================================================ */

int ozayn_dep_register_node(ozayn_dep_manager_t *mgr,
                            const char *name,
                            void *context) {
    if (!mgr || !mgr->initialized || !name || !name[0]) return -1;
    if (mgr->node_count >= OZAYN_DEP_MAX_NODES) {
        LOG_WARN("DEPENDENCY", "Node limit reached — cannot register '%s'", name);
        return -1;
    }

    /* Check for duplicate */
    if (find_node(mgr, name)) {
        LOG_WARN("DEPENDENCY", "Node '%s' already registered", name);
        return -1;
    }

    ozayn_dep_node_t *node = &mgr->nodes[mgr->node_count];
    memset(node, 0, sizeof(ozayn_dep_node_t));
    node->active  = 1;
    node->state   = OZAYN_DEP_STATE_DISCOVERED;
    node->context = context;
    node->dep_version_min = -1;
    node->dep_version_max = -1;
    strncpy(node->name, name, OZAYN_DEP_MAX_NAME - 1);
    node->name[OZAYN_DEP_MAX_NAME - 1] = '\0';

    mgr->node_count++;

    LOG_DEBUG("DEPENDENCY", "Registered node '%s'", name);
    return 0;
}

int ozayn_dep_register_simple(ozayn_dep_manager_t *mgr, const char *name) {
    return ozayn_dep_register_node(mgr, name, NULL);
}

/* ================================================================
 * EDGE REGISTRATION
 * ================================================================ */

int ozayn_dep_add_dependency(ozayn_dep_manager_t *mgr,
                             const char *dependent,
                             const char *dependency,
                             ozayn_dep_type_t type) {
    if (!mgr || !mgr->initialized) return -1;
    if (!dependent || !dependent[0] || !dependency || !dependency[0]) return -1;

    ozayn_dep_node_t *node = find_node(mgr, dependent);
    if (!node) {
        LOG_WARN("DEPENDENCY", "Cannot add dependency: node '%s' not found", dependent);
        return -1;
    }

    if (node->dep_count >= OZAYN_DEP_MAX_PER_NODE) {
        LOG_WARN("DEPENDENCY", "Dependency limit reached for '%s'", dependent);
        return -1;
    }

    /* Check for self-dependency */
    if (strcmp(dependent, dependency) == 0) {
        LOG_WARN("DEPENDENCY", "Self-dependency detected for '%s'", dependent);
        return -1;
    }

    /* Check for duplicate edge */
    int dep_idx = find_node_index(mgr, dependency);
    for (int i = 0; i < node->dep_count; i++) {
        if (node->deps[i] == dep_idx) {
            LOG_DEBUG("DEPENDENCY", "Dependency '%s' -> '%s' already exists", dependent, dependency);
            return 0;
        }
    }

    /* If dependency doesn't exist yet, auto-register it */
    if (dep_idx < 0) {
        if (ozayn_dep_register_simple(mgr, dependency) != 0) return -1;
        dep_idx = find_node_index(mgr, dependency);
    }

    node->deps[node->dep_count]      = dep_idx;
    node->dep_types[node->dep_count] = type;
    node->dep_count++;

    mgr->total_edges++;

    LOG_INFO("DEPENDENCY", "Added: %s -> %s (%s)",
             dependent, dependency, ozayn_dep_type_name(type));
    return 0;
}

int ozayn_dep_add_required(ozayn_dep_manager_t *mgr,
                           const char *dependent,
                           const char *dependency) {
    return ozayn_dep_add_dependency(mgr, dependent, dependency, OZAYN_DEP_TYPE_REQUIRED);
}

int ozayn_dep_add_optional(ozayn_dep_manager_t *mgr,
                           const char *dependent,
                           const char *dependency) {
    return ozayn_dep_add_dependency(mgr, dependent, dependency, OZAYN_DEP_TYPE_OPTIONAL);
}

/* ================================================================
 * CYCLE DETECTION (DFS)
 * ================================================================ */

/* DFS states: 0=unvisited, 1=in-progress, 2=done */
static int dfs_cycle_detect(const ozayn_dep_manager_t *mgr,
                            int node_idx,
                            int *visited,
                            int *in_stack,
                            int *cycle_start,
                            int *cycle_end) {
    visited[node_idx]  = 1;
    in_stack[node_idx] = 1;

    const ozayn_dep_node_t *node = &mgr->nodes[node_idx];

    for (int i = 0; i < node->dep_count; i++) {
        int dep_idx = node->deps[i];
        if (dep_idx < 0 || dep_idx >= mgr->node_count) continue;
        if (!mgr->nodes[dep_idx].active) continue;

        if (!visited[dep_idx]) {
            if (dfs_cycle_detect(mgr, dep_idx, visited, in_stack, cycle_start, cycle_end)) {
                return 1;
            }
        } else if (in_stack[dep_idx]) {
            /* Found cycle */
            *cycle_start = dep_idx;
            *cycle_end   = node_idx;
            return 1;
        }
    }

    in_stack[node_idx] = 0;
    return 0;
}

int ozayn_dep_has_cycles(ozayn_dep_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return 0;

    int visited[OZAYN_DEP_MAX_NODES]  = {0};
    int in_stack[OZAYN_DEP_MAX_NODES] = {0};
    int cycle_start = -1, cycle_end = -1;

    for (int i = 0; i < mgr->node_count; i++) {
        if (!mgr->nodes[i].active) continue;
        if (!visited[i]) {
            if (dfs_cycle_detect(mgr, i, visited, in_stack, &cycle_start, &cycle_end)) {
                return 1;
            }
        }
    }
    return 0;
}

int ozayn_dep_detect_cycles(ozayn_dep_manager_t *mgr,
                            char cycle_buf[][OZAYN_DEP_MAX_NAME],
                            int max_names) {
    if (!mgr || !mgr->initialized || !cycle_buf || max_names <= 0) return 0;

    int visited[OZAYN_DEP_MAX_NODES]  = {0};
    int in_stack[OZAYN_DEP_MAX_NODES] = {0};
    int cycle_start = -1, cycle_end = -1;

    /* Find any cycle */
    for (int i = 0; i < mgr->node_count; i++) {
        if (!mgr->nodes[i].active) continue;
        if (!visited[i]) {
            if (dfs_cycle_detect(mgr, i, visited, in_stack, &cycle_start, &cycle_end)) {
                break;
            }
        }
    }

    if (cycle_start < 0) return 0;

    /* Extract cycle path from cycle_end back to cycle_start */
    int path[OZAYN_DEP_MAX_NODES];
    int path_len = 0;

    /* Walk the stack to find the path */
    int cur = cycle_end;
    path[path_len++] = cur;
    while (cur != cycle_start && path_len < mgr->node_count) {
        const ozayn_dep_node_t *node = &mgr->nodes[cur];
        for (int i = 0; i < node->dep_count; i++) {
            int dep_idx = node->deps[i];
            if (in_stack[dep_idx]) {
                path[path_len++] = dep_idx;
                cur = dep_idx;
                break;
            }
        }
    }

    /* Reverse to get forward order and copy to output */
    int count = path_len < max_names ? path_len : max_names;
    for (int i = 0; i < count; i++) {
        strncpy(cycle_buf[i], mgr->nodes[path[count - 1 - i]].name, OZAYN_DEP_MAX_NAME - 1);
        cycle_buf[i][OZAYN_DEP_MAX_NAME - 1] = '\0';
    }

    return count;
}

/* ================================================================
 * TOPOLOGICAL SORT (Kahn's algorithm)
 * ================================================================ */

int ozayn_dep_resolve(ozayn_dep_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return -1;

    /* Check for cycles first */
    if (ozayn_dep_has_cycles(mgr)) {
        mgr->cycles_detected++;
        LOG_ERROR("DEPENDENCY", "Circular dependency detected — resolution failed");

        char cycle_buf[OZAYN_DEP_MAX_NODES][OZAYN_DEP_MAX_NAME];
        int count = ozayn_dep_detect_cycles(mgr, cycle_buf, OZAYN_DEP_MAX_NODES);
        if (count > 0) {
            LOG_ERROR("DEPENDENCY", "Cycle path:");
            for (int i = 0; i < count; i++) {
                LOG_ERROR("DEPENDENCY", "  %s -> %s", cycle_buf[i],
                          cycle_buf[(i + 1) % count]);
            }
        }

        return -1;
    }

    /* Kahn's algorithm: compute in-degree for each node */
    int in_degree[OZAYN_DEP_MAX_NODES] = {0};

    for (int i = 0; i < mgr->node_count; i++) {
        if (!mgr->nodes[i].active) continue;
        const ozayn_dep_node_t *node = &mgr->nodes[i];
        for (int d = 0; d < node->dep_count; d++) {
            int dep_idx = node->deps[d];
            if (dep_idx >= 0 && dep_idx < mgr->node_count) {
                /* This node has a dependency; in the reverse graph,
                 * the dependency points back to us. But Kahn's needs
                 * in-degree of the actual graph. Our edge is:
                 *   dependent --depends-on--> dependency
                 * For startup order, we want to process dependencies first.
                 * So reverse the edges for topological sort. */
            }
        }
    }

    /* Build reverse adjacency: for each edge A->B (A depends on B),
     * store B -> A (B must come before A in startup). */
    int reverse_adj[OZAYN_DEP_MAX_NODES][OZAYN_DEP_MAX_PER_NODE];
    int reverse_count[OZAYN_DEP_MAX_NODES] = {0};

    memset(reverse_adj, 0, sizeof(reverse_adj));

    /* Compute in-degree in the REVERSED graph.
     * Edge: dependent -> dependency means dependency must start before dependent.
     * In the startup order graph: dependency -> dependent.
     * So in-degree[dependent]++ for each dependency edge. */
    for (int i = 0; i < mgr->node_count; i++) {
        if (!mgr->nodes[i].active) continue;
        const ozayn_dep_node_t *node = &mgr->nodes[i];
        for (int d = 0; d < node->dep_count; d++) {
            int dep_idx = node->deps[d];
            if (dep_idx >= 0 && dep_idx < mgr->node_count && mgr->nodes[dep_idx].active) {
                /* dep_idx must come before i */
                if (reverse_count[dep_idx] < OZAYN_DEP_MAX_PER_NODE) {
                    reverse_adj[dep_idx][reverse_count[dep_idx]] = i;
                    reverse_count[dep_idx]++;
                }
                in_degree[i]++;
            }
        }
    }

    /* Kahn's: start with nodes that have in-degree 0 */
    int queue[OZAYN_DEP_MAX_NODES];
    int queue_front = 0, queue_back = 0;

    for (int i = 0; i < mgr->node_count; i++) {
        if (!mgr->nodes[i].active) continue;
        if (in_degree[i] == 0) {
            queue[queue_back++] = i;
        }
    }

    mgr->startup_order_count = 0;

    while (queue_front < queue_back) {
        int idx = queue[queue_front++];
        mgr->startup_order[mgr->startup_order_count++] = idx;

        /* Reduce in-degree for nodes that depend on this one */
        for (int r = 0; r < reverse_count[idx]; r++) {
            int neighbor = reverse_adj[idx][r];
            in_degree[neighbor]--;
            if (in_degree[neighbor] == 0) {
                queue[queue_back++] = neighbor;
            }
        }
    }

    /* Build shutdown order (reverse of startup) */
    mgr->shutdown_order_count = 0;
    for (int i = mgr->startup_order_count - 1; i >= 0; i--) {
        mgr->shutdown_order[mgr->shutdown_order_count++] = mgr->startup_order[i];
    }

    /* Update node states based on resolution */
    mgr->ready_count    = 0;
    mgr->blocked_count  = 0;

    for (int i = 0; i < mgr->node_count; i++) {
        if (!mgr->nodes[i].active) continue;

        ozayn_dep_node_t *node = &mgr->nodes[i];
        int all_deps_ready = 1;
        int has_missing    = 0;
        int has_blocked    = 0;

        for (int d = 0; d < node->dep_count; d++) {
            int dep_idx = node->deps[d];
            if (dep_idx < 0 || dep_idx >= mgr->node_count) continue;
            ozayn_dep_state_t dep_state = mgr->nodes[dep_idx].state;

            if (dep_state == OZAYN_DEP_STATE_UNAVAILABLE ||
                dep_state == OZAYN_DEP_STATE_INCOMPATIBLE) {
                if (node->dep_types[d] == OZAYN_DEP_TYPE_REQUIRED) {
                    has_missing = 1;
                    all_deps_ready = 0;
                }
            } else if (dep_state == OZAYN_DEP_STATE_BLOCKED ||
                       dep_state == OZAYN_DEP_STATE_FAILED) {
                has_blocked = 1;
                all_deps_ready = 0;
            } else if (dep_state != OZAYN_DEP_STATE_READY &&
                       dep_state != OZAYN_DEP_STATE_DISCOVERED) {
                all_deps_ready = 0;
            }
        }

        if (has_missing) {
            node->state = OZAYN_DEP_STATE_UNAVAILABLE;
            mgr->missing_detected++;
        } else if (has_blocked) {
            node->state = OZAYN_DEP_STATE_BLOCKED;
            mgr->blocked_count++;
        } else if (all_deps_ready) {
            node->state = OZAYN_DEP_STATE_READY;
            mgr->ready_count++;
        }

        node->resolved_time = time(NULL);
    }

    LOG_INFO("DEPENDENCY", "Resolution complete: %d nodes, %d edges, startup_order=%d, blocked=%d, ready=%d",
             mgr->node_count, mgr->total_edges, mgr->startup_order_count,
             mgr->blocked_count, mgr->ready_count);

    return 0;
}

/* ================================================================
 * STATE MANAGEMENT
 * ================================================================ */

int ozayn_dep_set_state(ozayn_dep_manager_t *mgr,
                        const char *name,
                        ozayn_dep_state_t state) {
    if (!mgr || !mgr->initialized || !name) return -1;

    ozayn_dep_node_t *node = find_node(mgr, name);
    if (!node) {
        LOG_WARN("DEPENDENCY", "Node '%s' not found", name);
        return -1;
    }

    ozayn_dep_state_t old_state = node->state;
    node->state = state;

    if (state == OZAYN_DEP_STATE_FAILED) {
        node->failed_time = time(NULL);
    }

    if (state != old_state) {
        LOG_INFO("DEPENDENCY", "State changed: %s [%s -> %s]",
                 name, ozayn_dep_state_name(old_state), ozayn_dep_state_name(state));
    }

    return 0;
}

int ozayn_dep_propagate_failure(ozayn_dep_manager_t *mgr,
                                const char *failed_name) {
    if (!mgr || !mgr->initialized || !failed_name) return -1;

    int failed_idx = find_node_index(mgr, failed_name);
    if (failed_idx < 0) return -1;

    int propagated = 0;

    /* For each node that depends on the failed node with REQUIRED type,
     * mark it as BLOCKED (or FAILED if it's required). */
    for (int i = 0; i < mgr->node_count; i++) {
        if (!mgr->nodes[i].active) continue;
        if (i == failed_idx) continue;

        ozayn_dep_node_t *node = &mgr->nodes[i];
        for (int d = 0; d < node->dep_count; d++) {
            if (node->deps[d] == failed_idx &&
                node->dep_types[d] == OZAYN_DEP_TYPE_REQUIRED) {
                if (node->state != OZAYN_DEP_STATE_BLOCKED &&
                    node->state != OZAYN_DEP_STATE_FAILED) {
                    node->state = OZAYN_DEP_STATE_BLOCKED;
                    LOG_INFO("DEPENDENCY", "Propagation: %s BLOCKED (depends on failed %s)",
                             node->name, failed_name);
                    propagated++;
                }
                break;
            }
        }
    }

    mgr->blocked_count += propagated;
    return propagated;
}

/* ================================================================
 * DEPENDENCY CHECK
 * ================================================================ */

int ozayn_dep_can_start(const ozayn_dep_manager_t *mgr, const char *name) {
    if (!mgr || !mgr->initialized || !name) return 0;

    const ozayn_dep_node_t *node = find_node_const(mgr, name);
    if (!node) return 0;

    for (int d = 0; d < node->dep_count; d++) {
        int dep_idx = node->deps[d];
        if (dep_idx < 0 || dep_idx >= mgr->node_count) {
            if (node->dep_types[d] == OZAYN_DEP_TYPE_REQUIRED) return 0;
            continue;
        }

        const ozayn_dep_node_t *dep = &mgr->nodes[dep_idx];
        if (node->dep_types[d] == OZAYN_DEP_TYPE_REQUIRED) {
            if (dep->state != OZAYN_DEP_STATE_READY &&
                dep->state != OZAYN_DEP_STATE_DISCOVERED) {
                return 0;
            }
        }
    }

    return 1;
}

int ozayn_dep_all_ready(const ozayn_dep_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return 0;

    for (int i = 0; i < mgr->node_count; i++) {
        if (!mgr->nodes[i].active) continue;
        if (mgr->nodes[i].state != OZAYN_DEP_STATE_READY &&
            mgr->nodes[i].state != OZAYN_DEP_STATE_DISCOVERED &&
            mgr->nodes[i].state != OZAYN_DEP_STATE_STOPPED) {
            return 0;
        }
    }
    return 1;
}

/* ================================================================
 * QUERY
 * ================================================================ */

ozayn_dep_state_t ozayn_dep_get_state(const ozayn_dep_manager_t *mgr,
                                      const char *name) {
    if (!mgr || !name) return OZAYN_DEP_STATE_UNKNOWN;

    const ozayn_dep_node_t *node = find_node_const(mgr, name);
    if (!node) return OZAYN_DEP_STATE_UNKNOWN;

    return node->state;
}

const ozayn_dep_node_t *ozayn_dep_get_node(const ozayn_dep_manager_t *mgr,
                                           const char *name) {
    return find_node_const(mgr, name);
}

int ozayn_dep_node_count(const ozayn_dep_manager_t *mgr) {
    return mgr ? mgr->node_count : 0;
}

int ozayn_dep_edge_count(const ozayn_dep_manager_t *mgr) {
    return mgr ? mgr->total_edges : 0;
}

/* ================================================================
 * FORWARD LOOKUP: what does A depend on?
 * ================================================================ */

int ozayn_dep_get_dependencies(const ozayn_dep_manager_t *mgr,
                               const char *name,
                               char deps[][OZAYN_DEP_MAX_NAME],
                               int max_deps) {
    if (!mgr || !name || !deps || max_deps <= 0) return 0;

    const ozayn_dep_node_t *node = find_node_const(mgr, name);
    if (!node) return 0;

    int count = node->dep_count < max_deps ? node->dep_count : max_deps;
    for (int i = 0; i < count; i++) {
        int dep_idx = node->deps[i];
        if (dep_idx >= 0 && dep_idx < mgr->node_count) {
            strncpy(deps[i], mgr->nodes[dep_idx].name, OZAYN_DEP_MAX_NAME - 1);
            deps[i][OZAYN_DEP_MAX_NAME - 1] = '\0';
        } else {
            deps[i][0] = '\0';
        }
    }

    return node->dep_count < max_deps ? node->dep_count : max_deps;
}

/* ================================================================
 * REVERSE LOOKUP: who depends on A?
 * ================================================================ */

int ozayn_dep_get_dependents(const ozayn_dep_manager_t *mgr,
                             const char *name,
                             char dependents[][OZAYN_DEP_MAX_NAME],
                             int max_dependents) {
    if (!mgr || !name || !dependents || max_dependents <= 0) return 0;

    int target_idx = find_node_index(mgr, name);
    if (target_idx < 0) return 0;

    int count = 0;
    for (int i = 0; i < mgr->node_count && count < max_dependents; i++) {
        if (!mgr->nodes[i].active) continue;
        if (i == target_idx) continue;

        const ozayn_dep_node_t *node = &mgr->nodes[i];
        for (int d = 0; d < node->dep_count; d++) {
            if (node->deps[d] == target_idx) {
                strncpy(dependents[count], node->name, OZAYN_DEP_MAX_NAME - 1);
                dependents[count][OZAYN_DEP_MAX_NAME - 1] = '\0';
                count++;
                break;
            }
        }
    }

    return count;
}

/* ================================================================
 * STARTUP/SHUTDOWN ORDER QUERY
 * ================================================================ */

int ozayn_dep_get_startup_order(const ozayn_dep_manager_t *mgr,
                                char order[][OZAYN_DEP_MAX_NAME],
                                int max_entries) {
    if (!mgr || !order || max_entries <= 0) return 0;

    int count = mgr->startup_order_count < max_entries ?
                mgr->startup_order_count : max_entries;

    for (int i = 0; i < count; i++) {
        int idx = mgr->startup_order[i];
        strncpy(order[i], mgr->nodes[idx].name, OZAYN_DEP_MAX_NAME - 1);
        order[i][OZAYN_DEP_MAX_NAME - 1] = '\0';
    }

    return count;
}

int ozayn_dep_get_shutdown_order(const ozayn_dep_manager_t *mgr,
                                 char order[][OZAYN_DEP_MAX_NAME],
                                 int max_entries) {
    if (!mgr || !order || max_entries <= 0) return 0;

    int count = mgr->shutdown_order_count < max_entries ?
                mgr->shutdown_order_count : max_entries;

    for (int i = 0; i < count; i++) {
        int idx = mgr->shutdown_order[i];
        strncpy(order[i], mgr->nodes[idx].name, OZAYN_DEP_MAX_NAME - 1);
        order[i][OZAYN_DEP_MAX_NAME - 1] = '\0';
    }

    return count;
}

/* ================================================================
 * STATISTICS
 * ================================================================ */

int ozayn_dep_blocked_count(const ozayn_dep_manager_t *mgr) {
    if (!mgr) return 0;
    int count = 0;
    for (int i = 0; i < mgr->node_count; i++) {
        if (mgr->nodes[i].active && mgr->nodes[i].state == OZAYN_DEP_STATE_BLOCKED)
            count++;
    }
    return count;
}

int ozayn_dep_ready_count(const ozayn_dep_manager_t *mgr) {
    if (!mgr) return 0;
    int count = 0;
    for (int i = 0; i < mgr->node_count; i++) {
        if (mgr->nodes[i].active && mgr->nodes[i].state == OZAYN_DEP_STATE_READY)
            count++;
    }
    return count;
}

int ozayn_dep_failed_count(const ozayn_dep_manager_t *mgr) {
    if (!mgr) return 0;
    int count = 0;
    for (int i = 0; i < mgr->node_count; i++) {
        if (mgr->nodes[i].active && mgr->nodes[i].state == OZAYN_DEP_STATE_FAILED)
            count++;
    }
    return count;
}

/* ================================================================
 * PRINT
 * ================================================================ */

void ozayn_dep_print_graph(const ozayn_dep_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    LOG_INFO("DEPENDENCY", "=== Dependency Graph (%d nodes, %d edges) ===",
             mgr->node_count, mgr->total_edges);

    for (int i = 0; i < mgr->node_count; i++) {
        if (!mgr->nodes[i].active) continue;
        const ozayn_dep_node_t *node = &mgr->nodes[i];

        if (node->dep_count == 0) {
            LOG_INFO("DEPENDENCY", "  %s (no dependencies)", node->name);
        } else {
            for (int d = 0; d < node->dep_count; d++) {
                int dep_idx = node->deps[d];
                const char *dep_name = (dep_idx >= 0 && dep_idx < mgr->node_count) ?
                                       mgr->nodes[dep_idx].name : "???";
                LOG_INFO("DEPENDENCY", "  %s -> %s (%s)",
                         node->name, dep_name,
                         ozayn_dep_type_name(node->dep_types[d]));
            }
        }
    }
}

void ozayn_dep_print_status(const ozayn_dep_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    int ready = 0, blocked = 0, failed = 0, other = 0;
    for (int i = 0; i < mgr->node_count; i++) {
        if (!mgr->nodes[i].active) continue;
        switch (mgr->nodes[i].state) {
            case OZAYN_DEP_STATE_READY: ready++; break;
            case OZAYN_DEP_STATE_BLOCKED: blocked++; break;
            case OZAYN_DEP_STATE_FAILED: failed++; break;
            default: other++; break;
        }
    }

    LOG_INFO("DEPENDENCY", "=== Dependency Status ===");
    LOG_INFO("DEPENDENCY", "  Nodes:   %d", mgr->node_count);
    LOG_INFO("DEPENDENCY", "  Edges:   %d", mgr->total_edges);
    LOG_INFO("DEPENDENCY", "  Ready:   %d", ready);
    LOG_INFO("DEPENDENCY", "  Blocked: %d", blocked);
    LOG_INFO("DEPENDENCY", "  Failed:  %d", failed);

    for (int i = 0; i < mgr->node_count; i++) {
        if (!mgr->nodes[i].active) continue;
        const ozayn_dep_node_t *node = &mgr->nodes[i];
        LOG_INFO("DEPENDENCY", "  %-24s %s", node->name, ozayn_dep_state_name(node->state));
    }
}

void ozayn_dep_print_startup_order(const ozayn_dep_manager_t *mgr) {
    if (!mgr || !mgr->initialized) return;

    LOG_INFO("DEPENDENCY", "=== Startup Order (%d components) ===", mgr->startup_order_count);

    for (int i = 0; i < mgr->startup_order_count; i++) {
        int idx = mgr->startup_order[i];
        LOG_INFO("DEPENDENCY", "  %d. %s [%s]",
                 i + 1, mgr->nodes[idx].name,
                 ozayn_dep_state_name(mgr->nodes[idx].state));
    }

    LOG_INFO("DEPENDENCY", "=== Shutdown Order (reverse) ===");
    for (int i = 0; i < mgr->shutdown_order_count; i++) {
        int idx = mgr->shutdown_order[i];
        LOG_INFO("DEPENDENCY", "  %d. %s",
                 i + 1, mgr->nodes[idx].name);
    }
}
