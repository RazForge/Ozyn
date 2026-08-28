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
    QGraphicsDropShadowEffect, QApplication
)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QPoint
from PyQt6.QtGui import QFont, QColor, QPixmap, QImage, QCursor



from ozayn.workers import WorkerMixin
from ozayn.voice_commands import VoiceCommandEngine, fix_mic_volume


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
        nr.setSpacing(3)
        for ch in "1234567890":
            btn = self._key_btn(ch, 48, 46, small=True)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            nr.addWidget(btn)
        layout.addWidget(self._num_row_widget)

        row1 = QHBoxLayout()
        row1.setSpacing(3)
        for ch in "QWERTYUIOP":
            btn = self._key_btn(ch, 52, 54)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            row1.addWidget(btn)
        layout.addLayout(row1)

        row2 = QHBoxLayout()
        row2.setSpacing(3)
        row2.addSpacing(20)
        for ch in "ASDFGHJKL":
            btn = self._key_btn(ch, 52, 54)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            row2.addWidget(btn)
        row2.addSpacing(20)
        layout.addLayout(row2)

        row3 = QHBoxLayout()
        row3.setSpacing(3)
        row3.addSpacing(40)
        for ch in "ZXCVBNM":
            btn = self._key_btn(ch, 52, 54)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            row3.addWidget(btn)
        row3.addSpacing(10)
        bs = self._key_btn("⌫", 70, 54, color="#00e5ff")
        bs.clicked.connect(self.backspace_pressed.emit)
        row3.addWidget(bs)
        row3.addSpacing(40)
        layout.addLayout(row3)

        row4 = QHBoxLayout()
        row4.setSpacing(5)
        shift = self._key_btn("⇧", 60, 54, color="#00e5ff")
        shift.clicked.connect(self._toggle_shift)
        self._shift_btn = shift
        row4.addWidget(shift)
        space = self._key_btn("SPACE", 280, 54, color="#0a1a30")
        space.clicked.connect(lambda: self._emit(" "))
        row4.addWidget(space)
        enter = self._key_btn("ENTER", 100, 54, color="#00e5ff")
        enter.clicked.connect(self.enter_pressed.emit)
        row4.addWidget(enter)
        layout.addLayout(row4)

        self._sym_widget = QWidget()
        sr = QHBoxLayout(self._sym_widget)
        sr.setContentsMargins(0, 0, 0, 0)
        sr.setSpacing(3)
        for ch in "!@#$%^&*()_+-=[]{}|;:',.<>?/":
            btn = self._key_btn(ch, 44, 44, small=True)
            btn.clicked.connect(lambda _, c=ch: self._emit(c))
            sr.addWidget(btn)
        self._sym_widget.hide()
        layout.addWidget(self._sym_widget)

        bottom = QHBoxLayout()
        bottom.setSpacing(5)
        num_switch = self._key_btn("?123", 70, 48, color="#00e5ff")
        num_switch.clicked.connect(self._toggle_symbols)
        bottom.addWidget(num_switch)
        bottom.addStretch()
        abc_switch = self._key_btn("ABC", 60, 48, color="#00e5ff")
        abc_switch.clicked.connect(self._show_alpha)
        abc_switch.hide()
        self._abc_btn = abc_switch
        bottom.addWidget(abc_switch)
        close_btn = self._key_btn("✕", 48, 48, color="#ff453a")
        close_btn.clicked.connect(lambda: self.hide())
        bottom.addWidget(close_btn)
        layout.addLayout(bottom)

    def _key_btn(self, text, w, h, color=None, small=False):
        btn = QPushButton(text)
        btn.setFixedSize(w, h)
        btn.setCursor(Qt.CursorShape.PointingHandCursor)
        fs = "14px" if small else "18px"
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


# ─── Fullscreen CIA Camera + Hand Gesture Mouse ────────────────────────────

