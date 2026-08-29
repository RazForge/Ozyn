/*
 * test_plugin.c — Valid OZAYN plugin for testing.
 *
 * Demonstrates the full plugin lifecycle:
 *   discovery → validation → load → init → start → stop → shutdown → unload
 *
 * Compile: gcc -shared -fPIC -o plugins/test_plugin.so plugins/test_plugin.c -Iinclude
 */

#include "plugins.h"
#include "logger.h"
#include <stdio.h>

/* Plugin private data */
static int g_counter = 0;

/* Metadata */
static const ozayn_plugin_info_t g_info = {
    .id          = "test_plugin",
    .name        = "Test Plugin",
    .version     = "0.1",
    .api_version = OZAYN_PLUGIN_API_VERSION,
    .author      = "OZYAN",
    .description = "Harmless test plugin demonstrating full lifecycle",
};

static const ozayn_plugin_info_t *test_get_info(void) {
    return &g_info;
}

static ozayn_result_t test_init(const ozayn_plugin_context_t *ctx) {
    (void)ctx;
    g_counter = 0;
    LOG_INFO("TEST_PLUGIN", "Initialized (counter=%d)", g_counter);
    return OZAYN_OK;
}

static ozayn_result_t test_start(const ozayn_plugin_context_t *ctx) {
    (void)ctx;
    g_counter = 42;
    LOG_INFO("TEST_PLUGIN", "Started (counter=%d)", g_counter);
    return OZAYN_OK;
}

static void test_stop(const ozayn_plugin_context_t *ctx) {
    (void)ctx;
    LOG_INFO("TEST_PLUGIN", "Stopped (counter=%d)", g_counter);
}

static void test_shutdown(const ozayn_plugin_context_t *ctx) {
    (void)ctx;
    g_counter = 0;
    LOG_INFO("TEST_PLUGIN", "Shut down — resources released");
}

/* API table — the single exported symbol */
static ozayn_plugin_api_t g_api = {
    .get_info  = test_get_info,
    .init      = test_init,
    .start     = test_start,
    .stop      = test_stop,
    .shutdown  = test_shutdown,
};

/* Entry point — Plugin Manager resolves this symbol */
ozayn_plugin_api_t *ozayn_plugin_entry(void) {
    return &g_api;
}
