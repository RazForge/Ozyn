#!/usr/bin/env python3
"""Ozayn Camera Viewer — V4L2 + tkinter window. Guaranteed to show a window."""
import tkinter as tk
from PIL import Image, ImageTk
import subprocess
import struct
import threading
import sys

IMG_W, IMG_H = 640, 480

class CameraViewer:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Ozayn Camera")
        self.root.geometry(f"{IMG_W}x{IMG_H + 40}")
        self.root.configure(bg='black')
        self.root.protocol("WM_DELETE_WINDOW", self.quit)

        self.label = tk.Label(self.root, text="Starting camera...", fg='white', bg='black',
                              font=('Arial', 16))
        self.label.pack(side=tk.BOTTOM, fill=tk.X)

        self.canvas = tk.Canvas(self.root, width=IMG_W, height=IMG_H, bg='black')
        self.canvas.pack()

        self.running = True
        self.proc = None
        self.frame_count = 0

        self.start_camera()
        self.root.mainloop()

    def start_camera(self):
        # Compile tiny C helper
        c_code = '''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#define W 640
#define H 480
int main(void) {
    int fd = open("/dev/video0", O_RDWR);
    if (fd < 0) { fprintf(stderr, "CAM_ERR"); return 1; }
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = W;
    fmt.fmt.pix.height = H;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    ioctl(fd, VIDIOC_S_FMT, &fmt);
    struct v4l2_requestbuffers req = {0};
    req.count = 1; req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; req.memory = V4L2_MEMORY_MMAP;
    ioctl(fd, VIDIOC_REQBUFS, &req);
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; buf.memory = V4L2_MEMORY_MMAP; buf.index = 0;
    ioctl(fd, VIDIOC_QUERYBUF, &buf);
    void *mem = mmap(NULL, buf.length, PROT_READ, MAP_SHARED, fd, buf.m.offset);
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMON, &type);
    while (1) {
        ioctl(fd, VIDIOC_QBUF, &buf);
        fd_set fds; FD_ZERO(&fds); FD_SET(fd, &fds);
        struct timeval tv = {1, 0};
        if (select(fd+1, &fds, NULL, NULL, &tv) <= 0) continue;
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) continue;
        putchar('F'); fflush(stdout);
        fwrite(mem, 1, W * H * 2, stdout);
        fflush(stdout);
    }
    return 0;
}
'''
        with open("/tmp/ozayn_cap.c", "w") as f:
            f.write(c_code)

        import os
        os.system("gcc -O2 -o /tmp/ozayn_cap /tmp/ozayn_cap.c 2>/dev/null")

        self.proc = subprocess.Popen(["/tmp/ozayn_cap"],
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE)

        # Start frame reading thread
        t = threading.Thread(target=self.read_frames, daemon=True)
        t.start()

    def read_frames(self):
        frame_size = IMG_W * IMG_H * 2
        while self.running:
            marker = self.proc.stdout.read(1)
            if not marker or marker != b'F':
                break

            data = b''
            while len(data) < frame_size:
                chunk = self.proc.stdout.read(frame_size - len(data))
                if not chunk:
                    break
                data += chunk

            if len(data) < frame_size:
                break

            self.frame_count += 1
            self.update_image(data)

    def update_image(self, yuyv_data):
        try:
            import numpy as np
            import cv2

            yuyv = np.frombuffer(yuyv_data, dtype=np.uint8).reshape((IMG_H, IMG_W, 2))
            bgr = cv2.cvtColor(yuyv, cv2.COLOR_YUV2BGR_YUYV)
            rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)

            img = Image.fromarray(rgb)
            imgtk = ImageTk.PhotoImage(image=img)

            self.root.after(0, self._display, imgtk)
        except Exception as e:
            self.root.after(0, self._set_text, f"Error: {e}")

    def _display(self, imgtk):
        self.canvas.imgtk = imgtk
        self.canvas.create_image(0, 0, anchor=tk.NW, image=imgtk)
        self._set_text(f"Frame: {self.frame_count} | Camera active")

    def _set_text(self, text):
        self.label.config(text=text)

    def quit(self):
        self.running = False
        if self.proc:
            self.proc.terminate()
        self.root.destroy()

if __name__ == "__main__":
    print("[VIEW] Starting Ozayn Camera Viewer...")
    print("[VIEW] A window will appear showing your camera feed")
    CameraViewer()
    print("[VIEW] Done")
