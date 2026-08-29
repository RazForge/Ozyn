/*
 * duplicate_test_plugin.c — Plugin with same ID as test_plugin.
 *
 * Used to test that Plugin Manager rejects duplicate plugin identities.
 * The ID is "test_plugin" — same as test_plugin.so.
 *
 * Compile: gcc -shared -fPIC -o plugins/duplicate_test_plugin.so plugins/duplicate_test_plugin.c -Iinclude
 */

#include "plugins.h"
#include "logger.h"

static const ozayn_plugin_info_t g_info = {
    .id          = "test_plugin",  /* same ID as test_plugin.so */
    .name        = "Duplicate Test Plugin",
    .version     = "0.2",
    .api_version = OZAYN_PLUGIN_API_VERSION,
    .author      = "OZYAN",
    .description = "Plugin with duplicate ID for testing rejection",
};

static const ozayn_plugin_info_t *dup_get_info(void) {
    return &g_info;
}

static ozayn_result_t dup_init(const ozayn_plugin_context_t *ctx) {
    (void)ctx;
    LOG_INFO("DUP_PLUGIN", "Init called — this should not happen if duplicate is rejected");
    return OZAYN_OK;
}

static ozayn_plugin_api_t g_api = {
    .get_info  = dup_get_info,
    .init      = dup_init,
    .start     = NULL,
    .stop      = NULL,
    .shutdown  = NULL,
};

ozayn_plugin_api_t *ozayn_plugin_entry(void) {
    return &g_api;
}
