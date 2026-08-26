"""
Ozayn Login Window — CIA Surveillance Style
Fullscreen camera background with blue monochromatic filter.
Glass-effect floating login form. Auto-voice on launch.
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
    QLineEdit, QPushButton, QStackedWidget, QDialog, QFrame,
    QGraphicsDropShadowEffect
)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QSize, QRect
from PyQt6.QtGui import QFont, QColor, QPixmap, QImage, QPainter, QPen

from ozayn.workers import WorkerMixin


# ─── Full Virtual Keyboard ──────────────────────────────────────────────────

class VirtualKeyboard(QWidget):
    """Full QWERTY on-screen keyboard."""

    key_pressed = pyqtSignal(str)
    enter_pressed = pyqtSignal()
    backspace_pressed = pyqtSignal()
    shift_pressed = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.shift_on = False
        self.mode = "alpha"
        self._init_ui()

    def _init_ui(self):
        self.setStyleSheet(
            "background: rgba(5,12,28,0.92); border: 1px solid rgba(0,180,255,0.15); "
            "border-radius: 12px; padding: 6px;"
        )
        layout = QVBoxLayout(self)
        layout.setContentsMargins(6, 6, 6, 6)
        layout.setSpacing(3)

        # Number row
        self._num_row_widget = QWidget()
        nr = QHBoxLayout(self._num_row_widget)
        nr.setContentsMargins(0, 0, 0, 0)
        nr.setSpacing(2)
        for ch in "1234567890":
            btn = self._key_btn(ch, 30, 32, small=True)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            nr.addWidget(btn)
        layout.addWidget(self._num_row_widget)

        # Row 1
        row1 = QHBoxLayout()
        row1.setSpacing(2)
        for ch in "QWERTYUIOP":
            btn = self._key_btn(ch, 34, 38)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            row1.addWidget(btn)
        layout.addLayout(row1)

        # Row 2
        row2 = QHBoxLayout()
        row2.setSpacing(2)
        row2.addSpacing(16)
        for ch in "ASDFGHJKL":
            btn = self._key_btn(ch, 34, 38)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            row2.addWidget(btn)
        row2.addSpacing(16)
        layout.addLayout(row2)

        # Row 3
        row3 = QHBoxLayout()
        row3.setSpacing(2)
        row3.addSpacing(32)
        for ch in "ZXCVBNM":
            btn = self._key_btn(ch, 34, 38)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            row3.addWidget(btn)
        row3.addSpacing(8)
        bs = self._key_btn("⌫", 50, 38, color="#00b4ff")
        bs.clicked.connect(self.backspace_pressed.emit)
        row3.addWidget(bs)
        row3.addSpacing(32)
        layout.addLayout(row3)

        # Row 4
        row4 = QHBoxLayout()
        row4.setSpacing(4)
        shift = self._key_btn("⇧", 44, 38, color="#00b4ff")
        shift.clicked.connect(self._toggle_shift)
        self._shift_btn = shift
        row4.addWidget(shift)
        space = self._key_btn("SPACE", 200, 38, color="#0a2040")
        space.clicked.connect(lambda: self._emit(" "))
        row4.addWidget(space)
        enter = self._key_btn("ENTER", 70, 38, color="#00b4ff")
        enter.clicked.connect(self.enter_pressed.emit)
        row4.addWidget(enter)
        layout.addLayout(row4)

        # Symbol row
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

        # Bottom row
        bottom = QHBoxLayout()
        bottom.setSpacing(4)
        num_switch = self._key_btn("?123", 52, 34, color="#00b4ff")
        num_switch.clicked.connect(self._toggle_symbols)
        bottom.addWidget(num_switch)
        bottom.addStretch()
        abc_switch = self._key_btn("ABC", 46, 34, color="#00b4ff")
        abc_switch.clicked.connect(self._show_alpha)
        abc_switch.hide()
        self._abc_btn = abc_switch
        bottom.addWidget(abc_switch)
        close_btn = self._key_btn("✕", 34, 34, color="#00b4ff")
        close_btn.clicked.connect(lambda: self.hide())
        bottom.addWidget(close_btn)
        layout.addLayout(bottom)

    def _key_btn(self, text, w, h, color=None, small=False):
        btn = QPushButton(text)
        btn.setFixedSize(w, h)
        btn.setCursor(Qt.CursorShape.PointingHandCursor)
        font_size = "10px" if small else "13px"
        if color:
            r, g, b = self._hex_to_rgb(color)
            bg = f"rgba({r},{g},{b},0.2)"
            border = f"rgba({r},{g},{b},0.5)"
        else:
            bg = "rgba(0,180,255,0.06)"
            border = "rgba(0,180,255,0.15)"
        btn.setStyleSheet(f"""
            QPushButton {{
                background: {bg};
                color: #00e5ff;
                border: 1px solid {border};
                border-radius: 6px;
                font-size: {font_size};
                font-weight: 600;
                font-family: 'Courier New', monospace;
            }}
            QPushButton:hover {{
                background: rgba(0,180,255,0.25);
                border-color: rgba(0,229,255,0.6);
                color: #ffffff;
            }}
            QPushButton:pressed {{
                background: rgba(0,180,255,0.4);
            }}
        """)
        return btn

    def _hex_to_rgb(self, hex_color):
        h = hex_color.lstrip('#')
        if len(h) == 3:
            h = h[0]*2 + h[1]*2 + h[2]*2
        return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)

    def _emit(self, ch):
        if self.shift_on:
            ch = ch.upper()
            self._toggle_shift()
        else:
            ch = ch.lower()
        self.key_pressed.emit(ch)

    def _toggle_shift(self):
        self.shift_on = not self.shift_on
        self._shift_btn.setStyleSheet(f"""
            QPushButton {{
                background: rgba(0,180,255,{0.4 if self.shift_on else 0.2});
                color: #00e5ff;
                border: 1px solid #00b4ff;
                border-radius: 6px;
                font-size: 14px;
                font-weight: bold;
            }}
        """)

    def _toggle_symbols(self):
        self._sym_widget.setVisible(not self._sym_widget.isVisible())
        self._num_row_widget.setVisible(not self._sym_widget.isVisible())
        self._abc_btn.setVisible(self._sym_widget.isVisible())

    def _show_alpha(self):
        self._sym_widget.hide()
        self._num_row_widget.show()
        self._abc_btn.hide()


# ─── Fullscreen CIA Camera Widget ──────────────────────────────────────────

class CIAFullscreenCamera(QLabel):
    """Full-screen camera feed with CIA blue monochromatic filter + scan lines."""

    face_detected = pyqtSignal()

    _CASCADE_PATH = None

    @classmethod
    def _find_cascade(cls):
        if cls._CASCADE_PATH:
            return cls._CASCADE_PATH
        import os
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
            cv2_dir = os.path.dirname(cv2.__file__)
            for root, dirs, files in os.walk(cv2_dir):
                for f in files:
                    if f == "haarcascade_frontalface_default.xml":
                        cls._CASCADE_PATH = os.path.join(root, f)
                        return cls._CASCADE_PATH
        except Exception:
            pass
        for p in [
            "/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",
            "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml",
        ]:
            if os.path.isfile(p):
                cls._CASCADE_PATH = p
                return p
        return None

    def __init__(self, parent=None):
        super().__init__(parent)
        self._camera = None
        self._timer = None
        self._cascade = None
        self._frame_ref = None
        self._face_box = None
        self._scan_offset = 0

    def start(self):
        try:
            import cv2
        except ImportError:
            return

        self._camera = cv2.VideoCapture(0)
        if not self._camera.isOpened():
            return

        cascade_path = self._find_cascade()
        if cascade_path:
            try:
                self._cascade = cv2.CascadeClassifier(cascade_path)
                if self._cascade.empty():
                    self._cascade = None
            except Exception:
                self._cascade = None

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._capture)
        self._timer.start(66)  # ~15fps

    def _capture(self):
        if not self._camera or not self._camera.isOpened():
            return
        ret, frame = self._camera.read()
        if not ret:
            return

        try:
            import cv2

            # Face detection
            self._face_box = None
            if self._cascade:
                try:
                    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                    faces = self._cascade.detectMultiScale(gray, 1.3, 5)
                    if len(faces) > 0:
                        self._face_box = faces[0]
                        self.face_detected.emit()
                except Exception:
                    pass

            # === CIA BLUE MONOCHROMATIC FILTER ===
            # Convert to grayscale
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

            # Apply contrast enhancement (CLAHE)
            clahe = cv2.createCLAHE(clipLimit=3.0, tileGridSize=(8, 8))
            enhanced = clahe.apply(gray)

            # Create blue monochromatic: map to blue channel
            # Blue tint: R=low, G=medium-low, B=high
            h, w = enhanced.shape
            cia = np.zeros((h, w, 3), dtype=np.uint8)
            cia[:, :, 0] = np.clip(enhanced * 0.15, 0, 255).astype(np.uint8)   # R
            cia[:, :, 1] = np.clip(enhanced * 0.45, 0, 255).astype(np.uint8)   # G
            cia[:, :, 2] = np.clip(enhanced * 1.0, 0, 255).astype(np.uint8)    # B

            # Draw face detection box
            if self._face_box is not None:
                x, y, fw, fh = self._face_box
                cv2.rectangle(cia, (x, y), (x+fw, y+fh), (0, 255, 255), 2)
                # Corner markers
                corner_len = min(20, fw // 4, fh // 4)
                color = (0, 255, 255)
                # Top-left
                cv2.line(cia, (x, y), (x+corner_len, y), color, 3)
                cv2.line(cia, (x, y), (x, y+corner_len), color, 3)
                # Top-right
                cv2.line(cia, (x+fw, y), (x+fw-corner_len, y), color, 3)
                cv2.line(cia, (x+fw, y), (x+fw, y+corner_len), color, 3)
                # Bottom-left
                cv2.line(cia, (x, y+fh), (x+corner_len, y+fh), color, 3)
                cv2.line(cia, (x, y+fh), (x, y+fh-corner_len), color, 3)
                # Bottom-right
                cv2.line(cia, (x+fw, y+fh), (x+fw-corner_len, y+fh), color, 3)
                cv2.line(cia, (x+fw, y+fh), (x+fw, y+fh-corner_len), color, 3)

            # Scan line effect
            self._scan_offset = (self._scan_offset + 2) % h
            scan_y = self._scan_offset
            cv2.line(cia, (0, scan_y), (w, scan_y), (0, 200, 255), 1)

            # Vignette effect (darken edges)
            rows, cols = cia.shape[:2]
            X = cv2.getGaussianKernel(cols, cols * 0.6)
            Y = cv2.getGaussianKernel(rows, rows * 0.6)
            mask = Y * X.T
            mask = mask / mask.max()
            mask = 1.0 - mask  # invert for vignette
            mask = (mask * 255).astype(np.uint8)
            for c in range(3):
                cia[:, :, c] = np.minimum(cia[:, :, c], mask)

            # Add scan line texture
            for sy in range(0, rows, 3):
                cia[sy, :, :] = (cia[sy, :, :] * 0.85).astype(np.uint8)

            # Convert to Qt
            rgb = cv2.cvtColor(cia, cv2.COLOR_BGR2RGB)
            h2, w2, ch2 = rgb.shape
            bytes_per_line = ch2 * w2
            self._frame_ref = rgb.copy()
            qt_img = QImage(self._frame_ref.data, w2, h2, bytes_per_line, QImage.Format.Format_RGB888)
            pixmap = QPixmap.fromImage(qt_img)

            # Scale to fill widget while keeping aspect ratio
            scaled = pixmap.scaled(
                self.size(),
                Qt.AspectRatioMode.KeepAspectRatioByExpanding,
                Qt.TransformationMode.FastTransformation
            )
            self.setPixmap(scaled)

        except Exception:
            pass

    def stop(self):
        if self._timer and self._timer.isActive():
            self._timer.stop()
        if self._camera and self._camera.isOpened():
            self._camera.release()
            self._camera = None
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
            QDialog { background: rgba(5,12,28,0.95); color: #00e5ff; }
            QLabel { color: #00e5ff; }
            QLineEdit {
                background: rgba(0,180,255,0.08);
                color: #00e5ff;
                border: 1px solid rgba(0,180,255,0.3);
                border-radius: 8px;
                padding: 10px;
                font-size: 20px;
                letter-spacing: 8px;
            }
            QLineEdit:focus { border-color: #00b4ff; }
            QPushButton {
                background: rgba(0,180,255,0.15);
                color: #00e5ff;
                border: 1px solid rgba(0,180,255,0.3);
                border-radius: 6px;
                padding: 8px 16px;
                font-size: 13px;
            }
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
        self.showMaximized()
        self.setMinimumSize(1024, 768)
        self.setStyleSheet("background: #000000;")

        self._active_field = None
        self._keyboard_visible = False
        self._camera_widget = None
        self._voice_active = False

        self._login_result.connect(self._on_login_result)
        self._reg_result.connect(self._on_reg_result)
        self._face_result.connect(self._on_face_login_result)

        # ── Layer 1: Fullscreen CIA Camera Background ──
        self._bg_camera = CIAFullscreenCamera(self)
        self._bg_camera.face_detected.connect(self._on_auto_face_login)

        # ── Layer 2: Dark overlay for depth ──
        self._overlay = QLabel(self)
        self._overlay.setStyleSheet("background: rgba(0,4,15,0.35);")
        self._overlay.hide()

        # ── Layer 3: Floating Glass Login Form ──
        self._glass_container = QWidget(self)
        self._glass_container.setFixedSize(380, 520)

        self.stack = QStackedWidget()
        self._build_login_page()
        self._build_register_page()
        self._build_virtual_keyboard()

        glass_layout = QVBoxLayout(self._glass_container)
        glass_layout.setContentsMargins(0, 0, 0, 0)
        glass_layout.addWidget(self.stack)

        # Glass shadow
        shadow = QGraphicsDropShadowEffect()
        shadow.setBlurRadius(60)
        shadow.setColor(QColor(0, 120, 255, 80))
        shadow.setOffset(0, 4)
        self._glass_container.setGraphicsEffect(shadow)

        self._apply_glass_style()

        # ── HUD overlay text ──
        self._hud_top = QLabel(self)
        self._hud_top.setStyleSheet("color: rgba(0,180,255,0.25); font-family: 'Courier New', monospace; font-size: 10px;")
        self._hud_top.setAlignment(Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft)
        self._hud_top.setFixedWidth(260)

        self._hud_bottom = QLabel(self)
        self._hud_bottom.setStyleSheet("color: rgba(0,180,255,0.2); font-family: 'Courier New', monospace; font-size: 9px;")
        self._hud_bottom.setAlignment(Qt.AlignmentFlag.AlignBottom | Qt.AlignmentFlag.AlignRight)

        # ── Voice status (top-right) ──
        self._voice_indicator = QLabel("● VOICE: INIT", self)
        self._voice_indicator.setStyleSheet(
            "color: rgba(0,180,255,0.5); font-family: 'Courier New', monospace; "
            "font-size: 10px; background: rgba(0,10,20,0.5); border: 1px solid rgba(0,180,255,0.15); "
            "border-radius: 4px; padding: 4px 10px;"
        )
        self._voice_indicator.adjustSize()

        # ── Camera status (top-right, below voice) ──
        self._cam_indicator = QLabel("● CAM: INIT", self)
        self._cam_indicator.setStyleSheet(
            "color: rgba(0,180,255,0.5); font-family: 'Courier New', monospace; "
            "font-size: 10px; background: rgba(0,10,20,0.5); border: 1px solid rgba(0,180,255,0.15); "
            "border-radius: 4px; padding: 4px 10px;"
        )
        self._cam_indicator.adjustSize()

        # Force layout after all widgets created
        QTimer.singleShot(100, self._do_layout)
        QTimer.singleShot(400, self._auto_start)

    # ─── Glass Styling ────────────────────────────────────────────────────

    def _apply_glass_style(self):
        self._glass_container.setStyleSheet("""
            #glassPanel {
                background: rgba(3,10,25,0.72);
                border: 1px solid rgba(0,180,255,0.18);
                border-radius: 16px;
            }
            #glassPanel * {
                font-family: 'Segoe UI', 'SF Pro', 'Courier New', sans-serif;
            }
        """)
        self._glass_container.setObjectName("glassPanel")
        # Make it semi-transparent
        self._glass_container.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground, False)

    # ─── Resize ───────────────────────────────────────────────────────────

    def _do_layout(self):
        """Force layout all children to correct positions/sizes."""
        sz = self.size()
        self._bg_camera.setGeometry(0, 0, sz.width(), sz.height())
        self._bg_camera.show()
        self._overlay.setGeometry(0, 0, sz.width(), sz.height())
        self._overlay.show()
        gw, gh = 380, 520
        self._glass_container.setFixedSize(gw, gh)
        self._glass_container.move((sz.width() - gw) // 2, (sz.height() - gh) // 2)
        self._glass_container.show()
        self._hud_top.move(20, sz.height() - 50)
        self._hud_bottom.move(sz.width() - 280, 20)
        self._voice_indicator.move(sz.width() - 160, 16)
        self._cam_indicator.move(sz.width() - 160, 44)

    def resizeEvent(self, event):
        super().resizeEvent(event)
        if not hasattr(self, '_bg_camera'):
            return
        self._do_layout()

    # ─── Auto Start ─────────────────────────────────────────────────────

    def _auto_start(self):
        """Auto-start camera + voice."""
        self._start_camera()
        self._auto_voice_listen()

    def _start_camera(self):
        if self._camera_widget and self._camera_widget.isVisible():
            return
        self._bg_camera.start()
        self._cam_indicator.setText("● CAM: ACTIVE")
        self._cam_indicator.setStyleSheet(
            "color: #00e5ff; font-family: 'Courier New', monospace; "
            "font-size: 10px; background: rgba(0,10,20,0.6); border: 1px solid rgba(0,180,255,0.3); "
            "border-radius: 4px; padding: 4px 10px;"
        )

    def _on_auto_face_login(self):
        self.login_error.setStyleSheet("color: #00e5ff; font-size: 12px; font-weight: bold;")
        self.login_error.setText("Face recognized — authenticating...")
        self._cam_indicator.setText("● CAM: FACE DETECTED")
        self._cam_indicator.setStyleSheet(
            "color: #00ff88; font-family: 'Courier New', monospace; "
            "font-size: 10px; background: rgba(0,20,10,0.6); border: 1px solid rgba(0,255,136,0.3); "
            "border-radius: 4px; padding: 4px 10px;"
        )
        self._run("face_login", {}, lambda r: self._face_result.emit(r))

    def _on_face_login_result(self, r):
        if r.get("success"):
            QTimer.singleShot(0, self.on_success)
        else:
            self.login_error.setStyleSheet("color: rgba(255,255,255,0.4); font-size: 12px;")
            self.login_error.setText("Face not in database — use credentials")

    # ─── Auto Voice ─────────────────────────────────────────────────────

    def _auto_voice_listen(self):
        """Auto-start voice listening in background."""
        self._voice_indicator.setText("● VOICE: LOADING...")
        self._voice_indicator.setStyleSheet(
            "color: #ff9f0a; font-family: 'Courier New', monospace; "
            "font-size: 10px; background: rgba(0,10,20,0.5); border: 1px solid rgba(255,159,10,0.15); "
            "border-radius: 4px; padding: 4px 10px;"
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
                    "font-size: 10px; background: rgba(20,5,5,0.5); border: 1px solid rgba(255,69,58,0.15); "
                    "border-radius: 4px; padding: 4px 10px;"
                )
                return

            self._voice_active = True
            self._voice_indicator.setText("● VOICE: ACTIVE")
            self._voice_indicator.setStyleSheet(
                "color: #00e5ff; font-family: 'Courier New', monospace; "
                "font-size: 10px; background: rgba(0,10,20,0.6); border: 1px solid rgba(0,180,255,0.3); "
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
                                    username = words[i + 1]
                                    self.login_user.setText(username)
                                    self.login_error.setStyleSheet("color: #00e5ff; font-size: 12px;")
                                    self.login_error.setText(f"Voice: user = '{username}'")
                                    break
                        elif any(w in text for w in ["password", "pass"]):
                            words = text.split()
                            for i, word in enumerate(words):
                                if word in ["password", "pass"] and i + 1 < len(words):
                                    password = words[i + 1]
                                    self.login_pass.setText(password)
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
            self._voice_indicator.setText("● VOICE: UNAVAILABLE")
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
        layout.setSpacing(10)
        layout.setContentsMargins(30, 20, 30, 20)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)

        # Logo
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
        subtitle.setStyleSheet("font-size: 8px; color: rgba(0,180,255,0.4); letter-spacing: 3px; background: transparent;")
        layout.addWidget(subtitle)

        layout.addSpacing(12)

        # Username
        self.login_user = QLineEdit()
        self.login_user.setPlaceholderText("USERNAME")
        self.login_user.setFixedHeight(38)
        self.login_user.setStyleSheet("""
            QLineEdit {
                background: rgba(0,180,255,0.06);
                color: #00e5ff;
                border: 1px solid rgba(0,180,255,0.2);
                border-radius: 8px;
                padding: 0 14px;
                font-size: 13px;
                font-family: 'Courier New', monospace;
                letter-spacing: 1px;
            }
            QLineEdit:focus { border-color: #00b4ff; background: rgba(0,180,255,0.1); }
            QLineEdit::placeholder { color: rgba(0,180,255,0.3); }
        """)
        layout.addWidget(self.login_user)

        # Password
        pass_row = QHBoxLayout()
        pass_row.setSpacing(0)
        self.login_pass = QLineEdit()
        self.login_pass.setPlaceholderText("PASSWORD")
        self.login_pass.setEchoMode(QLineEdit.EchoMode.Password)
        self.login_pass.setFixedHeight(38)
        self.login_pass.setStyleSheet("""
            QLineEdit {
                background: rgba(0,180,255,0.06);
                color: #00e5ff;
                border: 1px solid rgba(0,180,255,0.2);
                border-radius: 8px;
                padding: 0 14px;
                font-size: 13px;
                font-family: 'Courier New', monospace;
                letter-spacing: 1px;
            }
            QLineEdit:focus { border-color: #00b4ff; background: rgba(0,180,255,0.1); }
            QLineEdit::placeholder { color: rgba(0,180,255,0.3); }
        """)
        pass_row.addWidget(self.login_pass)

        self._eye_btn = QPushButton("VISIBILITY_OFF")
        self._eye_btn.setFixedSize(38, 38)
        self._eye_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self._eye_btn.setStyleSheet("""
            QPushButton {
                background: transparent; color: rgba(0,180,255,0.4);
                border: none; font-size: 12px; font-family: 'Courier New', monospace;
            }
            QPushButton:hover { color: #00e5ff; }
        """)
        self._eye_btn.clicked.connect(self._toggle_password_visibility)
        pass_row.addWidget(self._eye_btn)
        layout.addLayout(pass_row)

        # Login button
        self.login_btn = QPushButton("ACCESS SYSTEM")
        self.login_btn.setFixedHeight(40)
        self.login_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.login_btn.setStyleSheet("""
            QPushButton {
                background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                    stop:0 rgba(0,100,200,0.6), stop:1 rgba(0,180,255,0.4));
                color: #00e5ff; border: 1px solid rgba(0,180,255,0.4);
                border-radius: 8px;
                font-size: 13px; font-weight: bold; letter-spacing: 2px;
                font-family: 'Courier New', monospace;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                    stop:0 rgba(0,140,255,0.7), stop:1 rgba(0,220,255,0.5));
                border-color: #00e5ff;
            }
            QPushButton:pressed { background: rgba(0,80,160,0.6); }
            QPushButton:disabled { background: rgba(255,255,255,0.05); color: rgba(255,255,255,0.2); }
        """)
        self.login_btn.clicked.connect(self.do_login)
        layout.addWidget(self.login_btn)

        self.login_error = QLabel("")
        self.login_error.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.login_error.setStyleSheet("color: #ff453a; font-size: 11px; background: transparent;")
        layout.addWidget(self.login_error)

        layout.addSpacing(4)

        # Auth methods
        auth_label = QLabel("─ AUTHENTICATE WITH ─")
        auth_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        auth_label.setStyleSheet("color: rgba(0,180,255,0.3); font-size: 9px; letter-spacing: 2px; background: transparent;")
        layout.addWidget(auth_label)

        auth_row = QHBoxLayout()
        auth_row.setSpacing(8)
        auth_row.setAlignment(Qt.AlignmentFlag.AlignCenter)
        auth_row.addWidget(self._auth_btn("VOX", "Voice", self._manual_voice))
        auth_row.addWidget(self._auth_btn("FACE", "Face", self._toggle_camera))
        auth_row.addWidget(self._auth_btn("KEY", "Passkey", self._start_passkey))
        auth_row.addWidget(self._auth_btn("KBD", "Keys", self._toggle_virtual_keyboard))
        layout.addLayout(auth_row)

        layout.addSpacing(6)

        switch_reg = QPushButton("CREATE ACCOUNT >")
        switch_reg.setStyleSheet("""
            QPushButton { background: transparent; color: rgba(0,180,255,0.4); border: none;
                font-size: 11px; letter-spacing: 1px; font-family: 'Courier New', monospace; }
            QPushButton:hover { color: #00e5ff; }
        """)
        switch_reg.clicked.connect(lambda: self.stack.setCurrentIndex(1))
        layout.addWidget(switch_reg, alignment=Qt.AlignmentFlag.AlignCenter)

        self.stack.addWidget(page)

    def _auth_btn(self, icon, label, callback):
        btn = QPushButton(f"{icon}\n{label}")
        btn.setFixedSize(78, 52)
        btn.setCursor(Qt.CursorShape.PointingHandCursor)
        btn.setStyleSheet("""
            QPushButton {
                background: rgba(0,180,255,0.04);
                color: rgba(0,180,255,0.6);
                border: 1px solid rgba(0,180,255,0.12);
                border-radius: 8px;
                font-size: 10px;
                padding: 6px 2px;
                font-family: 'Courier New', monospace;
                letter-spacing: 1px;
            }
            QPushButton:hover {
                background: rgba(0,180,255,0.12);
                border-color: rgba(0,180,255,0.4);
                color: #00e5ff;
            }
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
                font-size: 11px; font-family: 'Courier New', monospace; }
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
        reg_sub.setStyleSheet("font-size: 8px; color: rgba(0,180,255,0.35); letter-spacing: 2px; background: transparent;")
        layout.addWidget(reg_sub)

        layout.addSpacing(8)

        field_style = """
            QLineEdit {
                background: rgba(0,180,255,0.06);
                color: #00e5ff;
                border: 1px solid rgba(0,180,255,0.2);
                border-radius: 8px;
                padding: 0 14px;
                font-size: 13px;
                height: 38px;
                font-family: 'Courier New', monospace;
            }
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
        reg_eye.setStyleSheet("""
            QPushButton { background: transparent; color: rgba(0,180,255,0.4); border: none;
                font-size: 12px; font-family: 'Courier New', monospace; }
            QPushButton:hover { color: #00e5ff; }
        """)
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
        reg_eye2.setStyleSheet("""
            QPushButton { background: transparent; color: rgba(0,180,255,0.4); border: none;
                font-size: 12px; font-family: 'Courier New', monospace; }
            QPushButton:hover { color: #00e5ff; }
        """)
        reg_eye2.clicked.connect(lambda: self._toggle_echo(self.reg_pass2, reg_eye2))
        reg_pass2_row.addWidget(reg_eye2)
        layout.addLayout(reg_pass2_row)

        self.reg_btn = QPushButton("INITIALIZE")
        self.reg_btn.setFixedHeight(40)
        self.reg_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.reg_btn.setStyleSheet("""
            QPushButton {
                background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                    stop:0 rgba(0,100,200,0.6), stop:1 rgba(0,180,255,0.4));
                color: #00e5ff; border: 1px solid rgba(0,180,255,0.4);
                border-radius: 8px;
                font-size: 13px; font-weight: bold; letter-spacing: 2px;
                font-family: 'Courier New', monospace;
            }
            QPushButton:hover { background: rgba(0,140,255,0.6); border-color: #00e5ff; }
            QPushButton:disabled { background: rgba(255,255,255,0.05); color: rgba(255,255,255,0.2); }
        """)
        self.reg_btn.clicked.connect(self.do_register)
        layout.addWidget(self.reg_btn)

        self.reg_error = QLabel("")
        self.reg_error.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.reg_error.setStyleSheet("color: #ff453a; font-size: 11px; background: transparent;")
        layout.addWidget(self.reg_error)

        layout.addSpacing(4)

        auth_label = QLabel("─ OR AUTHENTICATE WITH ─")
        auth_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        auth_label.setStyleSheet("color: rgba(0,180,255,0.3); font-size: 9px; letter-spacing: 2px; background: transparent;")
        layout.addWidget(auth_label)

        auth_row = QHBoxLayout()
        auth_row.setSpacing(8)
        auth_row.setAlignment(Qt.AlignmentFlag.AlignCenter)
        auth_row.addWidget(self._auth_btn("VOX", "Voice", self._manual_voice))
        auth_row.addWidget(self._auth_btn("FACE", "Face", self._toggle_camera))
        auth_row.addWidget(self._auth_btn("KEY", "Passkey", self._start_passkey))
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
            # Position below glass
            gpos = self._glass_container.pos()
            self._vk.move(gpos.x() + 10, gpos.y() + self._glass_container.height() + 5)
        else:
            self._vk.hide()

    def _vk_key_handler(self, key):
        if self._active_field:
            self._active_field.insert(key)

    def _vk_backspace(self):
        if self._active_field:
            text = self._active_field.text()
            self._active_field.setText(text[:-1])

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
                "font-size: 10px; background: rgba(10,10,10,0.5); border: 1px solid rgba(255,255,255,0.08); "
                "border-radius: 4px; padding: 4px 10px;"
            )
            return
        self._auto_voice_listen()

    # ─── Camera Toggle ──────────────────────────────────────────────────

    def _toggle_camera(self):
        if self._bg_camera._camera and self._bg_camera._camera.isOpened():
            self._bg_camera.stop()
            self._cam_indicator.setText("● CAM: OFF")
            self._cam_indicator.setStyleSheet(
                "color: rgba(255,255,255,0.3); font-family: 'Courier New', monospace; "
                "font-size: 10px; background: rgba(10,10,10,0.5); border: 1px solid rgba(255,255,255,0.08); "
                "border-radius: 4px; padding: 4px 10px;"
            )
        else:
            self._start_camera()

    # ─── Passkey ────────────────────────────────────────────────────────

    def _start_passkey(self):
        self.login_error.setStyleSheet("color: #00e5ff; font-size: 11px; background: transparent;")
        self.login_error.setText("Passkey auth requires FIDO2 security key")

    # ─── Login / Register ───────────────────────────────────────────────

    def do_login(self):
        u = self.login_user.text().strip()
        p = self.login_pass.text()
        if not u or not p:
            self.login_error.setStyleSheet("color: #ff453a; font-size: 11px; background: transparent;")
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
                    self.login_error.setStyleSheet("color: #ff453a; font-size: 11px; background: transparent;")
                    self.login_error.setText("2FA verification failed")
            else:
                QTimer.singleShot(0, self.on_success)
        else:
            self.login_error.setStyleSheet("color: #ff453a; font-size: 11px; background: transparent;")
            self.login_error.setText(r.get("error", "Login failed"))

    def do_register(self):
        u = self.reg_user.text().strip()
        p = self.reg_pass.text()
        p2 = self.reg_pass2.text()
        e = self.reg_email.text().strip() or None
        fn = self.reg_fullname.text().strip() or None

        if not u or not p:
            self.reg_error.setStyleSheet("color: #ff453a; font-size: 11px; background: transparent;")
            self.reg_error.setText("Enter username and password")
            return
        if p != p2:
            self.reg_error.setStyleSheet("color: #ff453a; font-size: 11px; background: transparent;")
            self.reg_error.setText("Passwords do not match")
            return
        if len(p) < 6:
            self.reg_error.setStyleSheet("color: #ff453a; font-size: 11px; background: transparent;")
            self.reg_error.setText("Password must be at least 6 characters")
            return

        self.reg_error.setText("")
        self.reg_btn.setEnabled(False)
        self.reg_btn.setText("CREATING...")
        self._run("register", {
            "username": u, "password": p, "email": e, "fullname": fn
        }, self._on_reg)

    def _on_reg(self, r):
        self._reg_result.emit(r)

    def _on_reg_result(self, r):
        self.reg_btn.setEnabled(True)
        self.reg_btn.setText("INITIALIZE")
        if r.get("success"):
            self.reg_error.setStyleSheet("color: #00e5ff; font-size: 11px; background: transparent;")
            self.reg_error.setText("Identity created — sign in")
            self.stack.setCurrentIndex(0)
            self.login_user.setText(self.reg_user.text().strip())
        else:
            self.reg_error.setStyleSheet("color: #ff453a; font-size: 11px; background: transparent;")
            self.reg_error.setText(r.get("error", "Registration failed"))

    def closeEvent(self, event):
        self._voice_active = False
        self._bg_camera.stop()
        event.accept()
