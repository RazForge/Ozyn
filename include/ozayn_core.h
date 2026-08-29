#ifndef OZAYN_CORE_H
#define OZAYN_CORE_H

#define OZAYN_NAME        "OZAYN"
#define OZAYN_VERSION     "0.1"
#define OZAYN_CODENAME    "Genesis"
#define OZAYN_STATUS      "ONLINE"

#define OZAYN_MAJOR       0
#define OZAYN_MINOR       1
#define OZAYN_PATCH       0

typedef struct {
    int initialized;
    int module_count;
    const char *platform;
    const char *status;
} ozayn_core_t;

int  ozayn_core_init(ozayn_core_t *core);
void ozayn_core_print_status(const ozayn_core_t *core);
void ozayn_core_shutdown(ozayn_core_t *core);

#endif
