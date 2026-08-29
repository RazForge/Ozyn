/**
 * ozayn_view — V4L2 camera viewer window.
 * Usage: ./ozayn_view [/dev/video0]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/videodev2.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#define W 640
#define H 480

static volatile int running = 1;
static void on_sig(int s) { (void)s; running = 0; }

int main(int argc, char *argv[])
{
    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);

    const char *dev = argc > 1 ? argv[1] : "/dev/video0";
    fprintf(stderr, "[VIEW] Opening camera %s...\n", dev);

    /* ── V4L2 ── */
    int fd = open(dev, O_RDWR);
    if (fd < 0) { perror("open camera"); return 1; }

    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = W;
    fmt.fmt.pix.height = H;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) { perror("S_FMT"); close(fd); return 1; }
    fprintf(stderr, "[VIEW] Camera format OK (%dx%d)\n", W, H);

    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) { perror("REQBUFS"); close(fd); return 1; }

    struct v4l2_buffer vbuf = {0};
    vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vbuf.memory = V4L2_MEMORY_MMAP;
    vbuf.index = 0;
    if (ioctl(fd, VIDIOC_QUERYBUF, &vbuf) < 0) { perror("QUERYBUF"); close(fd); return 1; }

    void *cam_mem = mmap(NULL, vbuf.length, PROT_READ, MAP_SHARED, fd, vbuf.m.offset);
    if (cam_mem == MAP_FAILED) { perror("mmap"); close(fd); return 1; }

    int vtype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &vtype) < 0) { perror("STREAMON"); close(fd); return 1; }
    fprintf(stderr, "[VIEW] Camera streaming\n");

    /* ── X11 ── */
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "Cannot open X11 display\n"); return 1; }

    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);

    /* Create window */
    XSetWindowAttributes attr;
    attr.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;

    Window win = XCreateWindow(dpy, root, 100, 100, W, H, 0,
                               DefaultDepth(dpy, scr), InputOutput,
                               DefaultVisual(dpy, scr),
                               CWEventMask, &attr);
    XStoreName(dpy, win, "Ozayn Camera");
    XMapWindow(dpy, win);

    /* Wait for map */
    XEvent ev;
    while (1) {
        XNextEvent(dpy, &ev);
        if (ev.type == MapNotify) break;
    }

    GC gc = XCreateGC(dpy, win, 0, NULL);
    fprintf(stderr, "[VIEW] X11 window created\n");

    /* Create XImage for pixel data */
    XImage *ximg = XCreateImage(dpy, DefaultVisual(dpy, scr), DefaultDepth(dpy, scr),
                                ZPixmap, 0, NULL, W, H, 32, 0);
    ximg->data = malloc(ximg->bytes_per_line * H);
    if (!ximg->data) { fprintf(stderr, "XImage alloc failed\n"); return 1; }

    unsigned char *yuyv_buf = malloc(W * H * 2);
    int frame_count = 0;
    struct timeval t0;
    gettimeofday(&t0, NULL);

    fprintf(stderr, "[VIEW] Running — press Q or close window to quit\n");

    /* ── Main loop ── */
    while (running) {
        /* Capture frame */
        ioctl(fd, VIDIOC_QBUF, &vbuf);
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv = {1, 0};
        if (select(fd + 1, &fds, NULL, NULL, &tv) <= 0) continue;
        if (ioctl(fd, VIDIOC_DQBUF, &vbuf) < 0) continue;

        frame_count++;
        memcpy(yuyv_buf, cam_mem, W * H * 2);

        /* YUYV → BGRA for X11 */
        unsigned int *dst = (unsigned int *)ximg->data;
        const unsigned char *src = yuyv_buf;
        for (int i = 0; i < W * H / 2; i++) {
            int yi = i * 4;
            int y0 = src[yi], y1 = src[yi + 2];
            int u  = src[yi + 1], v  = src[yi + 3];
            int c0 = y0 - 16, c1 = y1 - 16;
            int d = u - 128, e = v - 128;
            int r, g, b;

            r = c0 + (int)(1.402 * e);     if (r < 0) r = 0; if (r > 255) r = 255;
            g = c0 - (int)(0.344136 * d) - (int)(0.714136 * e);
            if (g < 0) g = 0; if (g > 255) g = 255;
            b = c0 + (int)(1.772 * d);     if (b < 0) b = 0; if (b > 255) b = 255;
            dst[i * 2] = 0xFF000000 | (r << 16) | (g << 8) | b;

            r = c1 + (int)(1.402 * e);     if (r < 0) r = 0; if (r > 255) r = 255;
            g = c1 - (int)(0.344136 * d) - (int)(0.714136 * e);
            if (g < 0) g = 0; if (g > 255) g = 255;
            b = c1 + (int)(1.772 * d);     if (b < 0) b = 0; if (b > 255) b = 255;
            dst[i * 2 + 1] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }

        /* Draw to window */
        XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, W, H);

        /* FPS overlay */
        struct timeval t1;
        gettimeofday(&t1, NULL);
        float elapsed = (float)(t1.tv_sec - t0.tv_sec) +
                        (float)(t1.tv_usec - t0.tv_usec) / 1000000.0f;
        float fps = (elapsed > 0) ? frame_count / elapsed : 0;
        char label[64];
        int len = snprintf(label, sizeof(label), "FPS: %.0f  Frame: %d", fps, frame_count);

        /* Draw text background */
        XSetForeground(dpy, gc, 0x00AA00);
        XFillRectangle(dpy, win, gc, 4, 4, len * 8 + 8, 20);
        XSetForeground(dpy, gc, 0x000000);
        XDrawString(dpy, win, gc, 8, 18, label, len);

        /* Hint */
        const char *hint = "Show hand to camera — Q to quit";
        int hlen = strlen(hint);
        XSetForeground(dpy, gc, 0x00AA00);
        XFillRectangle(dpy, win, gc, 4, H - 24, hlen * 8 + 8, 20);
        XSetForeground(dpy, gc, 0x000000);
        XDrawString(dpy, win, gc, 8, H - 8, hint, hlen);

        XFlush(dpy);

        /* Handle events */
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == KeyPress) {
                char kbuf[8];
                KeySym ksym;
                XLookupString(&ev.xkey, kbuf, sizeof(kbuf), &ksym, NULL);
                if (kbuf[0] == 'q' || kbuf[0] == 'Q' || ksym == XK_Escape)
                    running = 0;
            }
            if (ev.type == DestroyNotify)
                running = 0;
        }

        usleep(16000); /* ~60 FPS cap */
    }

    /* Cleanup */
    ioctl(fd, VIDIOC_STREAMOFF, &vtype);
    munmap(cam_mem, vbuf.length);
    close(fd);
    free(yuyv_buf);
    XDestroyImage(ximg);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    fprintf(stderr, "[VIEW] Done — %d frames\n", frame_count);
    return 0;
}
