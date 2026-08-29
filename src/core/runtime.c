#include "runtime.h"
#include "config.h"
#include "logger.h"
#include "events.h"
#include "processes.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

/* ---------- State name ---------- */

const char *ozayn_state_name(ozayn_state_t state) {
    switch (state) {
        case OZAYN_STATE_CREATED:      return "CREATED";
        case OZAYN_STATE_INITIALIZING: return "INITIALIZING";
        case OZAYN_STATE_RUNNING:      return "RUNNING";
        case OZAYN_STATE_STOPPING:     return "STOPPING";
        case OZAYN_STATE_STOPPED:      return "STOPPED";
        case OZAYN_STATE_FAILED:       return "FAILED";
    }
    return "UNKNOWN";
}

/* ---------- Validate transition ---------- */

static int is_valid_transition(ozayn_state_t from, ozayn_state_t to) {
    switch (from) {
        case OZAYN_STATE_CREATED:
            return to == OZAYN_STATE_INITIALIZING || to == OZAYN_STATE_FAILED;
        case OZAYN_STATE_INITIALIZING:
            return to == OZAYN_STATE_RUNNING || to == OZAYN_STATE_FAILED;
        case OZAYN_STATE_RUNNING:
            return to == OZAYN_STATE_STOPPING || to == OZAYN_STATE_FAILED;
        case OZAYN_STATE_STOPPING:
            return to == OZAYN_STATE_STOPPED || to == OZAYN_STATE_FAILED;
        case OZAYN_STATE_STOPPED:
            return 0; /* terminal state */
        case OZAYN_STATE_FAILED:
            return to == OZAYN_STATE_STOPPING || to == OZAYN_STATE_STOPPED;
    }
    return 0;
}

/* ---------- State transition ---------- */

static ozayn_result_t transition(ozayn_runtime_t *rt, ozayn_state_t to) {
    if (!rt) return OZAYN_ERR_NULL;
    if (!is_valid_transition(rt->state, to)) {
        LOG_ERROR("RUNTIME", "Invalid state transition: %s -> %s",
                  ozayn_state_name(rt->state), ozayn_state_name(to));
        return OZAYN_ERR_STATE;
    }
    rt->state = to;
    return OZAYN_OK;
}

/* ---------- Create ---------- */

ozayn_runtime_t *ozayn_runtime_create(void) {
    ozayn_runtime_t *rt = malloc(sizeof(ozayn_runtime_t));
    if (!rt) return NULL;
    rt->state = OZAYN_STATE_CREATED;
    rt->should_stop = 0;
    rt->stop_flag = NULL;
    rt->config = NULL;
    rt->events = NULL;
    rt->process_mgr = NULL;
    rt->module_mgr = NULL;
    return rt;
}

/* ---------- Init ---------- */

ozayn_result_t ozayn_runtime_init(ozayn_runtime_t *rt) {
    if (!rt) return OZAYN_ERR_NULL;

    ozayn_result_t r = transition(rt, OZAYN_STATE_INITIALIZING);
    if (r != OZAYN_OK) return r;

    /* Initialize core identity */
    r = ozayn_core_init(&rt->core);
    if (r != OZAYN_OK) {
        transition(rt, OZAYN_STATE_FAILED);
        return r;
    }

    /* Runtime is now ready */
    r = transition(rt, OZAYN_STATE_RUNNING);
    if (r != OZAYN_OK) return r;

    return OZAYN_OK;
}

/* ---------- Stop flag ---------- */

void ozayn_runtime_set_stop_flag(ozayn_runtime_t *rt, volatile sig_atomic_t *flag) {
    if (rt) rt->stop_flag = flag;
}

/* ---------- Config binding ---------- */

void ozayn_runtime_set_config(ozayn_runtime_t *rt, const ozayn_config_t *cfg) {
    if (rt) rt->config = cfg;
}

/* ---------- Event engine binding ---------- */

void ozayn_runtime_set_events(ozayn_runtime_t *rt, ozayn_event_engine_t *events) {
    if (rt) rt->events = events;
}

/* ---------- Process manager binding ---------- */

void ozayn_runtime_set_process_mgr(ozayn_runtime_t *rt, void *process_mgr) {
    if (rt) rt->process_mgr = process_mgr;
}

/* ---------- Module manager binding ---------- */

void ozayn_runtime_set_module_mgr(ozayn_runtime_t *rt, void *module_mgr) {
    if (rt) rt->module_mgr = module_mgr;
}

/* ---------- Run ---------- */

ozayn_result_t ozayn_runtime_run(ozayn_runtime_t *rt) {
    if (!rt) return OZAYN_ERR_NULL;
    if (rt->state != OZAYN_STATE_RUNNING) {
        LOG_ERROR("RUNTIME", "Cannot run: state is %s, expected RUNNING",
                  ozayn_state_name(rt->state));
        return OZAYN_ERR_STATE;
    }

    int interval = 1;
    if (rt->config) interval = rt->config->runtime_interval;

    while (rt->state == OZAYN_STATE_RUNNING && !rt->should_stop) {
        if (rt->stop_flag && *rt->stop_flag) break;

        /* Reap exited processes (non-blocking) */
        if (rt->process_mgr)
            ozayn_process_manager_reap((ozayn_process_manager_t *)rt->process_mgr);

        /* Process events */
        if (rt->events) ozayn_events_process(rt->events);

        sleep(interval);
    }

    return OZAYN_OK;
}

/* ---------- Shutdown ---------- */

void ozayn_runtime_shutdown(ozayn_runtime_t *rt) {
    if (!rt) return;
    if (rt->state == OZAYN_STATE_STOPPED || rt->state == OZAYN_STATE_CREATED) return;

    transition(rt, OZAYN_STATE_STOPPING);

    /* Release core resources */
    ozayn_core_shutdown(&rt->core);

    transition(rt, OZAYN_STATE_STOPPED);
}

/* ---------- Destroy ---------- */

void ozayn_runtime_destroy(ozayn_runtime_t *rt) {
    if (!rt) return;
    free(rt);
}

/* ---------- Request stop ---------- */

void ozayn_runtime_request_stop(ozayn_runtime_t *rt) {
    if (!rt) return;
    rt->should_stop = 1;
}

/* ---------- Query ---------- */

int ozayn_runtime_is_running(const ozayn_runtime_t *rt) {
    if (!rt) return 0;
    return rt->state == OZAYN_STATE_RUNNING && !rt->should_stop;
}
