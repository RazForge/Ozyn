#!/usr/bin/env python3
"""
Ozayn Desktop — Native PyQt6 Application
Syncs with the web backend (same PHP API, same database)
"""

import sys
import json
import os
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QLineEdit, QPushButton, QTextEdit, QListWidget, QListWidgetItem,
    QStackedWidget, QFrame, QSplitter, QMessageBox, QComboBox,
    QScrollArea, QSizePolicy, QSpacerItem, QInputDialog
)
from PyQt6.QtCore import Qt, QThread, pyqtSignal, QTimer, QSize
from PyQt6.QtGui import QFont, QColor, QPalette, QIcon, QTextCursor

from api_client import OzaynAPI

# ==================== Styles ====================

DARK_STYLE = """
QMainWindow, QWidget {
    background-color: #08081a;
    color: rgba(255, 255, 255, 0.92);
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    font-size: 14px;
}

QFrame#sidebar {
    background-color: rgba(22, 22, 50, 0.8);
    border-right: 1px solid rgba(255, 255, 255, 0.1);
}

QFrame#header {
    background-color: rgba(16, 16, 36, 0.9);
    border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

QLabel#title {
    color: #0a84ff;
    font-size: 16px;
    font-weight: bold;
    letter-spacing: 4px;
}

QLabel#subtitle {
    color: rgba(255, 255, 255, 0.5);
    font-size: 13px;
}

QLabel#section-label {
    color: rgba(255, 255, 255, 0.3);
    font-size: 11px;
    font-weight: bold;
    letter-spacing: 1px;
    padding: 8px 12px 4px;
}

QLineEdit {
    background-color: rgba(50, 50, 85, 0.3);
    border: 1px solid rgba(255, 255, 255, 0.1);
    border-radius: 12px;
    padding: 12px 16px;
    color: rgba(255, 255, 255, 0.92);
    font-size: 14px;
    selection-background-color: #0a84ff;
}

QLineEdit:focus {
    border-color: #0a84ff;
}

QLineEdit[echoMode="2"] {
    /* password field */
}

QPushButton {
    background-color: #0a84ff;
    color: white;
    border: none;
    border-radius: 12px;
    padding: 12px 24px;
    font-size: 14px;
    font-weight: bold;
}

QPushButton:hover {
    background-color: #409cff;
}

QPushButton:pressed {
    background-color: #0070e0;
}

QPushButton:disabled {
    background-color: rgba(10, 132, 255, 0.3);
    color: rgba(255, 255, 255, 0.3);
}

QPushButton#secondary {
    background-color: rgba(50, 50, 85, 0.3);
    border: 1px solid rgba(255, 255, 255, 0.1);
}

QPushButton#secondary:hover {
    background-color: rgba(255, 255, 255, 0.08);
}

QPushButton#danger {
    background-color: rgba(255, 69, 58, 0.15);
    color: #ff453a;
    border: 1px solid rgba(255, 69, 58, 0.3);
}

QPushButton#danger:hover {
    background-color: rgba(255, 69, 58, 0.25);
}

QPushButton#text-btn {
    background: transparent;
    color: #0a84ff;
    border: none;
    padding: 6px 12px;
    border-radius: 8px;
    font-size: 13px;
    font-weight: 500;
}

QPushButton#text-btn:hover {
    background-color: rgba(10, 132, 255, 0.12);
}

QPushButton#nav-btn {
    background: transparent;
    color: rgba(255, 255, 255, 0.3);
    border: none;
    padding: 8px 0;
    border-radius: 0;
    font-size: 11px;
}

QPushButton#nav-btn:hover {
    color: rgba(255, 255, 255, 0.6);
}

QPushButton#nav-btn[active="true"] {
    color: #0a84ff;
}

QListWidget {
    background: transparent;
    border: none;
    outline: none;
    padding: 4px;
}

QListWidget::item {
    padding: 10px 12px;
    border-radius: 8px;
    color: rgba(255, 255, 255, 0.5);
    margin-bottom: 2px;
}

QListWidget::item:hover {
    background-color: rgba(255, 255, 255, 0.06);
    color: rgba(255, 255, 255, 0.92);
}

QListWidget::item:selected {
    background-color: rgba(10, 132, 255, 0.12);
    color: #0a84ff;
}

QTextEdit#chat-messages {
    background: transparent;
    border: none;
    color: rgba(255, 255, 255, 0.92);
    font-size: 14px;
    padding: 16px;
    selection-background-color: #0a84ff;
}

QTextEdit#chat-input {
    background-color: rgba(50, 50, 85, 0.3);
    border: 1px solid rgba(255, 255, 255, 0.1);
    border-radius: 12px;
    padding: 12px 16px;
    color: rgba(255, 255, 255, 0.92);
    font-size: 14px;
    max-height: 120px;
}

QTextEdit#chat-input:focus {
    border-color: #0a84ff;
}

QComboBox {
    background-color: rgba(50, 50, 85, 0.3);
    border: 1px solid rgba(255, 255, 255, 0.1);
    border-radius: 12px;
    padding: 10px 14px;
    color: rgba(255, 255, 255, 0.92);
    font-size: 14px;
}

QComboBox::drop-down {
    border: none;
    width: 30px;
}

QComboBox QAbstractItemView {
    background-color: #161632;
    color: rgba(255, 255, 255, 0.92);
    border: 1px solid rgba(255, 255, 255, 0.1);
    border-radius: 8px;
    selection-background-color: rgba(10, 132, 255, 0.3);
}

QScrollArea {
    border: none;
    background: transparent;
}

QScrollBar:vertical {
    background: transparent;
    width: 6px;
    margin: 0;
}

QScrollBar::handle:vertical {
    background: rgba(255, 255, 255, 0.1);
    border-radius: 3px;
    min-height: 30px;
}

QScrollBar::handle:vertical:hover {
    background: rgba(255, 255, 255, 0.2);
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}

QScrollBar:horizontal {
    background: transparent;
    height: 0;
}
"""


