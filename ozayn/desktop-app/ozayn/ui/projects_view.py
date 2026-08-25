"""
Ozayn Projects View — Project management
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QListWidget,
    QListWidgetItem, QPushButton, QInputDialog, QTextEdit
)
from PyQt6.QtCore import Qt


class ProjectsView(QWidget):
    def __init__(self, worker_fn):
        super().__init__()
        self._run = worker_fn

        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # Project list
        list_frame = QWidget()
        ll = QVBoxLayout(list_frame)
        ll.setContentsMargins(16, 16, 8, 16)

        header = QHBoxLayout()
        header.addWidget(QLabel("Projects"))
        add_btn = QPushButton("+ New")
        add_btn.setFixedHeight(32)
        add_btn.clicked.connect(self.new_project)
        header.addWidget(add_btn)
        header.addStretch()
        ll.addLayout(header)

        self.project_list = QListWidget()
        self.project_list.currentItemChanged.connect(self._proj_click)
        ll.addWidget(self.project_list, 1)

        layout.addWidget(list_frame, 1)

        # Project detail
        self.detail = QLabel("Select a project")
        self.detail.setAlignment(Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft)
        self.detail.setWordWrap(True)
        self.detail.setStyleSheet("padding:24px;font-size:15px;color:rgba(255,255,255,0.7);")
        layout.addWidget(self.detail, 2)

        self._load()

    def _load(self):
        self._run("projects", callback=self._on_projs)

    def _on_projs(self, r):
        self.project_list.clear()
        projects = r.get("projects", [])
        if not projects:
            self.project_list.addItem(QListWidgetItem("No projects yet"))
            return
        for p in projects:
            item = QListWidgetItem(p.get("name", "Untitled"))
            item.setData(Qt.ItemDataRole.UserRole, p)
            self.project_list.addItem(item)

    def _proj_click(self, item, _prev):
        if not item:
            return
        p = item.data(Qt.ItemDataRole.UserRole)
        if p:
            desc = p.get("description") or "No description"
            self.detail.setText(f"<b style='font-size:20px;color:#0a84ff;'>{p.get('name', '')}</b><br><br>{desc}")

    def new_project(self):
        name, ok = QInputDialog.getText(self, "New Project", "Project name:")
        if ok and name.strip():
            self._run("create_project", {"name": name.strip()}, lambda r: self._load())
