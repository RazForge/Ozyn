"""
Ozayn Camera Overlay — Persistent camera + gesture control.
Stays on top of all windows after login. Picture-in-picture style.
"""

import os
import math
import numpy as np

from PyQt6.QtWidgets import QWidget, QLabel, QVBoxLayout
from PyQt6.QtCore import Qt, QTimer, pyqtSignal
from PyQt6.QtGui import QPixmap, QImage, QColor

from ozayn.gesture_engine import GestureEngine


class CameraOverlay(QWidget):
    """Floating camera overlay with gesture control. Persists after login."""

    # Signals for gesture commands
    gesture_command = pyqtSignal(dict)  # emits full command dict
    toggle_keyboard = pyqtSignal()
    toggle_mode = pyqtSignal()

    _CASCADE_PATH = None
    _MODELS_DIR = os.path.expanduser("~/.ozayn/models")

    @classmethod
    def _find_cascade(cls):
        if cls._CASCADE_PATH:
            return cls._CASCADE_PATH
        try:
            import cv2
            path = os.path.join(cv2.data.haarcascades, "haarcascade_frontalface_default.xml")
            if os.path.isfile(path):
                cls._CASCADE_PATH = path
                return path
        except (AttributeError, Exception):
            pass
        return None

    @classmethod
    def _ensure_models(cls):
        import urllib.request
        os.makedirs(cls._MODELS_DIR, exist_ok=True)
        models = {
            "hand_landmarker.task": "https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/latest/hand_landmarker.task",
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
        self.setFixedSize(240, 180)

        self._camera = None
        self._timer = None
        self._hand_landmarker = None
        self._gesture_engine = GestureEngine()
        self._frame_ref = None
        self._frame_count = 0
        self._collapsed = False
        self._drag_pos = None

        # Feed label
        self._feed = QLabel(self)
        self._feed.setFixedSize(240, 180)
        self._feed.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._feed.setStyleSheet("""
            QLabel {
                background: rgba(0,6,18,0.85);
                border: 1px solid rgba(0,180,255,0.3);
                border-radius: 10px;
            }
        """)

        # Mode label
        self._mode_label = QLabel("NORMAL", self)
        self._mode_label.setStyleSheet(
            "color: #00e5ff; font-family: 'Courier New', monospace; "
            "font-size: 10px; background: rgba(0,6,18,0.7); "
            "border: 1px solid rgba(0,180,255,0.2); border-radius: 3px; "
            "padding: 2px 6px;"
        )
        self._mode_label.move(5, 5)
        self._mode_label.adjustSize()

        # Gesture label
        self._gesture_label = QLabel("", self)
        self._gesture_label.setStyleSheet(
            "color: #00e5ff; font-family: 'Courier New', monospace; "
            "font-size: 9px; background: rgba(0,6,18,0.7); "
            "border: 1px solid rgba(0,180,255,0.15); border-radius: 3px; "
            "padding: 2px 6px;"
        )
        self._gesture_label.move(5, 25)
        self._gesture_label.adjustSize()

        # Toggle button
        self._toggle_btn = QLabel("▼", self)
        self._toggle_btn.setStyleSheet(
            "color: rgba(0,180,255,0.5); font-size: 12px; background: transparent;"
        )
        self._toggle_btn.move(225, 5)
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

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._capture)
        self._timer.start(33)  # ~30fps
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

            # ── Hand Tracking ──
            hand_lms = None
            if self._hand_landmarker:
                try:
                    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                    mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
                    ts = self._frame_count * 33
                    result = self._hand_landmarker.detect_for_video(mp_image, ts)
                    if result.hand_landmarks:
                        hand_lms = result.hand_landmarks[0]
                except Exception:
                    pass

            # ── Gesture Engine ──
            cmd = {"cursor_x": None, "cursor_y": None}
            if hand_lms:
                try:
                    import pyautogui
                    screen_w, screen_h = pyautogui.size()
                    cmd = self._gesture_engine.process(hand_lms, screen_w, screen_h)

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

            # ── Draw mini feed ──
            cia = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB).copy()
            cia_f = cia.astype(np.float32)
            cia_f[:, :, 0] *= 0.82
            cia_f[:, :, 1] *= 0.92
            cia_f[:, :, 2] = np.clip(cia_f[:, :, 2] * 1.12 + 10, 0, 255)
            cia = cia_f.astype(np.uint8)

            # Draw hand landmarks
            if hand_lms:
                connections = [
                    (0,1),(1,2),(2,3),(3,4),(0,5),(5,6),(6,7),(7,8),
                    (0,9),(9,10),(10,11),(11,12),(0,13),(13,14),(14,15),(15,16),
                    (0,17),(17,18),(18,19),(19,20),(5,9),(9,13),(13,17)
                ]
                for a, b in connections:
                    ax, ay = int(hand_lms[a].x * w), int(hand_lms[a].y * h)
                    bx, by = int(hand_lms[b].x * w), int(hand_lms[b].y * h)
                    cv2.line(cia, (ax, ay), (bx, by), (0, 200, 255), 1)
                for i, pt in enumerate(hand_lms):
                    px, py = int(pt.x * w), int(pt.y * h)
                    cv2.circle(cia, (px, py), 2, (255, 255, 255) if i in [4, 8] else (0, 200, 255), -1)

                # Cursor crosshair at index
                ix, iy = int(hand_lms[8].x * w), int(hand_lms[8].y * h)
                cv2.line(cia, (ix-8, iy), (ix+8, iy), (255, 255, 255), 1)
                cv2.line(cia, (ix, iy-8), (ix, iy+8), (255, 255, 255), 1)

            # Convert to Qt
            ch2 = 3
            bpl = ch2 * w
            self._frame_ref = cia.copy()
            qt_img = QImage(self._frame_ref.data, w, h, bpl, QImage.Format.Format_RGB888)
            pixmap = QPixmap.fromImage(qt_img).scaled(
                240, 180, Qt.AspectRatioMode.KeepAspectRatioByExpanding,
                Qt.TransformationMode.FastTransformation
            )
            self._feed.setPixmap(pixmap)

            # Update labels
            ge = self._gesture_engine
            self._mode_label.setText(ge.mode)
            gesture = ge.gesture
            if ge.is_locked:
                gesture = "LOCKED"
            elif ge.is_dragging:
                gesture = "DRAG"
            self._gesture_label.setText(gesture)

        except Exception:
            pass

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
            self.setFixedSize(240, 35)
            self._feed.hide()
            self._gesture_label.hide()
            self._toggle_btn.setText("▶")
        else:
            self.setFixedSize(240, 180)
            self._feed.show()
            self._gesture_label.show()
            self._toggle_btn.setText("▼")
