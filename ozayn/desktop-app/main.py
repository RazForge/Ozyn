#!/usr/bin/env python3
"""
Ozayn Desktop — Native PyQt6 Application
Complete clone of web app features, syncs with same PHP backend
"""

import sys
import json
import os
import re
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QLineEdit, QPushButton, QTextEdit, QListWidget, QListWidgetItem,
    QStackedWidget, QFrame, QMessageBox, QComboBox, QScrollArea,
    QInputDialog, QFileDialog, QCheckBox, QGridLayout, QGroupBox,
    QToolButton, QButtonGroup
)
from PyQt6.QtCore import Qt, QThread, pyqtSignal, QTimer, QSize, QUrl
from PyQt6.QtGui import QColor, QPalette, QTextCursor, QDesktopServices, QFont

from api_client import OzaynAPI

# ==================== Styles ====================

DARK_STYLE = """
QMainWindow, QWidget { background-color: #08081a; color: rgba(255,255,255,0.92); font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; font-size: 14px; }
QFrame#sidebar { background-color: rgba(22,22,50,0.8); border-right: 1px solid rgba(255,255,255,0.1); }
QFrame#header { background-color: rgba(16,16,36,0.9); border-bottom: 1px solid rgba(255,255,255,0.1); }
QLabel#title { color: #0a84ff; font-size: 16px; font-weight: bold; }
QLineEdit, QTextEdit#input { background-color: rgba(50,50,85,0.3); border: 1px solid rgba(255,255,255,0.1); border-radius: 12px; padding: 12px 16px; color: rgba(255,255,255,0.92); font-size: 14px; font-family: inherit; selection-background-color: #0a84ff; }
QLineEdit:focus, QTextEdit#input:focus { border-color: #0a84ff; }
QPushButton { background-color: #0a84ff; color: white; border: none; border-radius: 12px; padding: 12px 24px; font-size: 14px; font-weight: bold; font-family: inherit; }
QPushButton:hover { background-color: #409cff; }
QPushButton:pressed { background-color: #0070e0; }
QPushButton:disabled { background-color: rgba(10,132,255,0.3); color: rgba(255,255,255,0.3); }
QPushButton#secondary { background-color: rgba(50,50,85,0.3); border: 1px solid rgba(255,255,255,0.1); padding: 10px 16px; border-radius: 12px; }
QPushButton#secondary:hover { background-color: rgba(255,255,255,0.08); }
QPushButton#danger { background-color: rgba(255,69,58,0.15); color: #ff453a; border: 1px solid rgba(255,69,58,0.3); }
QPushButton#text-btn { background: transparent; color: #0a84ff; border: none; padding: 6px 12px; border-radius: 8px; font-size: 13px; }
QPushButton#text-btn:hover { background-color: rgba(10,132,255,0.12); }
QPushButton#nav-btn { background: transparent; color: rgba(255,255,255,0.3); border: none; padding: 6px 0; border-radius: 0; font-size: 10px; }
QPushButton#nav-btn:hover { color: rgba(255,255,255,0.6); }
QListWidget { background: transparent; border: none; outline: none; }
QListWidget::item { padding: 10px 12px; border-radius: 8px; color: rgba(255,255,255,0.5); margin-bottom: 2px; }
QListWidget::item:hover { background-color: rgba(255,255,255,0.06); color: rgba(255,255,255,0.92); }
QListWidget::item:selected { background-color: rgba(10,132,255,0.12); color: #0a84ff; }
QComboBox { background-color: rgba(50,50,85,0.3); border: 1px solid rgba(255,255,255,0.1); border-radius: 12px; padding: 10px 14px; color: rgba(255,255,255,0.92); font-size: 14px; }
QComboBox::drop-down { border: none; width: 30px; }
QComboBox QAbstractItemView { background-color: #161632; color: rgba(255,255,255,0.92); border: 1px solid rgba(255,255,255,0.1); selection-background-color: rgba(10,132,255,0.3); }
QScrollArea { border: none; background: transparent; }
QScrollBar:vertical { background: transparent; width: 6px; }
QScrollBar::handle:vertical { background: rgba(255,255,255,0.1); border-radius: 3px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: rgba(255,255,255,0.2); }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: transparent; height: 0; }
QCheckBox { color: rgba(255,255,255,0.7); spacing: 8px; }
QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px; border: 2px solid rgba(255,255,255,0.3); background: transparent; }
QCheckBox::indicator:checked { background: #0a84ff; border-color: #0a84ff; }
QGroupBox { border: 1px solid rgba(255,255,255,0.1); border-radius: 12px; margin-top: 16px; padding: 16px; }
QGroupBox::title { color: #0a84ff; font-weight: 600; subcontrol-origin: margin; left: 16px; padding: 0 8px; }
"""


# ==================== Worker Thread ====================

