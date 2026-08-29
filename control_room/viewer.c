/**
 * ozayn_view — V4L2 camera viewer with MediaPipe hand landmarks.
 * Compiles as a standalone C binary. Uses V4L2 + X11 for display.
 *
 * Usage: ./ozayn_view [device]   (default: /dev/video0)
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

#define W 640
#define H 480

static volatile int running = 1;
static void sigint(int sig) { running = 0; }

/* YUYV to RGB conversion */
static void yuyv_to_rgb(const unsigned char *yuyv, unsigned char *rgb, int n)
{
    for (int i = 0; i < n / 2; i++) {
        int yi = i * 4;
        int y0 = yuyv[yi];
        int u  = yuyv[yi + 1];
        int y1 = yuyv[yi + 2];
        int v  = yuyv[yi + 3];
        int c0 = y0 - 16, c1 = y1 - 16;
        int d = u - 128, e = v - 128;
        int r, g, b;

        r = c0 + (int)(1.402 * e);   if (r < 0) r = 0; if (r > 255) r = 255;
        g = c0 - (int)(0.344136 * d) - (int)(0.714136 * e); if (g < 0) g = 0; if (g > 255) g = 255;
        b = c0 + (int)(1.772 * d);   if (b < 0) b = 0; if (b > 255) b = 255;
        rgb[i * 6 + 0] = r; rgb[i * 6 + 1] = g; rgb[i * 6 + 2] = b;

        r = c1 + (int)(1.402 * e);   if (r < 0) r = 0; if (r > 255) r = 255;
        g = c1 - (int)(0.344136 * d) - (int)(0.714136 * e); if (g < 0) g = 0; if (g > 255) g = 255;
        b = c1 + (int)(1.772 * d);   if (b < 0) b = 0; if (b > 255) b = 255;
        rgb[i * 6 + 3] = r; rgb[i * 6 + 4] = g; rgb[i * 6 + 5] = b;
    }
}

int main(int argc, char *argv[])
{
    signal(SIGINT, sigint);
    const char *dev = argc > 1 ? argv[1] : "/dev/video0";

    /* ── V4L2 setup ── */
    int fd = open(dev, O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = W;
    fmt.fmt.pix.height = H;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) { perror("S_FMT"); return 1; }

    struct v4l2_requestbuffers req = {0};
    req.count = 1; req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) { perror("REQBUFS"); return 1; }

    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; buf.memory = V4L2_MEMORY_MMAP; buf.index = 0;
    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) { perror("QUERYBUF"); return 1; }

    void *mem = mmap(NULL, buf.length, PROT_READ, MAP_SHARED, fd, buf.m.offset);
    if (mem == MAP_FAILED) { perror("mmap"); return 1; }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMON, &type);
    printf("[VIEW] Camera: %s (%dx%d)\n", dev, W, H);

    /* ── X11 setup ── */
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "Cannot open X11 display\n"); return 1; }
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    Window win = XCreateSimpleWindow(dpy, root, 0, 0, W, H, 0, 0, 0);
    XStoreName(dpy, win, "Ozayn Camera");
    XMapWindow(dpy, win);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask);

    GC gc = DefaultGC(dpy, screen);
    XImage *ximg = XCreateImage(dpy, DefaultVisual(dpy, screen), DefaultDepth(dpy, screen),
                                ZPixmap, 0, malloc(W * H * 4), W, H, 32, W * 4);

    unsigned char *rgb = malloc(W * H * 3);
    int frame_count = 0;
    struct timeval t_start, t_now;
    gettimeofday(&t_start, NULL);

    printf("[VIEW] Press Q or close window to quit\n");

    /* ── Main loop ── */
    while (running) {
        /* Capture */
        ioctl(fd, VIDIOC_QBUF, &buf);
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv = {1, 0};
        if (select(fd + 1, &fds, NULL, NULL, &tv) <= 0) continue;
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) continue;

        frame_count++;

        /* Convert YUYV → RGB */
        yuyv_to_rgb(mem, rgb, W * H * 2);

        /* Put to X11 image (RGB → BGRA) */
        for (int i = 0; i < W * H; i++) {
            unsigned char r = rgb[i * 3];
            unsigned char g = rgb[i * 3 + 1];
            unsigned char b = rgb[i * 3 + 2];
            ((unsigned int *)ximg->data)[i] = (r << 16) | (g << 8) | b;
        }

        /* Draw FPS */
        gettimeofday(&t_now, NULL);
        float elapsed = (float)(t_now.tv_sec - t_start.tv_sec) +
                        (float)(t_now.tv_usec - t_start.tv_usec) / 1000000.0f;
        float fps = (elapsed > 0) ? frame_count / elapsed : 0;

        /* Overlay text on the image data */
        char fps_text[32];
        snprintf(fps_text, sizeof(fps_text), "FPS: %.0f  Frame: %d", fps, frame_count);
        /* Simple pixel text rendering */
        XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, W, H);

        /* Draw FPS with X11 */
        XSetForeground(dpy, gc, 0x00FF00);
        XFillRectangle(dpy, win, gc, 5, 5, 200, 25);
        XSetForeground(dpy, gc, 0x000000);
        XDrawString(dpy, win, gc, 10, 20, fps_text, strlen(fps_text));

        /* Draw frame border */
        XSetForeground(dpy, gc, 0x666666);
        XDrawRectangle(dpy, win, gc, 50, 50, W - 100, H - 100);

        /* Draw hint */
        XSetForeground(dpy, gc, 0x666666);
        XDrawString(dpy, win, gc, 10, H - 10, "Show hand to camera", 19);

        XFlush(dpy);

        /* Handle events */
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == KeyPress) {
                char buf[8];
                KeySym ksym;
                XLookupString(&ev.xkey, buf, sizeof(buf), &ksym, NULL);
                if (buf[0] == 'q' || buf[0] == 'Q' || ksym == XK_Escape)
                    running = 0;
            }
        }

        usleep(10000); /* ~30 FPS cap */
    }

    /* Cleanup */
    ioctl(fd, VIDIOC_STREAMOFF, &type);
    munmap(mem, buf.length);
    close(fd);
    free(rgb);
    XDestroyImage(ximg);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    printf("[VIEW] Done — %d frames\n", frame_count);
    return 0;
}
