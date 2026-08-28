/**
 * Ozayn Gesture Engine — MediaPipe hand landmarks via Python subprocess.
 * V4L2 captures RGB frames, pipes to hand_tracker.py, reads 21 landmarks,
 * controls X11 mouse using index finger + pinch click (AI Virtual Mouse style).
 */

#include "gesture.h"
#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#define IMG_W 640
#define IMG_H 480
#define SMOOTH_FACTOR 7
#define DEADZONE_PX 10

/* ── V4L2 Camera ── */
typedef struct {
    int fd;
    uint8_t *buf;
    size_t   buf_len;
} v4l2_cam_t;

static int cam_open(v4l2_cam_t *cam, int dev_id)
{
    char dev[32];
    snprintf(dev, sizeof(dev), "/dev/video%d", dev_id);

    cam->fd = open(dev, O_RDWR);
    if (cam->fd < 0) return -1;

    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = IMG_W;
    fmt.fmt.pix.height = IMG_H;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(cam->fd, VIDIOC_S_FMT, &fmt) < 0) { close(cam->fd); return -1; }

    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(cam->fd, VIDIOC_REQBUFS, &req) < 0) { close(cam->fd); return -1; }

    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if (ioctl(cam->fd, VIDIOC_QUERYBUF, &buf) < 0) { close(cam->fd); return -1; }

    cam->buf_len = buf.length;
    cam->buf = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, cam->fd, buf.m.offset);
    if (cam->buf == MAP_FAILED) { close(cam->fd); return -1; }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(cam->fd, VIDIOC_STREAMON, &type);

    printf("[GESTURE] Camera: %s (%dx%d)\n", dev, IMG_W, IMG_H);
    return 0;
}

static int cam_read_rgb(v4l2_cam_t *cam, uint8_t *rgb_out)
{
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    ioctl(cam->fd, VIDIOC_QBUF, &buf);

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(cam->fd, &fds);
    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    if (select(cam->fd + 1, &fds, NULL, NULL, &tv) <= 0) return -1;
    if (ioctl(cam->fd, VIDIOC_DQBUF, &buf) < 0) return -1;

    /* YUYV → RGB (safe conversion) */
    for (int i = 0; i < IMG_W * IMG_H; i += 2) {
        int yi = i * 2;
        int y0 = cam->buf[yi];
        int y1 = cam->buf[yi + 2];
        int u  = cam->buf[yi + 1];
        int v  = cam->buf[yi + 3];

        int c0 = y0 - 16, c1 = y1 - 16;
        int d = u - 128, e = v - 128;

        rgb_out[i * 3 + 0] = (uint8_t)(c0 + 1.402 * e);
        rgb_out[i * 3 + 1] = (uint8_t)(c0 - 0.344136 * d - 0.714136 * e);
        rgb_out[i * 3 + 2] = (uint8_t)(c0 + 1.772 * d);

        rgb_out[(i + 1) * 3 + 0] = (uint8_t)(c1 + 1.402 * e);
        rgb_out[(i + 1) * 3 + 1] = (uint8_t)(c1 - 0.344136 * d - 0.714136 * e);
        rgb_out[(i + 1) * 3 + 2] = (uint8_t)(c1 + 1.772 * d);
    }

    ioctl(cam->fd, VIDIOC_QBUF, &buf);
    return 0;
}

static void cam_close(v4l2_cam_t *cam)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(cam->fd, VIDIOC_STREAMOFF, &type);
    munmap(cam->buf, cam->buf_len);
    close(cam->fd);
}

/* ── X11 mouse control ── */
typedef struct {
    Display *dpy;
    int      screen;
    int      root_w;
    int      root_h;
} x11_ctx_t;

static int x11_init(x11_ctx_t *x)
{
    x->dpy = XOpenDisplay(NULL);
    if (!x->dpy) return -1;
    x->screen = DefaultScreen(x->dpy);
    x->root_w = DisplayWidth(x->dpy, x->screen);
    x->root_h = DisplayHeight(x->dpy, x->screen);
    printf("[GESTURE] X11: %dx%d screen\n", x->root_w, x->root_h);
    return 0;
}