# ==================== Worker Thread ====================

class APIWorker(QThread):
    finished = pyqtSignal(dict)

    def __init__(self, api, method, endpoint="", data=None):
        super().__init__()
        self.api = api
        self.method = method
        self.endpoint = endpoint
        self.data = data or {}

    def run(self):
        try:
            result = self._do_work()
        except Exception as e:
            result = {"success": False, "error": str(e)}
        self.finished.emit(result)

    def _do_work(self):
        m = self.method
        d = self.data
        if m == "login":
            return self.api.login(d["username"], d["password"])
        elif m == "register":
            return self.api.register(d["username"], d["password"], d.get("email"), d.get("full_name"))
        elif m == "chat":
            return self.api.send_chat(d["message"], d.get("conversation_id"), d.get("project_id"))
        elif m == "conversations":
            return self.api.list_conversations()
        elif m == "history":
            return self.api.get_chat_history(d["conversation_id"])
        elif m == "projects":
            return self.api.list_projects()
        elif m == "create_project":
            return self.api.create_project(d["name"], d.get("description", ""))
        elif m == "tasks":
            return self.api.list_tasks()
        elif m == "create_task":
            return self.api.create_task(d["title"], d.get("description", ""), d.get("priority", "medium"), d.get("project_id"))
        elif m == "update_task":
            return self.api.update_task(d["task_id"], d["status"])
        elif m == "knowledge":
            return self.api.list_knowledge()
        elif m == "add_knowledge":
            return self.api.add_knowledge(d["title"], d["content"], d.get("tags"), d.get("project_id"))
        elif m == "logout":
            self.api.logout()
            return {"success": True}
        return {"success": False, "error": "Unknown method"}


# ==================== Login Window ====================