class CIAFullscreenCamera(QLabel):
    """Dark background for login screen. Camera feed comes from CameraOverlay."""

    face_detected = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.setStyleSheet("background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #000814,stop:1 #001233);")
        self._camera = None
        self._timer = None
        self._hand_landmarker = None
        self._face_landmarker = None

    @property
    def _hands_lms(self):
        return []

    @property
    def _face_lms(self):
        return None

    def start(self):
        pass

    def stop(self):
        pass
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

        # Track focus changes to switch keyboard target
        QApplication.instance().focusChanged.connect(self._on_focus_changed)

    def _on_focus_changed(self, old, new):
        """Switch keyboard target when a field gets focus."""
        if not self._keyboard_visible:
            return
        if new == self.login_user or new == self.login_pass:
            self._active_field = new

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
        self._show_keyboard_fullscreen()

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
        self._hand_indicator.setText("● HAND: ACTIVE")
        self._hand_indicator.setStyleSheet(
            "color: #00e5ff; font-family: 'Courier New', monospace; "
            "font-size: 14px; background: rgba(0,6,18,0.6); border: 1px solid rgba(0,180,255,0.3); "
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
        """Start voice command engine."""
        fix_mic_volume()
        self._voice_engine = VoiceCommandEngine()
        self._voice_engine.start(
            command_callback=self._on_voice_command,
            status_callback=self._on_voice_status
        )

    def _on_voice_status(self, status):
        """Update voice indicator from engine status."""
        styles = {
            "ACTIVE": ("● VOICE: ACTIVE", "color: #00e5ff; font-family: 'Courier New', monospace; "
                        "font-size: 14px; background: rgba(0,6,18,0.6); border: 1px solid rgba(0,180,255,0.3); "
                        "border-radius: 4px; padding: 6px 14px;"),
            "STOPPED": ("● VOICE: STOPPED", "color: rgba(255,255,255,0.3); font-family: 'Courier New', monospace; "
                         "font-size: 14px; background: rgba(10,10,10,0.5); border: 1px solid rgba(255,255,255,0.08); "
                         "border-radius: 4px; padding: 6px 14px;"),
            "NO_MIC": ("● VOICE: NO MIC", "color: rgba(255,69,58,0.7); font-family: 'Courier New', monospace; "
                        "font-size: 14px; background: rgba(20,5,5,0.5); border: 1px solid rgba(255,69,58,0.15); "
                        "border-radius: 4px; padding: 6px 14px;"),
            "UNAVAILABLE": ("● VOICE: UNAVAIL", "color: rgba(255,69,58,0.7); font-family: 'Courier New', monospace; "
                             "font-size: 14px; background: rgba(20,5,5,0.5); border: 1px solid rgba(255,69,58,0.15); "
                             "border-radius: 4px; padding: 6px 14px;"),
        }
        if status in styles:
            text, style = styles[status]
            self._voice_indicator.setText(text)
            self._voice_indicator.setStyleSheet(style)
            self._voice_indicator.adjustSize()

    def _on_voice_command(self, command, raw_text):
        """Handle voice command from engine."""
        # Parse compound commands
        if command.startswith("USERNAME:"):
            username = command.split(":", 1)[1]
            self.login_user.setText(username)
            self.login_error.setStyleSheet("color: #00e5ff; font-size: 20px; background: transparent;")
            self.login_error.setText(f"Voice: username = '{username}'")
        elif command.startswith("PASSWORD:"):
            password = command.split(":", 1)[1]
            self.login_pass.setText(password)
            self.login_error.setStyleSheet("color: #00e5ff; font-size: 20px; background: transparent;")
            self.login_error.setText("Password entered via voice")
        elif command == "LOGIN":
            if self.login_user.text() and self.login_pass.text():
                self.do_login()
            else:
                self.login_error.setStyleSheet("color: #00e5ff; font-size: 20px; background: transparent;")
                self.login_error.setText("Say 'username [name]' then 'password [pass]'")
        elif command == "SUBMIT" or command == "CONFIRM":
            self.do_login()
        elif command == "KEYBOARD":
            if not self._keyboard_visible:
                self._show_keyboard_fullscreen()
        elif command == "HIDE_KEYBOARD":
            if self._keyboard_visible:
                self._keyboard_visible = False
                self._vk.hide()
                self._do_layout()
        elif command == "VOICE_STOP":
            self._voice_engine.stop()
        elif command == "VOICE_START":
            self._auto_voice_listen()

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
        auth_row.addWidget(self._auth_btn("HAND", "Mouse", self._toggle_hand_info))
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
        auth_row.addWidget(self._auth_btn("HAND", "Mouse", self._toggle_hand_info))
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

    def _show_keyboard_fullscreen(self):
        """Auto-show big keyboard below the login glass."""
        self._keyboard_visible = True
        sz = self.size()
        gw, gh = 380, 500
        kb_w = min(sz.width() - 40, 1200)
        kb_h = min(340, sz.height() // 3)

        # Move glass up to make room
        glass_x = (sz.width() - gw) // 2
        glass_y = max(20, (sz.height() - gh - kb_h - 30) // 2)
        self._glass_container.move(glass_x, glass_y)

        # Position keyboard below glass
        self._vk.setParent(self)
        self._vk.setFixedSize(kb_w, kb_h)
        self._vk.move((sz.width() - kb_w) // 2, glass_y + gh + 15)
        self._vk.raise_()
        self._vk.show()
        self._vk.update()

        # Target whichever field has focus, default to username
        focused = QApplication.focusWidget()
        if focused == self.login_pass:
            self._active_field = self.login_pass
        elif focused == self.login_user:
            self._active_field = self.login_user
        else:
            self._active_field = self.login_user
        self._active_field.setFocus()

    def _toggle_virtual_keyboard(self):
        self._keyboard_visible = not self._keyboard_visible
        if self._keyboard_visible:
            self._show_keyboard_fullscreen()
        else:
            self._vk.hide()
            # Re-center glass
            self._do_layout()

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
        if hasattr(self, '_voice_engine') and self._voice_engine.is_active:
            self._voice_engine.stop()
        else:
            self._auto_voice_listen()

    # ─── Camera / Hand ──────────────────────────────────────────────────

    def _toggle_camera(self):
        pass

    def _toggle_hand_info(self):
        self.login_error.setStyleSheet("color: #00e5ff; font-size: 20px; background: transparent;")
        self.login_error.setText("Pinch thumb+index to move cursor")

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
        if hasattr(self, '_voice_engine'):
            self._voice_engine.stop()
        self._bg_camera.stop()
        event.accept()
