#include "ozayn.h"
#include <stdio.h>
#include <string.h>

ozayn_result_t ozayn_core_init(ozayn_core_t *core) {
    if (!core) return OZAYN_ERR_NULL;

    memset(core, 0, sizeof(ozayn_core_t));

    core->initialized = 1;
    core->module_count = 0;
    core->platform = OZAYN_PLATFORM;
    core->version = OZAYN_VERSION;
    core->status = "ONLINE";

    return OZAYN_OK;
}

void ozayn_core_print_status(const ozayn_core_t *core) {
    if (!core) return;

    printf("\n");
    printf("  ╔══════════════════════════════════════╗\n");
    printf("  ║         %s CORE v%s                 ║\n", OZAYN_NAME, OZAYN_VERSION);
    printf("  ╠══════════════════════════════════════╣\n");
    printf("  ║  STATUS    : %-23s ║\n", core->status);
    printf("  ║  VERSION   : %-23s ║\n", core->version);
    printf("  ║  CODENAME  : %-23s ║\n", OZAYN_CODENAME);
    printf("  ║  MODULES   : %-23d ║\n", core->module_count);
    printf("  ║  PLATFORM  : %-23s ║\n", core->platform);
    printf("  ╚══════════════════════════════════════╝\n");
    printf("\n");
}

void ozayn_core_shutdown(ozayn_core_t *core) {
    if (!core) return;
    core->status = "OFFLINE";
    core->initialized = 0;
    printf("[%s] Core shutdown complete.\n", OZAYN_NAME);
}
