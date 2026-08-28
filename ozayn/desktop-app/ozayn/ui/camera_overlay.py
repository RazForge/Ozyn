"""
Ozayn Camera Overlay — Real camera footage + hand skeleton + gesture engine + dwell-to-click.
"""

import os
import math
import numpy as np

from PyQt6.QtWidgets import QWidget
from PyQt6.QtCore import Qt, QTimer, pyqtSignal
from PyQt6.QtGui import QPainter, QColor, QPen, QBrush, QRadialGradient, QImage, QPixmap

from ozayn.gesture_engine import GestureEngine
from ozayn.skin_tracker import SkinHandTracker


class CameraOverlay(QWidget):
    """Floating window: camera footage + hand skeleton + gesture control + dwell click."""

    gesture_command = pyqtSignal(dict)

    _MODELS_DIR = os.path.expanduser("~/.ozayn/models")

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint |
            Qt.WindowType.WindowStaysOnTopHint |
            Qt.WindowType.Tool
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.setFixedSize(280, 220)

        self._camera = None
        self._timer = None
        self._hand_landmarker = None
        self._gesture_engine = GestureEngine()
        self._skin_tracker = SkinHandTracker()
        self._hands_lms = []
        self._pixmap = None
        self._frame_count = 0

        # Dwell-to-click for skin tracker (non-MediaPipe path)
        self._skin_dwell_x = None
        self._skin_dwell_y = None
        self._skin_dwell_time = 0.0

        # Skin tracker velocity for gear system
        self._prev_skin_x = None
        self._prev_skin_y = None
        self._skin_vel = 0.0

    def start(self):
        import cv2
        try:
            import mediapipe as mp
            hand_path = os.path.join(self._MODELS_DIR, "hand_landmarker.task")
            if os.path.exists(hand_path):
                BaseOptions = mp.tasks.BaseOptions
                RunningMode = mp.tasks.vision.RunningMode
                self._hand_landmarker = mp.tasks.vision.HandLandmarker.create_from_options(
                    mp.tasks.vision.HandLandmarkerOptions(
                        base_options=BaseOptions(model_asset_path=hand_path),
                        running_mode=RunningMode.IMAGE,
                        num_hands=2,
                        min_hand_detection_confidence=0.5,
                        min_tracking_confidence=0.5,
                    ))
        except Exception:
            self._hand_landmarker = None

        self._camera = cv2.VideoCapture(0)
        if not self._camera.isOpened():
            return

        self._camera.set(cv2.CAP_PROP_FRAME_WIDTH, 320)
        self._camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 240)
        self._camera.set(cv2.CAP_PROP_FPS, 30)
        self._camera.set(cv2.CAP_PROP_BUFFERSIZE, 1)

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._capture)
        self._timer.start(33)
        self.show()

    def stop(self):
        if self._timer:
            self._timer.stop()
        if self._camera:
            self._camera.release()
            self._camera = None

    def _capture(self):
        import cv2

        if not self._camera or not self._camera.isOpened():
            return

        ret, frame = self._camera.read()
        if not ret:
            return

        self._frame_count += 1
        frame = cv2.flip(frame, 1)
        h, w = frame.shape[:2]

        # ── MediaPipe hand detection ──
        self._hands_lms = []
        if self._hand_landmarker:
            try:
                import mediapipe as mp
                rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                mp_img = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
                result = self._hand_landmarker.detect(mp_img)
                if result.hand_landmarks:
                    self._hands_lms = list(result.hand_landmarks)
            except Exception:
                pass

        # ── Draw on real footage ──
        overlay = frame.copy()

        # ── Gesture engine command ──
        cmd = {
            "cursor_x": None, "cursor_y": None,
            "click": False, "right_click": False,
            "drag_start": False, "drag_end": False,
            "scroll_delta": 0, "zoom_delta": 0,
            "swipe": None, "gesture": "", "mode": "NORMAL",
            "locked": False,
            "dwell_progress": 0.0, "dwell_click": False, "dwell_active": False,
            "cursor_gear": 2,
        }

        import time
        now = time.time()

        if self._hands_lms:
            # ── Use gesture engine (geometric pipeline) ──
            try:
                import pyautogui
                screen_w, screen_h = pyautogui.size()
                if len(self._hands_lms) >= 2:
                    cmd = self._gesture_engine.process_two_hands(
                        self._hands_lms[0], self._hands_lms[1], screen_w, screen_h)
                else:
                    cmd = self._gesture_engine.process(self._hands_lms[0], screen_w, screen_h)
            except Exception:
                pass

            # Draw hand skeletons
            colors = [(0, 255, 200), (255, 180, 0)]
            for i, lms in enumerate(self._hands_lms):
                color = colors[i % len(colors)]
                self._draw_hand(overlay, lms, w, h, color)
        else:
            # ── Skin tracker fallback with velocity-based gear + dwell ──
            sx, sy, contour = self._skin_tracker.detect(frame)
            if contour is not None:
                cv2.drawContours(overlay, [contour], -1, (0, 255, 200), 2)

                try:
                    import pyautogui
                    screen_w, screen_h = pyautogui.size()
                    cx = int(sx * screen_w)
                    cy = int(sy * screen_h)

                    # Compute velocity
                    if self._prev_skin_x is not None:
                        dx = cx - self._prev_skin_x
                        dy = cy - self._prev_skin_y
                        self._skin_vel = math.sqrt(dx * dx + dy * dy)
                    self._prev_skin_x = cx
                    self._prev_skin_y = cy

                    # Gear from velocity
                    vel = self._skin_vel
                    if vel < 3:
                        gear = 0  # STOP
                        gear_name = "STOP"
                    elif vel < 30:
                        gear = 1  # PRECISION
                        gear_name = "PRECISION"
                    elif vel < 150:
                        gear = 2  # NORMAL
                        gear_name = "NORMAL"
                    elif vel < 500:
                        gear = 3  # FAST
                        gear_name = "FAST"
                    else:
                        gear = 4  # TURBO
                        gear_name = "TURBO"

                    # Apply gear multiplier
                    multipliers = [0.0, 0.3, 1.0, 2.0, 3.5]
                    # Direct position mapping (not velocity-based for skin tracker)
                    pyautogui.moveTo(cx, cy, _pause=False)
                    cmd["cursor_x"] = cx
                    cmd["cursor_y"] = cy
                    cmd["gesture"] = "SKIN"
                    cmd["mode"] = gear_name
                    cmd["cursor_gear"] = gear

                    # Dwell-to-click for skin tracker
                    if self._skin_dwell_time == 0:
                        self._skin_dwell_x = cx
                        self._skin_dwell_y = cy
                        self._skin_dwell_time = now
                    else:
                        ddx = cx - self._skin_dwell_x
                        ddy = cy - self._skin_dwell_y
                        dist = math.sqrt(ddx * ddx + ddy * ddy)
                        if dist > 15:
                            self._skin_dwell_x = cx
                            self._skin_dwell_y = cy
                            self._skin_dwell_time = now
                        else:
                            elapsed = now - self._skin_dwell_time
                            progress = min(1.0, elapsed / 2.0)
                            cmd["dwell_progress"] = progress
                            cmd["dwell_active"] = progress > 0.15
                            if progress >= 1.0:
                                pyautogui.click(_pause=False)
                                cmd["dwell_click"] = True
                                cmd["click"] = True
                                self._skin_dwell_x = cx
                                self._skin_dwell_y = cy
                                self._skin_dwell_time = now

                except Exception:
                    pass
            else:
                self._skin_tracker.reset()
                self._prev_skin_x = None
                self._prev_skin_y = None
                self._skin_vel = 0.0
                self._skin_dwell_time = 0

        # ── Execute pyautogui commands from gesture engine ──
        try:
            import pyautogui
            if cmd["click"]:
                pyautogui.click(_pause=False)
            if cmd["right_click"]:
                pyautogui.rightClick(_pause=False)
            if cmd.get("drag_start"):
                pyautogui.mouseDown(_pause=False)
            if cmd.get("drag_end"):
                pyautogui.mouseUp(_pause=False)
            if cmd.get("scroll_delta", 0) != 0:
                pyautogui.scroll(-cmd["scroll_delta"], _pause=False)
            if cmd.get("zoom_delta", 0) != 0:
                pyautogui.keyDown("ctrl", _pause=False)
                pyautogui.scroll(-cmd["zoom_delta"] // 10, _pause=False)
                pyautogui.keyUp("ctrl", _pause=False)
            if cmd.get("dwell_click"):
                pyautogui.click(_pause=False)
        except Exception:
            pass

        self.gesture_command.emit(cmd)

        # ── Draw dwell thinking box on overlay ──
        if cmd.get("dwell_active") and cmd["cursor_x"] is not None:
            # Map screen cursor back to overlay coordinates
            try:
                import pyautogui
                screen_w, screen_h = pyautogui.size()
                bx = int(cmd["cursor_x"] / screen_w * w)
                by = int(cmd["cursor_y"] / screen_h * h)
                progress = cmd["dwell_progress"]
                # Arc from 0 to 360*progress
                color_interp = int(255 * progress)
                box_color = (0, 255, 200) if progress < 0.7 else (0, 200, 255)
                # Draw circle that fills up
                radius = 18
                cv2.circle(overlay, (bx, by), radius, box_color, 2)
                # Draw progress arc
                end_angle = int(360 * progress)
                cv2.ellipse(overlay, (bx, by), (radius, radius), -90, 0, end_angle, box_color, 3)
                # Draw center dot
                cv2.circle(overlay, (bx, by), 3, box_color, -1)
            except Exception:
                pass

        # ── Convert to QPixmap ──
        rgb = cv2.cvtColor(overlay, cv2.COLOR_BGR2RGB)
        hh, ww, ch = rgb.shape
        bytes_per_line = ch * ww
        qimg = QImage(rgb.data, ww, hh, bytes_per_line, QImage.Format.Format_RGB888)
        self._pixmap = QPixmap.fromImage(qimg)
        self.update()

    def _draw_hand(self, img, lms, w, h, color):
        import cv2
        connections = [
            (0, 1), (1, 2), (2, 3), (3, 4),
            (0, 5), (5, 6), (6, 7), (7, 8),
            (0, 9), (9, 10), (10, 11), (11, 12),
            (0, 13), (13, 14), (14, 15), (15, 16),
            (0, 17), (17, 18), (18, 19), (19, 20),
            (5, 9), (9, 13), (13, 17),
        ]
        for a, b in connections:
            ax, ay = int(lms[a].x * w), int(lms[a].y * h)
            bx, by = int(lms[b].x * w), int(lms[b].y * h)
            cv2.line(img, (ax, ay), (bx, by), color, 2)
        for i, pt in enumerate(lms):
            px, py = int(pt.x * w), int(pt.y * h)
            r = 4 if i in [4, 8] else 2
            c = (255, 255, 255) if i in [4, 8] else color
            cv2.circle(img, (px, py), r, c, -1)

    def paintEvent(self, event):
        if self._pixmap is None:
            return
        painter = QPainter(self)
        painter.drawPixmap(0, 0, self._pixmap.scaled(
            self.width(), self.height(),
            Qt.AspectRatioMode.IgnoreAspectRatio,
            Qt.TransformationMode.SmoothTransformation,
        ))
        painter.end()
