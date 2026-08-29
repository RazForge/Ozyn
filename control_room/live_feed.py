#!/usr/bin/env python3
"""
Ozayn Camera Viewer — shows live camera feed in a window.
Run directly in your terminal:  python3 live_feed.py
"""
import tkinter as tk
from PIL import Image, ImageTk
import subprocess, threading, struct, os, sys

W, H = 640, 480

class Viewer:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Ozayn Camera")
        self.root.configure(bg='black')
        self.root.resizable(False, False)

        self.canvas = tk.Canvas(self.root, width=W, height=H, bg='#111')
        self.canvas.pack()
        self.info = tk.Label(self.root, text="Starting camera...", fg='#0f0', bg='#111',
                             font=('monospace', 11), anchor='w')
        self.info.pack(fill=tk.X, padx=4, pady=2)

        self.running = True
        self.frames = 0
        self.proc = None

        self.root.protocol("WM_DELETE_WINDOW", self.stop)
        self.root.bind('<q>', lambda e: self.stop())
        self.root.bind('<Escape>', lambda e: self.stop())

        self.start_cam()
        self.root.mainloop()

    def start_cam(self):
        # Start C camera capture process
        try:
            self.proc = subprocess.Popen(
                ['/tmp/ozayn_cap'],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
        except FileNotFoundError:
            self.info.config(text="ERROR: /tmp/ozayn_cap not found. Run: gcc -O2 -o /tmp/ozayn_cap viewer_capture.c")
            return

        # Wait for camera ready
        def wait_ready():
            for line in self.proc.stderr:
                if b'CAMERA_READY' in line:
                    self.root.after(0, lambda: self.info.config(text="Camera ready — showing feed"))
                    self.read_frames()
                    break
                if b'ERR' in line:
                    self.root.after(0, lambda: self.info.config(text=f"Camera error: {line.decode().strip()}"))
                    break

        threading.Thread(target=wait_ready, daemon=True).start()

    def read_frames(self):
        frame_size = W * H * 2
        while self.running:
            marker = self.proc.stdout.read(1)
            if not marker or marker != b'F':
                break
            data = b''
            while len(data) < frame_size and self.running:
                chunk = self.proc.stdout.read(frame_size - len(data))
                if not chunk:
                    break
                data += chunk
            if len(data) < frame_size:
                break
            self.frames += 1
            self.show_frame(data)

    def show_frame(self, yuyv):
        try:
            import numpy as np
            import cv2
            arr = np.frombuffer(yuyv, dtype=np.uint8).reshape((H, W, 2))
            bgr = cv2.cvtColor(arr, cv2.COLOR_YUV2BGR_YUYV)
            rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
            img = Image.fromarray(rgb)
            imgtk = ImageTk.PhotoImage(image=img)
            self.root.after(0, self._draw, imgtk)
        except Exception as e:
            self.root.after(0, lambda: self.info.config(text=f"Error: {e}"))

    def _draw(self, imgtk):
        self.canvas.imgtk = imgtk
        self.canvas.create_image(0, 0, anchor=tk.NW, image=imgtk)
        self.info.config(text=f"Frame: {self.frames} | Camera active | Q to quit")

    def stop(self):
        self.running = False
        if self.proc:
            self.proc.terminate()
        self.root.destroy()

if __name__ == "__main__":
    print("Opening camera window...")
    Viewer()
    print("Done")
