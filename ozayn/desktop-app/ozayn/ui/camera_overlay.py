"""
Ozayn Camera Overlay — 3D hand avatar, no camera footage shown.
Camera runs internally for tracking only. Display shows glass 3D hand skeleton.
"""

import os
import math
import numpy as np

from PyQt6.QtWidgets import QWidget
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QPointF
from PyQt6.QtGui import QPainter, QColor, QPen, QBrush, QRadialGradient, QFont

from ozayn.gesture_engine import GestureEngine
from ozayn.hand_avatar import HandAvatar3D
from ozayn.skin_tracker import SkinHandTracker


class CameraOverlay(QWidget):
    """Floating window: 3D hand avatar + gesture info. No camera footage."""

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
        self.setFixedSize(320, 300)

        self._camera = None
        self._timer = None
        self._hand_landmarker = None
        self._gesture_engine = GestureEngine()
        self._skin_tracker = SkinHandTracker()
        self._avatar = HandAvatar3D()
        self._hands_lms = []
        self._frame_count = 0

        # State for display
        self._gesture = "NONE"
        self._mode = "NORMAL"
        self._dwell_progress = 0.0
        self._dwell_active = False
        self._dwell_click = False
        self._cursor_gear = 2
        self._confidence = 0.0
        self._hand_detected = False

        # Dwell state
        self._skin_dwell_x = 0
        self._skin_dwell_y = 0
        self._skin_dwell_time = 0.0

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
                        min_hand_detection_confidence=0.1,
                        min_tracking_confidence=0.1,
                    ))
        except Exception:
            self._hand_landmarker = None

        self._camera = cv2.VideoCapture(0)
        if not self._camera.isOpened():
            return

        self._camera.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        self._camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
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
        import cv2, time, pyautogui

        if not self._camera or not self._camera.isOpened():
            return

        ret, frame = self._camera.read()
        if not ret:
            return

        self._frame_count += 1
        frame = cv2.flip(frame, 1)

        # ── ONE SYSTEM: skin tracker → cursor ──
        sx, sy, area = self._skin_tracker.detect(frame)

        cmd = {
            "cursor_x": None, "cursor_y": None,
            "click": False, "right_click": False,
            "drag_start": False, "drag_end": False,
            "scroll_delta": 0, "zoom_delta": 0,
            "swipe": None, "gesture": "NONE", "mode": "NORMAL",
            "locked": False,
            "dwell_progress": 0.0, "dwell_click": False, "dwell_active": False,
            "cursor_gear": 2,
        }

        if sx is not None:
            sw, sh = pyautogui.size()
            cx = int(sx * sw)
            cy = int(sy * sh)

            pyautogui.moveTo(cx, cy, _pause=False)
            cmd["cursor_x"] = cx
            cmd["cursor_y"] = cy
            cmd["gesture"] = "TRACKING"
            cmd["mode"] = "NORMAL"

            # ── Dwell-to-click ──
            now = time.time()
            ddx = cx - self._skin_dwell_x
            ddy = cy - self._skin_dwell_y
            dist = math.sqrt(ddx * ddx + ddy * ddy)

            if dist > 25 or self._skin_dwell_time == 0:
                self._skin_dwell_x = cx
                self._skin_dwell_y = cy
                self._skin_dwell_time = now
            else:
                elapsed = now - self._skin_dwell_time
                progress = min(1.0, elapsed / 2.0)
                cmd["dwell_progress"] = progress
                cmd["dwell_active"] = progress > 0.1
                if progress >= 1.0:
                    pyautogui.click(_pause=False)
                    cmd["dwell_click"] = True
                    cmd["click"] = True
                    self._skin_dwell_x = cx
                    self._skin_dwell_y = cy
                    self._skin_dwell_time = now

            self._hand_detected = True
        else:
            self._hand_detected = False
            self._skin_dwell_time = 0

        # ── MediaPipe: gesture display only (runs every 3rd frame) ──
        self._hands_lms = []
        if self._hand_landmarker and self._frame_count % 3 == 0 and self._hand_detected:
            try:
                import mediapipe as mp
                rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                mp_img = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
                result = self._hand_landmarker.detect(mp_img)
                if result.hand_landmarks:
                    self._hands_lms = list(result.hand_landmarks)
            except Exception:
                pass

        self._gesture = cmd.get("gesture", "NONE")
        self._mode = cmd.get("mode", "NORMAL")
        self._dwell_progress = cmd.get("dwell_progress", 0.0)
        self._dwell_active = cmd.get("dwell_active", False)
        self._dwell_click = cmd.get("dwell_click", False)
        self._cursor_gear = cmd.get("cursor_gear", 2)

        self.gesture_command.emit(cmd)
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        w, h = self.width(), self.height()

        # ── Dark background ──
        bg = QRadialGradient(w // 2, h // 2, w * 0.7)
        bg.setColorAt(0.0, QColor(0, 12, 30, 230))
        bg.setColorAt(0.6, QColor(0, 8, 20, 240))
        bg.setColorAt(1.0, QColor(0, 4, 12, 245))
        painter.fillRect(0, 0, w, h, QBrush(bg))

        # ── Scan lines ──
        scan_pen = QPen(QColor(0, 180, 255, 8))
        scan_pen.setWidthF(0.5)
        painter.setPen(scan_pen)
        for y in range(0, h, 3):
            painter.drawLine(0, y, w, y)

        # ── Border ──
        border_pen = QPen(QColor(0, 180, 255, 50))
        border_pen.setWidthF(1)
        painter.setPen(border_pen)
        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.drawRoundedRect(0, 0, w, h, 8, 8)

        # ── 3D Hand Avatar ──
        if self._hands_lms:
            self._avatar.render(
                painter, self._hands_lms[0], w, h,
                gear=self._cursor_gear,
                dwell_progress=self._dwell_progress,
                dwell_active=self._dwell_active,
                gesture=self._gesture,
            )
            if len(self._hands_lms) > 1:
                self._avatar.render(
                    painter, self._hands_lms[1], w, h,
                    gear=self._cursor_gear,
                    dwell_progress=0.0,
                    dwell_active=False,
                    gesture="",
                )
        elif self._hand_detected:
            # Skin detected but MediaPipe missed — show simple tracking indicator
            pen = QPen(QColor(0, 255, 200, 120))
            pen.setWidth(2)
            painter.setPen(pen)
            painter.setBrush(Qt.BrushStyle.NoBrush)
            cx, cy = w // 2, h // 2
            painter.drawEllipse(QPointF(cx, cy), 30, 30)
            painter.drawEllipse(QPointF(cx, cy), 15, 15)
            # Dwell ring
            if self._dwell_active:
                angle = int(self._dwell_progress * 360)
                pen2 = QPen(QColor(255, 180, 0, 200))
                pen2.setWidth(3)
                painter.setPen(pen2)
                painter.drawArc(cx - 40, cy - 40, 80, 80, 90 * 16, -angle * 16)
        else:
            self._avatar._draw_idle_hand(painter, w, h)

        # ── Gesture info panel ──
        self._draw_info_panel(painter, w, h)

        painter.end()

    def _draw_info_panel(self, painter: QPainter, w: int, h: int):
        """Draw gesture state info at bottom of widget."""
        y_base = h - 60

        # Mode
        gear_colors = [
            QColor(100, 100, 100),
            QColor(0, 180, 255),
            QColor(0, 255, 200),
            QColor(255, 180, 0),
            QColor(255, 60, 60),
        ]
        gear_color = gear_colors[self._cursor_gear] if 0 <= self._cursor_gear < 5 else QColor(0, 255, 200)

        font = painter.font()
        font.setPointSize(9)
        font.setBold(True)
        painter.setFont(font)

        # Gesture
        painter.setPen(QPen(QColor(0, 200, 255, 200)))
        painter.drawText(QPointF(10, y_base), f"GESTURE: {self._gesture}")

        # Mode
        painter.setPen(QPen(gear_color, 200))
        painter.drawText(QPointF(10, y_base + 18), f"MODE: {self._mode}")

        # Dwell
        if self._dwell_active:
            progress_pct = int(self._dwell_progress * 100)
            color = QColor(255, 100, 50) if self._dwell_progress > 0.7 else QColor(0, 255, 200)
            painter.setPen(QPen(color))
            painter.drawText(QPointF(10, y_base + 36), f"CLICK IN: {progress_pct}%")
        else:
            painter.setPen(QPen(QColor(0, 180, 255, 100)))
            painter.drawText(QPointF(10, y_base + 36),
                           "HOLD STILL TO CLICK")

        # Hand status
        font.setBold(False)
        font.setPointSize(8)
        painter.setFont(font)
        painter.setPen(QPen(QColor(0, 180, 255, 60)))
        if self._hands_lms:
            status = "TRACKING: HAND"
        elif self._hand_detected:
            status = "TRACKING: SKIN"
        else:
            status = "TRACKING: SEARCHING"
        painter.drawText(QPointF(w - 120, y_base + 36), status)
