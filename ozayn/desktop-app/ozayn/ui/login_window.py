"""
Ozayn Login Window — CIA Command Center Style
Fullscreen camera with cyber blue/white/black filter, face + hand tracking,
hand gesture mouse control (pinch to move cursor), auto-voice.
"""

import os
os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = '1'
os.environ['SDL_AUDIODRIVER'] = 'dummy'

import warnings
warnings.filterwarnings("ignore")

import threading
import math
import numpy as np

from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QLabel,
    QLineEdit, QPushButton, QStackedWidget, QDialog,
    QGraphicsDropShadowEffect
)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QPoint
from PyQt6.QtGui import QFont, QColor, QPixmap, QImage, QCursor

from ozayn.workers import WorkerMixin


# ─── Full Virtual Keyboard ──────────────────────────────────────────────────

class VirtualKeyboard(QWidget):
    key_pressed = pyqtSignal(str)
    enter_pressed = pyqtSignal()
    backspace_pressed = pyqtSignal()
    shift_pressed = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.shift_on = False
        self._init_ui()

    def _init_ui(self):
        self.setStyleSheet(
            "background: rgba(0,6,18,0.94); border: 1px solid rgba(0,180,255,0.2); "
            "border-radius: 12px; padding: 6px;"
        )
        layout = QVBoxLayout(self)
        layout.setContentsMargins(6, 6, 6, 6)
        layout.setSpacing(3)

        self._num_row_widget = QWidget()
        nr = QHBoxLayout(self._num_row_widget)
        nr.setContentsMargins(0, 0, 0, 0)
        nr.setSpacing(2)
        for ch in "1234567890":
            btn = self._key_btn(ch, 30, 32, small=True)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            nr.addWidget(btn)
        layout.addWidget(self._num_row_widget)

        row1 = QHBoxLayout()
        row1.setSpacing(2)
        for ch in "QWERTYUIOP":
            btn = self._key_btn(ch, 34, 38)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            row1.addWidget(btn)
        layout.addLayout(row1)

        row2 = QHBoxLayout()
        row2.setSpacing(2)
        row2.addSpacing(16)
        for ch in "ASDFGHJKL":
            btn = self._key_btn(ch, 34, 38)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            row2.addWidget(btn)
        row2.addSpacing(16)
        layout.addLayout(row2)

        row3 = QHBoxLayout()
        row3.setSpacing(2)
        row3.addSpacing(32)
        for ch in "ZXCVBNM":
            btn = self._key_btn(ch, 34, 38)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            row3.addWidget(btn)
        row3.addSpacing(8)
        bs = self._key_btn("⌫", 50, 38, color="#00e5ff")
        bs.clicked.connect(self.backspace_pressed.emit)
        row3.addWidget(bs)
        row3.addSpacing(32)
        layout.addLayout(row3)

        row4 = QHBoxLayout()
        row4.setSpacing(4)
        shift = self._key_btn("⇧", 44, 38, color="#00e5ff")
        shift.clicked.connect(self._toggle_shift)
        self._shift_btn = shift
        row4.addWidget(shift)
        space = self._key_btn("SPACE", 200, 38, color="#0a1a30")
        space.clicked.connect(lambda: self._emit(" "))
        row4.addWidget(space)
        enter = self._key_btn("ENTER", 70, 38, color="#00e5ff")
        enter.clicked.connect(self.enter_pressed.emit)
        row4.addWidget(enter)
        layout.addLayout(row4)

        self._sym_widget = QWidget()
        sr = QHBoxLayout(self._sym_widget)
        sr.setContentsMargins(0, 0, 0, 0)
        sr.setSpacing(2)
        for ch in "!@#$%^&*()_+-=[]{}|;:',.<>?/":
            btn = self._key_btn(ch, 28, 32, small=True)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            sr.addWidget(btn)
        self._sym_widget.hide()
        layout.addWidget(self._sym_widget)

        bottom = QHBoxLayout()
        bottom.setSpacing(4)
        num_switch = self._key_btn("?123", 52, 34, color="#00e5ff")
        num_switch.clicked.connect(self._toggle_symbols)
        bottom.addWidget(num_switch)
        bottom.addStretch()
        abc_switch = self._key_btn("ABC", 46, 34, color="#00e5ff")
        abc_switch.clicked.connect(self._show_alpha)
        abc_switch.hide()
        self._abc_btn = abc_switch
        bottom.addWidget(abc_switch)
        close_btn = self._key_btn("✕", 34, 34, color="#ff453a")
        close_btn.clicked.connect(lambda: self.hide())
        bottom.addWidget(close_btn)
        layout.addLayout(bottom)

    def _key_btn(self, text, w, h, color=None, small=False):
        btn = QPushButton(text)
        btn.setFixedSize(w, h)
        btn.setCursor(Qt.CursorShape.PointingHandCursor)
        fs = "10px" if small else "13px"
        if color:
            r, g, b = self._hex(color)
            bg, border = f"rgba({r},{g},{b},0.2)", f"rgba({r},{g},{b},0.5)"
        else:
            bg, border = "rgba(0,180,255,0.06)", "rgba(0,180,255,0.15)"
        btn.setStyleSheet(f"""
            QPushButton {{ background: {bg}; color: #00e5ff; border: 1px solid {border};
                border-radius: 6px; font-size: {fs}; font-weight: 600;
                font-family: 'Courier New', monospace; }}
            QPushButton:hover {{ background: rgba(0,180,255,0.25); border-color: rgba(0,229,255,0.6); color: #fff; }}
            QPushButton:pressed {{ background: rgba(0,180,255,0.4); }}
        """)
        return btn

    def _hex(self, h):
        h = h.lstrip('#')
        if len(h) == 3:
            h = h[0]*2 + h[1]*2 + h[2]*2
        return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)

    def _emit(self, ch):
        ch = ch.upper() if self.shift_on else ch.lower()
        if self.shift_on:
            self._toggle_shift()
        self.key_pressed.emit(ch)

    def _toggle_shift(self):
        self.shift_on = not self.shift_on
        self._shift_btn.setStyleSheet(f"""
            QPushButton {{ background: rgba(0,180,255,{0.4 if self.shift_on else 0.2});
                color: #00e5ff; border: 1px solid #00b4ff; border-radius: 6px;
                font-size: 14px; font-weight: bold; }}
        """)

    def _toggle_symbols(self):
        self._sym_widget.setVisible(not self._sym_widget.isVisible())
        self._num_row_widget.setVisible(not self._sym_widget.isVisible())
        self._abc_btn.setVisible(self._sym_widget.isVisible())

    def _show_alpha(self):
        self._sym_widget.hide()
        self._num_row_widget.show()
        self._abc_btn.hide()


