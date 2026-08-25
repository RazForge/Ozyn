"""
Ozayn Login Window — Multimodal Authentication UI
Auto-starts camera + voice on launch.
Supports: Text, Voice, Face, Passkey, Virtual Keyboard, 2FA
"""

import os
os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = '1'
os.environ['SDL_AUDIODRIVER'] = 'dummy'

import warnings
warnings.filterwarnings("ignore")

import threading

from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QLabel,
    QLineEdit, QPushButton, QStackedWidget, QDialog,
    QGridLayout, QSizePolicy
)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QSize
from PyQt6.QtGui import QFont, QColor, QPixmap, QImage

from ozayn.theme.dark import DARK_STYLE
from ozayn.workers import WorkerMixin


# ─── Full Virtual Keyboard ──────────────────────────────────────────────────

class VirtualKeyboard(QWidget):
    """Full QWERTY on-screen keyboard with numbers, symbols, and clear visibility."""

    key_pressed = pyqtSignal(str)
    enter_pressed = pyqtSignal()
    backspace_pressed = pyqtSignal()
    shift_pressed = pyqtSignal()
    tab_pressed = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.shift_on = False
        self.mode = "alpha"  # alpha, numbers, symbols
        self._init_ui()

    def _init_ui(self):
        self.setStyleSheet("background: rgba(20,20,30,0.95); border-radius: 12px; padding: 6px;")
        layout = QVBoxLayout(self)
        layout.setContentsMargins(6, 6, 6, 6)
        layout.setSpacing(3)

        # Number row (always visible in alpha mode as number shortcuts)
        self._num_row_widget = QWidget()
        nr = QHBoxLayout(self._num_row_widget)
        nr.setContentsMargins(0, 0, 0, 0)
        nr.setSpacing(2)
        for ch in "1234567890":
            btn = self._key_btn(ch, 30, 32, small=True)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            nr.addWidget(btn)
        layout.addWidget(self._num_row_widget)

        # Row 1: QWERTYUIOP
        row1 = QHBoxLayout()
        row1.setSpacing(2)
        for ch in "QWERTYUIOP":
            btn = self._key_btn(ch, 34, 38)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            row1.addWidget(btn)
        layout.addLayout(row1)

        # Row 2: ASDFGHJKL
        row2 = QHBoxLayout()
        row2.setSpacing(2)
        row2.addSpacing(16)
        for ch in "ASDFGHJKL":
            btn = self._key_btn(ch, 34, 38)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            row2.addWidget(btn)
        row2.addSpacing(16)
        layout.addLayout(row2)

        # Row 3: ZXCVBNM + Backspace
        row3 = QHBoxLayout()
        row3.setSpacing(2)
        row3.addSpacing(32)
        for ch in "ZXCVBNM":
            btn = self._key_btn(ch, 34, 38)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            row3.addWidget(btn)
        row3.addSpacing(8)
        bs = self._key_btn("⌫", 50, 38, color="#ff6b6b")
        bs.clicked.connect(self.backspace_pressed.emit)
        row3.addWidget(bs)
        row3.addSpacing(32)
        layout.addLayout(row3)

        # Row 4: Shift + Space + Enter
        row4 = QHBoxLayout()
        row4.setSpacing(4)

        shift = self._key_btn("⇧", 44, 38, color="#0a84ff")
        shift.clicked.connect(self._toggle_shift)
        self._shift_btn = shift
        row4.addWidget(shift)

        space = self._key_btn("SPACE", 200, 38, color="#555")
        space.clicked.connect(lambda: self._emit(" "))
        row4.addWidget(space)

        enter = self._key_btn("ENTER", 70, 38, color="#30d158")
        enter.clicked.connect(self.enter_pressed.emit)
        row4.addWidget(enter)

        layout.addLayout(row4)

        # Symbol row (hidden by default)
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

        # Bottom row: mode switchers
        bottom = QHBoxLayout()
        bottom.setSpacing(4)

        num_switch = self._key_btn("?123", 52, 34, color="#0a84ff")
        num_switch.clicked.connect(self._toggle_symbols)
        bottom.addWidget(num_switch)

        bottom.addStretch()

        abc_switch = self._key_btn("ABC", 46, 34, color="#30d158")
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
        font_size = "10px" if small else "13px"
        bg = f"rgba({self._hex_to_rgb(color)},0.2)" if color else "rgba(255,255,255,0.08)"
        border_color = color if color else "rgba(255,255,255,0.15)"
        btn.setStyleSheet(f"""
            QPushButton {{
                background: {bg};
                color: #ffffff;
                border: 1px solid {border_color};
                border-radius: 6px;
                font-size: {font_size};
                font-weight: 600;
                font-family: 'Segoe UI', 'SF Pro', monospace;
            }}
            QPushButton:hover {{
                background: rgba(255,255,255,0.15);
                border-color: rgba(10,132,255,0.6);
            }}
            QPushButton:pressed {{
                background: rgba(10,132,255,0.4);
            }}
        """)
        return btn

    def _hex_to_rgb(self, hex_color):
        if not hex_color:
            return "255,255,255"
        h = hex_color.lstrip('#')
        if len(h) == 3:
            h = h[0]*2 + h[1]*2 + h[2]*2
        r, g, b = int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)
        return f"{r},{g},{b}"

    def _emit(self, ch):
        if self.shift_on:
            ch = ch.upper()
            self._toggle_shift()
        else:
            ch = ch.lower()
        self.key_pressed.emit(ch)

    def _toggle_shift(self):
        self.shift_on = not self.shift_on
        bg = "rgba(10,132,255,0.4)" if self.shift_on else "rgba(10,132,255,0.2)"
        self._shift_btn.setStyleSheet(f"""
            QPushButton {{
                background: {bg};
                color: #ffffff;
                border: 1px solid #0a84ff;
                border-radius: 6px;
                font-size: 14px;
                font-weight: bold;
            }}
            QPushButton:hover {{ background: rgba(10,132,255,0.5); }}
        """)

    def _toggle_symbols(self):
        self._sym_widget.setVisible(not self._sym_widget.isVisible())
        self._num_row_widget.setVisible(not self._sym_widget.isVisible())
        self._abc_btn.setVisible(self._sym_widget.isVisible())

    def _show_alpha(self):
        self._sym_widget.hide()
        self._num_row_widget.show()
        self._abc_btn.hide()


