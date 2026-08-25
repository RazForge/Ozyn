"""
Ozayn Audit View — Audit log viewer
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QListWidget,
    QListWidgetItem, QLineEdit, QPushButton
)
from PyQt6.QtCore import Qt


class AuditView(QWidget):
    def __init__(self, worker_fn):
        super().__init__()
        self._run = worker_fn

        layout = QVBoxLayout(self)
        layout.setContentsMargins(16, 16, 16, 16)

        header = QHBoxLayout()
        header.addWidget(QLabel("Audit Log"))

        self.search_input = QLineEdit()
        self.search_input.setPlaceholderText("Search audit log...")
        self.search_input.setFixedWidth(250)
        self.search_input.returnPressed.connect(self._search)
        header.addWidget(self.search_input)

        search_btn = QPushButton("Search")
        search_btn.setFixedHeight(32)
        search_btn.clicked.connect(self._search)
        header.addWidget(search_btn)

        header.addStretch()
        layout.addLayout(header)

        self.aud_list = QListWidget()
        layout.addWidget(self.aud_list, 1)

        self._load()

    def _load(self):
        self._run("audit", callback=self._on_audit)

    def _on_audit(self, r):
        self.aud_list.clear()
        log = r.get("log", [])
        if not log:
            self.aud_list.addItem(QListWidgetItem("No audit entries yet."))
            return
        for e in log:
            result = e.get("result", "")
            color = "#30d158" if result == "success" else "#ff453a" if result else "rgba(255,255,255,0.5)"
            text = f"{e.get('created_at', '')}  |  {e.get('action', '')}  |  <span style='color:{color}'>{result}</span>"
            item = QListWidgetItem(text)
            self.aud_list.addItem(item)

    def _search(self):
        q = self.search_input.text().strip()
        if q:
            self._run("audit_search", {"query": q}, self._on_audit)
