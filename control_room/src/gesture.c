/**
 * Ozayn Gesture Engine — Full pipeline.
 *
 * V4L2 camera → RGB → Python MediaPipe → 21 landmarks × 2 hands
 * → hand_data → gesture_classifier → gesture_state → command
 * → cursor_engine + X11 mouse actions
 */

#include "gesture.h"
#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#define IMG_W 640
#define IMG_H 480

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

    /* YUYV → RGB */
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

static void x11_click(x11_ctx_t *x, unsigned int button)
{
    XTestFakeButtonEvent(x->dpy, button, True, CurrentTime);
    XTestFakeButtonEvent(x->dpy, button, False, CurrentTime);
    XFlush(x->dpy);
}

static void x11_close(x11_ctx_t *x)
{
    if (x->dpy) XCloseDisplay(x->dpy);
}

/* ── Python hand tracker subprocess ── */
typedef struct {
    pid_t pid;
    int   stdin_fd;
    int   stdout_fd;
    bool  running;
} tracker_proc_t;

static int tracker_start(tracker_proc_t *t, const char *script, const char *model)
{
    int stdin_pipe[2], stdout_pipe[2];
    pipe(stdin_pipe);
    pipe(stdout_pipe);

    t->pid = fork();
    if (t->pid == 0) {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        execlp("python3", "python3", script, model, NULL);
        perror("execlp hand_tracker");
        _exit(1);
    }

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    t->stdin_fd  = stdin_pipe[1];
    t->stdout_fd = stdout_pipe[0];
    t->running   = true;

    printf("[GESTURE] Hand tracker started (pid %d)\n", t->pid);
    usleep(800000); /* Wait for Python model load */
    return 0;
}

static int tracker_send_frame(tracker_proc_t *t, const uint8_t *rgb, int w, int h)
{
    uint8_t header[5] = { 'F', w & 0xFF, (w >> 8) & 0xFF, h & 0xFF, (h >> 8) & 0xFF };
    if (write(t->stdin_fd, header, 5) != 5) return -1;
    if (write(t->stdin_fd, rgb, w * h * 3) != w * h * 3) return -1;
    return 0;
}

