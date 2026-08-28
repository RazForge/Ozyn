/**
 * Ozayn Gesture Engine — Hand tracking + cursor control.
 * Uses OpenCV for camera + skin color tracking.
 * Motion-based: only tracks when hand moves.
 */

#ifndef OZAYN_GESTURE_H
#define OZAYN_GESTURE_H

#include "ozayn_core.h"
#include <pthread.h>

/* Gesture engine context */
typedef struct {
    bool      active;
    pthread_t thread;
    int       ipc_fd;
    int       camera_id;
    int       frame_w;
    int       frame_h;
    void     *cap;           /* cv::VideoCapture */
    void     *prev_gray;     /* Previous frame for motion */
    int       motion_frames; /* Consecutive motion frames */
    /* Smoothed cursor position */
    float     sx;
    float     sy;
    bool      has_pos;
    /* Dwell-to-click */
    float     dwell_x;
    float     dwell_y;
    double    dwell_start;
    float     dwell_progress;
    bool      dwell_click;
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
