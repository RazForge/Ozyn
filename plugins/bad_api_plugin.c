/*
 * bad_api_plugin.c — Plugin declaring an incompatible API version.
 *
 * The plugin reports api_version=99 while OZAYN expects api_version=1.
 * Plugin Manager must reject this during load.
 *
 * Compile: gcc -shared -fPIC -o plugins/bad_api_plugin.so plugins/bad_api_plugin.c -Iinclude
 */

#include "plugins.h"
#include "logger.h"

static const ozayn_plugin_info_t g_info = {
    .id          = "bad_api",
    .name        = "Bad API Plugin",
    .version     = "0.1",
    .api_version = 99,  /* intentionally wrong */
    .author      = "OZYAN",
    .description = "Plugin with incompatible API version for testing rejection",
};

static const ozayn_plugin_info_t *bad_get_info(void) {
    return &g_info;
}

static ozayn_result_t bad_init(const ozayn_plugin_context_t *ctx) {
    (void)ctx;
    return OZAYN_OK;
}

static ozayn_plugin_api_t g_api = {
    .get_info  = bad_get_info,
    .init      = bad_init,
    .start     = NULL,
    .stop      = NULL,
    .shutdown  = NULL,
};

ozayn_plugin_api_t *ozayn_plugin_entry(void) {
    return &g_api;
}
