"""
Ozayn Main Window — Navigation and view container
"""

import time, sys

from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QLabel,
    QPushButton, QStackedWidget, QFrame, QMessageBox
)
from PyQt6.QtCore import Qt, QSize
from PyQt6.QtGui import QKeySequence, QShortcut

from ozayn.theme.dark import DARK_STYLE
from ozayn.workers import WorkerMixin


def _dbg(msg):
    print(f"  [MW] {msg}", flush=True)


class MainWindow(QMainWindow, WorkerMixin):
    def __init__(self, api, on_logout):
        super().__init__()
        self.api = api
        self.on_logout = on_logout
        self._init_workers()
        _dbg("init workers done")
        self.setWindowTitle("Ozayn — AI Digital Twin")
        self.setMinimumSize(1200, 750)
        self.resize(1400, 850)
        self.setStyleSheet(DARK_STYLE)
        _dbg("stylesheet applied")

        central = QWidget()
        self.setCentralWidget(central)
        layout = QHBoxLayout(central)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        _dbg("layout created")

        # Sidebar
        sidebar = QFrame()
        sidebar.setObjectName("sidebar")
        sidebar.setFixedWidth(72)
        sl = QVBoxLayout(sidebar)
        sl.setContentsMargins(0, 16, 0, 12)
        sl.setSpacing(0)

        logo = QLabel("O")
        logo.setAlignment(Qt.AlignmentFlag.AlignCenter)
        logo.setStyleSheet("font-size:28px;font-weight:bold;color:#0a84ff;padding:12px;")
        sl.addWidget(logo)

        sl.addSpacing(16)

        self._nav_btns = []
        nav_items = [
            ("Chat", 0), ("Projects", 1), ("Tasks", 2), ("Knowledge", 3),
            ("ARWE", 4), ("Decisions", 5), ("Audit", 6), ("System", 7), ("Settings", 8)
        ]
        for label, idx in nav_items:
            btn = QPushButton(label)
            btn.setObjectName("nav-btn")
            btn.setCheckable(True)
            btn.clicked.connect(lambda _, i=idx: self._view(i))
            sl.addWidget(btn)
            self._nav_btns.append(btn)

        sl.addStretch()

        user_label = QLabel(str(api.user.get("username", ""))[:2])
        user_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        user_label.setStyleSheet("font-size:16px;font-weight:bold;color:#0a84ff;padding:8px;background:rgba(10,132,255,0.12);border-radius:16px;width:32px;height:32px;")
        sl.addWidget(user_label)

        logout_btn = QPushButton("X")
        logout_btn.setObjectName("danger")
        logout_btn.setFixedSize(36, 36)
        logout_btn.clicked.connect(self.logout)
        sl.addWidget(logout_btn, alignment=Qt.AlignmentFlag.AlignHCenter)

        layout.addWidget(sidebar)

        # Views
        self.stack = QStackedWidget()
        _dbg("creating ChatView...")
        from ozayn.ui.chat_view import ChatView
        self.chat_view = ChatView(self._run)
        self.stack.addWidget(self.chat_view)
        _dbg("ChatView done")

        from ozayn.ui.projects_view import ProjectsView
        _dbg("creating ProjectsView...")
        self.projects_view = ProjectsView(self._run)
        self.stack.addWidget(self.projects_view)
        _dbg("ProjectsView done")

        from ozayn.ui.tasks_view import TasksView
        _dbg("creating TasksView...")
        self.tasks_view = TasksView(self._run)
        self.stack.addWidget(self.tasks_view)
        _dbg("TasksView done")

        from ozayn.ui.knowledge_view import KnowledgeView
        _dbg("creating KnowledgeView...")
        self.knowledge_view = KnowledgeView(self._run)
        self.stack.addWidget(self.knowledge_view)
        _dbg("KnowledgeView done")

        from ozayn.ui.arwe_view import ARWEView
        _dbg("creating ARWEView...")
        self.arwe_view = ARWEView(self._run)
        self.stack.addWidget(self.arwe_view)
        _dbg("ARWEView done")

        from ozayn.ui.decisions_view import DecisionsView
        _dbg("creating DecisionsView...")
        self.decisions_view = DecisionsView(self._run)
        self.stack.addWidget(self.decisions_view)
        _dbg("DecisionsView done")

        from ozayn.ui.audit_view import AuditView
        _dbg("creating AuditView...")
        self.audit_view = AuditView(self._run)
        self.stack.addWidget(self.audit_view)
        _dbg("AuditView done")

        from ozayn.ui.system_view import SystemView
        _dbg("creating SystemView...")
        self.system_view = SystemView()
        self.stack.addWidget(self.system_view)
        _dbg("SystemView done")

        from ozayn.ui.settings_view import SettingsView
        _dbg("creating SettingsView...")
        self.settings_view = SettingsView(api, self._run)
        self.stack.addWidget(self.settings_view)
        _dbg("SettingsView done")

        layout.addWidget(self.stack, 1)
        _dbg("all views added to stack")

        # Shortcuts
        QShortcut(QKeySequence("Ctrl+N"), self, self.chat_view.new_chat)
        QShortcut(QKeySequence("Ctrl+K"), self, lambda: self.chat_view.chat_input.setFocus())

        self._view(0)
        _dbg("MainWindow __init__ COMPLETE")

    def _view(self, idx):
        self.stack.setCurrentIndex(idx)
        for i, btn in enumerate(self._nav_btns):
            btn.setChecked(i == idx)

    def logout(self):
        reply = QMessageBox.question(self, "Logout", "Are you sure?",
                                     QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
        if reply == QMessageBox.StandardButton.Yes:
            self.api.logout()
            self.on_logout()

    def closeEvent(self, event):
        for w in self._workers:
            w.wait(2000)
        event.accept()