# ─── Inline Camera Widget ───────────────────────────────────────────────────

class InlineCameraWidget(QWidget):
    """Small camera feed that shows in the login form."""

    face_detected = pyqtSignal()
    face_failed = pyqtSignal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._camera = None
        self._timer = None
        self._init_ui()

    def _init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)

        self._feed = QLabel("📷 Starting camera...")
        self._feed.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._feed.setFixedSize(200, 150)
        self._feed.setStyleSheet("""
            QLabel {
                background: #111122;
                color: rgba(255,255,255,0.5);
                border: 2px solid rgba(10,132,255,0.3);
                border-radius: 10px;
                font-size: 12px;
            }
        """)
        layout.addWidget(self._feed, alignment=Qt.AlignmentFlag.AlignCenter)

        self._status = QLabel("Initializing camera...")
        self._status.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._status.setStyleSheet("color: rgba(255,255,255,0.5); font-size: 10px;")
        layout.addWidget(self._status)

    def start(self):
        try:
            import cv2
            self._camera = cv2.VideoCapture(0)
            if self._camera.isOpened():
                self._status.setText("Looking for face...")
                self._status.setStyleSheet("color: #0a84ff; font-size: 10px;")
                self._timer = QTimer()
                self._timer.timeout.connect(self._capture)
                self._timer.start(150)
            else:
                self._status.setText("Camera not available")
                self._status.setStyleSheet("color: #ff9f0a; font-size: 10px;")
        except ImportError:
            self._status.setText("OpenCV not installed")
            self._status.setStyleSheet("color: #ff9f0a; font-size: 10px;")

    def _capture(self):
        if not self._camera or not self._camera.isOpened():
            return
        ret, frame = self._camera.read()
        if not ret:
            return

        try:
            import cv2
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            cascade = cv2.CascadeClassifier(
                cv2.data.haarcascades + "haarcascade_frontalface_default.xml"
            )
            faces = cascade.detectMultiScale(gray, 1.3, 5)

            if len(faces) > 0:
                for (x, y, w, h) in faces:
                    cv2.rectangle(frame, (x, y), (x+w, y+h), (10, 132, 255), 2)

                self._status.setText("Face detected!")
                self._status.setStyleSheet("color: #30d158; font-size: 10px; font-weight: bold;")
                self.stop()
                self.face_detected.emit()
                return

            self._status.setText("Looking for face...")
            self._status.setStyleSheet("color: #0a84ff; font-size: 10px;")

            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            h, w, ch = rgb.shape
            bytes_per_line = ch * w
            qt_img = QImage(rgb.data, w, h, bytes_per_line, QImage.Format.Format_RGB888)
            pixmap = QPixmap.fromImage(qt_img).scaled(
                200, 150, Qt.AspectRatioMode.KeepAspectRatio,
                Qt.TransformationMode.SmoothTransformation
            )
            self._feed.setPixmap(pixmap)
        except Exception as e:
            self._status.setText(f"Error: {str(e)[:30]}")

    def stop(self):
        if self._timer and self._timer.isActive():
            self._timer.stop()
        if self._camera and self._camera.isOpened():
            self._camera.release()
            self._camera = None

    def hideEvent(self, event):
        self.stop()
        super().hideEvent(event)


