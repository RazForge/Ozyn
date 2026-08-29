#include "ozayn.h"
#include <stdio.h>
#include <signal.h>

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    /* Load configuration */
    ozayn_config_object_t cfg;
    if (ozayn_config_load(&cfg) != OZAYN_OK) {
        fprintf(stderr, "[%s] Failed to load configuration.\n", OZAYN_NAME);
        return 1;
    }

    if (ozayn_config_validate(&cfg) != OZAYN_OK) {
        fprintf(stderr, "[%s] Configuration validation failed.\n", OZAYN_NAME);
        ozayn_config_destroy(&cfg);
        return 1;
    }

    /* Create runtime */
    ozayn_runtime_t *rt = ozayn_runtime_create();
    if (!rt) {
        fprintf(stderr, "[%s] Failed to create runtime.\n", OZAYN_NAME);
        ozayn_config_destroy(&cfg);
        return 1;
    }

    /* Bind config to runtime */
    ozayn_runtime_set_config(rt, &cfg.values);

    /* Initialize runtime */
    if (ozayn_runtime_init(rt) != OZAYN_OK) {
        fprintf(stderr, "[%s] Failed to initialize runtime.\n", OZAYN_NAME);
        ozayn_runtime_destroy(rt);
        ozayn_config_destroy(&cfg);
        return 1;
    }

    ozayn_core_print_status(&rt->core);

    ozayn_runtime_set_stop_flag(rt, &g_stop);
    ozayn_runtime_run(rt);
    ozayn_runtime_shutdown(rt);
    ozayn_runtime_destroy(rt);
    ozayn_config_destroy(&cfg);

    return 0;
}
