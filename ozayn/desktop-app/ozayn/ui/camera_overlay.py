"""
Ozayn Camera Overlay — Real camera footage with hand lines + face circles drawn on top.
Persists after login. Picture-in-picture style.
"""

import os
import math
import numpy as np

from PyQt6.QtWidgets import QWidget, QLabel
from PyQt6.QtCore import Qt, QTimer, pyqtSignal
from PyQt6.QtGui import QPixmap, QImage, QColor

from ozayn.gesture_engine import GestureEngine


class CameraOverlay(QWidget):
    """Floating camera overlay with gesture control. Real footage + HUD lines."""

    gesture_command = pyqtSignal(dict)
    toggle_keyboard = pyqtSignal()
    toggle_mode = pyqtSignal()

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
        self._frame_ref = None
        self._frame_count = 0
        self._collapsed = False
        self._drag_pos = None

        # Detection results
        self._hand_lms = None
        self._hand_lms_right = None
        self._face_lms = None
        self._gesture = ""
        self._mode = "NORMAL"

        # Feed label (real camera footage)
        self._feed = QLabel(self)
        self._feed.setFixedSize(280, 220)
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
        self._mode_label.move(8, 8)
        self._mode_label.adjustSize()

        # Gesture label
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

        self._camera.set(cv2.CAP_PROP_FRAME_WIDTH, 320)
        self._camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 240)
        self._camera.set(cv2.CAP_PROP_FPS, 30)
        self._camera.set(cv2.CAP_PROP_BUFFERSIZE, 1)

        # Hand landmarker — 2 hands
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
                    num_hands=2,
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

            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)

            # Hand tracking — 2 hands
            self._hand_lms = None
            self._hand_lms_right = None
            if self._hand_landmarker:
                try:
                    result = self._hand_landmarker.detect_for_video(mp_image, ts)
                    if result.hand_landmarks:
                        self._hand_lms = result.hand_landmarks[0]
                        if len(result.hand_landmarks) > 1:
                            self._hand_lms_right = result.hand_landmarks[1]
                except Exception:
                    pass

            # Face tracking
            self._face_lms = None
            if self._face_landmarker:
                try:
                    result = self._face_landmarker.detect_for_video(mp_image, ts)
                    if result.face_landmarks:
                        self._face_lms = result.face_landmarks[0]
                except Exception:
                    pass

            # Gesture engine
            cmd = {"cursor_x": None, "cursor_y": None}
            if self._hand_lms:
                try:
                    import pyautogui
                    screen_w, screen_h = pyautogui.size()
                    if self._hand_lms_right:
                        cmd = self._gesture_engine.process_two_hands(
                            self._hand_lms, self._hand_lms_right, screen_w, screen_h)
                    else:
                        cmd = self._gesture_engine.process(
                            self._hand_lms, screen_w, screen_h)

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

            # ── Draw HUD on real camera footage ──
            cia = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB).copy()
            cia_f = cia.astype(np.float32)
            cia_f[:, :, 0] *= 0.82
            cia_f[:, :, 1] *= 0.92
            cia_f[:, :, 2] = np.clip(cia_f[:, :, 2] * 1.12 + 10, 0, 255)
            cia = cia_f.astype(np.uint8)

            # Draw hand landmarks — left hand
            self._draw_hand(cia, self._hand_lms, w, h, (0, 200, 255))
            # Draw hand landmarks — right hand
            self._draw_hand(cia, self._hand_lms_right, w, h, (0, 255, 150))

            # Draw face pattern
            if self._face_lms:
                self._draw_face(cia, self._face_lms, w, h)

            # Convert to Qt
            ch2 = 3
            bpl = ch2 * w
            self._frame_ref = cia.copy()
            qt_img = QImage(self._frame_ref.data, w, h, bpl, QImage.Format.Format_RGB888)
            pixmap = QPixmap.fromImage(qt_img).scaled(
                280, 220, Qt.AspectRatioMode.KeepAspectRatioByExpanding,
                Qt.TransformationMode.FastTransformation
            )
            self._feed.setPixmap(pixmap)

            # Update labels
            ge = self._gesture_engine
            self._mode_label.setText(ge.mode)
            self._gesture = ge.gesture
            if ge.is_locked:
                self._gesture = "LOCKED"
            elif ge.is_dragging:
                self._gesture = "DRAG"
            self._gesture_label.setText(self._gesture)

        except Exception:
            pass

    def _draw_hand(self, img, lms, w, h, color):
        """Draw hand skeleton on camera frame."""
        if lms is None:
            return
        import cv2

        connections = [
            (0,1),(1,2),(2,3),(3,4),
            (0,5),(5,6),(6,7),(7,8),
            (0,9),(9,10),(10,11),(11,12),
            (0,13),(13,14),(14,15),(15,16),
            (0,17),(17,18),(18,19),(19,20),
            (5,9),(9,13),(13,17)
        ]

        for a, b in connections:
            ax, ay = int(lms[a].x * w), int(lms[a].y * h)
            bx, by = int(lms[b].x * w), int(lms[b].y * h)
            cv2.line(img, (ax, ay), (bx, by), color, 2)

        for i, pt in enumerate(lms):
            px, py = int(pt.x * w), int(pt.y * h)
            c = (255, 255, 255) if i in [4, 8] else color
            r = 4 if i in [4, 8] else 2
            cv2.circle(img, (px, py), r, c, -1)

        # Crosshair at index tip
        ix, iy = int(lms[8].x * w), int(lms[8].y * h)
        cv2.line(img, (ix - 10, iy), (ix + 10, iy), (255, 255, 255), 1)
        cv2.line(img, (ix, iy - 10), (ix, iy + 10), (255, 255, 255), 1)
        cv2.circle(img, (ix, iy), 12, color, 1)

    def _draw_face(self, img, lms, w, h):
        """Draw face detection pattern on camera frame."""
        import cv2

        # Key indices
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
        CHIN = 152
        FOREHEAD = 10
        LEFT_FACE = 234
        RIGHT_FACE = 454

        def px(idx):
            return int(lms[idx].x * w), int(lms[idx].y * h)

        # Face oval
        forehead = px(FOREHEAD)
        chin = px(CHIN)
        left_f = px(LEFT_FACE)
        right_f = px(RIGHT_FACE)
        fcx = (left_f[0] + right_f[0]) // 2
        fcy = (forehead[1] + chin[1]) // 2
        fw = abs(right_f[0] - left_f[0]) // 2
        fh = abs(chin[1] - forehead[1]) // 2
        cv2.ellipse(img, (fcx, fcy), (fw, fh), 0, 0, 360, (0, 180, 255), 1)

        # Eyes
        for ein, eout, etop, ebot in [
            (LEFT_EYE_INNER, LEFT_EYE_OUTER, LEFT_EYE_TOP, LEFT_EYE_BOTTOM),
            (RIGHT_EYE_INNER, RIGHT_EYE_OUTER, RIGHT_EYE_TOP, RIGHT_EYE_BOTTOM),
        ]:
            ci = ((px(ein)[0] + px(eout)[0]) // 2, (px(etop)[1] + px(ebot)[1]) // 2)
            rx = abs(px(eout)[0] - px(ein)[0]) // 2
            ry = abs(px(etop)[1] - px(ebot)[1]) // 2
            cv2.ellipse(img, ci, (rx, ry), 0, 0, 360, (0, 229, 255), 1)

        # Nose line
        cv2.line(img, px(NOSE_TIP), px(FOREHEAD), (0, 229, 255), 1)

        # Mouth
        cv2.line(img, px(MOUTH_LEFT), px(MOUTH_RIGHT), (0, 200, 255), 1)
        cv2.line(img, px(MOUTH_LEFT), px(MOUTH_TOP), (0, 200, 255), 1)
        cv2.line(img, px(MOUTH_TOP), px(MOUTH_RIGHT), (0, 200, 255), 1)

        # Nose dot
        nt = px(NOSE_TIP)
        cv2.circle(img, nt, 3, (0, 255, 100), -1)

        # Label
        cv2.putText(img, "FACE", (left_f[0], forehead[1] - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 200, 255), 1)

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
            self._feed.hide()
            self._gesture_label.hide()
            self._toggle_btn.setText("▶")
        else:
            self.setFixedSize(280, 220)
            self._feed.show()
            self._gesture_label.show()
            self._toggle_btn.setText("▼")