static void x11_move(x11_ctx_t *x, int px, int py)
{
    XWarpPointer(x->dpy, None, DefaultRootWindow(x->dpy), 0, 0, 0, 0, px, py);
    XFlush(x->dpy);
}

static void x11_click(x11_ctx_t *x)
{
    XTestFakeButtonEvent(x->dpy, Button1, True, CurrentTime);
    XTestFakeButtonEvent(x->dpy, Button1, False, CurrentTime);
    XFlush(x->dpy);
}

static void x11_right_click(x11_ctx_t *x)
{
    XTestFakeButtonEvent(x->dpy, Button3, True, CurrentTime);
    XTestFakeButtonEvent(x->dpy, Button3, False, CurrentTime);
    XFlush(x->dpy);
}

static void x11_close(x11_ctx_t *x)
{
    if (x->dpy) XCloseDisplay(x->dpy);
}

/* ── Hand tracker subprocess (Python MediaPipe) ── */
typedef struct {
    pid_t pid;
    int   stdin_fd;
    int   stdout_fd;
    bool  running;
} tracker_proc_t;

/* Hand landmark result from Python */
typedef struct {
    bool    detected;
    float   index_x, index_y;
    float   middle_x, middle_y;
    float   ring_x, ring_y;
    float   pinky_x, pinky_y;
    float   thumb_x, thumb_y;
    uint8_t fingers_up;   /* bit0=thumb, bit1=index, bit2=middle, bit3=ring, bit4=pinky */
    float   pinch_dist;   /* 0.0-1.0 */
} hand_result_t;

static int tracker_start(tracker_proc_t *t, const char *script_path, const char *model_path)
{
    int stdin_pipe[2], stdout_pipe[2];
    pipe(stdin_pipe);
    pipe(stdout_pipe);

    t->pid = fork();
    if (t->pid == 0) {
        /* Child: redirect stdin/stdout */
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        execlp("python3", "python3", script_path, model_path, NULL);
        perror("execlp hand_tracker");
        _exit(1);
    }

    /* Parent */
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    t->stdin_fd  = stdin_pipe[1];
    t->stdout_fd = stdout_pipe[0];
    t->running   = true;

    printf("[GESTURE] Hand tracker started (pid %d)\n", t->pid);

    /* Python sends 'Ready' on stderr (not blocking on it) */
    usleep(500000); /* Wait 500ms for Python to initialize model */

    return 0;
}

static int tracker_send_frame(tracker_proc_t *t, const uint8_t *rgb, int w, int h)
{
    /* Protocol: 'F' + uint16(w) + uint16(h) + RGB data */
    uint8_t header[5] = { 'F', w & 0xFF, (w >> 8) & 0xFF, h & 0xFF, (h >> 8) & 0xFF };
    if (write(t->stdin_fd, header, 5) != 5) return -1;
    if (write(t->stdin_fd, rgb, w * h * 3) != w * h * 3) return -1;
    return 0;
}

