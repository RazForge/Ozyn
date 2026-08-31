#include "../test_framework.h"
#include "events.h"
#include <string.h>

/*
 * test_events.c — Unit tests for Event Engine (Stage 03).
 *
 * Tests: init, publish, subscribe, unsubscribe, dispatch, name queries.
 */

/* ---- Tracking state for callbacks ---- */
static int _cb_count = 0;
static ozayn_event_type_t _cb_last_type = OZAYN_EVENT_NONE;

static void test_handler(const ozayn_event_t *event, void *ctx) {
    (void)ctx;
    _cb_count++;
    _cb_last_type = event->type;
}

/* ---- Tests ---- */

TEST(events_init_returns_ok) {
    ozayn_event_engine_t eng;
    ozayn_event_config_t cfg = { .queue_capacity = 64, .max_subscribers = 16 };
    int r = ozayn_events_init(&eng, &cfg);
    ASSERT_EQ(r, 0);
    ASSERT(eng.initialized);
    ozayn_events_shutdown(&eng);
    return 0;
}

TEST(events_shutdown_clears_state) {
    ozayn_event_engine_t eng;
    ozayn_event_config_t cfg = { .queue_capacity = 64, .max_subscribers = 16 };
    ozayn_events_init(&eng, &cfg);
    ozayn_events_shutdown(&eng);
    ASSERT(!eng.initialized);
    return 0;
}

TEST(events_publish_single) {
    ozayn_event_engine_t eng;
    ozayn_event_config_t cfg = { .queue_capacity = 64, .max_subscribers = 16 };
    ozayn_events_init(&eng, &cfg);

    int r = ozayn_events_publish(&eng, OZAYN_EVENT_CORE_STARTED, OZAYN_SRC_CORE, NULL);
    ASSERT_EQ(r, 0);
    ASSERT_EQ(ozayn_events_queue_count(&eng), 1);

    ozayn_events_shutdown(&eng);
    return 0;
}

TEST(events_publish_multiple) {
    ozayn_event_engine_t eng;
    ozayn_event_config_t cfg = { .queue_capacity = 64, .max_subscribers = 16 };
    ozayn_events_init(&eng, &cfg);

    ozayn_events_publish(&eng, OZAYN_EVENT_CORE_STARTED, OZAYN_SRC_CORE, NULL);
    ozayn_events_publish(&eng, OZAYN_EVENT_CORE_STOPPING, OZAYN_SRC_CORE, NULL);
    ozayn_events_publish(&eng, OZAYN_EVENT_CONFIG_LOADED, OZAYN_SRC_CONFIG, NULL);

    ASSERT_EQ(ozayn_events_queue_count(&eng), 3);

    ozayn_events_shutdown(&eng);
    return 0;
}

TEST(events_subscribe_and_dispatch) {
    ozayn_event_engine_t eng;
    ozayn_event_config_t cfg = { .queue_capacity = 64, .max_subscribers = 16 };
    ozayn_events_init(&eng, &cfg);

    _cb_count = 0;
    _cb_last_type = OZAYN_EVENT_NONE;

    int sub = ozayn_events_subscribe(&eng, OZAYN_EVENT_CORE_STARTED, test_handler, NULL);
    ASSERT(sub >= 0);

    ozayn_events_publish(&eng, OZAYN_EVENT_CORE_STARTED, OZAYN_SRC_CORE, NULL);
    ozayn_events_process(&eng);

    ASSERT_EQ(_cb_count, 1);
    ASSERT_EQ(_cb_last_type, OZAYN_EVENT_CORE_STARTED);

    ozayn_events_shutdown(&eng);
    return 0;
}

TEST(events_unsubscribe_stops_delivery) {
    ozayn_event_engine_t eng;
    ozayn_event_config_t cfg = { .queue_capacity = 64, .max_subscribers = 16 };
    ozayn_events_init(&eng, &cfg);

    _cb_count = 0;

    int sub = ozayn_events_subscribe(&eng, OZAYN_EVENT_CORE_STARTED, test_handler, NULL);
    ASSERT(sub >= 0);
    ozayn_events_unsubscribe(&eng, sub);

    ozayn_events_publish(&eng, OZAYN_EVENT_CORE_STARTED, OZAYN_SRC_CORE, NULL);
    ozayn_events_process(&eng);

    ASSERT_EQ(_cb_count, 0);

    ozayn_events_shutdown(&eng);
    return 0;
}

