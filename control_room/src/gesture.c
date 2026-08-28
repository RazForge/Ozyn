/**
 * Ozayn Gesture Engine — Real camera tracking + mouse control.
 * V4L2 for camera, X11/XTest for mouse. Zero heavy deps.
 * Motion-based: only tracks when hand moves.
 */

#include "gesture.h"
#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#define IMG_W 640
#define IMG_H 480
#define BLUR_SIZE 15
#define MOTION_THRESH 12
#define MOTION_AREA_MIN 50
#define MOTION_FRAMES_NEEDED 2
#define SMOOTH_ALPHA 0.3f
#define DWELL_TIME_S 2.0f
#define DWELL_RESET_PX 25

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

    cam->fd = open(dev, O_RDWR | O_NONBLOCK);
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

    printf("[GESTURE] Camera opened: %s (%dx%d)\n", dev, IMG_W, IMG_H);
    return 0;
}

static int cam_read(v4l2_cam_t *cam, uint8_t *gray_out)
{
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(cam->fd, VIDIOC_DQBUF, &buf) < 0) return -1;

    /* YUYV → grayscale (extract Y channel, every other pixel) */
    for (int y = 0; y < IMG_H; y++) {
        for (int x = 0; x < IMG_W; x++) {
            int idx = (y * IMG_W + x) * 2;
            gray_out[y * IMG_W + x] = cam->buf[idx];
        }
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

/* ── Simple box blur ── */
static void blur_box(const uint8_t *src, uint8_t *dst, int w, int h, int r)
{
    int area = (2*r+1) * (2*r+1);
    for (int y = r; y < h-r; y++) {
        for (int x = r; x < w-r; x++) {
            int sum = 0;
            for (int dy = -r; dy <= r; dy++)
                for (int dx = -r; dx <= r; dx++)
                    sum += src[(y+dy)*w + (x+dx)];
            dst[y*w+x] = sum / area;
        }
    }
}

/* ── Skin color detection (YCrCb) ── */
static int is_skin_ycc(uint8_t y, uint8_t cr, uint8_t cb)
{
    return (cr > 100 && cr < 180 && cb > 60 && cb < 140 && y > 40);
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

static void x11_close(x11_ctx_t *x)
{
    if (x->dpy) XCloseDisplay(x->dpy);
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

    /* Allocate buffers */
    int npix = IMG_W * IMG_H;
    uint8_t *cur_gray  = malloc(npix);
    uint8_t *prev_gray = malloc(npix);
    uint8_t *blur_buf  = malloc(npix);
    memset(prev_gray, 0, npix);

    ctx->sx = 0.5f;
    ctx->sy = 0.5f;
    ctx->motion_frames = 0;

    printf("[GESTURE] Tracking active — move your hand\n");

    double dwell_start = 0;
    float  dwell_x = 0, dwell_y = 0;

    while (ctx->active) {
        /* Capture frame */
        if (cam_read(&cam, cur_gray) < 0) {
            usleep(33000);
            continue;
        }

        /* Blur to reduce noise */
        blur_box(cur_gray, blur_buf, IMG_W, IMG_H, BLUR_SIZE/2);

        /* ── Motion detection ── */
        int motion_pixels = 0;
        int motion_cx = 0, motion_cy = 0;

        for (int y = 5; y < IMG_H-5; y += 2) {
            for (int x = 5; x < IMG_W-5; x += 2) {
                int diff = abs(blur_buf[y*IMG_W+x] - prev_gray[y*IMG_W+x]);
                if (diff > MOTION_THRESH) {
                    motion_pixels++;
                    motion_cx += x;
                    motion_cy += y;
                }
            }
        }

        /* Copy current to prev */
        memcpy(prev_gray, blur_buf, npix);

        /* Track motion state */
        bool has_motion = false;
        if (motion_pixels > MOTION_AREA_MIN) {
            has_motion = true;
            ctx->motion_frames = ctx->motion_frames + 1;
            if (ctx->motion_frames > 15) ctx->motion_frames = 15;
        } else {
            ctx->motion_frames = ctx->motion_frames - 1;
            if (ctx->motion_frames < 0) ctx->motion_frames = 0;
        }

        bool motion_on = ctx->motion_frames >= MOTION_FRAMES_NEEDED;

        if (!motion_on) {
            /* Hand is STILL — hold cursor position, check dwell */
            if (ctx->has_pos) {
                double now = (double)time(NULL);
                float dx = ctx->sx * x11.root_w - dwell_x;
                float dy = ctx->sy * x11.root_h - dwell_y;
                float dist = sqrtf(dx*dx + dy*dy);

                if (dist > DWELL_RESET_PX || dwell_start == 0) {
                    dwell_x = ctx->sx * x11.root_w;
                    dwell_y = ctx->sy * x11.root_h;
                    dwell_start = now;
                    ctx->dwell_progress = 0;
                } else {
                    double elapsed = now - dwell_start;
                    ctx->dwell_progress = (float)(elapsed / DWELL_TIME_S);
                    if (ctx->dwell_progress > 1.0f) {
                        ctx->dwell_progress = 1.0f;
                        ctx->dwell_click = true;
                        x11_click(&x11);
                        printf("[GESTURE] DWELL CLICK!\n");
                        dwell_x = ctx->sx * x11.root_w;
                        dwell_y = ctx->sy * x11.root_h;
                        dwell_start = now;
                    }
                }
            }
            usleep(33000);
            continue;
        }

        /* ── Motion detected — find hand via skin color ── */
        int hand_cx = 0, hand_cy = 0, hand_count = 0;

        /* Re-read a color frame for skin detection */
        /* We use the motion centroid region to search for skin */
        int m_cx = motion_cx / (motion_pixels > 0 ? motion_pixels : 1);
        int m_cy = motion_cy / (motion_pixels > 0 ? motion_pixels : 1);

        /* Expand search region around motion centroid */
        int x0 = (m_cx - 100); if (x0 < 0) x0 = 0;
        int x1 = (m_cx + 100); if (x1 > IMG_W) x1 = IMG_W;
        int y0 = (m_cy - 100); if (y0 < 0) y0 = 0;
        int y1 = (m_cy + 100); if (y1 > IMG_H) y1 = IMG_H;

        /* Use grayscale intensity as proxy for skin detection */
        /* (In production: read YUYV color channel for proper YCrCb) */
        for (int y = y0; y < y1; y += 2) {
            for (int x = x0; x < x1; x += 2) {
                /* Simple heuristic: skin regions tend to be bright in Y channel */
                uint8_t val = cur_gray[y*IMG_W+x];
                if (val > 60 && val < 220) {
                    hand_cx += x;
                    hand_cy += y;
                    hand_count++;
                }
            }
        }

        if (hand_count < 20) {
            /* No clear hand region, use motion centroid */
            hand_cx = m_cx;
            hand_cy = m_cy;
            hand_count = 1;
        }

        /* Normalize to 0-1 (mirrored X for natural control) */
        float nx = 1.0f - (float)hand_cx / (float)(hand_count * IMG_W);
        float ny = (float)hand_cy / (float)(hand_count * IMG_H);

        /* Clamp */
        if (nx < 0) nx = 0; if (nx > 1) nx = 1;
        if (ny < 0) ny = 0; if (ny > 1) ny = 1;

        /* Smooth */
        if (!ctx->has_pos) {
            ctx->sx = nx;
            ctx->sy = ny;
            ctx->has_pos = true;
        } else {
            ctx->sx = ctx->sx * (1 - SMOOTH_ALPHA) + nx * SMOOTH_ALPHA;
            ctx->sy = ctx->sy * (1 - SMOOTH_ALPHA) + ny * SMOOTH_ALPHA;
        }

        /* Move mouse */
        int px = (int)(ctx->sx * x11.root_w);
        int py = (int)(ctx->sy * x11.root_h);
        x11_move(&x11, px, py);

        /* Reset dwell */
        dwell_x = (float)px;
        dwell_y = (float)py;
        dwell_start = (double)time(NULL);
        ctx->dwell_progress = 0;
        ctx->dwell_click = false;

        usleep(33000); /* ~30 FPS */
    }

    free(cur_gray);
    free(prev_gray);
    free(blur_buf);
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
