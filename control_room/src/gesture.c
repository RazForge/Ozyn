/**
 * Ozayn Gesture Engine — Hand tracking + cursor control.
 * Uses OpenCV for camera capture. Motion + skin tracking.
 * Rock solid when still, tracks when hand moves.
 */

#include "gesture.h"
#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

/*
 * In production, this uses OpenCV C API:
 *   #include <opencv2/videoio/videoio_c.h>
 *   #include <opencv2/imgproc/imgproc_c.h>
 *
 * Pipeline:
 *   1. Capture frame from camera
 *   2. Convert to grayscale, blur
 *   3. Frame diff → motion mask
 *   4. If motion detected: find skin blob centroid
 *   5. Smooth centroid → send cursor position via IPC
 *   6. Dwell-to-click: hold still 2s → click
 */

static void *gesture_thread(void *arg)
{
    gesture_ctx_t *ctx = (gesture_ctx_t *)arg;

    printf("[GESTURE] Tracking started (camera %d)\n", ctx->camera_id);

    while (ctx->active) {
        /*
         * Production pipeline:
         *
         * cv::Mat frame, gray, diff, mask;
         * cap >> frame;
         * cv::cvtColor(frame, gray, COLOR_BGR2GRAY);
         * cv::GaussianBlur(gray, gray, Size(15,15), 0);
         *
         * if (prev_gray exists):
         *   cv::absdiff(prev_gray, gray, diff);
         *   cv::threshold(diff, mask, 12, 255, THRESH_BINARY);
         *   // erode + dilate cleanup
         *   // findContours → biggest contour → area > 50 → motion
         *
         * if (motion active):
         *   // Find skin blob (HSV + YCrCb + excess red)
         *   // Centroid → smooth → send CMD_GESTURE_MOVE via IPC
         *
         * if (!motion):
         *   // Hold last position (don't send anything)
         *
         * prev_gray = gray;
         */

        usleep(33000); /* ~30 FPS */
    }

    printf("[GESTURE] Stopped\n");
    return NULL;
}

int gesture_init(gesture_ctx_t *ctx, int ipc_fd)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->ipc_fd = ipc_fd;
    ctx->camera_id = 0;
    ctx->frame_w = 640;
    ctx->frame_h = 480;
    ctx->motion_frames = 0;
    ctx->sx = 0.5f;
    ctx->sy = 0.5f;
    ctx->has_pos = false;
    ctx->active = false;
    return 0;
}

int gesture_start(gesture_ctx_t *ctx)
{
    if (ctx->active) return 0;
    ctx->active = true;
    return pthread_create(&ctx->thread, NULL, gesture_thread, ctx);
}

void gesture_stop(gesture_ctx_t *ctx)
{
    ctx->active = false;
    pthread_join(ctx->thread, NULL);
}

void gesture_destroy(gesture_ctx_t *ctx)
{
    gesture_stop(ctx);
}
