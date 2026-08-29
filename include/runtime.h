#ifndef OZAYN_RUNTIME_H
#define OZAYN_RUNTIME_H

#include "ozayn.h"
#include <signal.h>

/*
 * ozayn_runtime.h — Runtime lifecycle management.
 *
 * The Runtime is the continuous execution environment of OZAYN.
 * It owns the state machine that keeps OZAYN alive and coordinates shutdown.
 */

/* Runtime states */
typedef enum {
    OZAYN_STATE_CREATED       = 0,
    OZAYN_STATE_INITIALIZING  = 1,
    OZAYN_STATE_RUNNING       = 2,
    OZAYN_STATE_STOPPING      = 3,
    OZAYN_STATE_STOPPED       = 4,
    OZAYN_STATE_FAILED        = 5,
} ozayn_state_t;

/* Runtime object */
typedef struct {
    ozayn_state_t state;
    int           should_stop;
    volatile sig_atomic_t *stop_flag;  /* points to signal handler's flag */
    ozayn_core_t  core;
} ozayn_runtime_t;

/* Lifecycle */
ozayn_runtime_t *ozayn_runtime_create(void);
ozayn_result_t   ozayn_runtime_init(ozayn_runtime_t *rt);
ozayn_result_t   ozayn_runtime_run(ozayn_runtime_t *rt);
void             ozayn_runtime_shutdown(ozayn_runtime_t *rt);
void             ozayn_runtime_destroy(ozayn_runtime_t *rt);

/* Stop flag — set from signal handler */
void ozayn_runtime_set_stop_flag(ozayn_runtime_t *rt, volatile sig_atomic_t *flag);

/* Query */
const char      *ozayn_state_name(ozayn_state_t state);
int              ozayn_runtime_is_running(const ozayn_runtime_t *rt);

#endif
