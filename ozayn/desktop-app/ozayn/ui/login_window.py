"""
Ozayn Login Window — Authentication UI
"""

from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QLabel,
    QLineEdit, QPushButton, QStackedWidget, QFrame
)
from PyQt6.QtCore import Qt

from ozayn.theme.dark import DARK_STYLE
from ozayn.workers import WorkerMixin


class LoginWindow(QMainWindow, WorkerMixin):
    def __init__(self, api, on_success):
        super().__init__()
        self.api = api
        self.on_success = on_success
        self._init_workers()
        self.setWindowTitle("Ozayn — Login")
        self.setFixedSize(420, 580)
        self.setStyleSheet(DARK_STYLE)

        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.setSpacing(0)

        self.stack = QStackedWidget()
        layout.addWidget(self.stack)

        # Login page
        login_page = QWidget()
        lp = QVBoxLayout(login_page)
        lp.setSpacing(12)
        lp.setAlignment(Qt.AlignmentFlag.AlignCenter)

        logo = QLabel("O")
        logo.setAlignment(Qt.AlignmentFlag.AlignCenter)
        logo.setStyleSheet("font-size:64px;font-weight:bold;color:#0a84ff;background:rgba(10,132,255,0.12);border-radius:32px;width:80px;height:80px;")
        logo.setFixedSize(80, 80)
        lp.addWidget(logo)

        title = QLabel("Ozayn")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title.setStyleSheet("font-size:28px;font-weight:bold;color:#0a84ff;margin-top:8px;")
        lp.addWidget(title)

        subtitle = QLabel("AI Digital Twin")
        subtitle.setAlignment(Qt.AlignmentFlag.AlignCenter)
        subtitle.setStyleSheet("font-size:14px;color:rgba(255,255,255,0.5);")
        lp.addWidget(subtitle)

        lp.addSpacing(30)

        self.login_user = QLineEdit()
        self.login_user.setPlaceholderText("Username")
        lp.addWidget(self.login_user)

        self.login_pass = QLineEdit()
        self.login_pass.setPlaceholderText("Password")
        self.login_pass.setEchoMode(QLineEdit.EchoMode.Password)
        lp.addWidget(self.login_pass)

        self.login_btn = QPushButton("Sign In")
        self.login_btn.clicked.connect(self.do_login)
        lp.addWidget(self.login_btn)

        self.login_error = QLabel("")
        self.login_error.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.login_error.setStyleSheet("color:#ff453a;font-size:13px;")
        lp.addWidget(self.login_error)

        lp.addSpacing(10)

        switch_reg = QPushButton("Create Account")
        switch_reg.setObjectName("text-btn")
        switch_reg.clicked.connect(lambda: self.stack.setCurrentIndex(1))
        lp.addWidget(switch_reg)

        self.stack.addWidget(login_page)

        # Register page
        reg_page = QWidget()
        rp = QVBoxLayout(reg_page)
        rp.setSpacing(12)
        rp.setAlignment(Qt.AlignmentFlag.AlignCenter)

        back_btn = QPushButton("< Back")
        back_btn.setObjectName("text-btn")
        back_btn.clicked.connect(lambda: self.stack.setCurrentIndex(0))
        rp.addWidget(back_btn)

        rp.addSpacing(10)

        reg_title = QLabel("Create Account")
        reg_title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        reg_title.setStyleSheet("font-size:24px;font-weight:bold;color:#0a84ff;")
        rp.addWidget(reg_title)

        rp.addSpacing(20)

        self.reg_user = QLineEdit()
        self.reg_user.setPlaceholderText("Username")
        rp.addWidget(self.reg_user)

        self.reg_email = QLineEdit()
        self.reg_email.setPlaceholderText("Email (optional)")
        rp.addWidget(self.reg_email)

        self.reg_pass = QLineEdit()
        self.reg_pass.setPlaceholderText("Password")
        self.reg_pass.setEchoMode(QLineEdit.EchoMode.Password)
        rp.addWidget(self.reg_pass)

        self.reg_btn = QPushButton("Create Account")
        self.reg_btn.clicked.connect(self.do_register)
        rp.addWidget(self.reg_btn)

        self.reg_error = QLabel("")
        self.reg_error.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.reg_error.setStyleSheet("color:#ff453a;font-size:13px;")
        rp.addWidget(self.reg_error)

        self.stack.addWidget(reg_page)

    def do_login(self):
        u = self.login_user.text().strip()
        p = self.login_pass.text()
        if not u or not p:
            self.login_error.setText("Please enter username and password")
            return
        self.login_error.setText("")
        self.login_btn.setEnabled(False)
        self._run("login", {"username": u, "password": p}, self._on_login)

    def _on_login(self, r):
        self.login_btn.setEnabled(True)
        if r.get("success"):
            self.on_success()
        else:
            self.login_error.setText(r.get("error", "Login failed"))

    def do_register(self):
        u = self.reg_user.text().strip()
        p = self.reg_pass.text()
        e = self.reg_email.text().strip() or None
        if not u or not p:
            self.reg_error.setText("Please enter username and password")
            return
        self.reg_error.setText("")
        self.reg_btn.setEnabled(False)
        self._run("register", {"username": u, "password": p, "email": e}, self._on_reg)

    def _on_reg(self, r):
        self.reg_btn.setEnabled(True)
        if r.get("success"):
            self.reg_error.setStyleSheet("color:#30d158;font-size:13px;")
            self.reg_error.setText("Account created! You can now sign in.")
            self.stack.setCurrentIndex(0)
        else:
            self.reg_error.setStyleSheet("color:#ff453a;font-size:13px;")
            self.reg_error.setText(r.get("error", "Registration failed"))