static int tracker_read_result(tracker_proc_t *t, hand_result_t *result)
{
    uint8_t tag;

    /* Wait up to 2 seconds for response */
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(t->stdout_fd, &fds);
    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    if (select(t->stdout_fd + 1, &fds, NULL, NULL, &tv) <= 0) {
        result->detected = false;
        return 0;
    }

    if (read(t->stdout_fd, &tag, 1) != 1) { result->detected = false; return -1; }

    if (tag == 'N') {
        result->detected = false;
        return 0;
    }

    if (tag != 'H') { result->detected = false; return -1; }

    int total;

    /* Read 10 uint16 (5 fingertips x 2 coords) = 20 bytes */
    uint8_t tip_buf[20] = {0};
    total = 0;
    while (total < 20) {
        fd_set fds2;
        FD_ZERO(&fds2);
        FD_SET(t->stdout_fd, &fds2);
        struct timeval tv2 = { .tv_sec = 2, .tv_usec = 0 };
        if (select(t->stdout_fd + 1, &fds2, NULL, NULL, &tv2) <= 0) { result->detected = false; return -1; }
        int n = read(t->stdout_fd, tip_buf + total, 20 - total);
        if (n <= 0) { result->detected = false; return -1; }
        total += n;
    }

    /* Parse tips as uint16 LE */
    uint16_t tips[10];
    for (int i = 0; i < 10; i++)
        tips[i] = tip_buf[i*2] | (tip_buf[i*2+1] << 8);

    /* Read fingers_up (uint8) + pinch_dist (uint16) = 3 bytes */
    uint8_t fp_buf[3] = {0};
    total = 0;
    while (total < 3) {
        fd_set fds3;
        FD_ZERO(&fds3);
        FD_SET(t->stdout_fd, &fds3);
        struct timeval tv3 = { .tv_sec = 2, .tv_usec = 0 };
        if (select(t->stdout_fd + 1, &fds3, NULL, NULL, &tv3) <= 0) { result->detected = false; return -1; }
        int n = read(t->stdout_fd, fp_buf + total, 3 - total);
        if (n <= 0) { result->detected = false; return -1; }
        total += n;
    }
    uint8_t fingers = fp_buf[0];
    uint16_t pinch = fp_buf[1] | (fp_buf[2] << 8);

    result->detected  = true;
    result->thumb_x   = tips[0] / 65535.0f;
    result->thumb_y   = tips[1] / 65535.0f;
    result->index_x   = tips[2] / 65535.0f;
    result->index_y   = tips[3] / 65535.0f;
    result->middle_x  = tips[4] / 65535.0f;
    result->middle_y  = tips[5] / 65535.0f;
    result->ring_x    = tips[6] / 65535.0f;
    result->ring_y    = tips[7] / 65535.0f;
    result->pinky_x   = tips[8] / 65535.0f;
    result->pinky_y   = tips[9] / 65535.0f;
    result->fingers_up = fingers;
    result->pinch_dist = pinch / 65535.0f;

    return 0;
}

static void tracker_stop(tracker_proc_t *t)
{
    if (!t->running) return;
    t->running = false;
    close(t->stdin_fd);
    close(t->stdout_fd);
    kill(t->pid, SIGTERM);
    waitpid(t->pid, NULL, 0);
    printf("[GESTURE] Hand tracker stopped\n");
}