static int tracker_read_hands(tracker_proc_t *t, hand_system_t *sys)
{
    uint8_t tag;

    /* Wait for response with timeout */
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(t->stdout_fd, &fds);
    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    if (select(t->stdout_fd + 1, &fds, NULL, NULL, &tv) <= 0) {
        sys->num_hands = 0;
        return 0;
    }
    if (read(t->stdout_fd, &tag, 1) != 1) { sys->num_hands = 0; return -1; }

    if (tag == 'N') {
        sys->num_hands = 0;
        return 0;
    }
    if (tag != 'D') { sys->num_hands = 0; return -1; }

    /* Read num_hands */
    uint8_t num_hands;
    if (read(t->stdout_fd, &num_hands, 1) != 1) { sys->num_hands = 0; return -1; }
    num_hands = (num_hands > 2) ? 2 : num_hands;
    sys->num_hands = num_hands;

    for (int h = 0; h < num_hands; h++) {
        hand_state_t *hs = &sys->hands[h];

        /* Read handedness */
        uint8_t handedness;
        if (read(t->stdout_fd, &handedness, 1) != 1) { hs->valid = false; continue; }

        /* Read 21 landmarks (x,y pairs = 42 uint16 = 84 bytes) */
        uint8_t lm_buf[84];
        int total = 0;
        while (total < 84) {
            FD_ZERO(&fds);
            FD_SET(t->stdout_fd, &fds);
            struct timeval tv2 = { .tv_sec = 2, .tv_usec = 0 };
            if (select(t->stdout_fd + 1, &fds, NULL, NULL, &tv2) <= 0) break;
            int n = read(t->stdout_fd, lm_buf + total, 84 - total);
            if (n <= 0) break;
            total += n;
        }
        if (total < 84) { hs->valid = false; continue; }

        /* Read fingers_up */
        uint8_t fingers_up;
        if (read(t->stdout_fd, &fingers_up, 1) != 1) { hs->valid = false; continue; }

        /* Convert to hand_state */
        uint16_t raw_xy[42];
        for (int i = 0; i < 42; i++)
            raw_xy[i] = lm_buf[i * 2] | (lm_buf[i * 2 + 1] << 8);

        hand_state_from_raw(hs, raw_xy, fingers_up, handedness);
    }

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

/* ── Execute command via X11 ── */
static void execute_command(gesture_ctx_t *ctx, command_t cmd, x11_ctx_t *x11)
{
    /* Only execute on gesture edges (new confirmation) */
    bool is_new = (cmd.type != ctx->prev_cmd.type) ||
                  (cmd.type != CMD_MOVE_CURSOR && cmd.type != CMD_NONE);

    switch (cmd.type) {
    case CMD_MOVE_CURSOR:
        if (ctx->hand_sys.num_hands > 0) {
            hand_state_t *h = &ctx->hand_sys.hands[0];
            cursor_update(&ctx->cursor, h->palm.center.x, h->palm.center.y,
                          h->vel_x, h->vel_y);
            x11_move(x11, cursor_px(&ctx->cursor), cursor_py(&ctx->cursor));
        }
        break;
    case CMD_LEFT_CLICK:
        if (is_new) {
            x11_click(x11, Button1);
            printf("[GESTURE] LEFT CLICK\n");
        }
        break;
    case CMD_RIGHT_CLICK:
        if (is_new) {
            x11_click(x11, Button3);
            printf("[GESTURE] RIGHT CLICK\n");
        }
        break;
    case CMD_LOCK:
        if (is_new) {
            ctx->cursor.locked = true;
            printf("[GESTURE] LOCKED\n");
        }
        break;
    case CMD_UNLOCK:
        if (is_new) {
            ctx->cursor.locked = false;
            printf("[GESTURE] UNLOCKED\n");
        }
        break;
    case CMD_CONFIRM:
        if (is_new)
            printf("[GESTURE] CONFIRMED (thumb up)\n");
        break;
    case CMD_CANCEL:
        if (is_new)
            printf("[GESTURE] CANCELLED (thumb down)\n");
        break;
    default:
        break;
    }

    ctx->prev_cmd = cmd;
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

    /* Start Python hand tracker */
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

    /* Init pipeline components */
    hand_system_init(&ctx->hand_sys);
    gesture_state_init(&ctx->gesture_state);
    cursor_init(&ctx->cursor, x11.root_w, x11.root_h);
    ctx->prev_cmd.type = CMD_NONE;

    uint8_t *rgb_buf = malloc(IMG_W * IMG_H * 3);
    struct timeval frame_start, frame_end;
    int log_count = 0;

    printf("[GESTURE] Tracking active — show your hand\n");

    while (ctx->active) {
        gettimeofday(&frame_start, NULL);

        /* Capture RGB */
        if (cam_read_rgb(&cam, rgb_buf) < 0) {
            usleep(33000);
            continue;
        }

        /* Send to Python */
        if (tracker_send_frame(&tracker, rgb_buf, IMG_W, IMG_H) < 0) {
            printf("[GESTURE] SEND FAILED frame %d\n", ctx->frame_count);
            usleep(33000);
            continue;
        }

        /* Read hand landmarks */
        if (tracker_read_hands(&tracker, &ctx->hand_sys) < 0) {
            printf("[GESTURE] READ FAILED frame %d\n", ctx->frame_count);
            usleep(33000);
            continue;
        }

        ctx->frame_count++;

        /* Update pipeline: landmarks → features */
        hand_system_update(&ctx->hand_sys);

        /* Classify gesture */
        gesture_result_t gresult;
        if (ctx->hand_sys.num_hands >= 2)
            gresult = gesture_classify_two_hands(&ctx->hand_sys);
        else if (ctx->hand_sys.num_hands == 1)
            gresult = gesture_classify_single(&ctx->hand_sys.hands[0]);
        else
            gresult.type = GESTURE_NONE;

        /* Compute frame dt */
        gettimeofday(&frame_end, NULL);
        float dt = (float)(frame_end.tv_sec - frame_start.tv_sec) +
                   (float)(frame_end.tv_usec - frame_start.tv_usec) / 1000000.0f;

        /* Update state machine */
        gesture_state_update(&ctx->gesture_state, gresult, dt);

        /* Map to command */
        command_t cmd = gesture_to_command(&ctx->gesture_state);

        /* Execute command */
        execute_command(ctx, cmd, &x11);

        /* Log every 60 frames (~2s) */
        log_count++;
        if (log_count % 60 == 0) {
            if (ctx->hand_sys.num_hands > 0) {
                hand_state_t *h = &ctx->hand_sys.hands[0];
                uint8_t fu = 0;
                if (h->fingers[FINGER_THUMB].extended)  fu |= 0x01;
                if (h->fingers[FINGER_INDEX].extended)  fu |= 0x02;
                if (h->fingers[FINGER_MIDDLE].extended) fu |= 0x04;
                if (h->fingers[FINGER_RING].extended)   fu |= 0x08;
                if (h->fingers[FINGER_PINKY].extended)  fu |= 0x10;
                printf("[GESTURE] hands=%d fingers=0x%02x gesture=%s cmd=%s speed=%.3f pinch=%.3f palm=(%.2f,%.2f)\n",
                       ctx->hand_sys.num_hands, fu,
                       gesture_name(ctx->gesture_state.current),
                       command_name(cmd.type),
                       h->speed, h->thumb_index_dist,
                       h->palm.center.x, h->palm.center.y);
            } else {
                printf("[GESTURE] hands=0 gesture=%s\n",
                       gesture_name(ctx->gesture_state.current));
            }
        }
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