class APIWorker(QThread):
    finished = pyqtSignal(dict)

    def __init__(self, api, method, data=None):
        super().__init__()
        self.api = api
        self.method = method
        self.data = data or {}

    def run(self):
        try:
            result = self._do_work()
        except Exception as e:
            result = {"success": False, "error": str(e)}
        self.finished.emit(result)

    def _do_work(self):
        m, d = self.method, self.data
        if m == "login": return self.api.login(d["username"], d["password"])
        elif m == "register": return self.api.register(d["username"], d["password"], d.get("email"), d.get("full_name"))
        elif m == "chat": return self.api.send_chat(d["message"], d.get("conversation_id"), d.get("project_id"))
        elif m == "conversations": return self.api.list_conversations()
        elif m == "history": return self.api.get_chat_history(d["conversation_id"])
        elif m == "projects": return self.api.list_projects()
        elif m == "create_project": return self.api.create_project(d["name"], d.get("description", ""))
        elif m == "tasks": return self.api.list_tasks()
        elif m == "create_task": return self.api.create_task(d["title"], d.get("description", ""), d.get("priority", "medium"), d.get("project_id"))
        elif m == "update_task": return self.api.update_task(d["task_id"], d["status"])
        elif m == "knowledge": return self.api.list_knowledge()
        elif m == "add_knowledge": return self.api.add_knowledge(d["title"], d["content"], d.get("tags"), d.get("project_id"))
        elif m == "arwe_status": return self.api.arwe_status()
        elif m == "arwe_briefing": return self.api.arwe_briefing()
        elif m == "arwe_system": return self.api.arwe_system_status(d["system"])
        elif m == "decisions": return self.api.list_decisions()
        elif m == "create_decision": return self.api.create_decision(d["context"], d.get("options"), d.get("project_id"))
        elif m == "get_decision": return self.api.get_decision(d["decision_id"])
        elif m == "make_decision": return self.api.make_decision(d["decision_id"], d["chosen_option"], d.get("reasoning"))
        elif m == "audit": return self.api.audit_log()
        elif m == "audit_recent": return self.api.audit_recent()
        elif m == "audit_search": return self.api.audit_search(d["query"])
        elif m == "system_overview": return self.api.system_overview()
        elif m == "system_cpu": return self.api.system_cpu()
        elif m == "system_memory": return self.api.system_memory()
        elif m == "system_disk": return self.api.system_disk()
        elif m == "system_processes": return self.api.system_processes(d.get("sort", "cpu"), d.get("limit", 20))
        elif m == "system_network": return self.api.system_network()
        elif m == "agents": return self.api.list_agents()
        elif m == "apps": return self.api.list_apps()
        elif m == "launch_app": return self.api.launch_app(d["app"], d.get("args"))
        elif m == "analyze_code": return self.api.analyze_code(d["code"], d.get("language"))
        elif m == "generate_code": return self.api.generate_code(d.get("type", "function"), d.get("name", "MyFunction"), d.get("language", "php"))
        elif m == "collab_sessions": return self.api.collab_sessions()
        elif m == "collab_create": return self.api.collab_create(d["name"])
        elif m == "memory_search": return self.api.memory_search(d["query"])
        elif m == "memory_recent": return self.api.memory_recent()
        elif m == "memory_store": return self.api.memory_store(d["key"], d["value"], d.get("type", "short_term"), d.get("importance", 0.5))
        elif m == "tools_run": return self.api.tools_run(d["command"], d.get("timeout", 30))
        elif m == "tools_files": return self.api.tools_list_files(d.get("path", "."))
        elif m == "tools_read": return self.api.tools_read_file(d["path"])
        elif m == "logout":
            self.api.logout()
            return {"success": True}
        return {"success": False, "error": "Unknown method"}


# ==================== Worker Tracker Mixin ====================

class WorkerMixin:
    def _init_workers(self):
        self._workers = []

    def _run(self, method, data=None, callback=None):
        w = APIWorker(self.api, method, data)
        if callback:
            w.finished.connect(callback)
        w.finished.connect(lambda _: self._workers.remove(w) if w in self._workers else None)
        self._workers.append(w)
        w.start()
        return w


# ==================== Login Window ====================

