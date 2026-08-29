#include "ozayn_core.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

static volatile int running = 1;

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    ozayn_core_t core;
    if (ozayn_core_init(&core) != 0) {
        fprintf(stderr, "[OZAYN] Failed to initialize core.\n");
        return 1;
    }

    ozayn_core_print_status(&core);

    printf("[%s] Core runtime active. Press Ctrl+C to stop.\n", OZAYN_NAME);

    while (running) {
        /* placeholder: event loop will go here */
    }

    ozayn_core_shutdown(&core);
    return 0;
}
