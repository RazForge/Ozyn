/**
 * Ozayn Gesture Engine — Full pipeline.
 *
 * Pipeline: V4L2 camera → Python MediaPipe → hand_data → gesture_classifier
 *          → gesture_state → command → cursor_engine / X11 actions
 *
 * C is main. Python is a subprocess for MediaPipe inference only.
 */

#ifndef OZAYN_GESTURE_H
#define OZAYN_GESTURE_H

#include "ozayn_core.h"
#include "hand_data.h"
#include "gesture_classifier.h"
#include "cursor_engine.h"
#include <pthread.h>

/* ── Gesture engine context ── */
typedef struct {
    bool      active;
    pthread_t thread;
    int       ipc_fd;
    int       camera_id;
    int       frame_w;
    int       frame_h;

    /* Pipeline components */
    hand_system_t   hand_sys;        /* Hand tracking state */
    gesture_state_t gesture_state;   /* Gesture state machine */
    cursor_state_t  cursor;          /* Cursor engine */

    /* Previous command (for detecting edges) */
    command_t       prev_cmd;

    /* Stats */
    int             frame_count;
    int             gesture_count;
} gesture_ctx_t;

/* Initialize gesture engine */
int  gesture_init(gesture_ctx_t *ctx, int ipc_fd);

/* Start tracking (spawns thread) */
int  gesture_start(gesture_ctx_t *ctx);

/* Stop tracking */
void gesture_stop(gesture_ctx_t *ctx);

/* Cleanup */
void gesture_destroy(gesture_ctx_t *ctx);

#endif /* OZAYN_GESTURE_H */
