"""
Ozayn Login Window — Multimodal Authentication UI
Supports: Text, Voice, Face, Passkey, Virtual Keyboard, 2FA
"""

from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QLabel,
    QLineEdit, QPushButton, QStackedWidget, QFrame, QDialog,
    QGridLayout, QSizePolicy, QSpacerItem, QGraphicsDropShadowEffect,
    QTextEdit, QScrollArea
)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QThread, QSize
from PyQt6.QtGui import QFont, QColor, QIcon, QPainter, QPen, QPixmap

from ozayn.theme.dark import DARK_STYLE
from ozayn.workers import WorkerMixin


# ─── Virtual Keyboard Widget ────────────────────────────────────────────────

class VirtualKeyboard(QWidget):
    """On-screen keyboard for secure input without physical keyboard."""

    key_pressed = pyqtSignal(str)
    enter_pressed = pyqtSignal()
    backspace_pressed = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.caps_lock = False
        self.show_numbers = False
        self._init_ui()

    def _init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)
        layout.setSpacing(3)

        # Row 1: QWERTYUIOP
        row1 = QHBoxLayout()
        row1.setSpacing(2)
        for ch in "QWERTYUIOP":
            btn = self._make_key(ch, 32, 36)
            btn.clicked.connect(lambda _, c=ch: self._on_key(c))
            row1.addWidget(btn)
        layout.addLayout(row1)

        # Row 2: ASDFGHJKL
        row2 = QHBoxLayout()
        row2.setSpacing(2)
        row2.addSpacing(14)
        for ch in "ASDFGHJKL":
            btn = self._make_key(ch, 32, 36)
            btn.clicked.connect(lambda _, c=ch: self._on_key(c))
            row2.addWidget(btn)
        row2.addSpacing(14)
        layout.addLayout(row2)

        # Row 3: ZXCVBNM + Backspace
        row3 = QHBoxLayout()
        row3.setSpacing(2)
        row3.addSpacing(28)
        for ch in "ZXCVBNM":
            btn = self._make_key(ch, 32, 36)
            btn.clicked.connect(lambda _, c=ch: self._on_key(c))
            row3.addWidget(btn)
        row3.addSpacing(10)
        bs_btn = self._make_key("⌫", 44, 36)
        bs_btn.setStyleSheet(bs_btn.styleSheet().replace("color:#e0e0e0", "color:#ff6b6b"))
        bs_btn.clicked.connect(self.backspace_pressed.emit)
        row3.addWidget(bs_btn)
        row3.addSpacing(28)
        layout.addLayout(row3)

        # Row 4: Numbers toggle + Space + Enter + Caps
        row4 = QHBoxLayout()
        row4.setSpacing(4)

        num_btn = self._make_key("123", 48, 36)
        num_btn.setStyleSheet(num_btn.styleSheet().replace("background:rgba(255,255,255,0.06)", "background:rgba(10,132,255,0.2)"))
        num_btn.clicked.connect(self._toggle_numbers)
        row4.addWidget(num_btn)

        caps_btn = self._make_key("⇧", 40, 36)
        caps_btn.setStyleSheet(caps_btn.styleSheet().replace("background:rgba(255,255,255,0.06)", "background:rgba(255,255,255,0.1)"))
        caps_btn.clicked.connect(self._toggle_caps)
        self._caps_btn = caps_btn
        row4.addWidget(caps_btn)

        sp_btn = self._make_key("SPACE", 160, 36)
        sp_btn.clicked.connect(lambda: self.key_pressed.emit(" "))
        row4.addWidget(sp_btn)

        enter_btn = self._make_key("ENTER", 60, 36)
        enter_btn.setStyleSheet(enter_btn.styleSheet().replace("background:rgba(255,255,255,0.06)", "background:rgba(48,209,88,0.25)"))
        enter_btn.clicked.connect(self.enter_pressed.emit)
        row4.addWidget(enter_btn)

        layout.addLayout(row4)

        # Number/symbol row (hidden by default)
        self._num_row = QWidget()
        nr = QHBoxLayout(self._num_row)
        nr.setContentsMargins(0, 0, 0, 0)
        nr.setSpacing(2)
        for ch in "1234567890!@#$%^&*()":
            btn = self._make_key(ch, 28, 30)
            btn.clicked.connect(lambda _, c=ch: self._on_key(c))
            nr.addWidget(btn)
        self._num_row.hide()
        layout.insertWidget(0, self._num_row)

    def _make_key(self, text, w, h):
        btn = QPushButton(text)
        btn.setFixedSize(w, h)
        btn.setCursor(Qt.CursorShape.PointingHandCursor)
        btn.setStyleSheet("""
            QPushButton {
                background: rgba(255,255,255,0.06);
                color: #e0e0e0;
                border: 1px solid rgba(255,255,255,0.1);
                border-radius: 5px;
                font-size: 12px;
                font-weight: 500;
            }
            QPushButton:hover {
                background: rgba(255,255,255,0.12);
                border-color: rgba(10,132,255,0.4);
            }
            QPushButton:pressed {
                background: rgba(10,132,255,0.3);
            }
        """)
        return btn

    def _on_key(self, ch):
        if self.caps_lock:
            ch = ch.upper()
        else:
            ch = ch.lower()
        self.key_pressed.emit(ch)

    def _toggle_caps(self):
        self.caps_lock = not self.caps_lock
        color = "rgba(10,132,255,0.3)" if self.caps_lock else "rgba(255,255,255,0.1)"
        self._caps_btn.setStyleSheet(f"""
            QPushButton {{
                background: {color};
                color: #e0e0e0;
                border: 1px solid rgba(255,255,255,0.1);
                border-radius: 5px;
                font-size: 14px;
                font-weight: bold;
            }}
            QPushButton:hover {{ background: rgba(255,255,255,0.15); }}
        """)

    def _toggle_numbers(self):
        self.show_numbers = not self.show_numbers
        self._num_row.setVisible(self.show_numbers)


