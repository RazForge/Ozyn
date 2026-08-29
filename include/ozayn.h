#ifndef OZAYN_H
#define OZAYN_H

/*
 * ozayn.h — Unified entry point for OZAYN Core.
 *
 * Include this single header to access the core runtime.
 * Individual subsystem headers will be added in later stages.
 */

#define OZAYN_NAME        "OZAYN"
#define OZAYN_VERSION     "0.1"
#define OZAYN_CODENAME    "Genesis"

#define OZAYN_MAJOR       0
#define OZAYN_MINOR       1
#define OZAYN_PATCH       0

/* Platform detection */
#if defined(_WIN32)
    #define OZAYN_PLATFORM "Windows"
#elif defined(__APPLE__)
    #define OZAYN_PLATFORM "macOS"
#elif defined(__linux__)
    #define OZAYN_PLATFORM "Linux"
#else
    #define OZAYN_PLATFORM "Unknown"
#endif

/* Result codes */
typedef enum {
    OZAYN_OK          =  0,
    OZAYN_ERR         = -1,
    OZAYN_ERR_NULL    = -2,
    OZAYN_ERR_INIT    = -3,
    OZAYN_ERR_STATE   = -4,
} ozayn_result_t;

/* Core runtime state */
typedef struct {
    int initialized;
    int module_count;
    const char *platform;
    const char *version;
    const char *status;
} ozayn_core_t;

/* Core lifecycle */
ozayn_result_t ozayn_core_init(ozayn_core_t *core);
void           ozayn_core_print_status(const ozayn_core_t *core);
void           ozayn_core_shutdown(ozayn_core_t *core);

/* Runtime — included here for unified access */
#include "runtime.h"

/* Configuration — included after runtime (uses forward declaration) */
#include "config.h"

#endif