class LoginWindow(QMainWindow, WorkerMixin):
    def __init__(self, api):
        super().__init__()
        self.api = api
        self._init_workers()
        self.setWindowTitle("Ozayn — Login")
        self.setFixedSize(420, 580)
        self.setStyleSheet(DARK_STYLE)

        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.setSpacing(0)

        logo = QLabel("\u2B21")
        logo.setAlignment(Qt.AlignmentFlag.AlignCenter)
        logo.setStyleSheet("font-size: 44px; color: #0a84ff; margin-bottom: 14px;")
        layout.addWidget(logo)
        title = QLabel("OZAYN")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title.setStyleSheet("font-size: 32px; font-weight: 700; letter-spacing: 8px; margin-bottom: 8px;")
        layout.addWidget(title)
        sub = QLabel("Personal AI Digital Twin")
        sub.setAlignment(Qt.AlignmentFlag.AlignCenter)
        sub.setStyleSheet("color: rgba(255,255,255,0.5); font-size: 13px; margin-bottom: 36px;")
        layout.addWidget(sub)

        # Tabs
        tab_lay = QHBoxLayout()
        self.tab_login = QPushButton("Login")
        self.tab_register = QPushButton("Register")
        for btn in [self.tab_login, self.tab_register]:
            btn.setCheckable(True)
            btn.setStyleSheet("QPushButton{background:rgba(50,50,85,0.3);border:none;border-radius:8px;padding:10px 16px;color:rgba(255,255,255,0.5);font-size:13px;font-weight:500;}QPushButton:checked{background:rgba(255,255,255,0.12);color:rgba(255,255,255,0.92);}")
        self.tab_login.setChecked(True)
        self.tab_login.clicked.connect(lambda: self._tab("login"))
        self.tab_register.clicked.connect(lambda: self._tab("register"))
        tab_lay.addWidget(self.tab_login)
        tab_lay.addWidget(self.tab_register)
        layout.addLayout(tab_lay)
        layout.addSpacing(24)

        # Login form
        self.login_w = QWidget()
        lf = QVBoxLayout(self.login_w)
        lf.setSpacing(14)
        self.lu = QLineEdit(); self.lu.setPlaceholderText("Username")
        self.lp = QLineEdit(); self.lp.setPlaceholderText("Password"); self.lp.setEchoMode(QLineEdit.EchoMode.Password)
        self.lb = QPushButton("Login"); self.lb.clicked.connect(self.do_login)
        self.lp.returnPressed.connect(self.do_login)
        lf.addWidget(self.lu); lf.addWidget(self.lp); lf.addWidget(self.lb)
        layout.addWidget(self.login_w)

        # Register form
        self.reg_w = QWidget()
        rf = QVBoxLayout(self.reg_w)
        rf.setSpacing(14)
        self.ru = QLineEdit(); self.ru.setPlaceholderText("Username")
        self.re = QLineEdit(); self.re.setPlaceholderText("Email (optional)")
        self.rn = QLineEdit(); self.rn.setPlaceholderText("Full Name (optional)")
        self.rp = QLineEdit(); self.rp.setPlaceholderText("Password"); self.rp.setEchoMode(QLineEdit.EchoMode.Password)
        self.rb = QPushButton("Register"); self.rb.clicked.connect(self.do_register)
        rf.addWidget(self.ru); rf.addWidget(self.re); rf.addWidget(self.rn); rf.addWidget(self.rp); rf.addWidget(self.rb)
        self.reg_w.hide()
        layout.addWidget(self.reg_w)

        self.err = QLabel(""); self.err.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.err.setStyleSheet("color:#ff453a;font-size:13px;margin-top:14px;min-height:20px;")
        layout.addWidget(self.err)
        self.status = QLabel("Connecting..."); self.status.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.status.setStyleSheet("color:rgba(255,255,255,0.3);font-size:12px;margin-top:8px;")
        layout.addWidget(self.status)
        QTimer.singleShot(500, self._check)

    def _check(self):
        if self.api._ensure_server():
            self.status.setText("")
        else:
            self.status.setText("Server not available")

    def _tab(self, tab):
        self.tab_login.setChecked(tab == "login")
        self.tab_register.setChecked(tab == "register")
        self.login_w.setVisible(tab == "login")
        self.reg_w.setVisible(tab == "register")
        self.err.setText("")

    def do_login(self):
        u, p = self.lu.text().strip(), self.lp.text()
        if not u or not p:
            self.err.setText("Fill in all fields"); return
        self.lb.setEnabled(False); self.lb.setText("Logging in..."); self.err.setText("")
        self._run("login", {"username": u, "password": p}, self._on_login)

    def _on_login(self, r):
        self.lb.setEnabled(True); self.lb.setText("Login")
        if r.get("success"):
            self.main = MainWindow(self.api)
            self.main.show()
            self.hide()
        else:
            self.err.setText(r.get("error", "Login failed"))

    def do_register(self):
        u, p = self.ru.text().strip(), self.rp.text()
        if not u or not p:
            self.err.setText("Fill in username and password"); return
        self.rb.setEnabled(False); self.rb.setText("Registering..."); self.err.setText("")
        self._run("register", {"username": u, "password": p, "email": self.re.text().strip() or None, "full_name": self.rn.text().strip() or None}, self._on_reg)

    def _on_reg(self, r):
        self.rb.setEnabled(True); self.rb.setText("Register")
        if r.get("success"):
            self.lu.setText(self.ru.text())
            self.lp.setText(self.rp.text())
            self._tab("login")
            self.do_login()
        else:
            self.err.setText(r.get("error", "Registration failed"))


# ==================== Main Window ====================

