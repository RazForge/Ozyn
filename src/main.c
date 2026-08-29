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

    ozayn_runtime_t *rt = ozayn_runtime_create();
    if (!rt) {
        fprintf(stderr, "[%s] Failed to create runtime.\n", OZAYN_NAME);
        return 1;
    }

    if (ozayn_runtime_init(rt) != OZAYN_OK) {
        fprintf(stderr, "[%s] Failed to initialize runtime.\n", OZAYN_NAME);
        ozayn_runtime_destroy(rt);
        return 1;
    }

    ozayn_core_print_status(&rt->core);

    ozayn_runtime_set_stop_flag(rt, &g_stop);
    ozayn_runtime_run(rt);
    ozayn_runtime_shutdown(rt);
    ozayn_runtime_destroy(rt);

    return 0;
}
