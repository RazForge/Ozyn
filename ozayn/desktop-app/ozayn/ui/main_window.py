"""
Ozayn Main Window — Navigation and view container
"""

from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QLabel,
    QPushButton, QStackedWidget, QFrame, QMessageBox
)
from PyQt6.QtCore import Qt, QSize
from PyQt6.QtGui import QKeySequence, QShortcut

from ozayn.theme.dark import DARK_STYLE
from ozayn.workers import WorkerMixin
from ozayn.ui.dashboard_view import DashboardView
from ozayn.ui.chat_view import ChatView
from ozayn.ui.projects_view import ProjectsView
from ozayn.ui.tasks_view import TasksView
from ozayn.ui.knowledge_view import KnowledgeView
from ozayn.ui.arwe_view import ARWEView
from ozayn.ui.decisions_view import DecisionsView
from ozayn.ui.audit_view import AuditView
from ozayn.ui.settings_view import SettingsView
from ozayn.ui.system_view import SystemView


class MainWindow(QMainWindow, WorkerMixin):
    def __init__(self, api, on_logout):
        super().__init__()
        self.api = api
        self.on_logout = on_logout
        self._init_workers()
        self.setWindowTitle("Ozayn — Intelligence Command Center")
        self.setMinimumSize(1200, 750)
        self.resize(1400, 850)
        self.setStyleSheet(DARK_STYLE)

        central = QWidget()
        self.setCentralWidget(central)
        layout = QHBoxLayout(central)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # Sidebar
        sidebar = QFrame()
        sidebar.setObjectName("sidebar")
        sidebar.setFixedWidth(72)
        sl = QVBoxLayout(sidebar)
        sl.setContentsMargins(0, 16, 0, 12)
        sl.setSpacing(0)

        logo = QLabel("⬡")
        logo.setAlignment(Qt.AlignmentFlag.AlignCenter)
        logo.setStyleSheet("font-size:28px;color:#0a84ff;padding:12px;")
        sl.addWidget(logo)

        sl.addSpacing(12)

        self._nav_btns = []
        nav_items = [
            ("Dashboard", 0), ("Chat", 1), ("Projects", 2), ("Tasks", 3),
            ("Knowledge", 4), ("ARWE", 5), ("Decisions", 6), ("Audit", 7),
            ("System", 8), ("Settings", 9)
        ]
        for label, idx in nav_items:
            btn = QPushButton(label)
            btn.setObjectName("nav-btn")
            btn.setCheckable(True)
            btn.clicked.connect(lambda _, i=idx: self._view(i))
            sl.addWidget(btn)
            self._nav_btns.append(btn)

        sl.addStretch()

        user_label = QLabel(str(api.user.get("username", ""))[:2].upper())
        user_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        user_label.setStyleSheet(
            "font-size:14px;font-weight:bold;color:#0a84ff;"
            "padding:8px;background:rgba(10,132,255,0.12);"
            "border-radius:16px;width:32px;height:32px;"
        )
        sl.addWidget(user_label)

        logout_btn = QPushButton("X")
        logout_btn.setObjectName("danger")
        logout_btn.setFixedSize(36, 36)
        logout_btn.clicked.connect(self.logout)
        sl.addWidget(logout_btn, alignment=Qt.AlignmentFlag.AlignHCenter)

        layout.addWidget(sidebar)

        # Views
        self.stack = QStackedWidget()

        self.dashboard_view = DashboardView(self._run, api, navigate_fn=self._view)
        self.stack.addWidget(self.dashboard_view)

        self.chat_view = ChatView(self._run)
        self.stack.addWidget(self.chat_view)

        self.projects_view = ProjectsView(self._run)
        self.stack.addWidget(self.projects_view)

        self.tasks_view = TasksView(self._run)
        self.stack.addWidget(self.tasks_view)

        self.knowledge_view = KnowledgeView(self._run)
        self.stack.addWidget(self.knowledge_view)

        self.arwe_view = ARWEView(self._run)
        self.stack.addWidget(self.arwe_view)

        self.decisions_view = DecisionsView(self._run)
        self.stack.addWidget(self.decisions_view)

        self.audit_view = AuditView(self._run)
        self.stack.addWidget(self.audit_view)

        self.system_view = SystemView()
        self.stack.addWidget(self.system_view)

        self.settings_view = SettingsView(api, self._run)
        self.stack.addWidget(self.settings_view)

        layout.addWidget(self.stack, 1)

        # Shortcuts
        QShortcut(QKeySequence("Ctrl+N"), self, self.chat_view.new_chat)
        QShortcut(QKeySequence("Ctrl+K"), self, lambda: self.chat_view.chat_input.setFocus())
        QShortcut(QKeySequence("Ctrl+D"), self, lambda: self._view(0))

        self._view(0)

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
