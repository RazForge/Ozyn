"""
Ozayn Tasks View — Task management with priorities
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QListWidget,
    QListWidgetItem, QPushButton, QInputDialog, QComboBox
)
from PyQt6.QtCore import Qt


class TasksView(QWidget):
    def __init__(self, worker_fn):
        super().__init__()
        self._run = worker_fn

        layout = QVBoxLayout(self)
        layout.setContentsMargins(16, 16, 16, 16)

        header = QHBoxLayout()
        header.addWidget(QLabel("Tasks"))
        header.addStretch()

        self.filter_combo = QComboBox()
        self.filter_combo.addItems(["All", "Pending", "In Progress", "Completed"])
        self.filter_combo.currentTextChanged.connect(lambda _: self._load())
        header.addWidget(self.filter_combo)

        add_btn = QPushButton("+ New Task")
        add_btn.setFixedHeight(32)
        add_btn.clicked.connect(self.new_task)
        header.addWidget(add_btn)
        layout.addLayout(header)

        self.task_list = QListWidget()
        self.task_list.itemDoubleClicked.connect(self._toggle_task)
        layout.addWidget(self.task_list, 1)

        self._load()

    def _load(self):
        self._run("tasks", callback=self._on_tasks)

    def _on_tasks(self, r):
        self.task_list.clear()
        tasks = r.get("tasks", [])
        if not tasks:
            self.task_list.addItem(QListWidgetItem("No tasks yet"))
            return

        filt = self.filter_combo.currentText().lower().replace(" ", "_")
        if filt != "all":
            tasks = [t for t in tasks if t.get("status") == filt]

        for t in tasks:
            status = t.get("status", "pending")
            priority = t.get("priority", "medium")
            icon = {"high": "[!]", "medium": "[~]", "low": "[-]"}.get(priority, "[ ]")
            check = "[x]" if status == "completed" else "[ ]"
            text = f"{icon} {check} {t.get('title', 'Untitled')}"
            item = QListWidgetItem(text)
            item.setData(Qt.ItemDataRole.UserRole, t)
            self.task_list.addItem(item)

    def new_task(self):
        title, ok = QInputDialog.getText(self, "New Task", "Task title:")
        if ok and title.strip():
            priority, _ = QInputDialog.getItem(self, "Priority", "Select priority:",
                                                ["low", "medium", "high"], 1, False)
            self._run("create_task", {"title": title.strip(), "priority": priority},
                      lambda r: self._load())

    def _toggle_task(self, item):
        t = item.data(Qt.ItemDataRole.UserRole)
        if not t:
            return
        new_status = "completed" if t.get("status") != "completed" else "pending"
        self._run("update_task", {"task_id": t["id"], "status": new_status},
                  lambda r: self._load())
