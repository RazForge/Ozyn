#include "runtime.h"
#include "config.h"
#include "logger.h"
#include "events.h"
#include "processes.h"
#include "scheduler.h"
#include "monitoring.h"
#include "diagnostics.h"
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
    rt->plugin_mgr = NULL;
    rt->ipc_mgr = NULL;
    rt->registry_mgr = NULL;
    rt->resource_mgr = NULL;
    rt->scheduler_mgr = NULL;
    rt->monitoring_mgr = NULL;
    rt->diagnostics_mgr = NULL;
    rt->security_boundary_mgr = NULL;
    rt->state_mgr = NULL;
    rt->lifecycle_mgr = NULL;
    rt->dependency_mgr = NULL;
    rt->svc_lifecycle_mgr = NULL;
    rt->config_mgr = NULL;
    rt->api_mgr = NULL;
    rt->reload_mgr = NULL;
    rt->perf_mgr = NULL;
    rt->defense_mgr = NULL;
    rt->cb_mgr = NULL;
    rt->rl_mgr = NULL;
    rt->ht_mgr = NULL;
    rt->cl_mgr = NULL;
    rt->cv_mgr = NULL;
    rt->release_mgr = NULL;
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

/* ---------- Plugin manager binding ---------- */

void ozayn_runtime_set_plugin_mgr(ozayn_runtime_t *rt, void *plugin_mgr) {
    if (rt) rt->plugin_mgr = plugin_mgr;
}

/* ---------- IPC manager binding ---------- */

void ozayn_runtime_set_ipc_mgr(ozayn_runtime_t *rt, void *ipc_mgr) {
    if (rt) rt->ipc_mgr = ipc_mgr;
}

/* ---------- Service registry binding ---------- */

void ozayn_runtime_set_registry_mgr(ozayn_runtime_t *rt, void *registry_mgr) {
    if (rt) rt->registry_mgr = registry_mgr;
}

/* ---------- Security manager binding ---------- */

void ozayn_runtime_set_security_mgr(ozayn_runtime_t *rt, void *security_mgr) {
    if (rt) rt->security_mgr = security_mgr;
}

/* ---------- Authorization manager binding ---------- */

void ozayn_runtime_set_authorization_mgr(ozayn_runtime_t *rt, void *authorization_mgr) {
    if (rt) rt->authorization_mgr = authorization_mgr;
}

/* ---------- Resource manager binding ---------- */

void ozayn_runtime_set_resource_mgr(ozayn_runtime_t *rt, void *resource_mgr) {
    if (rt) rt->resource_mgr = resource_mgr;
}

/* ---------- Scheduler manager binding ---------- */

void ozayn_runtime_set_scheduler_mgr(ozayn_runtime_t *rt, void *scheduler_mgr) {
    if (rt) rt->scheduler_mgr = scheduler_mgr;
}

/* ---------- Monitoring manager binding ---------- */

void ozayn_runtime_set_monitoring_mgr(ozayn_runtime_t *rt, void *monitoring_mgr) {
    if (rt) rt->monitoring_mgr = monitoring_mgr;
}

/* ---------- Diagnostics manager binding ---------- */

void ozayn_runtime_set_diagnostics_mgr(ozayn_runtime_t *rt, void *diagnostics_mgr) {
    if (rt) rt->diagnostics_mgr = diagnostics_mgr;
}

/* ---------- Security boundary manager binding ---------- */

void ozayn_runtime_set_security_boundary_mgr(ozayn_runtime_t *rt, void *security_boundary_mgr) {
    if (rt) rt->security_boundary_mgr = security_boundary_mgr;
}

/* ---------- State manager binding ---------- */

void ozayn_runtime_set_state_mgr(ozayn_runtime_t *rt, void *state_mgr) {
    if (rt) rt->state_mgr = state_mgr;
}

/* ---------- Lifecycle manager binding ---------- */

void ozayn_runtime_set_lifecycle_mgr(ozayn_runtime_t *rt, void *lifecycle_mgr) {
    if (rt) rt->lifecycle_mgr = lifecycle_mgr;
}

/* ---------- Dependency manager binding ---------- */

void ozayn_runtime_set_dependency_mgr(ozayn_runtime_t *rt, void *dependency_mgr) {
    if (rt) rt->dependency_mgr = dependency_mgr;
}

void ozayn_runtime_set_svc_lifecycle_mgr(ozayn_runtime_t *rt, void *svc_lifecycle_mgr) {
    if (rt) rt->svc_lifecycle_mgr = svc_lifecycle_mgr;
}

void ozayn_runtime_set_config_mgr(ozayn_runtime_t *rt, void *config_mgr) {
    if (rt) rt->config_mgr = config_mgr;
}

void ozayn_runtime_set_api_mgr(ozayn_runtime_t *rt, void *api_mgr) {
    if (rt) rt->api_mgr = api_mgr;
}

void ozayn_runtime_set_reload_mgr(ozayn_runtime_t *rt, void *reload_mgr) {
    if (rt) rt->reload_mgr = reload_mgr;
}

void ozayn_runtime_set_perf_mgr(ozayn_runtime_t *rt, void *perf_mgr) {
    if (rt) rt->perf_mgr = perf_mgr;
}

void ozayn_runtime_set_defense_mgr(ozayn_runtime_t *rt, void *defense_mgr) {
    if (rt) rt->defense_mgr = defense_mgr;
}

void ozayn_runtime_set_cb_mgr(ozayn_runtime_t *rt, void *cb_mgr) {
    if (rt) rt->cb_mgr = cb_mgr;
}

void ozayn_runtime_set_rl_mgr(ozayn_runtime_t *rt, void *rl_mgr) {
    if (rt) rt->rl_mgr = rl_mgr;
}

void ozayn_runtime_set_ht_mgr(ozayn_runtime_t *rt, void *ht_mgr) {
    if (rt) rt->ht_mgr = ht_mgr;
}

void ozayn_runtime_set_cl_mgr(ozayn_runtime_t *rt, void *cl_mgr) {
    if (rt) rt->cl_mgr = cl_mgr;
}

void ozayn_runtime_set_cv_mgr(ozayn_runtime_t *rt, void *cv_mgr) {
    if (rt) rt->cv_mgr = cv_mgr;
}

void ozayn_runtime_set_release_mgr(ozayn_runtime_t *rt, void *release_mgr) {
    if (rt) rt->release_mgr = release_mgr;
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

        /* Process IPC (accept, receive, dispatch) */
        if (rt->ipc_mgr)
            ozayn_ipc_manager_process((ozayn_ipc_manager_t *)rt->ipc_mgr);

        /* Scheduler tick — dispatch ready tasks */
        if (rt->scheduler_mgr)
            ozayn_scheduler_tick((ozayn_scheduler_manager_t *)rt->scheduler_mgr);

        /* Monitoring collect — run health checks */
        if (rt->monitoring_mgr)
            ozayn_monitoring_collect((ozayn_monitoring_manager_t *)rt->monitoring_mgr);

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