# ─── 2FA Dialog ─────────────────────────────────────────────────────────────

class TwoFADialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Two-Factor Authentication")
        self.setFixedSize(320, 220)
        self.setStyleSheet(DARK_STYLE)
        self.verified = False

        layout = QVBoxLayout(self)
        layout.setSpacing(12)

        icon = QLabel("🔐")
        icon.setAlignment(Qt.AlignmentFlag.AlignCenter)
        icon.setStyleSheet("font-size: 36px;")
        layout.addWidget(icon)

        title = QLabel("Enter 2FA Code")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title.setStyleSheet("font-size: 16px; font-weight: bold; color: #0a84ff;")
        layout.addWidget(title)

        self.code_input = QLineEdit()
        self.code_input.setPlaceholderText("000000")
        self.code_input.setMaxLength(6)
        self.code_input.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.code_input.setFont(QFont("Courier", 20))
        self.code_input.setStyleSheet("""
            QLineEdit {
                background: rgba(255,255,255,0.06);
                color: #ffffff;
                border: 1px solid rgba(255,255,255,0.15);
                border-radius: 8px;
                padding: 10px;
                letter-spacing: 8px;
            }
            QLineEdit:focus { border-color: #0a84ff; }
        """)
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
    # Signals to marshal worker callbacks to main thread
    _login_result = pyqtSignal(dict)
    _reg_result = pyqtSignal(dict)
    _face_result = pyqtSignal(dict)

    def __init__(self, api, on_success):
        super().__init__()
        self.api = api
        self.on_success = on_success
        self._init_workers()
        self.setWindowTitle("Ozayn — Multimodal Authentication")
        self.setFixedSize(480, 780)
        self.setStyleSheet(DARK_STYLE)

        self._active_field = None
        self._keyboard_visible = False
        self._camera_widget = None
        self._voice_active = False

        # Connect signals to main-thread handlers
        self._login_result.connect(self._on_login_result)
        self._reg_result.connect(self._on_reg_result)
        self._face_result.connect(self._on_face_login_result)

        central = QWidget()
        self.setCentralWidget(central)
        self.main_layout = QVBoxLayout(central)
        self.main_layout.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.main_layout.setSpacing(0)

        self.stack = QStackedWidget()
        self.main_layout.addWidget(self.stack)

        self._build_login_page()
        self._build_register_page()
        self._build_virtual_keyboard()

        # Auto-start camera + voice after window shows
        QTimer.singleShot(500, self._auto_start)

    # ─── Auto Start ─────────────────────────────────────────────────────

    def _auto_start(self):
        """Auto-start camera on login screen. Voice only on manual click."""
        try:
            self._start_camera()
        except Exception:
            pass
        self._voice_status.setText("🎙 Click Voice to enable")
        self._voice_status.setStyleSheet("color: rgba(255,255,255,0.4); font-size: 10px;")

    def _start_camera(self):
        """Start inline camera for face detection."""
        if self._camera_widget and self._camera_widget.isVisible():
            return
        try:
            self._camera_widget = InlineCameraWidget()
            self._camera_widget.face_detected.connect(self._on_auto_face_login)
            self._camera_widget.face_failed.connect(self._on_face_failed)
            self.main_layout.insertWidget(self.main_layout.count() - 1, self._camera_widget)
            self._camera_widget.show()
            self._camera_widget.start()
        except Exception:
            pass

    def _on_auto_face_login(self):
        """Auto-login when face is detected."""
        self.login_error.setStyleSheet("color: #30d158; font-size: 12px; font-weight: bold;")
        self.login_error.setText("Face recognized! Logging in...")
        self._run("face_login", {}, lambda r: self._face_result.emit(r))

    def _on_face_login_result(self, r):
        if r.get("success"):
            self.on_success()
        else:
            self.login_error.setStyleSheet("color: #ff9f0a; font-size: 12px;")
            self.login_error.setText("Face not in database — use username/password")

    def _on_face_failed(self, msg):
        pass  # Silently handle — camera stays running

    def _auto_voice_listen(self):
        """Auto-start voice listening in background."""
        try:
            # Suppress ALSA/JACK stderr noise during pyaudio init
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
                self._voice_status.setText("🎙 Voice unavailable (no microphone)")
                self._voice_status.setStyleSheet("color: #ff9f0a; font-size: 10px;")
                return

            self._voice_active = True
            self._voice_status.setText("🎙 Listening for voice...")
            self._voice_status.setStyleSheet("color: #0a84ff; font-size: 10px;")

            recognizer = sr.Recognizer()

            def listen():
                import time
                while self._voice_active:
                    try:
                        with mic as source:
                            recognizer.adjust_for_ambient_noise(source, duration=0.5)
                            audio = recognizer.listen(source, timeout=3, phrase_time_limit=5)
                        if not self._voice_active:
                            break
                        text = recognizer.recognize_google(audio).lower()

                        if any(word in text for word in ["login", "sign in", "enter", "open"]):
                            # Extract username from voice
                            words = text.split()
                            for i, word in enumerate(words):
                                if word in ["login", "sign", "enter", "open", "with"] and i + 1 < len(words):
                                    username = words[i + 1]
                                    self.login_user.setText(username)
                                    self.login_error.setStyleSheet("color: #30d158; font-size: 12px;")
                                    self.login_error.setText(f"Voice: username = '{username}'")
                                    break
                        elif any(word in text for word in ["password", "pass"]):
                            words = text.split()
                            for i, word in enumerate(words):
                                if word in ["password", "pass"] and i + 1 < len(words):
                                    password = words[i + 1]
                                    self.login_pass.setText(password)
                                    self.login_error.setStyleSheet("color: #30d158; font-size: 12px;")
                                    self.login_error.setText("Password entered via voice")
                                    break
                        elif any(word in text for word in ["go", "submit", "ok"]):
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
            self._voice_status.setText("🎙 Voice unavailable")
            self._voice_status.setStyleSheet("color: #ff9f0a; font-size: 10px;")
        except Exception:
            self._voice_status.setText("🎙 Voice unavailable")
            self._voice_status.setStyleSheet("color: #ff9f0a; font-size: 10px;")

    # ─── Login Page ─────────────────────────────────────────────────────

    def _build_login_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setSpacing(8)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)

        # Logo
        logo = QLabel("⬡")
        logo.setAlignment(Qt.AlignmentFlag.AlignCenter)
        logo.setStyleSheet("""
            font-size: 44px; color: #0a84ff;
            background: rgba(10,132,255,0.1);
            border-radius: 26px; width: 60px; height: 60px;
        """)
        logo.setFixedSize(60, 60)
        layout.addWidget(logo)

        title = QLabel("OZAYN")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title.setStyleSheet("font-size: 24px; font-weight: bold; color: #0a84ff; letter-spacing: 4px;")
        layout.addWidget(title)

        subtitle = QLabel("DIGITAL TWIN INTELLIGENCE")
        subtitle.setAlignment(Qt.AlignmentFlag.AlignCenter)
        subtitle.setStyleSheet("font-size: 9px; color: rgba(255,255,255,0.4); letter-spacing: 2px;")
        layout.addWidget(subtitle)

        layout.addSpacing(10)

        # Username
        self.login_user = QLineEdit()
        self.login_user.setPlaceholderText("Username")
        self.login_user.setFixedHeight(40)
        self.login_user.setStyleSheet("""
            QLineEdit {
                background: rgba(255,255,255,0.06);
                color: #ffffff;
                border: 1px solid rgba(255,255,255,0.12);
                border-radius: 10px;
                padding: 0 14px;
                font-size: 14px;
            }
            QLineEdit:focus { border-color: #0a84ff; }
        """)
        layout.addWidget(self.login_user)

        # Password with eye toggle
        pass_row = QHBoxLayout()
        pass_row.setSpacing(0)
        self.login_pass = QLineEdit()
        self.login_pass.setPlaceholderText("Password")
        self.login_pass.setEchoMode(QLineEdit.EchoMode.Password)
        self.login_pass.setFixedHeight(40)
        self.login_pass.setStyleSheet("""
            QLineEdit {
                background: rgba(255,255,255,0.06);
                color: #ffffff;
                border: 1px solid rgba(255,255,255,0.12);
                border-radius: 10px;
                padding: 0 14px;
                font-size: 14px;
            }
            QLineEdit:focus { border-color: #0a84ff; }
        """)
        pass_row.addWidget(self.login_pass)

        self._eye_btn = QPushButton("👁")
        self._eye_btn.setFixedSize(40, 40)
        self._eye_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self._eye_btn.setStyleSheet("""
            QPushButton {
                background: transparent; color: rgba(255,255,255,0.4);
                border: none; font-size: 16px; margin-left: -40px;
            }
            QPushButton:hover { color: #ffffff; }
        """)
        self._eye_btn.clicked.connect(self._toggle_password_visibility)
        pass_row.addWidget(self._eye_btn)
        layout.addLayout(pass_row)

        # Login button
        self.login_btn = QPushButton("ENTER OZAYN")
        self.login_btn.setFixedHeight(42)
        self.login_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.login_btn.setStyleSheet("""
            QPushButton {
                background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                    stop:0 #0a84ff, stop:1 #5e5ce6);
                color: white; border: none; border-radius: 10px;
                font-size: 14px; font-weight: bold; letter-spacing: 1px;
            }
            QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                stop:0 #409cff, stop:1 #7a78ff); }
            QPushButton:pressed { background: #0060cc; }
            QPushButton:disabled { background: rgba(255,255,255,0.1); color: rgba(255,255,255,0.3); }
        """)
        self.login_btn.clicked.connect(self.do_login)
        layout.addWidget(self.login_btn)

        self.login_error = QLabel("")
        self.login_error.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.login_error.setStyleSheet("color: #ff453a; font-size: 12px;")
        layout.addWidget(self.login_error)

        # Voice status indicator
        self._voice_status = QLabel("")
        self._voice_status.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._voice_status.setStyleSheet("color: rgba(255,255,255,0.4); font-size: 10px;")
        layout.addWidget(self._voice_status)

        layout.addSpacing(4)

        # Auth methods row
        auth_label = QLabel("Authenticate with")
        auth_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        auth_label.setStyleSheet("color: rgba(255,255,255,0.3); font-size: 10px;")
        layout.addWidget(auth_label)

        auth_row = QHBoxLayout()
        auth_row.setSpacing(6)
        auth_row.setAlignment(Qt.AlignmentFlag.AlignCenter)

        auth_row.addWidget(self._auth_btn("🎙", "Voice", self._manual_voice))
        auth_row.addWidget(self._auth_btn("👤", "Face", self._toggle_camera))
        auth_row.addWidget(self._auth_btn("🔑", "Passkey", self._start_passkey))
        auth_row.addWidget(self._auth_btn("⌨", "Keyboard", self._toggle_virtual_keyboard))
        layout.addLayout(auth_row)

        layout.addSpacing(4)

        switch_reg = QPushButton("Create Account →")
        switch_reg.setStyleSheet("""
            QPushButton { background: transparent; color: rgba(255,255,255,0.5); border: none; font-size: 12px; }
            QPushButton:hover { color: #0a84ff; }
        """)
        switch_reg.clicked.connect(lambda: self.stack.setCurrentIndex(1))
        layout.addWidget(switch_reg, alignment=Qt.AlignmentFlag.AlignCenter)

        self.stack.addWidget(page)

    def _auth_btn(self, icon, label, callback):
        btn = QPushButton(f"{icon}\n{label}")
        btn.setFixedSize(80, 58)
        btn.setCursor(Qt.CursorShape.PointingHandCursor)
        btn.setStyleSheet("""
            QPushButton {
                background: rgba(255,255,255,0.04);
                color: rgba(255,255,255,0.7);
                border: 1px solid rgba(255,255,255,0.08);
                border-radius: 10px;
                font-size: 11px;
                padding: 6px 2px;
            }
            QPushButton:hover {
                background: rgba(10,132,255,0.15);
                border-color: rgba(10,132,255,0.4);
                color: #ffffff;
            }
        """)
        btn.clicked.connect(callback)
        return btn

    # ─── Register Page ──────────────────────────────────────────────────

    def _build_register_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setSpacing(8)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)

        back_btn = QPushButton("← Back")
        back_btn.setStyleSheet("""
            QPushButton { background: transparent; color: rgba(255,255,255,0.5); border: none; font-size: 12px; }
            QPushButton:hover { color: #0a84ff; }
        """)
        back_btn.clicked.connect(lambda: self.stack.setCurrentIndex(0))
        layout.addWidget(back_btn)

        layout.addSpacing(2)

        reg_title = QLabel("Create Account")
        reg_title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        reg_title.setStyleSheet("font-size: 22px; font-weight: bold; color: #0a84ff;")
        layout.addWidget(reg_title)

        reg_sub = QLabel("Set up your Digital Twin")
        reg_sub.setAlignment(Qt.AlignmentFlag.AlignCenter)
        reg_sub.setStyleSheet("font-size: 10px; color: rgba(255,255,255,0.4);")
        layout.addWidget(reg_sub)

        layout.addSpacing(8)

        field_style = """
            QLineEdit {
                background: rgba(255,255,255,0.06);
                color: #ffffff;
                border: 1px solid rgba(255,255,255,0.12);
                border-radius: 10px;
                padding: 0 14px;
                font-size: 14px;
                height: 40px;
            }
            QLineEdit:focus { border-color: #0a84ff; }
        """

        self.reg_user = QLineEdit()
        self.reg_user.setPlaceholderText("Username")
        self.reg_user.setFixedHeight(40)
        self.reg_user.setStyleSheet(field_style)
        layout.addWidget(self.reg_user)

        self.reg_email = QLineEdit()
        self.reg_email.setPlaceholderText("Email (optional)")
        self.reg_email.setFixedHeight(40)
        self.reg_email.setStyleSheet(field_style)
        layout.addWidget(self.reg_email)

        self.reg_fullname = QLineEdit()
        self.reg_fullname.setPlaceholderText("Full Name (optional)")
        self.reg_fullname.setFixedHeight(40)
        self.reg_fullname.setStyleSheet(field_style)
        layout.addWidget(self.reg_fullname)

        # Password
        reg_pass_row = QHBoxLayout()
        reg_pass_row.setSpacing(0)
        self.reg_pass = QLineEdit()
        self.reg_pass.setPlaceholderText("Password")
        self.reg_pass.setEchoMode(QLineEdit.EchoMode.Password)
        self.reg_pass.setFixedHeight(40)
        self.reg_pass.setStyleSheet(field_style)
        reg_pass_row.addWidget(self.reg_pass)
        reg_eye = QPushButton("👁")
        reg_eye.setFixedSize(40, 40)
        reg_eye.setCursor(Qt.CursorShape.PointingHandCursor)
        reg_eye.setStyleSheet("""
            QPushButton { background: transparent; color: rgba(255,255,255,0.4); border: none; font-size: 16px; margin-left: -40px; }
            QPushButton:hover { color: #ffffff; }
        """)
        reg_eye.clicked.connect(lambda: self._toggle_echo(self.reg_pass, reg_eye))
        reg_pass_row.addWidget(reg_eye)
        layout.addLayout(reg_pass_row)

        # Confirm password
        reg_pass2_row = QHBoxLayout()
        reg_pass2_row.setSpacing(0)
        self.reg_pass2 = QLineEdit()
        self.reg_pass2.setPlaceholderText("Confirm Password")
        self.reg_pass2.setEchoMode(QLineEdit.EchoMode.Password)
        self.reg_pass2.setFixedHeight(40)
        self.reg_pass2.setStyleSheet(field_style)
        reg_pass2_row.addWidget(self.reg_pass2)
        reg_eye2 = QPushButton("👁")
        reg_eye2.setFixedSize(40, 40)
        reg_eye2.setCursor(Qt.CursorShape.PointingHandCursor)
        reg_eye2.setStyleSheet("""
            QPushButton { background: transparent; color: rgba(255,255,255,0.4); border: none; font-size: 16px; margin-left: -40px; }
            QPushButton:hover { color: #ffffff; }
        """)
        reg_eye2.clicked.connect(lambda: self._toggle_echo(self.reg_pass2, reg_eye2))
        reg_pass2_row.addWidget(reg_eye2)
        layout.addLayout(reg_pass2_row)

        self.reg_btn = QPushButton("CREATE ACCOUNT")
        self.reg_btn.setFixedHeight(42)
        self.reg_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.reg_btn.setStyleSheet("""
            QPushButton {
                background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                    stop:0 #30d158, stop:1 #34c759);
                color: white; border: none; border-radius: 10px;
                font-size: 14px; font-weight: bold; letter-spacing: 1px;
            }
            QPushButton:hover { background: #28b74a; }
            QPushButton:disabled { background: rgba(255,255,255,0.1); color: rgba(255,255,255,0.3); }
        """)
        self.reg_btn.clicked.connect(self.do_register)
        layout.addWidget(self.reg_btn)

        self.reg_error = QLabel("")
        self.reg_error.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.reg_error.setStyleSheet("color: #ff453a; font-size: 12px;")
        layout.addWidget(self.reg_error)

        layout.addSpacing(6)

        auth_label = QLabel("Or authenticate with")
        auth_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        auth_label.setStyleSheet("color: rgba(255,255,255,0.3); font-size: 10px;")
        layout.addWidget(auth_label)

        auth_row = QHBoxLayout()
        auth_row.setSpacing(6)
        auth_row.setAlignment(Qt.AlignmentFlag.AlignCenter)
        auth_row.addWidget(self._auth_btn("🎙", "Voice", self._manual_voice))
        auth_row.addWidget(self._auth_btn("👤", "Face", self._toggle_camera))
        auth_row.addWidget(self._auth_btn("🔑", "Passkey", self._start_passkey))
        auth_row.addWidget(self._auth_btn("⌨", "Keyboard", self._toggle_virtual_keyboard))
        layout.addLayout(auth_row)

        self.stack.addWidget(page)

    # ─── Virtual Keyboard ───────────────────────────────────────────────

    def _build_virtual_keyboard(self):
        self._vk = VirtualKeyboard()
        self._vk.hide()
        self._vk.key_pressed.connect(self._vk_key_handler)
        self._vk.backspace_pressed.connect(self._vk_backspace)
        self._vk.enter_pressed.connect(self._vk_enter)
        self.main_layout.addWidget(self._vk)

    def _toggle_virtual_keyboard(self):
        self._keyboard_visible = not self._keyboard_visible
        self._vk.setVisible(self._keyboard_visible)
        if self._keyboard_visible:
            self._active_field = self.login_pass
            self.login_pass.setFocus()

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
            self._eye_btn.setText("🙈")
        else:
            self.login_pass.setEchoMode(QLineEdit.EchoMode.Password)
            self._eye_btn.setText("👁")

    def _toggle_echo(self, field, btn):
        if field.echoMode() == QLineEdit.EchoMode.Password:
            field.setEchoMode(QLineEdit.EchoMode.Normal)
            btn.setText("🙈")
        else:
            field.setEchoMode(QLineEdit.EchoMode.Password)
            btn.setText("👁")

    # ─── Voice ──────────────────────────────────────────────────────────

    def _manual_voice(self):
        """Manual voice trigger — listen once. Only imports pyaudio on click."""
        if self._voice_active:
            self._voice_active = False
            self._voice_status.setText("🎙 Voice stopped")
            self._voice_status.setStyleSheet("color: rgba(255,255,255,0.4); font-size: 10px;")
            return
        self._auto_voice_listen()

    # ─── Camera Toggle ──────────────────────────────────────────────────

    def _toggle_camera(self):
        if self._camera_widget and self._camera_widget.isVisible():
            self._camera_widget.stop()
            self._camera_widget.hide()
        else:
            self._start_camera()

    # ─── Passkey ────────────────────────────────────────────────────────

    def _start_passkey(self):
        self.login_error.setStyleSheet("color: #0a84ff; font-size: 12px;")
        self.login_error.setText("Passkey auth requires a security key (FIDO2)")

    # ─── Login / Register ───────────────────────────────────────────────

    def do_login(self):
        u = self.login_user.text().strip()
        p = self.login_pass.text()
        if not u or not p:
            self.login_error.setStyleSheet("color: #ff453a; font-size: 12px;")
            self.login_error.setText("Please enter username and password")
            return
        self.login_error.setText("")
        self.login_btn.setEnabled(False)
        self.login_btn.setText("AUTHENTICATING...")
        self._run("login", {"username": u, "password": p}, self._on_login)

    def _on_login(self, r):
        self._login_result.emit(r)

    def _on_login_result(self, r):
        self.login_btn.setEnabled(True)
        self.login_btn.setText("ENTER OZAYN")
        if r.get("success"):
            if r.get("requires_2fa"):
                dialog = TwoFADialog(self)
                if dialog.exec() == QDialog.DialogCode.Accepted and dialog.verified:
                    self.on_success()
                else:
                    self.login_error.setStyleSheet("color: #ff453a; font-size: 12px;")
                    self.login_error.setText("2FA verification failed")
            else:
                self.on_success()
        else:
            self.login_error.setStyleSheet("color: #ff453a; font-size: 12px;")
            self.login_error.setText(r.get("error", "Login failed"))

    def do_register(self):
        u = self.reg_user.text().strip()
        p = self.reg_pass.text()
        p2 = self.reg_pass2.text()
        e = self.reg_email.text().strip() or None
        fn = self.reg_fullname.text().strip() or None

        if not u or not p:
            self.reg_error.setStyleSheet("color: #ff453a; font-size: 12px;")
            self.reg_error.setText("Please enter username and password")
            return
        if p != p2:
            self.reg_error.setStyleSheet("color: #ff453a; font-size: 12px;")
            self.reg_error.setText("Passwords do not match")
            return
        if len(p) < 6:
            self.reg_error.setStyleSheet("color: #ff453a; font-size: 12px;")
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
        self.reg_btn.setText("CREATE ACCOUNT")
        if r.get("success"):
            self.reg_error.setStyleSheet("color: #30d158; font-size: 12px;")
            self.reg_error.setText("Account created! You can now sign in.")
            self.stack.setCurrentIndex(0)
            self.login_user.setText(self.reg_user.text().strip())
        else:
            self.reg_error.setStyleSheet("color: #ff453a; font-size: 12px;")
            self.reg_error.setText(r.get("error", "Registration failed"))

    def closeEvent(self, event):
        """Clean up camera and voice on close."""
        self._voice_active = False
        if self._camera_widget:
            self._camera_widget.stop()
        event.accept()