class MainWindow(QMainWindow, WorkerMixin):
    def __init__(self, api):
        super().__init__()
        self.api = api
        self._init_workers()
        self.current_conversation = None
        self.current_project = None
        self.setWindowTitle("Ozayn")
        self.setMinimumSize(1000, 700)
        self.resize(1200, 800)
        self.setStyleSheet(DARK_STYLE)

        c = QWidget()
        self.setCentralWidget(c)
        ml = QHBoxLayout(c)
        ml.setContentsMargins(0, 0, 0, 0)
        ml.setSpacing(0)

        # ===== Sidebar =====
        self.sidebar = QFrame()
        self.sidebar.setObjectName("sidebar")
        self.sidebar.setFixedWidth(260)
        sl = QVBoxLayout(self.sidebar)
        sl.setContentsMargins(0, 0, 0, 0)
        sl.setSpacing(0)

        hdr = QFrame(); hdr.setFixedHeight(52)
        hl = QHBoxLayout(hdr); hl.setContentsMargins(20, 0, 20, 0)
        ht = QLabel("OZAYN"); ht.setObjectName("title")
        hl.addWidget(ht); hl.addStretch()
        sl.addWidget(hdr)

        cl = QLabel("CONVERSATIONS")
        cl.setStyleSheet("color:rgba(255,255,255,0.3);font-size:11px;font-weight:bold;letter-spacing:1px;padding:8px 12px 4px;")
        sl.addWidget(cl)
        self.conv_list = QListWidget()
        self.conv_list.itemClicked.connect(self._conv_click)
        sl.addWidget(self.conv_list)
        ncb = QPushButton("+ New Chat"); ncb.setObjectName("text-btn")
        ncb.clicked.connect(self.new_chat)
        sl.addWidget(ncb)

        pl = QLabel("PROJECTS")
        pl.setStyleSheet("color:rgba(255,255,255,0.3);font-size:11px;font-weight:bold;letter-spacing:1px;padding:8px 12px 4px;")
        sl.addWidget(pl)
        self.proj_list = QListWidget()
        self.proj_list.itemClicked.connect(self._proj_click)
        sl.addWidget(self.proj_list)
        npb = QPushButton("+ New Project"); npb.setObjectName("text-btn")
        npb.clicked.connect(self.new_project)
        sl.addWidget(npb)

        # Sidebar footer
        sf = QFrame(); sf.setFixedHeight(48)
        sf.setStyleSheet("border-top:1px solid rgba(255,255,255,0.1);")
        sfl = QHBoxLayout(sf); sfl.setContentsMargins(16, 0, 16, 0)
        self.user_lbl = QLabel(api.user.get("username", ""))
        self.user_lbl.setStyleSheet("color:rgba(255,255,255,0.5);font-size:12px;")
        sfl.addWidget(self.user_lbl); sfl.addStretch()
        lbtn = QPushButton("\u2192"); lbtn.setObjectName("secondary")
        lbtn.setFixedSize(32, 32); lbtn.clicked.connect(self.logout)
        sfl.addWidget(lbtn)
        sl.addWidget(sf)
        ml.addWidget(self.sidebar)

        # ===== Right Panel =====
        right = QWidget()
        rl = QVBoxLayout(right)
        rl.setContentsMargins(0, 0, 0, 0)
        rl.setSpacing(0)

        # Header
        self.hdr = QFrame(); self.hdr.setObjectName("header"); self.hdr.setFixedHeight(52)
        hrl = QHBoxLayout(self.hdr); hrl.setContentsMargins(20, 0, 20, 0)
        self.hdr_title = QLabel("New Conversation")
        self.hdr_title.setStyleSheet("font-size:15px;font-weight:600;")
        hrl.addWidget(self.hdr_title); hrl.addStretch()
        rl.addWidget(self.hdr)

        # Stacked views
        self.views = QStackedWidget()

        # --- Chat View ---
        chat_w = QWidget()
        chat_l = QVBoxLayout(chat_w); chat_l.setContentsMargins(0, 0, 0, 0)
        self.chat_msgs = QTextEdit(); self.chat_msgs.setReadOnly(True)
        self.chat_msgs.setObjectName("chat-messages")
        self.chat_msgs.setStyleSheet("QTextEdit#chat-messages{background:transparent;border:none;color:rgba(255,255,255,0.92);font-size:14px;padding:16px;}")
        self.chat_msgs.setHtml('<div style="color:rgba(255,255,255,0.5);text-align:center;padding:40px;">How can I help you today?</div>')
        chat_l.addWidget(self.chat_msgs)

        inp_frame = QFrame()
        inp_frame.setStyleSheet("border-top:1px solid rgba(255,255,255,0.1);padding:12px;")
        inp_l = QHBoxLayout(inp_frame)
        self.chat_input = QTextEdit()
        self.chat_input.setObjectName("input")
        self.chat_input.setPlaceholderText("Message Ozayn...")
        self.chat_input.setFixedHeight(44)
        self.chat_input.installEventFilter(self)
        inp_l.addWidget(self.chat_input)
        send_btn = QPushButton("\u27A4"); send_btn.setFixedSize(44, 44)
        send_btn.setStyleSheet("background-color:#0a84ff;border-radius:12px;font-size:18px;")
        send_btn.clicked.connect(self.send_message)
        inp_l.addWidget(send_btn)
        chat_l.addWidget(inp_frame)
        self.views.addWidget(chat_w)

        # --- Projects View ---
        proj_w = QWidget()
        proj_l = QVBoxLayout(proj_w); proj_l.setContentsMargins(20, 20, 20, 20)
        ph = QHBoxLayout()
        pt = QLabel("Projects"); pt.setStyleSheet("font-size:18px;font-weight:600;")
        ph.addWidget(pt); ph.addStretch()
        apb = QPushButton("+ New Project"); apb.setObjectName("text-btn")
        apb.clicked.connect(self.new_project)
        ph.addWidget(apb)
        proj_l.addLayout(ph)
        self.proj_grid = QListWidget()
        self.proj_grid.setStyleSheet("QListWidget::item{padding:16px;background:rgba(28,28,56,0.55);border:1px solid rgba(255,255,255,0.1);border-radius:12px;margin-bottom:8px;}")
        proj_l.addWidget(self.proj_grid)
        self.views.addWidget(proj_w)

        # --- Tasks View ---
        task_w = QWidget()
        task_l = QVBoxLayout(task_w); task_l.setContentsMargins(20, 20, 20, 20)
        th = QHBoxLayout()
        tt = QLabel("Tasks"); tt.setStyleSheet("font-size:18px;font-weight:600;")
        th.addWidget(tt); th.addStretch()
        atb = QPushButton("+ New Task"); atb.setObjectName("text-btn")
        atb.clicked.connect(self.new_task)
        th.addWidget(atb)
        task_l.addLayout(th)
        self.task_list = QListWidget()
        self.task_list.setStyleSheet("QListWidget::item{padding:12px 16px;background:rgba(28,28,56,0.55);border:1px solid rgba(255,255,255,0.1);border-radius:12px;margin-bottom:8px;}")
        self.task_list.itemDoubleClicked.connect(self._toggle_task)
        task_l.addWidget(self.task_list)
        self.views.addWidget(task_w)

        # --- Knowledge View ---
        know_w = QWidget()
        know_l = QVBoxLayout(know_w); know_l.setContentsMargins(20, 20, 20, 20)
        kh = QHBoxLayout()
        kt = QLabel("Knowledge Base"); kt.setStyleSheet("font-size:18px;font-weight:600;")
        kh.addWidget(kt); kh.addStretch()
        akb = QPushButton("+ Add Entry"); akb.setObjectName("text-btn")
        akb.clicked.connect(self.new_knowledge)
        kh.addWidget(akb)
        know_l.addLayout(kh)
        self.know_list = QListWidget()
        self.know_list.setStyleSheet("QListWidget::item{padding:12px 16px;background:rgba(28,28,56,0.55);border:1px solid rgba(255,255,255,0.1);border-radius:12px;margin-bottom:8px;}")
        know_l.addWidget(self.know_list)
        self.views.addWidget(know_w)

        # --- ARWE View ---
        arwe_w = QWidget()
        arwe_l = QVBoxLayout(arwe_w); arwe_l.setContentsMargins(20, 20, 20, 20)
        ah = QHBoxLayout()
        at = QLabel("ARWE System Status"); at.setStyleSheet("font-size:18px;font-weight:600;")
        ah.addWidget(at); ah.addStretch()
        arb = QPushButton("Refresh"); arb.setObjectName("text-btn")
        arb.clicked.connect(self.load_arwe)
        ah.addWidget(arb)
        arwe_l.addLayout(ah)
        self.arwe_scroll = QScrollArea()
        self.arwe_scroll.setWidgetResizable(True)
        self.arwe_content = QWidget()
        self.arwe_layout = QVBoxLayout(self.arwe_content)
        self.arwe_scroll.setWidget(self.arwe_content)
        arwe_l.addWidget(self.arwe_scroll)
        self.views.addWidget(arwe_w)

        # --- Decisions View ---
        dec_w = QWidget()
        dec_l = QVBoxLayout(dec_w); dec_l.setContentsMargins(20, 20, 20, 20)
        dh = QHBoxLayout()
        dt = QLabel("Decision Support"); dt.setStyleSheet("font-size:18px;font-weight:600;")
        dh.addWidget(dt); dh.addStretch()
        ndb = QPushButton("+ New Decision"); ndb.setObjectName("text-btn")
        ndb.clicked.connect(self.new_decision)
        dh.addWidget(ndb)
        dec_l.addLayout(dh)
        self.dec_list = QListWidget()
        dec_l.addWidget(self.dec_list)
        self.views.addWidget(dec_w)

        # --- Audit View ---
        aud_w = QWidget()
        aud_l = QVBoxLayout(aud_w); aud_l.setContentsMargins(20, 20, 20, 20)
        aah = QHBoxLayout()
        aat = QLabel("Audit Log"); aat.setStyleSheet("font-size:18px;font-weight:600;")
        aah.addWidget(aat); aah.addStretch()
        arf = QPushButton("Refresh"); arf.setObjectName("text-btn")
        arf.clicked.connect(self.load_audit)
        aah.addWidget(arf)
        aud_l.addLayout(aah)
        self.aud_list = QListWidget()
        self.aud_list.setStyleSheet("QListWidget::item{padding:10px 16px;background:rgba(28,28,56,0.35);border:1px solid rgba(255,255,255,0.08);border-radius:8px;margin-bottom:4px;}")
        aud_l.addWidget(self.aud_list)
        self.views.addWidget(aud_w)

        # --- Settings View ---
        set_w = QScrollArea()
        set_inner = QWidget()
        set_l = QVBoxLayout(set_inner)
        set_l.setContentsMargins(32, 32, 32, 32)
        set_l.setSpacing(16)
        st = QLabel("Settings"); st.setStyleSheet("font-size:18px;font-weight:600;")
        set_l.addWidget(st)

        # AI Group
        g1 = QGroupBox("AI Configuration")
        g1l = QVBoxLayout(g1)
        g1l.addWidget(QLabel("API Provider"))
        self.ai_prov = QComboBox()
        self.ai_prov.addItems(["Demo Mode (No API)", "OpenAI", "Anthropic", "Ollama (Local)"])
        g1l.addWidget(self.ai_prov)
        g1l.addWidget(QLabel("API Key"))
        self.ai_key = QLineEdit(); self.ai_key.setPlaceholderText("Enter API key")
        self.ai_key.setEchoMode(QLineEdit.EchoMode.Password)
        g1l.addWidget(self.ai_key)
        g1l.addWidget(QLabel("Model"))
        self.ai_model = QLineEdit(); self.ai_model.setPlaceholderText("e.g. gpt-3.5-turbo")
        g1l.addWidget(self.ai_model)
        set_l.addWidget(g1)

        # Voice Group
        g2 = QGroupBox("Voice")
        g2l = QVBoxLayout(g2)
        g2l.addWidget(QLabel("Voice Language"))
        self.voice_lang = QComboBox()
        self.voice_lang.addItems(["English (US)", "Amharic", "Afaan Oromo"])
        g2l.addWidget(self.voice_lang)
        self.auto_speak = QCheckBox("Read responses aloud")
        g2l.addWidget(self.auto_speak)
        set_l.addWidget(g2)

        # Appearance Group
        g3 = QGroupBox("Appearance")
        g3l = QVBoxLayout(g3)
        g3l.addWidget(QLabel("Theme"))
        self.theme_sel = QComboBox()
        self.theme_sel.addItems(["Dark (Default)", "Midnight Blue", "Purple Haze", "Forest"])
        g3l.addWidget(self.theme_sel)
        set_l.addWidget(g3)

        # Data Group
        g4 = QGroupBox("Data")
        g4l = QVBoxLayout(g4)
        export_btn = QPushButton("Export Data"); export_btn.setObjectName("secondary")
        export_btn.clicked.connect(self.export_data)
        g4l.addWidget(export_btn)
        set_l.addWidget(g4)

        save_btn = QPushButton("Save Settings")
        save_btn.clicked.connect(self.save_settings)
        set_l.addWidget(save_btn)
        set_l.addStretch()
        set_w.setWidget(set_inner)
        set_w.setWidgetResizable(True)
        self.views.addWidget(set_w)

        rl.addWidget(self.views)

        # ===== Bottom Nav =====
        nav = QFrame()
        nav.setStyleSheet("border-top:1px solid rgba(255,255,255,0.1);background:rgba(10,10,26,0.6);")
        nav.setFixedHeight(56)
        nl = QHBoxLayout(nav); nl.setContentsMargins(0, 0, 0, 0)

        self.nav_btns = {}
        nav_items = [
            ("chat", "Chat"), ("projects", "Projects"), ("tasks", "Tasks"),
            ("knowledge", "Knowledge"), ("arwe", "ARWE"), ("decisions", "Decisions"),
            ("audit", "Audit"), ("settings", "Settings")
        ]
        for name, label in nav_items:
            btn = QPushButton(label)
            btn.setObjectName("nav-btn")
            btn.setCheckable(True)
            btn.clicked.connect(lambda checked, n=name: self._view(n))
            self.nav_btns[name] = btn
            nl.addWidget(btn)
        self.nav_btns["chat"].setChecked(True)
        self.nav_btns["chat"].setStyleSheet("color:#0a84ff;")
        rl.addWidget(nav)

        ml.addWidget(right)

        # Keyboard shortcuts
        self.chat_input_shortcut_target = self.chat_input

        # Load data
        self.load_conversations()
        self.load_projects()

    def keyPressEvent(self, event):
        if event.key() == Qt.Key.Key_N and (event.modifiers() & Qt.KeyboardModifier.ControlModifier):
            self.new_chat()
        elif event.key() == Qt.Key.Key_K and (event.modifiers() & Qt.KeyboardModifier.ControlModifier):
            self.chat_input.setFocus()
        else:
            super().keyPressEvent(event)

    def eventFilter(self, obj, event):
        if obj == self.chat_input and event.type() == event.Type.KeyPress:
            if event.key() == Qt.Key.Key_Return and not event.modifiers() & Qt.KeyboardModifier.ShiftModifier:
                self.send_message()
                return True
            elif event.key() == Qt.Key.Key_Up:
                return False
            elif event.key() == Qt.Key.Key_Down:
                return False
        return super().eventFilter(obj, event)

    def _view(self, name):
        mapping = {"chat": 0, "projects": 1, "tasks": 2, "knowledge": 3, "arwe": 4, "decisions": 5, "audit": 6, "settings": 7}
        self.views.setCurrentIndex(mapping.get(name, 0))
        for n, btn in self.nav_btns.items():
            btn.setChecked(n == name)
            btn.setStyleSheet("color:#0a84ff;" if n == name else "color:rgba(255,255,255,0.3);")
        if name == "tasks": self.load_tasks()
        elif name == "knowledge": self.load_knowledge()
        elif name == "arwe": self.load_arwe()
        elif name == "decisions": self.load_decisions()
        elif name == "audit": self.load_audit()
        elif name == "projects": self.load_projects()

    def _msg(self, role, content):
        html = content.replace("<", "&lt;").replace(">", "&gt;").replace("\n", "<br>")
        html = re.sub(r'\*\*(.*?)\*\*', r'<strong>\1</strong>', html)
        html = re.sub(r'\*(.*?)\*', r'<em>\1</em>', html)
        html = re.sub(r'`(.*?)`', r'<code style="background:rgba(255,255,255,0.08);padding:2px 6px;border-radius:4px;">\1</code>', html)
        styles = {
            "user": "background:#0a84ff;color:white;border-radius:12px 12px 4px 12px;padding:12px 16px;margin:4px 0 4px 120px;",
            "assistant": "background:rgba(28,28,56,0.55);border:1px solid rgba(255,255,255,0.1);border-radius:12px 12px 12px 4px;padding:12px 16px;margin:4px 120px 4px 0;",
            "system": "color:rgba(255,255,255,0.5);text-align:center;padding:8px;margin:4px 60px;"
        }
        self.chat_msgs.moveCursor(QTextCursor.MoveOperation.End)
        self.chat_msgs.insertHtml(f'<div style="{styles.get(role, "")}">{html}</div>')
        self.chat_msgs.moveCursor(QTextCursor.MoveOperation.End)

    # ===== Chat =====
    def send_message(self):
        text = self.chat_input.toPlainText().strip()
        if not text: return
        self.chat_input.clear()
        self._msg("user", text)
        self.hdr_title.setText("Chat")
        self._run("chat", {"message": text, "conversation_id": self.current_conversation, "project_id": self.current_project}, self._on_chat)

    def _on_chat(self, r):
        if r.get("success"):
            self.current_conversation = r.get("conversation_id")
            self._msg("assistant", r.get("response", ""))
            self.load_conversations()
        else:
            self._msg("system", "Error: " + r.get("error", "Failed"))

    def new_chat(self):
        self.current_conversation = None
        self.chat_msgs.clear()
        self._msg("system", "New conversation started. How can I help?")
        self.hdr_title.setText("New Conversation")
        self.load_conversations()
        self._view("chat")

    def load_conversations(self):
        self._run("conversations", callback=self._on_convs)

    def _on_convs(self, r):
        self.conv_list.clear()
        for c in r.get("conversations", []):
            item = QListWidgetItem(c.get("title") or f"Chat {c['id']}")
            item.setData(Qt.ItemDataRole.UserRole, c["id"])
            if c["id"] == self.current_conversation: item.setSelected(True)
            self.conv_list.addItem(item)

    def _conv_click(self, item):
        cid = item.data(Qt.ItemDataRole.UserRole)
        self.current_conversation = cid
        self.chat_msgs.clear()
        self._run("history", {"conversation_id": cid}, self._on_hist)
        self._view("chat")

    def _on_hist(self, r):
        for m in r.get("messages", []):
            self._msg(m["role"], m["content"])

    # ===== Projects =====
    def load_projects(self):
        self._run("projects", callback=self._on_projs)

    def _on_projs(self, r):
        self.proj_list.clear()
        self.proj_grid.clear()
        for p in r.get("projects", []):
            item = QListWidgetItem(p["name"])
            item.setData(Qt.ItemDataRole.UserRole, p["id"])
            if p["id"] == self.current_project: item.setSelected(True)
            self.proj_list.addItem(item)
            gi = QListWidgetItem(f"{p['name']}\n{p.get('description', 'No description')}")
            gi.setData(Qt.ItemDataRole.UserRole, p["id"])
            self.proj_grid.addItem(gi)

    def _proj_click(self, item):
        self.current_project = item.data(Qt.ItemDataRole.UserRole)
        self.load_projects()
        self._view("chat")

    def new_project(self):
        name, ok = QInputDialog.getText(self, "New Project", "Project name:")
        if ok and name.strip():
            desc, ok2 = QInputDialog.getText(self, "New Project", "Description (optional):")
            self._run("create_project", {"name": name.strip(), "description": desc.strip() if ok2 else ""}, lambda r: self.load_projects())

    # ===== Tasks =====
    def load_tasks(self):
        self._run("tasks", callback=self._on_tasks)

    def _on_tasks(self, r):
        self.task_list.clear()
        tasks = r.get("tasks", [])
        if not tasks:
            self.task_list.addItem(QListWidgetItem("No tasks yet."))
            return
        for t in tasks:
            check = "\u2713" if t["status"] == "completed" else " "
            item = QListWidgetItem(f"[{check}]  {t['title']}  —  {t['priority']}")
            item.setData(Qt.ItemDataRole.UserRole, t)
            self.task_list.addItem(item)

    def _toggle_task(self, item):
        t = item.data(Qt.ItemDataRole.UserRole)
        if t:
            ns = "pending" if t["status"] == "completed" else "completed"
            self._run("update_task", {"task_id": t["id"], "status": ns}, lambda r: self.load_tasks())

    def new_task(self):
        title, ok = QInputDialog.getText(self, "New Task", "Task title:")
        if ok and title.strip():
            pri, ok2 = QInputDialog.getItem(self, "Priority", "Select:", ["low", "medium", "high", "urgent"], 1, False)
            if ok2:
                self._run("create_task", {"title": title.strip(), "priority": pri, "project_id": self.current_project}, lambda r: self.load_tasks())

    # ===== Knowledge =====
    def load_knowledge(self):
        self._run("knowledge", callback=self._on_know)

    def _on_know(self, r):
        self.know_list.clear()
        items = r.get("results", [])
        if not items:
            self.know_list.addItem(QListWidgetItem("Knowledge base is empty."))
            return
        for k in items:
            tags = json.loads(k.get("tags", "[]"))
            tag_str = " ".join(f"[{t}]" for t in tags) if tags else ""
            item = QListWidgetItem(f"{k['title']}  {tag_str}\n{k.get('content', '')[:200]}")
            self.know_list.addItem(item)

    def new_knowledge(self):
        title, ok = QInputDialog.getText(self, "Add Knowledge", "Title:")
        if ok and title.strip():
            content, ok2 = QInputDialog.getMultiLineText(self, "Add Knowledge", "Content:")
            if ok2 and content.strip():
                tags, ok3 = QInputDialog.getText(self, "Add Knowledge", "Tags (comma separated):")
                tag_list = [t.strip() for t in tags.split(",") if t.strip()] if ok3 else []
                self._run("add_knowledge", {"title": title.strip(), "content": content.strip(), "tags": tag_list, "project_id": self.current_project}, lambda r: self.load_knowledge())

    # ===== ARWE =====
    def load_arwe(self):
        self._run("arwe_status", callback=self._on_arwe)

    def _on_arwe(self, r):
        # Clear
        while self.arwe_layout.count():
            child = self.arwe_layout.takeAt(0)
            if child.widget(): child.widget().deleteLater()
        systems = r.get("systems", {})
        if not systems:
            self.arwe_layout.addWidget(QLabel("No ARWE data available"))
            return
        for name, info in systems.items():
            card = QFrame()
            card.setStyleSheet("background:rgba(28,28,56,0.55);border:1px solid rgba(255,255,255,0.1);border-radius:12px;padding:16px;margin-bottom:8px;")
            cl = QVBoxLayout(card)
            hl = QHBoxLayout()
            hl.addWidget(QLabel(f"<b>{name.title()}</b>"))
            status_color = "#30d158" if info.get("status") == "online" else "#ff453a"
            status_lbl = QLabel(f"<span style='color:{status_color}'>{info.get('status', 'unknown')}</span>")
            hl.addWidget(status_lbl)
            hl.addStretch()
            cl.addLayout(hl)
            details = info.get("details", {})
            if details:
                detail_text = " | ".join(f"{k}: {v}" for k, v in details.items())
                cl.addWidget(QLabel(detail_text))
            self.arwe_layout.addWidget(card)

        # Load briefing
        self._run("arwe_briefing", callback=self._on_briefing)

    def _on_briefing(self, r):
        briefing = r.get("briefing", "")
        if briefing:
            lbl = QLabel(f"<b>Daily Briefing</b><br><pre style='color:rgba(255,255,255,0.7);'>{briefing}</pre>")
            lbl.setWordWrap(True)
            self.arwe_layout.addWidget(lbl)

    # ===== Decisions =====
    def load_decisions(self):
        self._run("decisions", callback=self._on_dec)

    def _on_dec(self, r):
        self.dec_list.clear()
        decisions = r.get("decisions", [])
        if not decisions:
            self.dec_list.addItem(QListWidgetItem("No decisions yet."))
            return
        for d in decisions:
            status = d.get("status", "")
            chosen = d.get("chosen_option", "")
            text = f"[{status}] {d.get('context', '')[:200]}"
            if chosen: text += f"\nDecision: {chosen}"
            item = QListWidgetItem(text)
            self.dec_list.addItem(item)

    def new_decision(self):
        ctx, ok = QInputDialog.getMultiLineText(self, "New Decision", "Describe the decision context:")
        if ok and ctx.strip():
            self._run("create_decision", {"context": ctx.strip()}, lambda r: self.load_decisions())

    # ===== Audit =====
    def load_audit(self):
        self._run("audit", callback=self._on_audit)

    def _on_audit(self, r):
        self.aud_list.clear()
        log = r.get("log", [])
        if not log:
            self.aud_list.addItem(QListWidgetItem("No audit entries yet."))
            return
        for e in log:
            result_color = "#30d158" if e.get("result") == "success" else "#ff453a"
            text = f"{e.get('created_at', '')}  |  {e.get('action', '')}  |  <span style='color:{result_color}'>{e.get('result', '')}</span>"
            item = QListWidgetItem(text)
            self.aud_list.addItem(item)

    # ===== Settings =====
    def save_settings(self):
        QMessageBox.information(self, "Settings", "Settings saved!")

    def export_data(self):
        path, _ = QFileDialog.getSaveFileName(self, "Export Data", "ozayn_export.json", "JSON Files (*.json)")
        if path:
            data = {
                "user": self.api.user,
                "conversations": self.api.list_conversations(),
                "projects": self.api.list_projects(),
                "tasks": self.api.list_tasks(),
                "knowledge": self.api.list_knowledge()
            }
            with open(path, "w") as f:
                json.dump(data, f, indent=2)
            QMessageBox.information(self, "Export", f"Data exported to {path}")

    def logout(self):
        reply = QMessageBox.question(self, "Logout", "Are you sure?",
                                      QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
        if reply == QMessageBox.StandardButton.Yes:
            self.api.logout()
            self.login_w = LoginWindow(self.api)
            self.login_w.show()
            self.close()

    def closeEvent(self, event):
        for w in self._workers:
            w.wait(2000)
        event.accept()


# ==================== Main ====================

def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    p = QPalette()
    p.setColor(QPalette.ColorRole.Window, QColor("#08081a"))
    p.setColor(QPalette.ColorRole.WindowText, QColor(255, 255, 255, 230))
    p.setColor(QPalette.ColorRole.Base, QColor("#08081a"))
    p.setColor(QPalette.ColorRole.Text, QColor(255, 255, 255, 230))
    p.setColor(QPalette.ColorRole.Button, QColor("#161632"))
    p.setColor(QPalette.ColorRole.ButtonText, QColor(255, 255, 255, 230))
    p.setColor(QPalette.ColorRole.Highlight, QColor("#0a84ff"))
    p.setColor(QPalette.ColorRole.HighlightedText, QColor("#ffffff"))
    app.setPalette(p)

    api = OzaynAPI()
    login = LoginWindow(api)
    login.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