class LoginWindow(QMainWindow):
    def __init__(self, api):
        super().__init__()
        self.api = api
        self._workers = []
        self.setWindowTitle("Ozayn — Login")
        self.setFixedSize(420, 580)
        self.setStyleSheet(DARK_STYLE)

        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.setSpacing(0)

        # Logo
        logo_icon = QLabel("\u2B21")
        logo_icon.setAlignment(Qt.AlignmentFlag.AlignCenter)
        logo_icon.setStyleSheet("font-size: 44px; color: #0a84ff; margin-bottom: 14px;")
        layout.addWidget(logo_icon)

        title = QLabel("OZAYN")
        title.setObjectName("title")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title.setStyleSheet("font-size: 32px; font-weight: 700; letter-spacing: 8px; color: rgba(255,255,255,0.92); margin-bottom: 8px;")
        layout.addWidget(title)

        subtitle = QLabel("Personal AI Digital Twin")
        subtitle.setObjectName("subtitle")
        subtitle.setAlignment(Qt.AlignmentFlag.AlignCenter)
        subtitle.setStyleSheet("color: rgba(255,255,255,0.5); font-size: 13px; margin-bottom: 36px;")
        layout.addWidget(subtitle)

        # Tabs
        tab_layout = QHBoxLayout()
        self.tab_login = QPushButton("Login")
        self.tab_register = QPushButton("Register")
        for btn in [self.tab_login, self.tab_register]:
            btn.setCheckable(True)
            btn.setStyleSheet("""
                QPushButton {
                    background: rgba(50,50,85,0.3);
                    border: none;
                    border-radius: 8px;
                    padding: 10px 16px;
                    color: rgba(255,255,255,0.5);
                    font-size: 13px;
                    font-weight: 500;
                }
                QPushButton:checked {
                    background: rgba(255,255,255,0.12);
                    color: rgba(255,255,255,0.92);
                }
            """)
        self.tab_login.setChecked(True)
        self.tab_login.clicked.connect(lambda: self.switch_tab("login"))
        self.tab_register.clicked.connect(lambda: self.switch_tab("register"))
        tab_layout.addWidget(self.tab_login)
        tab_layout.addWidget(self.tab_register)
        layout.addLayout(tab_layout)

        layout.addSpacing(24)

        # Login form
        self.login_widget = QWidget()
        login_form = QVBoxLayout(self.login_widget)
        login_form.setSpacing(14)
        self.login_user = QLineEdit()
        self.login_user.setPlaceholderText("Username")
        self.login_pass = QLineEdit()
        self.login_pass.setPlaceholderText("Password")
        self.login_pass.setEchoMode(QLineEdit.EchoMode.Password)
        self.login_btn = QPushButton("Login")
        self.login_btn.clicked.connect(self.do_login)
        self.login_pass.returnPressed.connect(self.do_login)
        login_form.addWidget(self.login_user)
        login_form.addWidget(self.login_pass)
        login_form.addWidget(self.login_btn)
        layout.addWidget(self.login_widget)

        # Register form
        self.register_widget = QWidget()
        reg_form = QVBoxLayout(self.register_widget)
        reg_form.setSpacing(14)
        self.reg_user = QLineEdit()
        self.reg_user.setPlaceholderText("Username")
        self.reg_email = QLineEdit()
        self.reg_email.setPlaceholderText("Email (optional)")
        self.reg_name = QLineEdit()
        self.reg_name.setPlaceholderText("Full Name (optional)")
        self.reg_pass = QLineEdit()
        self.reg_pass.setPlaceholderText("Password")
        self.reg_pass.setEchoMode(QLineEdit.EchoMode.Password)
        self.reg_btn = QPushButton("Register")
        self.reg_btn.clicked.connect(self.do_register)
        reg_form.addWidget(self.reg_user)
        reg_form.addWidget(self.reg_email)
        reg_form.addWidget(self.reg_name)
        reg_form.addWidget(self.reg_pass)
        reg_form.addWidget(self.reg_btn)
        self.register_widget.hide()
        layout.addWidget(self.register_widget)

        # Error
        self.error_label = QLabel("")
        self.error_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.error_label.setStyleSheet("color: #ff453a; font-size: 13px; margin-top: 14px; min-height: 20px;")
        layout.addWidget(self.error_label)

        # Status
        self.status_label = QLabel("Connecting to server...")
        self.status_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.status_label.setStyleSheet("color: rgba(255,255,255,0.3); font-size: 12px; margin-top: 8px;")
        layout.addWidget(self.status_label)

        # Check server
        QTimer.singleShot(500, self.check_server)

    def check_server(self):
        if self.api._ensure_server():
            self.status_label.setText("")
        else:
            self.status_label.setText("Server not available")

    def switch_tab(self, tab):
        if tab == "login":
            self.tab_login.setChecked(True)
            self.tab_register.setChecked(False)
            self.login_widget.show()
            self.register_widget.hide()
        else:
            self.tab_register.setChecked(True)
            self.tab_login.setChecked(False)
            self.register_widget.show()
            self.login_widget.hide()
        self.error_label.setText("")

    def _start_worker(self, method, data=None, callback=None):
        worker = APIWorker(self.api, method, data=data)
        if callback:
            worker.finished.connect(callback)
        worker.finished.connect(lambda _: self._workers.remove(worker) if worker in self._workers else None)
        self._workers.append(worker)
        worker.start()
        return worker

    def do_login(self):
        username = self.login_user.text().strip()
        password = self.login_pass.text()
        if not username or not password:
            self.error_label.setText("Please fill in all fields")
            return
        self.login_btn.setEnabled(False)
        self.login_btn.setText("Logging in...")
        self.error_label.setText("")

        self._start_worker("login", {"username": username, "password": password}, self.on_login_result)

    def on_login_result(self, result):
        self.login_btn.setEnabled(True)
        self.login_btn.setText("Login")
        if result.get("success"):
            self.main_window = MainWindow(self.api)
            self.main_window.show()
            self.hide()
        else:
            self.error_label.setText(result.get("error", "Login failed"))

    def do_register(self):
        username = self.reg_user.text().strip()
        password = self.reg_pass.text()
        if not username or not password:
            self.error_label.setText("Please fill in username and password")
            return
        self.reg_btn.setEnabled(False)
        self.reg_btn.setText("Registering...")
        self.error_label.setText("")

        self._start_worker("register", {
            "username": username, "password": password,
            "email": self.reg_email.text().strip() or None,
            "full_name": self.reg_name.text().strip() or None
        }, self.on_register_result)

    def on_register_result(self, result):
        self.reg_btn.setEnabled(True)
        self.reg_btn.setText("Register")
        if result.get("success"):
            self.login_user.setText(self.reg_user.text())
            self.login_pass.setText(self.reg_pass.text())
            self.switch_tab("login")
            self.do_login()
        else:
            self.error_label.setText(result.get("error", "Registration failed"))


