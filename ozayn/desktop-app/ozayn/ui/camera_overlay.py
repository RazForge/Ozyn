"""
Ozayn HUD Overlay — Stylized hand lines + face circles on dark background.
Crops camera to square for MediaPipe compatibility.
"""

import os
import math
import numpy as np

from PyQt6.QtWidgets import QWidget, QLabel
from PyQt6.QtCore import Qt, QTimer, pyqtSignal
from PyQt6.QtGui import QPainter, QColor, QPen, QBrush, QRadialGradient, QImage, QPixmap

from ozayn.gesture_engine import GestureEngine
from ozayn.skin_tracker import SkinHandTracker


class CameraOverlay(QWidget):
    """Floating HUD overlay with hand lines + face circles on dark background."""

    gesture_command = pyqtSignal(dict)

    _MODELS_DIR = os.path.expanduser("~/.ozayn/models")

    @classmethod
    def _ensure_models(cls):
        import urllib.request
        os.makedirs(cls._MODELS_DIR, exist_ok=True)
        models = {
            "hand_landmarker.task": "https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/latest/hand_landmarker.task",
            "face_landmarker.task": "https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/latest/face_landmarker.task",
        }
        for name, url in models.items():
            path = os.path.join(cls._MODELS_DIR, name)
            if not os.path.exists(path) or os.path.getsize(path) < 100000:
                try:
                    urllib.request.urlretrieve(url, path)
                except Exception:
                    pass

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
        self._face_landmarker = None
        self._gesture_engine = GestureEngine()
        self._skin_tracker = SkinHandTracker()
        self._frame_count = 0
        self._collapsed = False
        self._drag_pos = None

        self._hands_lms = []
        self._face_lms = None
        self._gesture = ""
        self._mode = "NORMAL"
        self._scan_y = 0

        self._mode_label = QLabel("NORMAL", self)
        self._mode_label.setStyleSheet(
            "color: #00e5ff; font-family: 'Courier New', monospace; "
            "font-size: 10px; background: rgba(0,6,18,0.7); "
            "border: 1px solid rgba(0,180,255,0.2); border-radius: 3px; "
            "padding: 2px 6px;"
        )
        self._mode_label.move(8, 8)
        self._mode_label.adjustSize()

        self._gesture_label = QLabel("", self)
        self._gesture_label.setStyleSheet(
            "color: #00e5ff; font-family: 'Courier New', monospace; "
            "font-size: 9px; background: rgba(0,6,18,0.7); "
            "border: 1px solid rgba(0,180,255,0.15); border-radius: 3px; "
            "padding: 2px 6px;"
        )
        self._gesture_label.move(8, 28)
        self._gesture_label.adjustSize()

        self._toggle_btn = QLabel("▼", self)
        self._toggle_btn.setStyleSheet(
            "color: rgba(0,180,255,0.5); font-size: 12px; background: transparent;"
        )
        self._toggle_btn.move(262, 8)
        self._toggle_btn.setCursor(Qt.CursorShape.PointingHandCursor)

        self._ensure_models()

    def start(self):
        try:
            import cv2
            import mediapipe as mp
        except ImportError:
            return

        self._camera = cv2.VideoCapture(0)
        if not self._camera.isOpened():
            return

        self._camera.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        self._camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
        self._camera.set(cv2.CAP_PROP_FPS, 30)
        self._camera.set(cv2.CAP_PROP_BUFFERSIZE, 1)

        BaseOptions = mp.tasks.BaseOptions
        RunningMode = mp.tasks.vision.RunningMode

        try:
            hand_path = os.path.join(self._MODELS_DIR, "hand_landmarker.task")
            if os.path.exists(hand_path):
                self._hand_landmarker = mp.tasks.vision.HandLandmarker.create_from_options(
                    mp.tasks.vision.HandLandmarkerOptions(
                        base_options=BaseOptions(model_asset_path=hand_path),
                        running_mode=RunningMode.IMAGE,
                        num_hands=2,
                        min_hand_detection_confidence=0.5,
                        min_tracking_confidence=0.5
                    ))
        except Exception:
            self._hand_landmarker = None

        try:
            face_path = os.path.join(self._MODELS_DIR, "face_landmarker.task")
            if os.path.exists(face_path):
                self._face_landmarker = mp.tasks.vision.FaceLandmarker.create_from_options(
                    mp.tasks.vision.FaceLandmarkerOptions(
                        base_options=BaseOptions(model_asset_path=face_path),
                        running_mode=RunningMode.IMAGE,
                        num_faces=1,
                        min_face_detection_confidence=0.5,
                        min_tracking_confidence=0.5
                    ))
        except Exception:
            self._face_landmarker = None

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._capture)
        self._timer.start(50)  # ~20fps (IMAGE mode is slower)
        self.show()

    def _capture(self):
        if not self._camera or not self._camera.isOpened():
            return

        self._camera.grab()
        ret, frame = self._camera.read()
        if not ret:
            return

        try:
            import cv2
            frame = cv2.flip(frame, 1)
            import mediapipe as mp

            self._frame_count += 1
            h, w = frame.shape[:2]

            # Crop to square center for MediaPipe
            size = min(h, w)
            x = (w - size) // 2
            y = (h - size) // 2
            crop = frame[y:y + size, x:x + size]

            rgb = cv2.cvtColor(crop, cv2.COLOR_BGR2RGB)
            mp_img = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)

            # Hand tracking — store ALL detected hands
            self._hands_lms = []
            if self._hand_landmarker:
                try:
                    result = self._hand_landmarker.detect(mp_img)
                    if result.hand_landmarks:
                        self._hands_lms = list(result.hand_landmarks)
                except Exception:
                    pass

            # Face tracking
            self._face_lms = None
            if self._face_landmarker:
                try:
                    result = self._face_landmarker.detect(mp_img)
                    if result.face_landmarks:
                        self._face_lms = result.face_landmarks[0]
                except Exception:
                    pass

            # Gesture engine — MediaPipe or skin tracker fallback
            cmd = {"cursor_x": None, "cursor_y": None}
            hand_detected = False

            if self._hands_lms:
                hand_detected = True
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
            else:
                # Fallback: skin color hand tracking
                sx, sy, _ = self._skin_tracker.detect(crop)
                if sx is not None:
                    hand_detected = True
                    try:
                        import pyautogui
                        screen_w, screen_h = pyautogui.size()
                        # Frame is already flipped, just map directly
                        cx = int(sx * screen_w)
                        cy = int(sy * screen_h)
                        pyautogui.moveTo(cx, cy, _pause=False)
                        cmd["cursor_x"] = cx
                        cmd["cursor_y"] = cy
                        cmd["gesture"] = "POINTER"
                        cmd["mode"] = "NORMAL"
                    except Exception:
                        pass
                else:
                    self._skin_tracker.reset()
                    self._gesture_engine.reset()

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
                self.gesture_command.emit(cmd)
            except Exception:
                pass

            ge = self._gesture_engine
            self._mode = ge.mode
            self._gesture = ge.gesture
            if ge.is_locked:
                self._gesture = "LOCKED"
            elif ge.is_dragging:
                self._gesture = "DRAG"

            self._mode_label.setText(self._mode)
            self._gesture_label.setText(self._gesture)
            self.update()

        except Exception:
            pass

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        w, h = self.width(), self.height()

        bg = QRadialGradient(w // 2, h // 2, w * 0.7)
        bg.setColorAt(0.0, QColor(0, 15, 35, 220))
        bg.setColorAt(0.6, QColor(0, 8, 20, 235))
        bg.setColorAt(1.0, QColor(0, 4, 12, 245))
        painter.fillRect(0, 0, w, h, QBrush(bg))

        border_pen = QPen(QColor(0, 180, 255, 60))
        border_pen.setWidth(1)
        painter.setPen(border_pen)
        painter.drawRoundedRect(0, 0, w, h, 10, 10)

        scan_pen = QPen(QColor(0, 180, 255, 12))
        scan_pen.setWidth(1)
        painter.setPen(scan_pen)
        for y in range(0, h, 4):
            painter.drawLine(0, y, w, y)

        self._scan_y = (self._scan_y + 2) % h
        scan2 = QPen(QColor(0, 180, 255, 30))
        scan2.setWidth(1)
        painter.setPen(scan2)
        painter.drawLine(0, self._scan_y, w, self._scan_y)

        if self._face_lms:
            self._draw_face_pattern(painter, w, h)

        if self._hands_lms:
            colors = [(0, 229, 255), (255, 180, 0)]
            for i, lms in enumerate(self._hands_lms):
                color = colors[i % len(colors)]
                self._draw_hand_skeleton(painter, lms, w, h, color)
        else:
            pen = QPen(QColor(0, 180, 255, 40))
            pen.setWidth(1)
            painter.setPen(pen)
            cx, cy = w // 2, h // 2
            painter.drawLine(cx - 15, cy, cx + 15, cy)
            painter.drawLine(cx, cy - 15, cx, cy + 15)

        painter.end()

    def _draw_hand_skeleton(self, painter, lms, w, h, color=(0, 229, 255)):
        if not lms:
            return

        connections = [
            (0,1),(1,2),(2,3),(3,4),
            (0,5),(5,6),(6,7),(7,8),
            (0,9),(9,10),(10,11),(11,12),
            (0,13),(13,14),(14,15),(15,16),
            (0,17),(17,18),(18,19),(19,20),
            (5,9),(9,13),(13,17)
        ]

        points = []
        for pt in lms:
            px = pt.x * w
            py = pt.y * h
            points.append((px, py))

        for a, b in connections:
            ax, ay = points[a]
            bx, by = points[b]
            glow_pen = QPen(QColor(color[0], color[1], color[2], 30))
            glow_pen.setWidth(3)
            painter.setPen(glow_pen)
            painter.drawLine(int(ax), int(ay), int(bx), int(by))
            line_pen = QPen(QColor(color[0], color[1], color[2], 180))
            line_pen.setWidth(1)
            painter.setPen(line_pen)
            painter.drawLine(int(ax), int(ay), int(bx), int(by))

        for i, (px, py) in enumerate(points):
            if i in [4, 8]:
                grad = QRadialGradient(px, py, 6)
                grad.setColorAt(0.0, QColor(255, 255, 255, 200))
                grad.setColorAt(0.3, QColor(color[0], color[1], color[2], 120))
                grad.setColorAt(1.0, QColor(color[0], color[1], color[2], 0))
                painter.setBrush(QBrush(grad))
                painter.setPen(Qt.PenStyle.NoPen)
                painter.drawEllipse(int(px) - 6, int(py) - 6, 12, 12)
            else:
                dot_pen = QPen(QColor(color[0], color[1], color[2], 150))
                dot_pen.setWidth(1)
                painter.setPen(dot_pen)
                painter.setBrush(QBrush(QColor(color[0], color[1], color[2], 100)))
                painter.drawEllipse(int(px) - 2, int(py) - 2, 4, 4)

        ix, iy = points[8]
        ch_pen = QPen(QColor(255, 255, 255, 160))
        ch_pen.setWidth(1)
        painter.setPen(ch_pen)
        painter.drawLine(int(ix) - 10, int(iy), int(ix) + 10, int(iy))
        painter.drawLine(int(ix), int(iy) - 10, int(ix), int(iy) + 10)
        painter.setBrush(Qt.BrushStyle.NoBrush)
        ring_pen = QPen(QColor(0, 229, 255, 80))
        ring_pen.setWidth(1)
        painter.setPen(ring_pen)
        painter.drawEllipse(int(ix) - 12, int(iy) - 12, 24, 24)

    def _draw_face_pattern(self, painter, w, h):
        lms = self._face_lms
        if not lms:
            return

        def to_px(idx):
            pt = lms[idx]
            return pt.x * w, pt.y * h

        painter.setPen(QPen(QColor(0, 180, 255, 40)))
        painter.setBrush(Qt.BrushStyle.NoBrush)
        forehead = to_px(10)
        chin = to_px(152)
        left_f = to_px(234)
        right_f = to_px(454)
        fw = abs(right_f[0] - left_f[0]) * 0.55
        fh = abs(chin[1] - forehead[1]) * 0.55
        fcx = (left_f[0] + right_f[0]) / 2
        fcy = (forehead[1] + chin[1]) / 2
        painter.drawEllipse(int(fcx - fw), int(fcy - fh), int(fw * 2), int(fh * 2))

        eye_pen = QPen(QColor(0, 229, 255, 80))
        eye_pen.setWidth(1)
        painter.setPen(eye_pen)
        for ein, eout, etop, ebot in [(133, 33, 159, 145), (362, 263, 386, 374)]:
            center = ((to_px(ein)[0] + to_px(eout)[0]) / 2,
                      (to_px(etop)[1] + to_px(ebot)[1]) / 2)
            rx = abs(to_px(eout)[0] - to_px(ein)[0]) * 0.6
            ry = abs(to_px(etop)[1] - to_px(ebot)[1]) * 0.6
            painter.drawEllipse(int(center[0] - rx), int(center[1] - ry),
                                int(rx * 2), int(ry * 2))

        painter.setPen(QPen(QColor(0, 229, 255, 60)))
        nose_tip = to_px(1)
        nose_top = to_px(10)
        painter.drawLine(int(nose_tip[0]), int(nose_tip[1]),
                         int(nose_top[0]), int(nose_top[1] + fh * 0.3))

        painter.setPen(QPen(QColor(0, 200, 255, 50)))
        ml = to_px(61)
        mr = to_px(291)
        mt = to_px(13)
        painter.drawLine(int(ml[0]), int(ml[1]), int(mr[0]), int(mr[1]))
        painter.drawLine(int(ml[0]), int(ml[1]), int(mt[0]), int(mt[1]))
        painter.drawLine(int(mt[0]), int(mt[1]), int(mr[0]), int(mr[1]))

    def stop(self):
        if self._timer and self._timer.isActive():
            self._timer.stop()
        if self._camera and self._camera.isOpened():
            self._camera.release()
            self._camera = None
        for lm in [self._hand_landmarker, self._face_landmarker]:
            if lm:
                try:
                    lm.close()
                except Exception:
                    pass
        self._hand_landmarker = None
        self._face_landmarker = None

    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            self._drag_pos = event.globalPosition().toPoint() - self.pos()

    def mouseMoveEvent(self, event):
        if self._drag_pos:
            self.move(event.globalPosition().toPoint() - self._drag_pos)

    def mouseReleaseEvent(self, event):
        self._drag_pos = None

    def mouseDoubleClickEvent(self, event):
        self._collapsed = not self._collapsed
        if self._collapsed:
            self.setFixedSize(280, 35)
            self._gesture_label.hide()
            self._toggle_btn.setText("▶")
        else:
            self.setFixedSize(280, 220)
            self._gesture_label.show()
            self._toggle_btn.setText("▼")
