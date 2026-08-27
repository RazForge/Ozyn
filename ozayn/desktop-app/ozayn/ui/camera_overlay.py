"""
Ozayn HUD Overlay — Gesture + face tracking visualization.
Shows hand skeleton lines and face detection circles on a dark background.
No real camera footage is displayed.
"""

import os
import math
import numpy as np

from PyQt6.QtWidgets import QWidget, QLabel
from PyQt6.QtCore import Qt, QTimer, pyqtSignal
from PyQt6.QtGui import QPainter, QColor, QPen, QBrush, QRadialGradient

from ozayn.gesture_engine import GestureEngine


class CameraOverlay(QWidget):
    """Floating HUD overlay with hand lines + face circles."""

    gesture_command = pyqtSignal(dict)
    toggle_keyboard = pyqtSignal()
    toggle_mode = pyqtSignal()

    _CASCADE_PATH = None
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
            if not os.path.exists(path):
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
        self._frame_count = 0
        self._collapsed = False
        self._drag_pos = None

        # Detection results (drawn by paintEvent)
        self._hand_lms = None
        self._face_lms = None
        self._face_score = 0.0
        self._gesture = ""
        self._mode = "NORMAL"
        self._locked = False

        # HUD labels
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

        self._face_label = QLabel("", self)
        self._face_label.setStyleSheet(
            "color: rgba(0,229,255,0.6); font-family: 'Courier New', monospace; "
            "font-size: 9px; background: rgba(0,6,18,0.5); "
            "border: 1px solid rgba(0,180,255,0.1); border-radius: 3px; "
            "padding: 2px 6px;"
        )
        self._face_label.move(8, 48)
        self._face_label.adjustSize()

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

        self._camera.set(cv2.CAP_PROP_FRAME_WIDTH, 320)
        self._camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 240)
        self._camera.set(cv2.CAP_PROP_FPS, 30)
        self._camera.set(cv2.CAP_PROP_BUFFERSIZE, 1)

        # Hand landmarker
        try:
            BaseOptions = mp.tasks.BaseOptions
            HandLandmarker = mp.tasks.vision.HandLandmarker
            HandLandmarkerOptions = mp.tasks.vision.HandLandmarkerOptions
            RunningMode = mp.tasks.vision.RunningMode
            hand_path = os.path.join(self._MODELS_DIR, "hand_landmarker.task")
            if os.path.exists(hand_path):
                opts = HandLandmarkerOptions(
                    base_options=BaseOptions(model_asset_path=hand_path),
                    running_mode=RunningMode.VIDEO,
                    num_hands=1,
                    min_hand_detection_confidence=0.5,
                    min_tracking_confidence=0.5
                )
                self._hand_landmarker = HandLandmarker.create_from_options(opts)
        except Exception:
            self._hand_landmarker = None

        # Face landmarker
        try:
            BaseOptions = mp.tasks.BaseOptions
            FaceLandmarker = mp.tasks.vision.FaceLandmarker
            FaceLandmarkerOptions = mp.tasks.vision.FaceLandmarkerOptions
            RunningMode = mp.tasks.vision.RunningMode
            face_path = os.path.join(self._MODELS_DIR, "face_landmarker.task")
            if os.path.exists(face_path):
                opts = FaceLandmarkerOptions(
                    base_options=BaseOptions(model_asset_path=face_path),
                    running_mode=RunningMode.VIDEO,
                    num_faces=1,
                    min_face_detection_confidence=0.5,
                    min_tracking_confidence=0.5
                )
                self._face_landmarker = FaceLandmarker.create_from_options(opts)
        except Exception:
            self._face_landmarker = None

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._capture)
        self._timer.start(33)
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
            import mediapipe as mp

            self._frame_count += 1
            h, w = frame.shape[:2]
            ts = self._frame_count * 33

            # Hand tracking
            self._hand_lms = None
            if self._hand_landmarker:
                try:
                    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                    mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
                    result = self._hand_landmarker.detect_for_video(mp_image, ts)
                    if result.hand_landmarks:
                        self._hand_lms = result.hand_landmarks[0]
                except Exception:
                    pass

            # Face tracking
            self._face_lms = None
            self._face_score = 0.0
            if self._face_landmarker:
                try:
                    if self._hand_lms is None:
                        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                        mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
                    result = self._face_landmarker.detect_for_video(mp_image, ts)
                    if result.face_landmarks:
                        self._face_lms = result.face_landmarks[0]
                        if result.face_blendshapes:
                            # Use overall face detection confidence
                            self._face_score = 0.95
                except Exception:
                    pass

            # Gesture engine
            cmd = {"cursor_x": None, "cursor_y": None}
            if self._hand_lms:
                try:
                    import pyautogui
                    screen_w, screen_h = pyautogui.size()
                    cmd = self._gesture_engine.process(self._hand_lms, screen_w, screen_h)

                    if cmd["cursor_x"] is not None:
                        pyautogui.moveTo(cmd["cursor_x"], cmd["cursor_y"], _pause=False)
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
            else:
                self._gesture_engine.reset()

            # Update labels
            ge = self._gesture_engine
            self._mode = ge.mode
            self._gesture = ge.gesture
            self._locked = ge.is_locked
            if ge.is_locked:
                self._gesture = "LOCKED"
            elif ge.is_dragging:
                self._gesture = "DRAG"

            self._mode_label.setText(self._mode)
            self._gesture_label.setText(self._gesture)

            face_txt = ""
            if self._face_lms:
                face_txt = "FACE: DETECTED"
            self._face_label.setText(face_txt)

            # Trigger repaint
            self.update()

        except Exception:
            pass

    def paintEvent(self, event):
        """Draw hand lines + face circles on dark background. No camera feed."""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        w, h = self.width(), self.height()

        # ── Dark background with radial glow ──
        bg = QRadialGradient(w // 2, h // 2, w * 0.7)
        bg.setColorAt(0.0, QColor(0, 15, 35, 220))
        bg.setColorAt(0.6, QColor(0, 8, 20, 235))
        bg.setColorAt(1.0, QColor(0, 4, 12, 245))
        painter.fillRect(0, 0, w, h, QBrush(bg))

        # ── Border glow ──
        border_pen = QPen(QColor(0, 180, 255, 60))
        border_pen.setWidth(1)
        painter.setPen(border_pen)
        painter.drawRoundedRect(0, 0, w, h, 10, 10)

        # ── Scan lines ──
        scan_pen = QPen(QColor(0, 180, 255, 12))
        scan_pen.setWidth(1)
        painter.setPen(scan_pen)
        for y in range(0, h, 4):
            painter.drawLine(0, y, w, y)

        # ── Draw face detection pattern ──
        if self._face_lms:
            self._draw_face_pattern(painter, w, h)

        # ── Draw hand skeleton ──
        if self._hand_lms:
            self._draw_hand_skeleton(painter, w, h)
        else:
            # No hand — draw idle crosshair
            pen = QPen(QColor(0, 180, 255, 40))
            pen.setWidth(1)
            painter.setPen(pen)
            cx, cy = w // 2, h // 2
            painter.drawLine(cx - 15, cy, cx + 15, cy)
            painter.drawLine(cx, cy - 15, cx, cy + 15)

        painter.end()

    def _draw_hand_skeleton(self, painter, w, h):
        """Draw hand landmark connections as stylized lines."""
        lms = self._hand_lms
        if not lms:
            return

        # Hand connections (mediapipe topology)
        connections = [
            (0,1),(1,2),(2,3),(3,4),     # Thumb
            (0,5),(5,6),(6,7),(7,8),     # Index
            (0,9),(9,10),(10,11),(11,12), # Middle
            (0,13),(13,14),(14,15),(15,16), # Ring
            (0,17),(17,18),(18,19),(19,20), # Pinky
            (5,9),(9,13),(13,17)          # Palm
        ]

        # Scale to widget coordinates (mirrored X)
        points = []
        for pt in lms:
            px = (1.0 - pt.x) * w
            py = pt.y * h
            points.append((px, py))

        # Draw connections — glow effect
        for a, b in connections:
            ax, ay = points[a]
            bx, by = points[b]

            # Outer glow
            glow_pen = QPen(QColor(0, 180, 255, 30))
            glow_pen.setWidth(3)
            painter.setPen(glow_pen)
            painter.drawLine(int(ax), int(ay), int(bx), int(by))

            # Inner line
            line_pen = QPen(QColor(0, 229, 255, 180))
            line_pen.setWidth(1)
            painter.setPen(line_pen)
            painter.drawLine(int(ax), int(ay), int(bx), int(by))

        # Draw joints
        for i, (px, py) in enumerate(points):
            if i in [4, 8]:  # Thumb tip, index tip — brighter
                # Glow
                grad = QRadialGradient(px, py, 6)
                grad.setColorAt(0.0, QColor(255, 255, 255, 200))
                grad.setColorAt(0.3, QColor(0, 229, 255, 120))
                grad.setColorAt(1.0, QColor(0, 180, 255, 0))
                painter.setBrush(QBrush(grad))
                painter.setPen(Qt.PenStyle.NoPen)
                painter.drawEllipse(int(px) - 6, int(py) - 6, 12, 12)
            else:
                dot_pen = QPen(QColor(0, 200, 255, 150))
                dot_pen.setWidth(1)
                painter.setPen(dot_pen)
                painter.setBrush(QBrush(QColor(0, 200, 255, 100)))
                painter.drawEllipse(int(px) - 2, int(py) - 2, 4, 4)

        # Index fingertip crosshair
        ix, iy = points[8]
        ch_pen = QPen(QColor(255, 255, 255, 160))
        ch_pen.setWidth(1)
        painter.setPen(ch_pen)
        painter.drawLine(int(ix) - 10, int(iy), int(ix) + 10, int(iy))
        painter.drawLine(int(ix), int(iy) - 10, int(ix), int(iy) + 10)
        # Crosshair ring
        painter.setBrush(Qt.BrushStyle.NoBrush)
        ring_pen = QPen(QColor(0, 229, 255, 80))
        ring_pen.setWidth(1)
        painter.setPen(ring_pen)
        painter.drawEllipse(int(ix) - 12, int(iy) - 12, 24, 24)

    def _draw_face_pattern(self, painter, w, h):
        """Draw face detection circles and structural lines."""
        lms = self._face_lms
        if not lms:
            return

        # Key face landmarks (mediapipe face mesh indices)
        NOSE_TIP = 1
        LEFT_EYE_INNER = 133
        LEFT_EYE_OUTER = 33
        LEFT_EYE_TOP = 159
        LEFT_EYE_BOTTOM = 145
        RIGHT_EYE_INNER = 362
        RIGHT_EYE_OUTER = 263
        RIGHT_EYE_TOP = 386
        RIGHT_EYE_BOTTOM = 374
        MOUTH_LEFT = 61
        MOUTH_RIGHT = 291
        MOUTH_TOP = 13
        MOUTH_BOTTOM = 14
        CHIN = 152
        FOREHEAD = 10
        LEFT_FACE = 234
        RIGHT_FACE = 454

        def to_px(idx):
            pt = lms[idx]
            return (1.0 - pt.x) * w, pt.y * h

        # Face oval outline
        oval_pen = QPen(QColor(0, 180, 255, 40))
        oval_pen.setWidth(1)
        painter.setPen(oval_pen)
        painter.setBrush(Qt.BrushStyle.NoBrush)
        forehead = to_px(FOREHEAD)
        chin = to_px(CHIN)
        left_f = to_px(LEFT_FACE)
        right_f = to_px(RIGHT_FACE)
        fw = abs(right_f[0] - left_f[0]) * 0.55
        fh = abs(chin[1] - forehead[1]) * 0.55
        fcx = (left_f[0] + right_f[0]) / 2
        fcy = (forehead[1] + chin[1]) / 2
        painter.drawEllipse(int(fcx - fw), int(fcy - fh), int(fw * 2), int(fh * 2))

        # Nose vertical line
        nose_pen = QPen(QColor(0, 229, 255, 60))
        nose_pen.setWidth(1)
        painter.setPen(nose_pen)
        nose_tip = to_px(NOSE_TIP)
        nose_top = to_px(FOREHEAD)
        painter.drawLine(int(nose_tip[0]), int(nose_tip[1]), int(nose_top[0]), int(nose_top[1] + fh * 0.3))

        # Eye circles
        eye_pen = QPen(QColor(0, 229, 255, 80))
        eye_pen.setWidth(1)
        painter.setPen(eye_pen)
        painter.setBrush(Qt.BrushStyle.NoBrush)

        # Left eye
        le_center = (
            (to_px(LEFT_EYE_INNER)[0] + to_px(LEFT_EYE_OUTER)[0]) / 2,
            (to_px(LEFT_EYE_TOP)[1] + to_px(LEFT_EYE_BOTTOM)[1]) / 2
        )
        le_rx = abs(to_px(LEFT_EYE_OUTER)[0] - to_px(LEFT_EYE_INNER)[0]) * 0.6
        le_ry = abs(to_px(LEFT_EYE_TOP)[1] - to_px(LEFT_EYE_BOTTOM)[1]) * 0.6
        painter.drawEllipse(int(le_center[0] - le_rx), int(le_center[1] - le_ry),
                            int(le_rx * 2), int(le_ry * 2))

        # Right eye
        re_center = (
            (to_px(RIGHT_EYE_INNER)[0] + to_px(RIGHT_EYE_OUTER)[0]) / 2,
            (to_px(RIGHT_EYE_TOP)[1] + to_px(RIGHT_EYE_BOTTOM)[1]) / 2
        )
        re_rx = abs(to_px(RIGHT_EYE_OUTER)[0] - to_px(RIGHT_EYE_INNER)[0]) * 0.6
        re_ry = abs(to_px(RIGHT_EYE_TOP)[1] - to_px(RIGHT_EYE_BOTTOM)[1]) * 0.6
        painter.drawEllipse(int(re_center[0] - re_rx), int(re_center[1] - re_ry),
                            int(re_rx * 2), int(re_ry * 2))

        # Mouth arc
        mouth_pen = QPen(QColor(0, 200, 255, 50))
        mouth_pen.setWidth(1)
        painter.setPen(mouth_pen)
        ml = to_px(MOUTH_LEFT)
        mr = to_px(MOUTH_RIGHT)
        mt = to_px(MOUTH_TOP)
        mb = to_px(MOUTH_BOTTOM)
        mcy = (mt[1] + mb[1]) / 2
        painter.drawLine(int(ml[0]), int(ml[1]), int(mr[0]), int(mr[1]))
        painter.drawLine(int(ml[0]), int(ml[1]), int(mt[0]), int(mt[1]))
        painter.drawLine(int(mt[0]), int(mt[1]), int(mr[0]), int(mr[1]))

        # Detection confidence dot at nose
        conf_pen = QPen(QColor(0, 255, 100, 120))
        conf_pen.setWidth(1)
        painter.setPen(conf_pen)
        painter.setBrush(QBrush(QColor(0, 255, 100, 60)))
        painter.drawEllipse(int(nose_tip[0]) - 3, int(nose_tip[1]) - 3, 6, 6)

    def stop(self):
        if self._timer and self._timer.isActive():
            self._timer.stop()
        if self._camera and self._camera.isOpened():
            self._camera.release()
            self._camera = None
        if self._hand_landmarker:
            try:
                self._hand_landmarker.close()
            except Exception:
                pass
            self._hand_landmarker = None
        if self._face_landmarker:
            try:
                self._face_landmarker.close()
            except Exception:
                pass
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
            self._face_label.hide()
            self._toggle_btn.setText("▶")
        else:
            self.setFixedSize(280, 220)
            self._gesture_label.show()
            self._face_label.show()
            self._toggle_btn.setText("▼")