# ─── Face Camera Widget ─────────────────────────────────────────────────────

class FaceCameraWidget(QWidget):
    """Camera view for face detection and authentication."""

    face_detected = pyqtSignal()
    face_failed = pyqtSignal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._camera = None
        self._init_ui()

    def _init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        # Camera feed placeholder
        self._feed = QLabel("📹 Camera Starting...")
        self._feed.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._feed.setFixedSize(320, 240)
        self._feed.setStyleSheet("""
            QLabel {
                background: #1a1a2e;
                color: rgba(255,255,255,0.5);
                border: 2px solid rgba(10,132,255,0.3);
                border-radius: 12px;
                font-size: 14px;
            }
        """)
        layout.addWidget(self._feed, alignment=Qt.AlignmentFlag.AlignCenter)

        # Status
        self._status = QLabel("Initializing camera...")
        self._status.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._status.setStyleSheet("color: rgba(255,255,255,0.6); font-size: 12px; margin-top: 8px;")
        layout.addWidget(self._status)

        # Cancel button
        cancel = QPushButton("Cancel")
        cancel.setObjectName("text-btn")
        cancel.setFixedWidth(100)
        cancel.clicked.connect(self.stop)
        layout.addWidget(cancel, alignment=Qt.AlignmentFlag.AlignCenter)

    def start(self):
        """Start camera capture."""
        try:
            import cv2
            self._camera = cv2.VideoCapture(0)
            if self._camera.isOpened():
                self._status.setText("Scanning for face...")
                self._timer = QTimer()
                self._timer.timeout.connect(self._capture_frame)
                self._timer.start(100)  # 10 fps
            else:
                self._status.setText("Camera not available")
                self.face_failed.emit("Camera not available")
        except ImportError:
            self._status.setText("OpenCV not installed — face detection unavailable")
            self.face_failed.emit("OpenCV not installed")

    def _capture_frame(self):
        if not self._camera or not self._camera.isOpened():
            return
        ret, frame = self._camera.read()
        if not ret:
            return

        # Detect face using OpenCV Haar cascade
        try:
            import cv2
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            cascade = cv2.CascadeClassifier(
                cv2.data.haarcascades + "haarcascade_frontalface_default.xml"
            )
            faces = cascade.detectMultiScale(gray, 1.3, 5)

            if len(faces) > 0:
                # Draw rectangle around face
                for (x, y, w, h) in faces:
                    cv2.rectangle(frame, (x, y), (x+w, y+h), (10, 132, 255), 2)

                self._status.setText("Face detected! Authenticating...")
                self._status.setStyleSheet("color: #30d158; font-size: 12px; margin-top: 8px;")
                self.stop()
                self.face_detected.emit()
            else:
                self._status.setText("Looking for face...")
                self._status.setStyleSheet("color: rgba(255,255,255,0.6); font-size: 12px; margin-top: 8px;")

            # Convert frame to QPixmap for display
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            h, w, ch = rgb.shape
            bytes_per_line = ch * w
            from PyQt6.QtGui import QImage
            qt_img = QImage(rgb.data, w, h, bytes_per_line, QImage.Format.Format_RGB888)
            pixmap = QPixmap.fromImage(qt_img).scaled(
                320, 240, Qt.AspectRatioMode.KeepAspectRatio,
                Qt.TransformationMode.SmoothTransformation
            )
            self._feed.setPixmap(pixmap)

        except Exception as e:
            self._status.setText(f"Error: {str(e)}")

    def stop(self):
        if hasattr(self, '_timer') and self._timer.isActive():
            self._timer.stop()
        if self._camera and self._camera.isOpened():
            self._camera.release()
            self._camera = None
        self._feed.setText("📹 Camera Off")
        self._feed.setPixmap(QPixmap())