TEST(events_type_name_returns_strings) {
    ASSERT_STR_EQ(ozayn_event_type_name(OZAYN_EVENT_NONE), "NONE");
    ASSERT_STR_EQ(ozayn_event_type_name(OZAYN_EVENT_CORE_STARTED), "CORE_STARTED");
    ASSERT_STR_EQ(ozayn_event_type_name(OZAYN_LC_EVENT_ONLINE), "LC_ONLINE");
    ASSERT_STR_EQ(ozayn_event_type_name(OZAYN_DEP_EVENT_CYCLE_DETECTED), "DEP_CYCLE_DETECTED");
    return 0;
}

TEST(events_source_name_returns_strings) {
    ASSERT_STR_EQ(ozayn_event_source_name(OZAYN_SRC_CORE), "CORE");
    ASSERT_STR_EQ(ozayn_event_source_name(OZAYN_SRC_RUNTIME), "RUNTIME");
    ASSERT_STR_EQ(ozayn_event_source_name(OZAYN_SRC_RELOAD), "RELOAD");
    return 0;
}

TEST(events_process_empty_queue) {
    ozayn_event_engine_t eng;
    ozayn_event_config_t cfg = { .queue_capacity = 64, .max_subscribers = 16 };
    ozayn_events_init(&eng, &cfg);

    int count = ozayn_events_process(&eng);
    ASSERT_EQ(count, 0);

    ozayn_events_shutdown(&eng);
    return 0;
}

TEST(events_dispatch_multiple_subscribers) {
    ozayn_event_engine_t eng;
    ozayn_event_config_t cfg = { .queue_capacity = 64, .max_subscribers = 16 };
    ozayn_events_init(&eng, &cfg);

    /* Use two subscribers — both should be called */
    _cb_count = 0;

    ozayn_events_subscribe(&eng, OZAYN_EVENT_TASK_CREATED, test_handler, NULL);
    ozayn_events_subscribe(&eng, OZAYN_EVENT_TASK_CREATED, test_handler, NULL);

    ozayn_events_publish(&eng, OZAYN_EVENT_TASK_CREATED, OZAYN_SRC_CORE, NULL);
    ozayn_events_process(&eng);

    /* Both subscribers should have been called */
    ASSERT_EQ(_cb_count, 2);

    ozayn_events_shutdown(&eng);
    return 0;
}

TEST(events_high_volume) {
    ozayn_event_engine_t eng;
    ozayn_event_config_t cfg = { .queue_capacity = 256, .max_subscribers = 8 };
    ozayn_events_init(&eng, &cfg);

    for (int i = 0; i < 200; i++) {
        ozayn_events_publish(&eng, OZAYN_EVENT_MONITORING_COLLECTED, OZAYN_SRC_CORE, NULL);
    }
    ASSERT_EQ(ozayn_events_queue_count(&eng), 200);

    int processed = ozayn_events_process(&eng);
    ASSERT_EQ(processed, 200);
    ASSERT_EQ(ozayn_events_queue_count(&eng), 0);

    ozayn_events_shutdown(&eng);
    return 0;
}

int run_events_tests(void) {
    SUITE_BEGIN("Event Engine");
    RUN(events_init_returns_ok);
    RUN(events_shutdown_clears_state);
    RUN(events_publish_single);
    RUN(events_publish_multiple);
    RUN(events_subscribe_and_dispatch);
    RUN(events_unsubscribe_stops_delivery);
    RUN(events_type_name_returns_strings);
    RUN(events_source_name_returns_strings);
    RUN(events_process_empty_queue);
    RUN(events_dispatch_multiple_subscribers);
    RUN(events_high_volume);
    SUITE_END();
    return TOTAL_FAIL();
}