# ─── Fullscreen CIA Camera + Mouth Tracking ────────────────────────────────

class CIAFullscreenCamera(QLabel):
    """Full-screen camera. Face + mouth tracking. Mouth moves mouse cursor."""

    face_detected = pyqtSignal()
    mouth_detected = pyqtSignal(float, float)

    _CASCADE_PATH = None

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
        try:
            import cv2
            for root, dirs, files in os.walk(os.path.dirname(cv2.__file__)):
                for f in files:
                    if f == "haarcascade_frontalface_default.xml":
                        cls._CASCADE_PATH = os.path.join(root, f)
                        return cls._CASCADE_PATH
        except Exception:
            pass
        for p in ["/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",
                  "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml"]:
            if os.path.isfile(p):
                cls._CASCADE_PATH = p
                return p
        return None

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.setStyleSheet("background: #000000;")
        self._camera = None
        self._timer = None
        self._cascade = None
        self._face_mesh = None
        self._frame_ref = None
        self._face_boxes = []
        self._mouth_landmarks = None
        self._mouth_pos = None
        self._prev_mouth = None
        self._mouth_open = False
        self._prev_mouth_open = False
        self._scan_y = 0
        self._frame_w = 0
        self._frame_h = 0
        self._clahe = None
        self._kernel_sharp = None

    def start(self):
        try:
            import cv2
        except ImportError:
            return

        self._camera = cv2.VideoCapture(0)
        if not self._camera.isOpened():
            return

        self._camera.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
        self._camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
        self._camera.set(cv2.CAP_PROP_FPS, 30)
        self._camera.set(cv2.CAP_PROP_BUFFERSIZE, 1)

        cascade_path = self._find_cascade()
        if cascade_path:
            try:
                self._cascade = cv2.CascadeClassifier(cascade_path)
                if self._cascade.empty():
                    self._cascade = None
            except Exception:
                self._cascade = None

        # Initialize MediaPipe Face Mesh for mouth tracking
        try:
            import mediapipe as mp
            self._face_mesh = mp.solutions.face_mesh.FaceMesh(
                static_image_mode=False,
                max_num_faces=1,
                min_detection_confidence=0.6,
                min_tracking_confidence=0.5
            )
        except Exception:
            self._face_mesh = None

        # CV2 quality enhancement kernels (initialized once)
        self._clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(4, 4))
        # Sharpen kernel
        self._kernel_sharp = np.array([
            [0, -1, 0],
            [-1, 5, -1],
            [0, -1, 0]
        ], dtype=np.float32)

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._capture)
        self._timer.start(33)  # ~30fps

    def _enhance_frame(self, frame):
        """CV2 methods to improve camera quality for real-time."""
        import cv2

        # 1. Denoise (fast bilateral filter)
        denoised = cv2.bilateralFilter(frame, 5, 50, 50)

        # 2. Convert to LAB for contrast enhancement
        lab = cv2.cvtColor(denoised, cv2.COLOR_BGR2LAB)
        l, a, b = cv2.split(lab)

        # 3. CLAHE on L channel for contrast
        l_enhanced = self._clahe.apply(l)

        # 4. Merge back
        lab_enhanced = cv2.merge([l_enhanced, a, b])
        enhanced = cv2.cvtColor(lab_enhanced, cv2.COLOR_LAB2BGR)

        # 5. Sharpen
        sharpened = cv2.filter2D(enhanced, -1, self._kernel_sharp)

        return sharpened

    def _capture(self):
        if not self._camera or not self._camera.isOpened():
            return

        # Flush old buffer for real-time
        self._camera.grab()
        ret, frame = self._camera.read()
        if not ret:
            return

        try:
            import cv2

            self._frame_h, self._frame_w = frame.shape[:2]

            # ── CV2 Quality Enhancement ──
            frame = self._enhance_frame(frame)

            # ── Face Detection (cascade) ──
            self._face_boxes = []
            if self._cascade:
                try:
                    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                    faces = self._cascade.detectMultiScale(gray, 1.1, 4)
                    self._face_boxes = faces
                except Exception:
                    pass

            # ── Mouth Tracking (MediaPipe Face Mesh) ──
            self._mouth_landmarks = None
            self._mouth_pos = None
            self._mouth_open = False

            if self._face_mesh:
                try:
                    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                    rgb.flags.writeable = False
                    results = self._face_mesh.process(rgb)
                    rgb.flags.writeable = True

                    if results.multi_face_landmarks:
                        face_lms = results.multi_face_landmarks[0]
                        lm = face_lms.landmark
                        h, w = frame.shape[:2]

                        # Upper lip top (13) and lower lip bottom (14)
                        upper = lm[13]
                        lower = lm[14]
                        mouth_top = (int(upper.x * w), int(upper.y * h))
                        mouth_bot = (int(lower.x * w), int(lower.y * h))

                        # Center of mouth
                        cx = (upper.x + lower.x) / 2
                        cy = (upper.y + lower.y) / 2
                        self._mouth_pos = (cx, cy)

                        # Mouth open distance (for click)
                        mouth_dist = math.sqrt(
                            (upper.x - lower.x) ** 2 +
                            (upper.y - lower.y) ** 2
                        )
                        self._mouth_open = mouth_dist > 0.025

                        # Store mouth landmarks for drawing
                        # Lips outline: key points
                        lips_indices = [61, 146, 91, 181, 84, 17, 314, 405, 321, 375, 291,
                                        409, 270, 269, 267, 0, 37, 39, 40, 185]
                        self._mouth_landmarks = []
                        for idx in lips_indices:
                            pt = lm[idx]
                            self._mouth_landmarks.append((int(pt.x * w), int(pt.y * h)))
                except Exception:
                    pass

            # ── Move mouse with mouth ──
            if self._mouth_pos:
                try:
                    import pyautogui
                    screen_w, screen_h = pyautogui.size()
                    # Mirror X
                    cx = (1.0 - self._mouth_pos[0]) * screen_w
                    cy = self._mouth_pos[1] * screen_h
                    # Smooth
                    if self._prev_mouth:
                        px, py = self._prev_mouth
                        cx = px + (cx - px) * 0.25
                        cy = py + (cy - py) * 0.25
                    pyautogui.moveTo(int(cx), int(cy), _pause=False)
                    self._prev_mouth = (cx, cy)

                    # Click on mouth open (if just opened)
                    if self._mouth_open and not self._prev_mouth_open:
                        pyautogui.click(_pause=False)
                except Exception:
                    pass
            else:
                self._prev_mouth = None

            self._prev_mouth_open = self._mouth_open

            # ── Subtle CIA blue tint on normal image ──
            h, w = frame.shape[:2]
            cia = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB).copy()
            cia_f = cia.astype(np.float32)
            cia_f[:, :, 0] = cia_f[:, :, 0] * 0.82
            cia_f[:, :, 1] = cia_f[:, :, 1] * 0.92
            cia_f[:, :, 2] = np.clip(cia_f[:, :, 2] * 1.12 + 10, 0, 255)
            cia = cia_f.astype(np.uint8)

            # Soft vignette
            rows, cols = cia.shape[:2]
            X = cv2.getGaussianKernel(cols, cols * 0.7)
            Y = cv2.getGaussianKernel(rows, rows * 0.7)
            vig = Y * X.T
            vig = vig / vig.max()
            vig = 0.75 + 0.25 * vig
            for c in range(3):
                cia[:, :, c] = np.clip(cia[:, :, c].astype(np.float32) * vig, 0, 255).astype(np.uint8)

            # Scan lines
            cia[::3, :, :] = (cia[::3, :, :].astype(np.float32) * 0.94).astype(np.uint8)
            self._scan_y = (self._scan_y + 2) % h
            cia[self._scan_y:self._scan_y+1, :, :] = \
                np.clip(cia[self._scan_y:self._scan_y+1, :, :].astype(np.float32) + 30, 0, 255).astype(np.uint8)

            # ── Draw Face Boxes ──
            for (fx, fy, fw, fh) in self._face_boxes:
                cv2.rectangle(cia, (fx, fy), (fx+fw, fy+fh), (0, 200, 255), 2)
                cl = min(25, fw // 4, fh // 4)
                wc = (255, 255, 255)
                bc = (0, 200, 255)
                cv2.line(cia, (fx, fy), (fx+cl, fy), wc, 3)
                cv2.line(cia, (fx, fy), (fx, fy+cl), wc, 3)
                cv2.line(cia, (fx+fw, fy), (fx+fw-cl, fy), wc, 3)
                cv2.line(cia, (fx+fw, fy), (fx+fw, fy+cl), wc, 3)
                cv2.line(cia, (fx, fy+fh), (fx+cl, fy+fh), wc, 3)
                cv2.line(cia, (fx, fy+fh), (fx, fy+fh-cl), wc, 3)
                cv2.line(cia, (fx+fw, fy+fh), (fx+fw-cl, fy+fh), wc, 3)
                cv2.line(cia, (fx+fw, fy+fh), (fx+fw, fy+fh-cl), wc, 3)
                cv2.putText(cia, "FACE", (fx, fy - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.6, bc, 2)

            self._face_detected = len(self._face_boxes) > 0
            if self._face_detected:
                self.face_detected.emit()

            # ── Draw Mouth Landmarks ──
            if self._mouth_landmarks:
                # Draw lip outline
                pts = np.array(self._mouth_landmarks, dtype=np.int32)
                cv2.polylines(cia, [pts], True, (0, 200, 255), 2, cv2.LINE_AA)

                # Center dot (cursor position)
                if self._mouth_pos:
                    h, w = frame.shape[:2]
                    mx = int(self._mouth_pos[0] * w)
                    my = int(self._mouth_pos[1] * h)
                    # Crosshair
                    cv2.line(cia, (mx-12, my), (mx+12, my), (0, 229, 255), 2)
                    cv2.line(cia, (mx, my-12), (mx, my+12), (0, 229, 255), 2)
                    cv2.circle(cia, (mx, my), 6, (0, 229, 255), 2)

                    # Label
                    label = "CLICK" if self._mouth_open else "TRACKING"
                    color = (0, 255, 128) if self._mouth_open else (0, 200, 255)
                    cv2.putText(cia, label, (mx + 18, my - 5),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)

            # ── Convert to Qt ──
            h2, w2, ch2 = cia.shape
            bpl = ch2 * w2
            self._frame_ref = cia.copy()
            qt_img = QImage(self._frame_ref.data, w2, h2, bpl, QImage.Format.Format_RGB888)
            pixmap = QPixmap.fromImage(qt_img)
            scaled = pixmap.scaled(
                self.size(),
                Qt.AspectRatioMode.KeepAspectRatioByExpanding,
                Qt.TransformationMode.FastTransformation
            )
            self.setPixmap(scaled)

        except Exception:
            pass

        except Exception as e:
            # Show error on black background
            err_pixmap = QPixmap(self.size())
            err_pixmap.fill(QColor(0, 0, 0))
            self.setPixmap(err_pixmap)

    def stop(self):
        if self._timer and self._timer.isActive():
            self._timer.stop()
        if self._camera and self._camera.isOpened():
            self._camera.release()
            self._camera = None
        if self._face_mesh:
            try:
                self._face_mesh.close()
            except Exception:
                pass
            self._face_mesh = None
        self._frame_ref = None

    def hideEvent(self, event):
        self.stop()
        super().hideEvent(event)


# ─── 2FA Dialog ─────────────────────────────────────────────────────────────

class TwoFADialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Two-Factor Authentication")
        self.setFixedSize(320, 220)
        self.setStyleSheet("""
            QDialog { background: rgba(0,6,18,0.95); color: #00e5ff; }
            QLabel { color: #00e5ff; }
            QLineEdit { background: rgba(0,180,255,0.08); color: #00e5ff;
                border: 1px solid rgba(0,180,255,0.3); border-radius: 8px;
                padding: 10px; font-size: 20px; letter-spacing: 8px; }
            QLineEdit:focus { border-color: #00b4ff; }
            QPushButton { background: rgba(0,180,255,0.15); color: #00e5ff;
                border: 1px solid rgba(0,180,255,0.3); border-radius: 6px;
                padding: 8px 16px; font-size: 13px; }
            QPushButton:hover { background: rgba(0,180,255,0.3); }
        """)
        self.verified = False
        layout = QVBoxLayout(self)
        layout.setSpacing(12)
        title = QLabel("ENTER 2FA CODE")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title.setStyleSheet("font-size: 14px; font-weight: bold; color: #00b4ff; letter-spacing: 3px;")
        layout.addWidget(title)
        self.code_input = QLineEdit()
        self.code_input.setPlaceholderText("••••••")
        self.code_input.setMaxLength(6)
        self.code_input.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.code_input.setFont(QFont("Courier", 20))
        layout.addWidget(self.code_input)
        btn_row = QHBoxLayout()
        cancel = QPushButton("Cancel")
        cancel.clicked.connect(self.reject)
        btn_row.addWidget(cancel)
        verify = QPushButton("Verify")
        verify.setDefault(True)
        verify.clicked.connect(self._verify)
        btn_row.addWidget(verify)
        layout.addLayout(btn_row)
        self._error = QLabel("")
        self._error.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._error.setStyleSheet("color: #ff453a; font-size: 12px;")
        layout.addWidget(self._error)

    def _verify(self):
        code = self.code_input.text().strip()
        if len(code) != 6:
            self._error.setText("Code must be 6 digits")
            return
        self.verified = True
        self.accept()


# ─── Main Login Window ──────────────────────────────────────────────────────

class LoginWindow(QMainWindow, WorkerMixin):
    _login_result = pyqtSignal(dict)
    _reg_result = pyqtSignal(dict)
    _face_result = pyqtSignal(dict)

    def __init__(self, api, on_success):
        super().__init__()
        self.api = api
        self.on_success = on_success
        self._init_workers()
        self.setWindowTitle("OZAYN — Digital Twin Intelligence")

        # True fullscreen
        self.setWindowFlags(Qt.WindowType.FramelessWindowHint)
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground, False)
        self.showFullScreen()

        self._active_field = None
        self._keyboard_visible = False
        self._voice_active = False

        self._login_result.connect(self._on_login_result)
        self._reg_result.connect(self._on_reg_result)
        self._face_result.connect(self._on_face_login_result)

        # Layer 1: Fullscreen CIA Camera
        self._bg_camera = CIAFullscreenCamera(self)
        self._bg_camera.face_detected.connect(self._on_auto_face_login)

        # Layer 2: Glass container
        self._glass_container = QWidget(self)
        self._glass_container.setFixedSize(380, 500)
        self._glass_container.setStyleSheet("""
            #glassPanel {
                background: rgba(0,6,18,0.75);
                border: 1px solid rgba(0,180,255,0.2);
                border-radius: 16px;
            }
        """)
        self._glass_container.setObjectName("glassPanel")

        shadow = QGraphicsDropShadowEffect()
        shadow.setBlurRadius(60)
        shadow.setColor(QColor(0, 120, 255, 60))
        shadow.setOffset(0, 0)
        self._glass_container.setGraphicsEffect(shadow)

        self.stack = QStackedWidget()
        self._build_login_page()
        self._build_register_page()
        self._build_virtual_keyboard()

        glass_layout = QVBoxLayout(self._glass_container)
        glass_layout.setContentsMargins(0, 0, 0, 0)
        glass_layout.addWidget(self.stack)

        # HUD indicators
        self._voice_indicator = QLabel("● VOICE: INIT", self)
        self._voice_indicator.setStyleSheet(
            "color: rgba(0,180,255,0.5); font-family: 'Courier New', monospace; "
            "font-size: 14px; background: rgba(0,6,18,0.6); border: 1px solid rgba(0,180,255,0.15); "
            "border-radius: 4px; padding: 6px 14px;"
        )
        self._voice_indicator.adjustSize()

        self._cam_indicator = QLabel("● CAM: INIT", self)
        self._cam_indicator.setStyleSheet(
            "color: rgba(0,180,255,0.5); font-family: 'Courier New', monospace; "
            "font-size: 14px; background: rgba(0,6,18,0.6); border: 1px solid rgba(0,180,255,0.15); "
            "border-radius: 4px; padding: 6px 14px;"
        )
        self._cam_indicator.adjustSize()

        self._hand_indicator = QLabel("● MOUTH: INIT", self)
        self._hand_indicator.setStyleSheet(
            "color: rgba(0,180,255,0.5); font-family: 'Courier New', monospace; "
            "font-size: 14px; background: rgba(0,6,18,0.6); border: 1px solid rgba(0,180,255,0.15); "
            "border-radius: 4px; padding: 6px 14px;"
        )
        self._hand_indicator.adjustSize()

        self._do_layout()
        QTimer.singleShot(300, self._auto_start)

    def _do_layout(self):
        sz = self.size()
        self._bg_camera.setGeometry(0, 0, sz.width(), sz.height())
        self._bg_camera.show()
        gw, gh = 380, 500
        self._glass_container.setFixedSize(gw, gh)
        self._glass_container.move((sz.width() - gw) // 2, (sz.height() - gh) // 2)
        self._glass_container.show()
        self._voice_indicator.move(sz.width() - 160, 16)
        self._cam_indicator.move(sz.width() - 160, 44)
        self._hand_indicator.move(sz.width() - 160, 72)

    def resizeEvent(self, event):
        super().resizeEvent(event)
        if hasattr(self, '_bg_camera'):
            self._do_layout()

    def keyPressEvent(self, event):
        if event.key() == Qt.Key.Key_Escape:
            self.close()

    def _auto_start(self):
        self._start_camera()
        self._auto_voice_listen()

    def _start_camera(self):
        self._bg_camera.start()
        self._cam_indicator.setText("● CAM: ACTIVE")
        self._cam_indicator.setStyleSheet(
            "color: #00e5ff; font-family: 'Courier New', monospace; "
            "font-size: 14px; background: rgba(0,6,18,0.6); border: 1px solid rgba(0,180,255,0.3); "
            "border-radius: 4px; padding: 6px 14px;"
        )
        # Update hand indicator based on mediapipe availability
        QTimer.singleShot(1000, self._update_hand_status)

    def _update_hand_status(self):
        if self._bg_camera._face_mesh:
            self._hand_indicator.setText("● MOUTH: TRACKING")
            self._hand_indicator.setStyleSheet(
                "color: #00e5ff; font-family: 'Courier New', monospace; "
                "font-size: 14px; background: rgba(0,6,18,0.6); border: 1px solid rgba(0,180,255,0.3); "
                "border-radius: 4px; padding: 6px 14px;"
            )
        else:
            self._hand_indicator.setText("● MOUTH: UNAVAIL")
            self._hand_indicator.setStyleSheet(
                "color: rgba(255,159,10,0.7); font-family: 'Courier New', monospace; "
                "font-size: 14px; background: rgba(20,10,0,0.6); border: 1px solid rgba(255,159,10,0.15); "
                "border-radius: 4px; padding: 6px 14px;"
            )

    def _on_auto_face_login(self):
        if hasattr(self, '_face_already_triggered'):
            return
        self._face_already_triggered = True
        self.login_error.setStyleSheet("color: #00e5ff; font-size: 12px; font-weight: bold;")
        self.login_error.setText("Face recognized — authenticating...")
        self._cam_indicator.setText("● CAM: FACE LOCK")
        self._cam_indicator.setStyleSheet(
            "color: #00ff88; font-family: 'Courier New', monospace; "
            "font-size: 14px; background: rgba(0,20,10,0.6); border: 1px solid rgba(0,255,136,0.3); "
            "border-radius: 4px; padding: 6px 14px;"
        )
        self._run("face_login", {}, lambda r: self._face_result.emit(r))

    def _on_face_login_result(self, r):
        if r.get("success"):
            QTimer.singleShot(0, self.on_success)
        else:
            if hasattr(self, '_face_already_triggered'):
                del self._face_already_triggered
            self.login_error.setStyleSheet("color: rgba(255,255,255,0.4); font-size: 12px;")
            self.login_error.setText("Face not in database — use credentials")

    # ─── Auto Voice ─────────────────────────────────────────────────────

    def _auto_voice_listen(self):
        self._voice_indicator.setText("● VOICE: LOADING...")
        self._voice_indicator.setStyleSheet(
            "color: #ff9f0a; font-family: 'Courier New', monospace; "
            "font-size: 14px; background: rgba(20,10,0,0.5); border: 1px solid rgba(255,159,10,0.15); "
            "border-radius: 4px; padding: 6px 14px;"
        )
        try:
            import sys
            old_stderr = sys.stderr
            sys.stderr = open(os.devnull, 'w')
            try:
                import speech_recognition as sr
            finally:
                sys.stderr.close()
                sys.stderr = old_stderr

            try:
                mic = sr.Microphone()
            except (AttributeError, OSError):
                self._voice_indicator.setText("● VOICE: NO MIC")
                self._voice_indicator.setStyleSheet(
            "color: rgba(255,69,58,0.7); font-family: 'Courier New', monospace; "
            "font-size: 14px; background: rgba(20,5,5,0.5); border: 1px solid rgba(255,69,58,0.15); "
            "border-radius: 4px; padding: 6px 14px;"
                )
                return

            self._voice_active = True
            self._voice_indicator.setText("● VOICE: ACTIVE")
            self._voice_indicator.setStyleSheet(
                "color: #00e5ff; font-family: 'Courier New', monospace; "
                "font-size: 10px; background: rgba(0,6,18,0.6); border: 1px solid rgba(0,180,255,0.3); "
                "border-radius: 4px; padding: 4px 10px;"
            )
            recognizer = sr.Recognizer()

            def listen():
                while self._voice_active:
                    try:
                        with mic as source:
                            recognizer.adjust_for_ambient_noise(source, duration=0.5)
                            audio = recognizer.listen(source, timeout=3, phrase_time_limit=5)
                        if not self._voice_active:
                            break
                        text = recognizer.recognize_google(audio).lower()
                        if any(w in text for w in ["login", "sign in", "enter", "open"]):
                            words = text.split()
                            for i, word in enumerate(words):
                                if word in ["login", "sign", "enter", "open", "with"] and i + 1 < len(words):
                                    self.login_user.setText(words[i + 1])
                                    self.login_error.setStyleSheet("color: #00e5ff; font-size: 12px;")
                                    self.login_error.setText(f"Voice: user = '{words[i + 1]}'")
                                    break
                        elif any(w in text for w in ["password", "pass"]):
                            words = text.split()
                            for i, word in enumerate(words):
                                if word in ["password", "pass"] and i + 1 < len(words):
                                    self.login_pass.setText(words[i + 1])
                                    self.login_error.setStyleSheet("color: #00e5ff; font-size: 12px;")
                                    self.login_error.setText("Password entered via voice")
                                    break
                        elif any(w in text for w in ["go", "submit", "ok"]):
                            self.do_login()
                    except sr.WaitTimeoutError:
                        continue
                    except sr.UnknownValueError:
                        continue
                    except Exception:
                        continue

            self._voice_thread = threading.Thread(target=listen, daemon=True)
            self._voice_thread.start()
        except ImportError:
            self._voice_indicator.setText("● VOICE: UNAVAIL")
            self._voice_indicator.setStyleSheet(
                "color: rgba(255,69,58,0.7); font-family: 'Courier New', monospace; "
                "font-size: 10px; background: rgba(20,5,5,0.5); border: 1px solid rgba(255,69,58,0.15); "
                "border-radius: 4px; padding: 4px 10px;"
            )
        except Exception:
            self._voice_indicator.setText("● VOICE: ERROR")
            self._voice_indicator.setStyleSheet(
                "color: rgba(255,69,58,0.7); font-family: 'Courier New', monospace; "
                "font-size: 10px; background: rgba(20,5,5,0.5); border: 1px solid rgba(255,69,58,0.15); "
                "border-radius: 4px; padding: 4px 10px;"
            )

    # ─── Login Page ─────────────────────────────────────────────────────

    def _build_login_page(self):
        page = QWidget()
        page.setStyleSheet("background: transparent;")
        layout = QVBoxLayout(page)
        layout.setSpacing(8)
        layout.setContentsMargins(30, 20, 30, 20)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)

        logo = QLabel("⬡")
        logo.setAlignment(Qt.AlignmentFlag.AlignCenter)
        logo.setStyleSheet("font-size: 36px; color: #00b4ff; background: transparent;")
        layout.addWidget(logo)

        title = QLabel("OZAYN")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title.setStyleSheet("font-size: 22px; font-weight: bold; color: #00e5ff; letter-spacing: 6px; background: transparent;")
        layout.addWidget(title)

        subtitle = QLabel("DIGITAL TWIN INTELLIGENCE")
        subtitle.setAlignment(Qt.AlignmentFlag.AlignCenter)
        subtitle.setStyleSheet("font-size: 12px; color: rgba(0,180,255,0.4); letter-spacing: 3px; background: transparent;")
        layout.addWidget(subtitle)

        layout.addSpacing(12)

        self.login_user = QLineEdit()
        self.login_user.setPlaceholderText("USERNAME")
        self.login_user.setFixedHeight(38)
        self.login_user.setStyleSheet("""
            QLineEdit { background: rgba(0,180,255,0.06); color: #00e5ff;
                border: 1px solid rgba(0,180,255,0.2); border-radius: 8px;
                padding: 0 14px; font-size: 13px; font-family: 'Courier New', monospace; }
            QLineEdit:focus { border-color: #00b4ff; background: rgba(0,180,255,0.1); }
            QLineEdit::placeholder { color: rgba(0,180,255,0.3); }
        """)
        layout.addWidget(self.login_user)

        pass_row = QHBoxLayout()
        pass_row.setSpacing(0)
        self.login_pass = QLineEdit()
        self.login_pass.setPlaceholderText("PASSWORD")
        self.login_pass.setEchoMode(QLineEdit.EchoMode.Password)
        self.login_pass.setFixedHeight(38)
        self.login_pass.setStyleSheet("""
            QLineEdit { background: rgba(0,180,255,0.06); color: #00e5ff;
                border: 1px solid rgba(0,180,255,0.2); border-radius: 8px;
                padding: 0 14px; font-size: 13px; font-family: 'Courier New', monospace; }
            QLineEdit:focus { border-color: #00b4ff; background: rgba(0,180,255,0.1); }
            QLineEdit::placeholder { color: rgba(0,180,255,0.3); }
        """)
        pass_row.addWidget(self.login_pass)
        self._eye_btn = QPushButton("VISIBILITY_OFF")
        self._eye_btn.setFixedSize(38, 38)
        self._eye_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self._eye_btn.setStyleSheet("""
            QPushButton { background: transparent; color: rgba(0,180,255,0.4); border: none;
                font-size: 12px; font-family: 'Courier New', monospace; }
            QPushButton:hover { color: #00e5ff; }
        """)
        self._eye_btn.clicked.connect(self._toggle_password_visibility)
        pass_row.addWidget(self._eye_btn)
        layout.addLayout(pass_row)

        self.login_btn = QPushButton("ACCESS SYSTEM")
        self.login_btn.setFixedHeight(40)
        self.login_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.login_btn.setStyleSheet("""
            QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                stop:0 rgba(0,100,200,0.6), stop:1 rgba(0,180,255,0.4));
                color: #00e5ff; border: 1px solid rgba(0,180,255,0.4); border-radius: 8px;
                font-size: 13px; font-weight: bold; letter-spacing: 2px;
                font-family: 'Courier New', monospace; }
            QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                stop:0 rgba(0,140,255,0.7), stop:1 rgba(0,220,255,0.5));
                border-color: #00e5ff; }
            QPushButton:pressed { background: rgba(0,80,160,0.6); }
            QPushButton:disabled { background: rgba(255,255,255,0.05); color: rgba(255,255,255,0.2); }
        """)
        self.login_btn.clicked.connect(self.do_login)
        layout.addWidget(self.login_btn)

        self.login_error = QLabel("")
        self.login_error.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.login_error.setStyleSheet("color: #ff453a; font-size: 20px; background: transparent;")
        layout.addWidget(self.login_error)

        layout.addSpacing(4)

        auth_label = QLabel("─ AUTHENTICATE WITH ─")
        auth_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        auth_label.setStyleSheet("color: rgba(0,180,255,0.3); font-size: 14px; letter-spacing: 2px; background: transparent;")
        layout.addWidget(auth_label)

        auth_row = QHBoxLayout()
        auth_row.setSpacing(8)
        auth_row.setAlignment(Qt.AlignmentFlag.AlignCenter)
        auth_row.addWidget(self._auth_btn("VOX", "Voice", self._manual_voice))
        auth_row.addWidget(self._auth_btn("FACE", "Face", self._toggle_camera))
        auth_row.addWidget(self._auth_btn("HAND", "Gesture", self._toggle_hand_info))
        auth_row.addWidget(self._auth_btn("KBD", "Keys", self._toggle_virtual_keyboard))
        layout.addLayout(auth_row)

        layout.addSpacing(6)

        switch_reg = QPushButton("CREATE ACCOUNT >")
        switch_reg.setStyleSheet("""
            QPushButton { background: transparent; color: rgba(0,180,255,0.4); border: none;
                font-size: 16px; letter-spacing: 1px; font-family: 'Courier New', monospace; }
            QPushButton:hover { color: #00e5ff; }
        """)
        switch_reg.clicked.connect(lambda: self.stack.setCurrentIndex(1))
        layout.addWidget(switch_reg, alignment=Qt.AlignmentFlag.AlignCenter)

        self.stack.addWidget(page)

    def _auth_btn(self, icon, label, callback):
        btn = QPushButton(f"{icon}\n{label}")
        btn.setFixedSize(100, 68)
        btn.setCursor(Qt.CursorShape.PointingHandCursor)
        btn.setStyleSheet("""
            QPushButton { background: rgba(0,180,255,0.04); color: rgba(0,180,255,0.6);
                border: 1px solid rgba(0,180,255,0.12); border-radius: 10px;
                font-size: 14px; padding: 8px 2px; font-family: 'Courier New', monospace;
                letter-spacing: 1px; }
            QPushButton:hover { background: rgba(0,180,255,0.12); border-color: rgba(0,180,255,0.4);
                color: #00e5ff; }
        """)
        btn.clicked.connect(callback)
        return btn

    # ─── Register Page ──────────────────────────────────────────────────

    def _build_register_page(self):
        page = QWidget()
        page.setStyleSheet("background: transparent;")
        layout = QVBoxLayout(page)
        layout.setSpacing(8)
        layout.setContentsMargins(30, 16, 30, 16)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)

        back_btn = QPushButton("< BACK")
        back_btn.setStyleSheet("""
            QPushButton { background: transparent; color: rgba(0,180,255,0.4); border: none;
                font-size: 16px; font-family: 'Courier New', monospace; }
            QPushButton:hover { color: #00e5ff; }
        """)
        back_btn.clicked.connect(lambda: self.stack.setCurrentIndex(0))
        layout.addWidget(back_btn)

        reg_title = QLabel("NEW IDENTITY")
        reg_title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        reg_title.setStyleSheet("font-size: 18px; font-weight: bold; color: #00e5ff; letter-spacing: 3px; background: transparent;")
        layout.addWidget(reg_title)

        reg_sub = QLabel("INITIALIZE DIGITAL TWIN")
        reg_sub.setAlignment(Qt.AlignmentFlag.AlignCenter)
        reg_sub.setStyleSheet("font-size: 12px; color: rgba(0,180,255,0.35); letter-spacing: 2px; background: transparent;")
        layout.addWidget(reg_sub)

        layout.addSpacing(8)

        field_style = """
            QLineEdit { background: rgba(0,180,255,0.06); color: #00e5ff;
                border: 1px solid rgba(0,180,255,0.2); border-radius: 8px;
                padding: 0 14px; font-size: 13px; height: 38px;
                font-family: 'Courier New', monospace; }
            QLineEdit:focus { border-color: #00b4ff; background: rgba(0,180,255,0.1); }
            QLineEdit::placeholder { color: rgba(0,180,255,0.3); }
        """

        self.reg_user = QLineEdit()
        self.reg_user.setPlaceholderText("USERNAME")
        self.reg_user.setFixedHeight(38)
        self.reg_user.setStyleSheet(field_style)
        layout.addWidget(self.reg_user)

        self.reg_email = QLineEdit()
        self.reg_email.setPlaceholderText("EMAIL (OPTIONAL)")
        self.reg_email.setFixedHeight(38)
        self.reg_email.setStyleSheet(field_style)
        layout.addWidget(self.reg_email)

        self.reg_fullname = QLineEdit()
        self.reg_fullname.setPlaceholderText("FULL NAME (OPTIONAL)")
        self.reg_fullname.setFixedHeight(38)
        self.reg_fullname.setStyleSheet(field_style)
        layout.addWidget(self.reg_fullname)

        reg_pass_row = QHBoxLayout()
        reg_pass_row.setSpacing(0)
        self.reg_pass = QLineEdit()
        self.reg_pass.setPlaceholderText("PASSWORD")
        self.reg_pass.setEchoMode(QLineEdit.EchoMode.Password)
        self.reg_pass.setFixedHeight(38)
        self.reg_pass.setStyleSheet(field_style)
        reg_pass_row.addWidget(self.reg_pass)
        reg_eye = QPushButton("VISIBILITY_OFF")
        reg_eye.setFixedSize(38, 38)
        reg_eye.setCursor(Qt.CursorShape.PointingHandCursor)
        reg_eye.setStyleSheet("QPushButton { background: transparent; color: rgba(0,180,255,0.4); border: none; font-size: 12px; font-family: 'Courier New', monospace; } QPushButton:hover { color: #00e5ff; }")
        reg_eye.clicked.connect(lambda: self._toggle_echo(self.reg_pass, reg_eye))
        reg_pass_row.addWidget(reg_eye)
        layout.addLayout(reg_pass_row)

        reg_pass2_row = QHBoxLayout()
        reg_pass2_row.setSpacing(0)
        self.reg_pass2 = QLineEdit()
        self.reg_pass2.setPlaceholderText("CONFIRM PASSWORD")
        self.reg_pass2.setEchoMode(QLineEdit.EchoMode.Password)
        self.reg_pass2.setFixedHeight(38)
        self.reg_pass2.setStyleSheet(field_style)
        reg_pass2_row.addWidget(self.reg_pass2)
        reg_eye2 = QPushButton("VISIBILITY_OFF")
        reg_eye2.setFixedSize(38, 38)
        reg_eye2.setCursor(Qt.CursorShape.PointingHandCursor)
        reg_eye2.setStyleSheet("QPushButton { background: transparent; color: rgba(0,180,255,0.4); border: none; font-size: 12px; font-family: 'Courier New', monospace; } QPushButton:hover { color: #00e5ff; }")
        reg_eye2.clicked.connect(lambda: self._toggle_echo(self.reg_pass2, reg_eye2))
        reg_pass2_row.addWidget(reg_eye2)
        layout.addLayout(reg_pass2_row)

        self.reg_btn = QPushButton("INITIALIZE")
        self.reg_btn.setFixedHeight(40)
        self.reg_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.reg_btn.setStyleSheet("""
            QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                stop:0 rgba(0,100,200,0.6), stop:1 rgba(0,180,255,0.4));
                color: #00e5ff; border: 1px solid rgba(0,180,255,0.4); border-radius: 8px;
                font-size: 13px; font-weight: bold; letter-spacing: 2px;
                font-family: 'Courier New', monospace; }
            QPushButton:hover { background: rgba(0,140,255,0.6); border-color: #00e5ff; }
            QPushButton:disabled { background: rgba(255,255,255,0.05); color: rgba(255,255,255,0.2); }
        """)
        self.reg_btn.clicked.connect(self.do_register)
        layout.addWidget(self.reg_btn)

        self.reg_error = QLabel("")
        self.reg_error.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.reg_error.setStyleSheet("color: #ff453a; font-size: 20px; background: transparent;")
        layout.addWidget(self.reg_error)

        layout.addSpacing(4)

        auth_label = QLabel("─ OR AUTHENTICATE WITH ─")
        auth_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        auth_label.setStyleSheet("color: rgba(0,180,255,0.3); font-size: 14px; letter-spacing: 2px; background: transparent;")
        layout.addWidget(auth_label)

        auth_row = QHBoxLayout()
        auth_row.setSpacing(8)
        auth_row.setAlignment(Qt.AlignmentFlag.AlignCenter)
        auth_row.addWidget(self._auth_btn("VOX", "Voice", self._manual_voice))
        auth_row.addWidget(self._auth_btn("FACE", "Face", self._toggle_camera))
        auth_row.addWidget(self._auth_btn("HAND", "Gesture", self._toggle_hand_info))
        auth_row.addWidget(self._auth_btn("KBD", "Keys", self._toggle_virtual_keyboard))
        layout.addLayout(auth_row)

        self.stack.addWidget(page)

    # ─── Virtual Keyboard ───────────────────────────────────────────────

    def _build_virtual_keyboard(self):
        self._vk = VirtualKeyboard()
        self._vk.hide()
        self._vk.key_pressed.connect(self._vk_key_handler)
        self._vk.backspace_pressed.connect(self._vk_backspace)
        self._vk.enter_pressed.connect(self._vk_enter)

    def _toggle_virtual_keyboard(self):
        self._keyboard_visible = not self._keyboard_visible
        if self._keyboard_visible:
            self._vk.setParent(self._glass_container)
            self._vk.show()
            self._active_field = self.login_pass
            self.login_pass.setFocus()
            gpos = self._glass_container.pos()
            self._vk.move(10, self._glass_container.height() + 5)
        else:
            self._vk.hide()

    def _vk_key_handler(self, key):
        if self._active_field:
            self._active_field.insert(key)

    def _vk_backspace(self):
        if self._active_field:
            self._active_field.setText(self._active_field.text()[:-1])

    def _vk_enter(self):
        if self.stack.currentIndex() == 0:
            self.do_login()
        else:
            self.do_register()

    # ─── Eye Toggle ─────────────────────────────────────────────────────

    def _toggle_password_visibility(self):
        if self.login_pass.echoMode() == QLineEdit.EchoMode.Password:
            self.login_pass.setEchoMode(QLineEdit.EchoMode.Normal)
            self._eye_btn.setText("VISIBILITY")
        else:
            self.login_pass.setEchoMode(QLineEdit.EchoMode.Password)
            self._eye_btn.setText("VISIBILITY_OFF")

    def _toggle_echo(self, field, btn):
        if field.echoMode() == QLineEdit.EchoMode.Password:
            field.setEchoMode(QLineEdit.EchoMode.Normal)
            btn.setText("VISIBILITY")
        else:
            field.setEchoMode(QLineEdit.EchoMode.Password)
            btn.setText("VISIBILITY_OFF")

    # ─── Voice ──────────────────────────────────────────────────────────

    def _manual_voice(self):
        if self._voice_active:
            self._voice_active = False
            self._voice_indicator.setText("● VOICE: STOPPED")
            self._voice_indicator.setStyleSheet(
                "color: rgba(255,255,255,0.3); font-family: 'Courier New', monospace; "
                "font-size: 14px; background: rgba(10,10,10,0.5); border: 1px solid rgba(255,255,255,0.08); "
                "border-radius: 4px; padding: 6px 14px;"
            )
            return
        self._auto_voice_listen()

    # ─── Camera / Hand ──────────────────────────────────────────────────

    def _toggle_camera(self):
        if self._bg_camera._camera and self._bg_camera._camera.isOpened():
            self._bg_camera.stop()
            self._cam_indicator.setText("● CAM: OFF")
            self._cam_indicator.setStyleSheet(
                "color: rgba(255,255,255,0.3); font-family: 'Courier New', monospace; "
                "font-size: 14px; background: rgba(10,10,10,0.5); border: 1px solid rgba(255,255,255,0.08); "
                "border-radius: 4px; padding: 6px 14px;"
            )
        else:
            self._start_camera()

    def _toggle_hand_info(self):
        if self._bg_camera._face_mesh:
            self.login_error.setStyleSheet("color: #00e5ff; font-size: 20px; background: transparent;")
            self.login_error.setText("Move your mouth to control cursor — open to click")
        else:
            self.login_error.setStyleSheet("color: #ff9f0a; font-size: 20px; background: transparent;")
            self.login_error.setText("MediaPipe not available for mouth tracking")

    # ─── Passkey ────────────────────────────────────────────────────────

    def _start_passkey(self):
        self.login_error.setStyleSheet("color: #00e5ff; font-size: 20px; background: transparent;")
        self.login_error.setText("Passkey auth requires FIDO2 security key")

    # ─── Login / Register ───────────────────────────────────────────────

    def do_login(self):
        u = self.login_user.text().strip()
        p = self.login_pass.text()
        if not u or not p:
            self.login_error.setStyleSheet("color: #ff453a; font-size: 20px; background: transparent;")
            self.login_error.setText("Enter username and password")
            return
        self.login_error.setText("")
        self.login_btn.setEnabled(False)
        self.login_btn.setText("AUTHENTICATING...")
        self._run("login", {"username": u, "password": p}, self._on_login)

    def _on_login(self, r):
        self._login_result.emit(r)

    def _on_login_result(self, r):
        self.login_btn.setEnabled(True)
        self.login_btn.setText("ACCESS SYSTEM")
        if r.get("success"):
            if r.get("requires_2fa"):
                dialog = TwoFADialog(self)
                if dialog.exec() == QDialog.DialogCode.Accepted and dialog.verified:
                    QTimer.singleShot(0, self.on_success)
                else:
                    self.login_error.setStyleSheet("color: #ff453a; font-size: 20px; background: transparent;")
                    self.login_error.setText("2FA verification failed")
            else:
                QTimer.singleShot(0, self.on_success)
        else:
            self.login_error.setStyleSheet("color: #ff453a; font-size: 20px; background: transparent;")
            self.login_error.setText(r.get("error", "Login failed"))

    def do_register(self):
        u = self.reg_user.text().strip()
        p = self.reg_pass.text()
        p2 = self.reg_pass2.text()
        e = self.reg_email.text().strip() or None
        fn = self.reg_fullname.text().strip() or None

        if not u or not p:
            self.reg_error.setStyleSheet("color: #ff453a; font-size: 20px; background: transparent;")
            self.reg_error.setText("Enter username and password")
            return
        if p != p2:
            self.reg_error.setStyleSheet("color: #ff453a; font-size: 20px; background: transparent;")
            self.reg_error.setText("Passwords do not match")
            return
        if len(p) < 6:
            self.reg_error.setStyleSheet("color: #ff453a; font-size: 20px; background: transparent;")
            self.reg_error.setText("Password must be at least 6 characters")
            return

        self.reg_error.setText("")
        self.reg_btn.setEnabled(False)
        self.reg_btn.setText("CREATING...")
        self._run("register", {"username": u, "password": p, "email": e, "fullname": fn}, self._on_reg)

    def _on_reg(self, r):
        self._reg_result.emit(r)

    def _on_reg_result(self, r):
        self.reg_btn.setEnabled(True)
        self.reg_btn.setText("INITIALIZE")
        if r.get("success"):
            self.reg_error.setStyleSheet("color: #00e5ff; font-size: 20px; background: transparent;")
            self.reg_error.setText("Identity created — sign in")
            self.stack.setCurrentIndex(0)
            self.login_user.setText(self.reg_user.text().strip())
        else:
            self.reg_error.setStyleSheet("color: #ff453a; font-size: 20px; background: transparent;")
            self.reg_error.setText(r.get("error", "Registration failed"))

    def closeEvent(self, event):
        self._voice_active = False
        self._bg_camera.stop()
        event.accept()
