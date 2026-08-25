"""
Ozayn Decisions View — Decision support system
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QListWidget,
    QListWidgetItem, QPushButton, QInputDialog
)
from PyQt6.QtCore import Qt


class DecisionsView(QWidget):
    def __init__(self, worker_fn):
        super().__init__()
        self._run = worker_fn

        layout = QVBoxLayout(self)
        layout.setContentsMargins(16, 16, 16, 16)

        header = QHBoxLayout()
        header.addWidget(QLabel("Decision Support"))
        header.addStretch()
        add_btn = QPushButton("+ New Decision")
        add_btn.setFixedHeight(32)
        add_btn.clicked.connect(self.new_decision)
        header.addWidget(add_btn)
        layout.addLayout(header)

        self.dec_list = QListWidget()
        layout.addWidget(self.dec_list, 1)

        self._load()

    def _load(self):
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
            if chosen:
                text += f"\nDecision: {chosen}"
            item = QListWidgetItem(text)
            item.setData(Qt.ItemDataRole.UserRole, d)
            self.dec_list.addItem(item)

    def new_decision(self):
        ctx, ok = QInputDialog.getMultiLineText(self, "New Decision", "Describe the decision context:")
        if ok and ctx.strip():
            self._run("create_decision", {"context": ctx.strip()}, lambda r: self._load())