/* ── Main gesture thread ── */
static void *gesture_thread(void *arg)
{
    gesture_ctx_t *ctx = (gesture_ctx_t *)arg;

    /* Open camera */
    v4l2_cam_t cam;
    if (cam_open(&cam, ctx->camera_id) < 0) {
        printf("[GESTURE] ERROR: Cannot open camera %d\n", ctx->camera_id);
        ctx->active = false;
        return NULL;
    }

    /* Open X11 */
    x11_ctx_t x11;
    if (x11_init(&x11) < 0) {
        printf("[GESTURE] ERROR: Cannot open X11 display\n");
        cam_close(&cam);
        ctx->active = false;
        return NULL;
    }

    /* Start hand tracker Python process */
    tracker_proc_t tracker;
    char model_path[512], script_path[512];
    char *base = getenv("OZAYN_BASE");
    if (!base) base = ".";
    snprintf(model_path, sizeof(model_path), "%s/lib/hand_landmarker.task", base);
    snprintf(script_path, sizeof(script_path), "%s/hand_tracker.py", base);

    if (tracker_start(&tracker, script_path, model_path) < 0) {
        printf("[GESTURE] ERROR: Cannot start hand tracker\n");
        x11_close(&x11);
        cam_close(&cam);
        ctx->active = false;
        return NULL;
    }

    /* RGB buffer */
    uint8_t *rgb_buf = malloc(IMG_W * IMG_H * 3);

    /* Smooth cursor position */
    float smooth_x = 0.5f, smooth_y = 0.5f;
    bool has_pos = false;
    bool was_pinching = false;

    printf("[GESTURE] Tracking active — show your hand\n");

    int frame_count = 0;

    while (ctx->active) {
        /* Capture RGB frame */
        if (cam_read_rgb(&cam, rgb_buf) < 0) {
            usleep(33000);
            continue;
        }
        frame_count++;

        /* Send to Python tracker */
        if (tracker_send_frame(&tracker, rgb_buf, IMG_W, IMG_H) < 0) {
            if (frame_count <= 5) printf("[GESTURE] send_frame failed (frame %d)\n", frame_count);
            usleep(33000);
            continue;
        }

        /* Read hand result */
        hand_result_t hand;
        if (tracker_read_result(&tracker, &hand) < 0) {
            if (frame_count <= 5) printf("[GESTURE] read_result failed (frame %d)\n", frame_count);
            usleep(33000);
            continue;
        }

        if (frame_count % 30 == 0)
            printf("[GESTURE] frame %d: hand=%d fingers=0x%02x pinch=%.2f\n",
                   frame_count, hand.detected, hand.fingers_up, hand.pinch_dist);

        if (!hand.detected) {
            ctx->has_pos = false;
            has_pos = false;
            usleep(33000);
            continue;
        }

        /* ── AI Virtual Mouse logic ── */
        int index_up  = (hand.fingers_up >> 1) & 1;
        int middle_up = (hand.fingers_up >> 2) & 1;
        int ring_up   = (hand.fingers_up >> 3) & 1;
        int pinky_up  = (hand.fingers_up >> 4) & 1;

        /* Frame reduction zone (100px from edges) */
        float frame_r = 100.0f / IMG_W;

        /* Only index finger up → MOVE mode */
        if (index_up && !middle_up) {
            /* Map index fingertip to screen (mirrored X) */
            float nx = 1.0f - hand.index_x;
            float ny = hand.index_y;

            /* Clamp to frame reduction zone */
            if (nx < frame_r) nx = frame_r;
            if (nx > 1.0f - frame_r) nx = 1.0f - frame_r;
            if (ny < frame_r) ny = frame_r;
            if (ny > 1.0f - frame_r) ny = 1.0f - frame_r;

            /* Remap from frame zone to full screen */
            float sx = (nx - frame_r) / (1.0f - 2.0f * frame_r);
            float sy = (ny - frame_r) / (1.0f - 2.0f * frame_r);

            /* Smooth (AI Virtual Mouse style: divisor=7) */
            if (!has_pos) {
                smooth_x = sx;
                smooth_y = sy;
                has_pos = true;
            } else {
                smooth_x = smooth_x + (sx - smooth_x) / (float)SMOOTH_FACTOR;
                smooth_y = smooth_y + (sy - smooth_y) / (float)SMOOTH_FACTOR;
            }

            ctx->sx = smooth_x;
            ctx->sy = smooth_y;
            ctx->has_pos = true;

            /* Move mouse */
            int px = (int)(smooth_x * x11.root_w);
            int py = (int)(smooth_y * x11.root_h);
            x11_move(&x11, px, py);
        }

        /* Index + middle up → CLICK mode */
        if (index_up && middle_up && hand.pinch_dist < 0.15f) {
            if (!was_pinching) {
                x11_click(&x11);
                printf("[GESTURE] LEFT CLICK (pinch %.2f)\n", hand.pinch_dist);
            }
            was_pinching = true;
        } else {
            was_pinching = false;
        }

        usleep(33000); /* ~30 FPS */
    }

    free(rgb_buf);
    tracker_stop(&tracker);
    x11_close(&x11);
    cam_close(&cam);

    printf("[GESTURE] Stopped\n");
    return NULL;
}

int gesture_init(gesture_ctx_t *ctx, int ipc_fd)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->ipc_fd = ipc_fd;
    ctx->camera_id = 0;
    ctx->frame_w = IMG_W;
    ctx->frame_h = IMG_H;
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