# ==================== Main Window ====================

class MainWindow(QMainWindow):
    def __init__(self, api):
        super().__init__()
        self.api = api
        self._workers = []
        self.current_conversation = None
        self.current_project = None
        self.setWindowTitle("Ozayn — Desktop")
        self.setMinimumSize(1000, 700)
        self.resize(1200, 800)
        self.setStyleSheet(DARK_STYLE)

        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QHBoxLayout(central)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)

        # Sidebar
        self.sidebar = QFrame()
        self.sidebar.setObjectName("sidebar")
        self.sidebar.setFixedWidth(260)
        sidebar_layout = QVBoxLayout(self.sidebar)
        sidebar_layout.setContentsMargins(0, 0, 0, 0)
        sidebar_layout.setSpacing(0)

        # Sidebar header
        sidebar_header = QFrame()
        sidebar_header.setFixedHeight(56)
        sidebar_header_layout = QHBoxLayout(sidebar_header)
        sidebar_header_layout.setContentsMargins(20, 0, 20, 0)
        header_title = QLabel("OZAYN")
        header_title.setObjectName("title")
        sidebar_header_layout.addWidget(header_title)
        sidebar_header_layout.addStretch()
        sidebar_layout.addWidget(sidebar_header)

        # Conversations
        conv_label = QLabel("CONVERSATIONS")
        conv_label.setObjectName("section-label")
        sidebar_layout.addWidget(conv_label)

        self.conv_list = QListWidget()
        self.conv_list.itemClicked.connect(self.on_conversation_click)
        sidebar_layout.addWidget(self.conv_list)

        new_chat_btn = QPushButton("+ New Chat")
        new_chat_btn.setObjectName("text-btn")
        new_chat_btn.clicked.connect(self.new_conversation)
        sidebar_layout.addWidget(new_chat_btn)

        # Projects
        proj_label = QLabel("PROJECTS")
        proj_label.setObjectName("section-label")
        sidebar_layout.addWidget(proj_label)

        self.proj_list = QListWidget()
        self.proj_list.itemClicked.connect(self.on_project_click)
        sidebar_layout.addWidget(self.proj_list)

        new_proj_btn = QPushButton("+ New Project")
        new_proj_btn.setObjectName("text-btn")
        new_proj_btn.clicked.connect(self.new_project)
        sidebar_layout.addWidget(new_proj_btn)

        # Sidebar footer
        sidebar_footer = QFrame()
        sidebar_footer.setFixedHeight(48)
        sidebar_footer.setStyleSheet("border-top: 1px solid rgba(255,255,255,0.1);")
        footer_layout = QHBoxLayout(sidebar_footer)
        footer_layout.setContentsMargins(16, 0, 16, 0)
        user_label = QLabel(self.api.user.get("username", ""))
        user_label.setStyleSheet("color: rgba(255,255,255,0.5); font-size: 12px;")
        footer_layout.addWidget(user_label)
        footer_layout.addStretch()
        logout_btn = QPushButton("\u2192")
        logout_btn.setObjectName("secondary")
        logout_btn.setFixedSize(32, 32)
        logout_btn.setToolTip("Logout")
        logout_btn.clicked.connect(self.logout)
        footer_layout.addWidget(logout_btn)
        sidebar_layout.addWidget(sidebar_footer)

        main_layout.addWidget(self.sidebar)

        # Right panel
        right_panel = QWidget()
        right_layout = QVBoxLayout(right_panel)
        right_layout.setContentsMargins(0, 0, 0, 0)
        right_layout.setSpacing(0)

        # Header
        self.header = QFrame()
        self.header.setObjectName("header")
        self.header.setFixedHeight(52)
        header_layout = QHBoxLayout(self.header)
        header_layout.setContentsMargins(20, 0, 20, 0)
        self.header_title = QLabel("New Conversation")
        self.header_title.setStyleSheet("font-size: 15px; font-weight: 600;")
        header_layout.addWidget(self.header_title)
        header_layout.addStretch()
        right_layout.addWidget(self.header)

        # Stacked views
        self.views = QStackedWidget()

        # Chat view
        chat_widget = QWidget()
        chat_layout = QVBoxLayout(chat_widget)
        chat_layout.setContentsMargins(0, 0, 0, 0)
        self.chat_messages = QTextEdit()
        self.chat_messages.setObjectName("chat-messages")
        self.chat_messages.setReadOnly(True)
        self.chat_messages.setHtml('<div style="color: rgba(255,255,255,0.5); text-align: center; padding: 40px;">How can I help you today?</div>')
        chat_layout.addWidget(self.chat_messages)

        # Input area
        input_frame = QFrame()
        input_frame.setStyleSheet("border-top: 1px solid rgba(255,255,255,0.1); padding: 12px;")
        input_layout = QHBoxLayout(input_frame)
        self.chat_input = QTextEdit()
        self.chat_input.setObjectName("chat-input")
        self.chat_input.setPlaceholderText("Message Ozayn...")
        self.chat_input.setFixedHeight(44)
        self.chat_input.installEventFilter(self)
        input_layout.addWidget(self.chat_input)

        send_btn = QPushButton("\u27A4")
        send_btn.setFixedSize(44, 44)
        send_btn.setStyleSheet("background-color: #0a84ff; border-radius: 12px; font-size: 18px;")
        send_btn.clicked.connect(self.send_message)
        input_layout.addWidget(send_btn)
        chat_layout.addWidget(input_frame)

        self.views.addWidget(chat_widget)

        # Tasks view
        tasks_widget = QWidget()
        tasks_layout = QVBoxLayout(tasks_widget)
        tasks_layout.setContentsMargins(20, 20, 20, 20)
        tasks_header = QHBoxLayout()
        tasks_title = QLabel("Tasks")
        tasks_title.setStyleSheet("font-size: 18px; font-weight: 600;")
        tasks_header.addWidget(tasks_title)
        tasks_header.addStretch()
        add_task_btn = QPushButton("+ New Task")
        add_task_btn.setObjectName("text-btn")
        add_task_btn.clicked.connect(self.new_task)
        tasks_header.addWidget(add_task_btn)
        tasks_layout.addLayout(tasks_header)

        self.tasks_list = QListWidget()
        self.tasks_list.setStyleSheet("QListWidget::item { padding: 12px 16px; background: rgba(28,28,56,0.55); border: 1px solid rgba(255,255,255,0.1); border-radius: 12px; margin-bottom: 8px; }")
        tasks_layout.addWidget(self.tasks_list)
        self.views.addWidget(tasks_widget)

        # Settings view
        settings_widget = QScrollArea()
        settings_inner = QWidget()
        settings_layout = QVBoxLayout(settings_inner)
        settings_layout.setContentsMargins(32, 32, 32, 32)
        settings_layout.setSpacing(20)

        settings_title = QLabel("Settings")
        settings_title.setStyleSheet("font-size: 18px; font-weight: 600;")
        settings_layout.addWidget(settings_title)

        # AI Settings
        ai_group = QLabel("AI Configuration")
        ai_group.setStyleSheet("color: #0a84ff; font-size: 14px; font-weight: 600; margin-top: 16px;")
        settings_layout.addWidget(ai_group)

        settings_layout.addWidget(QLabel("API Provider"))
        self.ai_provider = QComboBox()
        self.ai_provider.addItems(["Demo Mode (No API)", "OpenAI", "Anthropic", "Ollama (Local)"])
        settings_layout.addWidget(self.ai_provider)

        settings_layout.addWidget(QLabel("API Key"))
        self.ai_apikey = QLineEdit()
        self.ai_apikey.setPlaceholderText("Enter API key")
        self.ai_apikey.setEchoMode(QLineEdit.EchoMode.Password)
        settings_layout.addWidget(self.ai_apikey)

        settings_layout.addWidget(QLabel("Model"))
        self.ai_model = QLineEdit()
        self.ai_model.setPlaceholderText("e.g. gpt-3.5-turbo")
        settings_layout.addWidget(self.ai_model)

        save_btn = QPushButton("Save Settings")
        save_btn.clicked.connect(self.save_settings)
        settings_layout.addWidget(save_btn)
        settings_layout.addStretch()

        settings_widget.setWidget(settings_inner)
        settings_widget.setWidgetResizable(True)
        self.views.addWidget(settings_widget)

        right_layout.addWidget(self.views)

        # Bottom nav
        nav_frame = QFrame()
        nav_frame.setStyleSheet("border-top: 1px solid rgba(255,255,255,0.1); background: rgba(10,10,26,0.6);")
        nav_frame.setFixedHeight(56)
        nav_layout = QHBoxLayout(nav_frame)
        nav_layout.setContentsMargins(0, 0, 0, 0)

        self.nav_buttons = {}
        for name, label in [("chat", "Chat"), ("tasks", "Tasks"), ("settings", "Settings")]:
            btn = QPushButton(label)
            btn.setObjectName("nav-btn")
            btn.setCheckable(True)
            btn.clicked.connect(lambda checked, n=name: self.switch_view(n))
            self.nav_buttons[name] = btn
            nav_layout.addWidget(btn)

        self.nav_buttons["chat"].setChecked(True)
        self.nav_buttons["chat"].setStyleSheet("color: #0a84ff;")
        right_layout.addWidget(nav_frame)

        main_layout.addWidget(right_panel)

        # Load data
        self.load_conversations()
        self.load_projects()

    def switch_view(self, name):
        index = {"chat": 0, "tasks": 1, "settings": 2}.get(name, 0)
        self.views.setCurrentIndex(index)
        for n, btn in self.nav_buttons.items():
            btn.setChecked(n == name)
            btn.setStyleSheet("color: #0a84ff;" if n == name else "color: rgba(255,255,255,0.3);")
        if name == "tasks":
            self.load_tasks()

    def _start_worker(self, method, data=None, callback=None):
        worker = APIWorker(self.api, method, data=data)
        if callback:
            worker.finished.connect(callback)
        worker.finished.connect(lambda _: self._workers.remove(worker) if worker in self._workers else None)
        self._workers.append(worker)
        worker.start()
        return worker

    def send_message(self):
        text = self.chat_input.toPlainText().strip()
        if not text:
            return
        self.chat_input.clear()
        self.append_message("user", text)

        self.header_title.setText("Chat")
        self._start_worker("chat", {
            "message": text,
            "conversation_id": self.current_conversation,
            "project_id": self.current_project
        }, self.on_chat_response)

    def on_chat_response(self, result):
        if result.get("success"):
            self.current_conversation = result.get("conversation_id")
            self.append_message("assistant", result.get("response", ""))
            self.load_conversations()
        else:
            self.append_message("system", "Error: " + result.get("error", "Failed"))

    def append_message(self, role, content):
        html = content.replace("<", "&lt;").replace(">", "&gt;")
        html = html.replace("\n", "<br>")
        import re
        html = re.sub(r'\*\*(.*?)\*\*', r'<strong>\1</strong>', html)
        html = re.sub(r'\*(.*?)\*', r'<em>\1</em>', html)
        html = re.sub(r'`(.*?)`', r'<code style="background:rgba(255,255,255,0.08);padding:2px 6px;border-radius:4px;">\1</code>', html)

        colors = {
            "user": "background: #0a84ff; color: white; border-radius: 12px 12px 4px 12px; padding: 12px 16px; margin: 4px 0 4px 120px; text-align: left;",
            "assistant": "background: rgba(28,28,56,0.55); border: 1px solid rgba(255,255,255,0.1); border-radius: 12px 12px 12px 4px; padding: 12px 16px; margin: 4px 120px 4px 0;",
            "system": "color: rgba(255,255,255,0.5); text-align: center; padding: 8px; margin: 4px 60px;"
        }
        style = colors.get(role, "")
        block = f'<div style="{style}">{html}</div>'

        self.chat_messages.moveCursor(QTextCursor.MoveOperation.End)
        self.chat_messages.insertHtml(block)
        self.chat_messages.moveCursor(QTextCursor.MoveOperation.End)

    def eventFilter(self, obj, event):
        if obj == self.chat_input and event.type() == event.Type.KeyPress:
            if event.key() == Qt.Key.Key_Return and not event.modifiers() & Qt.KeyboardModifier.ShiftModifier:
                self.send_message()
                return True
        return super().eventFilter(obj, event)

    def new_conversation(self):
        self.current_conversation = None
        self.chat_messages.clear()
        self.append_message("system", "New conversation started. How can I help?")
        self.header_title.setText("New Conversation")
        self.load_conversations()
        self.switch_view("chat")

    def load_conversations(self):
        self._start_worker("conversations", callback=self._on_conversations)

    def _on_conversations(self, result):
        self.conv_list.clear()
        for conv in result.get("conversations", []):
            title = conv.get("title") or f"Chat {conv['id']}"
            item = QListWidgetItem(title)
            item.setData(Qt.ItemDataRole.UserRole, conv["id"])
            if conv["id"] == self.current_conversation:
                item.setSelected(True)
            self.conv_list.addItem(item)

    def on_conversation_click(self, item):
        conv_id = item.data(Qt.ItemDataRole.UserRole)
        self.current_conversation = conv_id
        self.chat_messages.clear()
        self._start_worker("history", {"conversation_id": conv_id}, self._on_history)
        self.switch_view("chat")

    def _on_history(self, result):
        for msg in result.get("messages", []):
            self.append_message(msg["role"], msg["content"])

    def load_projects(self):
        self._start_worker("projects", callback=self._on_projects)

    def _on_projects(self, result):
        self.proj_list.clear()
        for proj in result.get("projects", []):
            item = QListWidgetItem(proj["name"])
            item.setData(Qt.ItemDataRole.UserRole, proj["id"])
            if proj["id"] == self.current_project:
                item.setSelected(True)
            self.proj_list.addItem(item)

    def on_project_click(self, item):
        self.current_project = item.data(Qt.ItemDataRole.UserRole)
        self.load_projects()
        self.switch_view("chat")

    def new_project(self):
        name, ok = QInputDialog.getText(self, "New Project", "Project name:")
        if ok and name.strip():
            desc, ok2 = QInputDialog.getText(self, "New Project", "Description (optional):")
            self._start_worker("create_project", {
                "name": name.strip(), "description": desc.strip() if ok2 else ""
            }, lambda r: self.load_projects())

    def load_tasks(self):
        self._start_worker("tasks", callback=self._on_tasks)

    def _on_tasks(self, result):
        self.tasks_list.clear()
        tasks = result.get("tasks", [])
        if not tasks:
            item = QListWidgetItem("No tasks yet. Create one to get started.")
            item.setFlags(item.flags() & ~Qt.ItemFlag.ItemIsSelectable)
            self.tasks_list.addItem(item)
            return
        for task in tasks:
            check = "\u2713" if task["status"] == "completed" else ""
            text = f"{'[x]' if check else '[ ]'}  {task['title']}  —  {task['priority']}"
            item = QListWidgetItem(text)
            item.setData(Qt.ItemDataRole.UserRole, task)
            self.tasks_list.addItem(item)
        self.tasks_list.itemDoubleClicked.connect(self.toggle_task)

    def toggle_task(self, item):
        task = item.data(Qt.ItemDataRole.UserRole)
        if task:
            new_status = "pending" if task["status"] == "completed" else "completed"
            self._start_worker("update_task", {"task_id": task["id"], "status": new_status}, lambda r: self.load_tasks())

    def new_task(self):
        title, ok = QInputDialog.getText(self, "New Task", "Task title:")
        if ok and title.strip():
            priorities = ["low", "medium", "high", "urgent"]
            priority, ok2 = QInputDialog.getItem(self, "Priority", "Select priority:", priorities, 1, False)
            if ok2:
                self._start_worker("create_task", {
                    "title": title.strip(), "priority": priority, "project_id": self.current_project
                }, lambda r: self.load_tasks())

    def save_settings(self):
        QMessageBox.information(self, "Settings", "Settings saved!")

    def logout(self):
        reply = QMessageBox.question(self, "Logout", "Are you sure you want to logout?",
                                      QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
        if reply == QMessageBox.StandardButton.Yes:
            self.api.logout()
            self.login_window = LoginWindow(self.api)
            self.login_window.show()
            self.close()

    def closeEvent(self, event):
        for w in self._workers:
            w.wait(2000)
        self.api.server_process = None
        event.accept()


# ==================== Main ====================

def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")

    palette = QPalette()
    palette.setColor(QPalette.ColorRole.Window, QColor("#08081a"))
    palette.setColor(QPalette.ColorRole.WindowText, QColor(255, 255, 255, 230))
    palette.setColor(QPalette.ColorRole.Base, QColor("#08081a"))
    palette.setColor(QPalette.ColorRole.AlternateBase, QColor("#161632"))
    palette.setColor(QPalette.ColorRole.Text, QColor(255, 255, 255, 230))
    palette.setColor(QPalette.ColorRole.Button, QColor("#161632"))
    palette.setColor(QPalette.ColorRole.ButtonText, QColor(255, 255, 255, 230))
    palette.setColor(QPalette.ColorRole.Highlight, QColor("#0a84ff"))
    palette.setColor(QPalette.ColorRole.HighlightedText, QColor("#ffffff"))
    app.setPalette(palette)

    api = OzaynAPI()
    login = LoginWindow(api)
    login.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
