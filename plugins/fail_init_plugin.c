/*
 * fail_init_plugin.c — Plugin that intentionally fails initialization.
 *
 * Used to test that Plugin Manager handles init failures gracefully
 * without crashing OZAYN Core.
 *
 * Compile: gcc -shared -fPIC -o plugins/fail_init_plugin.so plugins/fail_init_plugin.c -Iinclude
 */

#include "plugins.h"
#include "logger.h"

static const ozayn_plugin_info_t g_info = {
    .id          = "fail_init",
    .name        = "Fail Init Plugin",
    .version     = "0.1",
    .api_version = OZAYN_PLUGIN_API_VERSION,
    .author      = "OZYAN",
    .description = "Plugin that always fails init for testing error isolation",
};

static const ozayn_plugin_info_t *fail_get_info(void) {
    return &g_info;
}

static ozayn_result_t fail_init(const ozayn_plugin_context_t *ctx) {
    (void)ctx;
    LOG_ERROR("FAIL_INIT", "Intentional init failure for demonstration");
    return OZAYN_ERR;
}

static ozayn_plugin_api_t g_api = {
    .get_info  = fail_get_info,
    .init      = fail_init,
    .start     = NULL,
    .stop      = NULL,
    .shutdown  = NULL,
};

ozayn_plugin_api_t *ozayn_plugin_entry(void) {
    return &g_api;
}
