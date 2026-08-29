#include "ozayn_core.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
    #define OZAYN_PLATFORM "Windows"
#elif defined(__APPLE__)
    #define OZAYN_PLATFORM "macOS"
#elif defined(__linux__)
    #define OZAYN_PLATFORM "Linux"
#else
    #define OZAYN_PLATFORM "Unknown"
#endif

int ozayn_core_init(ozayn_core_t *core) {
    if (!core) return -1;
    memset(core, 0, sizeof(ozayn_core_t));
    core->initialized = 1;
    core->module_count = 0;
    core->platform = OZAYN_PLATFORM;
    core->status = OZAYN_STATUS;
    return 0;
}

void ozayn_core_print_status(const ozayn_core_t *core) {
    if (!core) return;
    printf("\n");
    printf("  ╔══════════════════════════════════════╗\n");
    printf("  ║         %s CORE v%s.%s.%s            ║\n", OZAYN_NAME, "0", "1", "0");
    printf("  ╠══════════════════════════════════════╣\n");
    printf("  ║  STATUS    : %-23s ║\n", core->status);
    printf("  ║  VERSION   : %-23s ║\n", OZAYN_VERSION);
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
