#!/bin/bash
# Run this in your terminal to see the camera feed
cd "$(dirname "$0")"
echo "Starting Ozayn Camera Viewer..."
echo "A window should appear showing your camera feed"
echo "Press Q or close window to quit"
echo ""
python3 -c "
import tkinter as tk
from PIL import Image, ImageTk
import subprocess, struct, threading, numpy as np, cv2, os

IMG_W, IMG_H = 640, 480

# Compile C camera helper
c_code = '''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <linux/videodev2.h>
#define W 640
#define H 480
int main(void) {
    int fd = open(\"/dev/video0\", O_RDWR);
    if (fd < 0) return 1;
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
        putchar(\\\"F\\\"); fflush(stdout);
        fwrite(mem, 1, W * H * 2, stdout);
        fflush(stdout);
    }
    return 0;
}
'''
os.system('gcc -O2 -o /tmp/ozayn_cap /tmp/ozayn_cap.c 2>/dev/null')

proc = subprocess.Popen(['/tmp/ozayn_cap'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

root = tk.Tk()
root.title('Ozayn Camera')
root.geometry(f'{IMG_W}x{IMG_H+40}')
root.configure(bg='black')
root.attributes('-topmost', True)

canvas = tk.Canvas(root, width=IMG_W, height=IMG_H, bg='black')
canvas.pack()
status = tk.Label(root, text='Starting camera...', fg='white', bg='black', font=('Arial', 12))
status.pack(side=tk.BOTTOM, fill=tk.X)

frame_count = [0]
running = [True]

def read_frames():
    frame_size = IMG_W * IMG_H * 2
    while running[0]:
        marker = proc.stdout.read(1)
        if not marker or marker != b'F': break
        data = b''
        while len(data) < frame_size:
            chunk = proc.stdout.read(frame_size - len(data))
            if not chunk: break
            data += chunk
        if len(data) < frame_size: break
        frame_count[0] += 1
        try:
            yuyv = np.frombuffer(data, dtype=np.uint8).reshape((IMG_H, IMG_W, 2))
            bgr = cv2.cvtColor(yuyv, cv2.COLOR_YUV2BGR_YUYV)
            rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
            img = Image.fromarray(rgb)
            imgtk = ImageTk.PhotoImage(image=img)
            root.after(0, lambda i=imgtk: display(i))
        except Exception as e:
            root.after(0, lambda e=e: status.config(text=f'Error: {e}'))

def display(imgtk):
    canvas.imgtk = imgtk
    canvas.create_image(0, 0, anchor=tk.NW, image=imgtk)
    status.config(text=f'Frame: {frame_count[0]} | Camera active | Press Q to quit')

def on_key(e):
    if e.char in ('q', 'Q') or e.keysym == 'Escape':
        quit()

def quit():
    running[0] = False
    proc.terminate()
    root.destroy()

root.bind('<Key>', on_key)
root.protocol('WM_DELETE_WINDOW', quit)

t = threading.Thread(target=read_frames, daemon=True)
t.start()

print('Window open! Show your hand to the camera.')
root.mainloop()
print('Done')
"