# ─── 2FA Dialog ─────────────────────────────────────────────────────────────

class TwoFADialog(QDialog):
    """Two-Factor Authentication dialog."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Two-Factor Authentication")
        self.setFixedSize(320, 200)
        self.setStyleSheet(DARK_STYLE)
        self.verified = False
        self._init_ui()

    def _init_ui(self):
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
        # TODO: verify against server TOTP
        self.verified = True
        self.accept()


# ─── Auth Method Button Factory ─────────────────────────────────────────────

def make_auth_button(icon, label, tooltip, callback=None):
    """Create a styled auth method button."""
    btn = QPushButton(f"{icon}\n{label}")
    btn.setToolTip(tooltip)
    btn.setFixedSize(80, 64)
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
        QPushButton:pressed {
            background: rgba(10,132,255,0.25);
        }
    """)
    if callback:
        btn.clicked.connect(callback)
    return btn


# ─── Main Login Window ──────────────────────────────────────────────────────

class LoginWindow(QMainWindow, WorkerMixin):
    def __init__(self, api, on_success):
        super().__init__()
        self.api = api
        self.on_success = on_success
        self._init_workers()
        self.setWindowTitle("Ozayn — Multimodal Authentication")
        self.setFixedSize(460, 720)
        self.setStyleSheet(DARK_STYLE)

        self._active_field = None  # Which field the virtual keyboard targets
        self._keyboard_visible = False
        self._face_widget = None

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

    # ─── Login Page ─────────────────────────────────────────────────────

    def _build_login_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setSpacing(10)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)

        # Logo
        logo = QLabel("⬡")
        logo.setAlignment(Qt.AlignmentFlag.AlignCenter)
        logo.setStyleSheet("""
            font-size: 48px; color: #0a84ff;
            background: rgba(10,132,255,0.1);
            border-radius: 28px; width: 64px; height: 64px;
        """)
        logo.setFixedSize(64, 64)
        layout.addWidget(logo)

        title = QLabel("OZAYN")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title.setStyleSheet("font-size: 26px; font-weight: bold; color: #0a84ff; letter-spacing: 4px;")
        layout.addWidget(title)

        subtitle = QLabel("DIGITAL TWIN INTELLIGENCE")
        subtitle.setAlignment(Qt.AlignmentFlag.AlignCenter)
        subtitle.setStyleSheet("font-size: 10px; color: rgba(255,255,255,0.4); letter-spacing: 2px;")
        layout.addWidget(subtitle)

        layout.addSpacing(16)

        # Username
        self.login_user = QLineEdit()
        self.login_user.setPlaceholderText("Username")
        self.login_user.setFixedHeight(42)
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

        # Password with show/hide toggle
        pass_row = QHBoxLayout()
        pass_row.setSpacing(0)
        self.login_pass = QLineEdit()
        self.login_pass.setPlaceholderText("Password")
        self.login_pass.setEchoMode(QLineEdit.EchoMode.Password)
        self.login_pass.setFixedHeight(42)
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
        self._eye_btn.setFixedSize(42, 42)
        self._eye_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self._eye_btn.setStyleSheet("""
            QPushButton {
                background: transparent; color: rgba(255,255,255,0.4);
                border: none; font-size: 16px; margin-left: -42px;
            }
            QPushButton:hover { color: #ffffff; }
        """)
        self._eye_btn.clicked.connect(self._toggle_password_visibility)
        pass_row.addWidget(self._eye_btn)
        layout.addLayout(pass_row)

        # Login button
        self.login_btn = QPushButton("ENTER OZAYN")
        self.login_btn.setFixedHeight(44)
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

        layout.addSpacing(8)

        # ─── Auth Methods Row ───
        auth_label = QLabel("Authenticate with")
        auth_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        auth_label.setStyleSheet("color: rgba(255,255,255,0.35); font-size: 11px; margin-bottom: 4px;")
        layout.addWidget(auth_label)

        auth_row = QHBoxLayout()
        auth_row.setSpacing(6)
        auth_row.setAlignment(Qt.AlignmentFlag.AlignCenter)

        btn_voice = make_auth_button("🎙", "Voice", "Voice Login", self._start_voice_login)
        btn_face = make_auth_button("👤", "Face", "Face Login", self._start_face_login)
        btn_passkey = make_auth_button("🔑", "Passkey", "Passkey Login", self._start_passkey_login)
        btn_keyboard = make_auth_button("⌨", "Keyboard", "On-screen Keyboard", self._toggle_virtual_keyboard)

        auth_row.addWidget(btn_voice)
        auth_row.addWidget(btn_face)
        auth_row.addWidget(btn_passkey)
        auth_row.addWidget(btn_keyboard)
        layout.addLayout(auth_row)

        layout.addSpacing(6)

        # Switch to register
        switch_reg = QPushButton("Create Account →")
        switch_reg.setObjectName("text-btn")
        switch_reg.setStyleSheet("""
            QPushButton {
                background: transparent; color: rgba(255,255,255,0.5);
                border: none; font-size: 12px;
            }
            QPushButton:hover { color: #0a84ff; }
        """)
        switch_reg.clicked.connect(lambda: self.stack.setCurrentIndex(1))
        layout.addWidget(switch_reg, alignment=Qt.AlignmentFlag.AlignCenter)

        self.stack.addWidget(page)

    # ─── Register Page ──────────────────────────────────────────────────

    def _build_register_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setSpacing(10)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)

        back_btn = QPushButton("← Back")
        back_btn.setObjectName("text-btn")
        back_btn.setStyleSheet("""
            QPushButton { background: transparent; color: rgba(255,255,255,0.5); border: none; font-size: 12px; }
            QPushButton:hover { color: #0a84ff; }
        """)
        back_btn.clicked.connect(lambda: self.stack.setCurrentIndex(0))
        layout.addWidget(back_btn)

        layout.addSpacing(4)

        reg_title = QLabel("Create Account")
        reg_title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        reg_title.setStyleSheet("font-size: 22px; font-weight: bold; color: #0a84ff;")
        layout.addWidget(reg_title)

        reg_sub = QLabel("Set up your Digital Twin")
        reg_sub.setAlignment(Qt.AlignmentFlag.AlignCenter)
        reg_sub.setStyleSheet("font-size: 11px; color: rgba(255,255,255,0.4);")
        layout.addWidget(reg_sub)

        layout.addSpacing(12)

        field_style = """
            QLineEdit {
                background: rgba(255,255,255,0.06);
                color: #ffffff;
                border: 1px solid rgba(255,255,255,0.12);
                border-radius: 10px;
                padding: 0 14px;
                font-size: 14px;
                height: 42px;
            }
            QLineEdit:focus { border-color: #0a84ff; }
        """

        self.reg_user = QLineEdit()
        self.reg_user.setPlaceholderText("Username")
        self.reg_user.setFixedHeight(42)
        self.reg_user.setStyleSheet(field_style)
        layout.addWidget(self.reg_user)

        self.reg_email = QLineEdit()
        self.reg_email.setPlaceholderText("Email (optional)")
        self.reg_email.setFixedHeight(42)
        self.reg_email.setStyleSheet(field_style)
        layout.addWidget(self.reg_email)

        self.reg_fullname = QLineEdit()
        self.reg_fullname.setPlaceholderText("Full Name (optional)")
        self.reg_fullname.setFixedHeight(42)
        self.reg_fullname.setStyleSheet(field_style)
        layout.addWidget(self.reg_fullname)

        # Password with eye toggle
        reg_pass_row = QHBoxLayout()
        reg_pass_row.setSpacing(0)
        self.reg_pass = QLineEdit()
        self.reg_pass.setPlaceholderText("Password")
        self.reg_pass.setEchoMode(QLineEdit.EchoMode.Password)
        self.reg_pass.setFixedHeight(42)
        self.reg_pass.setStyleSheet(field_style)
        reg_pass_row.addWidget(self.reg_pass)

        reg_eye = QPushButton("👁")
        reg_eye.setFixedSize(42, 42)
        reg_eye.setCursor(Qt.CursorShape.PointingHandCursor)
        reg_eye.setStyleSheet("""
            QPushButton { background: transparent; color: rgba(255,255,255,0.4); border: none; font-size: 16px; margin-left: -42px; }
            QPushButton:hover { color: #ffffff; }
        """)
        reg_eye.clicked.connect(lambda: self._toggle_echo(
            self.reg_pass, reg_eye
        ))
        reg_pass_row.addWidget(reg_eye)
        layout.addLayout(reg_pass_row)

        # Confirm password
        reg_pass2_row = QHBoxLayout()
        reg_pass2_row.setSpacing(0)
        self.reg_pass2 = QLineEdit()
        self.reg_pass2.setPlaceholderText("Confirm Password")
        self.reg_pass2.setEchoMode(QLineEdit.EchoMode.Password)
        self.reg_pass2.setFixedHeight(42)
        self.reg_pass2.setStyleSheet(field_style)
        reg_pass2_row.addWidget(self.reg_pass2)

        reg_eye2 = QPushButton("👁")
        reg_eye2.setFixedSize(42, 42)
        reg_eye2.setCursor(Qt.CursorShape.PointingHandCursor)
        reg_eye2.setStyleSheet("""
            QPushButton { background: transparent; color: rgba(255,255,255,0.4); border: none; font-size: 16px; margin-left: -42px; }
            QPushButton:hover { color: #ffffff; }
        """)
        reg_eye2.clicked.connect(lambda: self._toggle_echo(
            self.reg_pass2, reg_eye2
        ))
        reg_pass2_row.addWidget(reg_eye2)
        layout.addLayout(reg_pass2_row)

        # Register button
        self.reg_btn = QPushButton("CREATE ACCOUNT")
        self.reg_btn.setFixedHeight(44)
        self.reg_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.reg_btn.setStyleSheet("""
            QPushButton {
                background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                    stop:0 #30d158, stop:1 #34c759);
                color: white; border: none; border-radius: 10px;
                font-size: 14px; font-weight: bold; letter-spacing: 1px;
            }
            QPushButton:hover { background: #28b74a; }
            QPushButton:pressed { background: #249c3f; }
            QPushButton:disabled { background: rgba(255,255,255,0.1); color: rgba(255,255,255,0.3); }
        """)
        self.reg_btn.clicked.connect(self.do_register)
        layout.addWidget(self.reg_btn)

        self.reg_error = QLabel("")
        self.reg_error.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.reg_error.setStyleSheet("color: #ff453a; font-size: 12px;")
        layout.addWidget(self.reg_error)

        layout.addSpacing(8)

        # Auth methods row (same as login)
        auth_label = QLabel("Or authenticate with")
        auth_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        auth_label.setStyleSheet("color: rgba(255,255,255,0.35); font-size: 11px; margin-bottom: 4px;")
        layout.addWidget(auth_label)

        auth_row = QHBoxLayout()
        auth_row.setSpacing(6)
        auth_row.setAlignment(Qt.AlignmentFlag.AlignCenter)

        auth_row.addWidget(make_auth_button("🎙", "Voice", "Voice Input", self._start_voice_login))
        auth_row.addWidget(make_auth_button("👤", "Face", "Face Scan", self._start_face_login))
        auth_row.addWidget(make_auth_button("🔑", "Passkey", "Passkey", self._start_passkey_login))
        auth_row.addWidget(make_auth_button("⌨", "Keyboard", "On-screen Keyboard", self._toggle_virtual_keyboard))
        layout.addLayout(auth_row)

        self.stack.addWidget(page)

    # ─── Virtual Keyboard (shared) ──────────────────────────────────────

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
        mode = self.login_pass.echoMode()
        if mode == QLineEdit.EchoMode.Password:
            self.login_pass.setEchoMode(QLineEdit.EchoMode.Normal)
            self._eye_btn.setText("🙈")
        else:
            self.login_pass.setEchoMode(QLineEdit.EchoMode.Password)
            self._eye_btn.setText("👁")

    def _toggle_echo(self, field, btn):
        mode = field.echoMode()
        if mode == QLineEdit.EchoMode.Password:
            field.setEchoMode(QLineEdit.EchoMode.Normal)
            btn.setText("🙈")
        else:
            field.setEchoMode(QLineEdit.EchoMode.Password)
            btn.setText("👁")

    # ─── Voice Login ────────────────────────────────────────────────────

    def _start_voice_login(self):
        """Start voice recognition to fill login fields."""
        try:
            import speech_recognition as sr
            self.login_error.setStyleSheet("color: #0a84ff; font-size: 12px;")
            self.login_error.setText("🎙 Listening... speak your username")

            recognizer = sr.Recognizer()
            mic = sr.Microphone()

            def listen():
                try:
                    with mic as source:
                        audio = recognizer.listen(source, timeout=5)
                    text = recognizer.recognize_google(audio)
                    self.login_user.setText(text)
                    self.login_error.setStyleSheet("color: #30d158; font-size: 12px;")
                    self.login_error.setText(f"Recognized: {text}")
                except sr.WaitTimeoutError:
                    self.login_error.setStyleSheet("color: #ff9f0a; font-size: 12px;")
                    self.login_error.setText("No speech detected — try again")
                except sr.UnknownValueError:
                    self.login_error.setStyleSheet("color: #ff9f0a; font-size: 12px;")
                    self.login_error.setText("Could not understand — try again")
                except Exception as e:
                    self.login_error.setStyleSheet("color: #ff453a; font-size: 12px;")
                    self.login_error.setText(f"Voice error: {str(e)}")

            self._voice_thread = QThread()
            self._voice_thread.run = listen
            self._voice_thread.start()

        except ImportError:
            self.login_error.setStyleSheet("color: #ff9f0a; font-size: 12px;")
            self.login_error.setText("Speech recognition not installed (pip install SpeechRecognition)")

    # ─── Face Login ─────────────────────────────────────────────────────

    def _start_face_login(self):
        """Start face detection for authentication."""
        if self._face_widget and self._face_widget.isVisible():
            self._face_widget.stop()
            self._face_widget.hide()
            return

        self._face_widget = FaceCameraWidget()
        self._face_widget.face_detected.connect(self._on_face_detected)
        self._face_widget.face_failed.connect(self._on_face_failed)
        self.main_layout.insertWidget(self.main_layout.count() - 1, self._face_widget)
        self._face_widget.show()
        self._face_widget.start()

    def _on_face_detected(self):
        self.login_error.setStyleSheet("color: #30d158; font-size: 12px;")
        self.login_error.setText("Face detected! Attempting login...")
        if self._face_widget:
            self._face_widget.hide()
        # Try auto-login with face (username from face recognition DB)
        self._run("face_login", {}, self._on_face_login_result)

    def _on_face_login_result(self, r):
        if r.get("success"):
            self.on_success()
        else:
            self.login_error.setStyleSheet("color: #ff9f0a; font-size: 12px;")
            self.login_error.setText("Face not recognized — use username/password")

    def _on_face_failed(self, msg):
        self.login_error.setStyleSheet("color: #ff9f0a; font-size: 12px;")
        self.login_error.setText(msg)
        if self._face_widget:
            self._face_widget.hide()

    # ─── Passkey Login ──────────────────────────────────────────────────

    def _start_passkey_login(self):
        """Start passkey (FIDO2/WebAuthn) authentication."""
        self.login_error.setStyleSheet("color: #0a84ff; font-size: 12px;")
        self.login_error.setText("Passkey authentication requires a security key")
        # TODO: Integrate with FIDO2 library
        # For now, show a message
        pass

    # ─── 2FA Flow ───────────────────────────────────────────────────────

    def _show_2fa_dialog(self):
        """Show 2FA verification dialog."""
        dialog = TwoFADialog(self)
        result = dialog.exec()
        if result == QDialog.DialogCode.Accepted and dialog.verified:
            return True
        return False

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
        self.login_btn.setEnabled(True)
        self.login_btn.setText("ENTER OZAYN")
        if r.get("success"):
            # Check if 2FA is required
            if r.get("requires_2fa"):
                if self._show_2fa_dialog():
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
            "username": u,
            "password": p,
            "email": e,
            "fullname": fn
        }, self._on_reg)

    def _on_reg(self, r):
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
