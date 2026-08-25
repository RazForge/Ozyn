"""
Ozayn Knowledge View — Knowledge base management
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QListWidget,
    QListWidgetItem, QPushButton, QInputDialog, QTextEdit
)
from PyQt6.QtCore import Qt


class KnowledgeView(QWidget):
    def __init__(self, worker_fn):
        super().__init__()
        self._run = worker_fn

        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # Knowledge list
        list_frame = QWidget()
        ll = QVBoxLayout(list_frame)
        ll.setContentsMargins(16, 16, 8, 16)

        header = QHBoxLayout()
        header.addWidget(QLabel("Knowledge Base"))
        add_btn = QPushButton("+ Add")
        add_btn.setFixedHeight(32)
        add_btn.clicked.connect(self.new_knowledge)
        header.addWidget(add_btn)
        header.addStretch()
        ll.addLayout(header)

        self.know_list = QListWidget()
        self.know_list.currentItemChanged.connect(self._know_click)
        ll.addWidget(self.know_list, 1)

        layout.addWidget(list_frame, 1)

        # Detail view
        self.detail = QLabel("Select an entry")
        self.detail.setAlignment(Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft)
        self.detail.setWordWrap(True)
        self.detail.setStyleSheet("padding:24px;font-size:15px;color:rgba(255,255,255,0.7);")
        layout.addWidget(self.detail, 2)

        self._load()

    def _load(self):
        self._run("knowledge", callback=self._on_know)

    def _on_know(self, r):
        self.know_list.clear()
        items = r.get("results", r.get("knowledge", []))
        if not items:
            self.know_list.addItem(QListWidgetItem("No knowledge entries yet"))
            return
        for k in items:
            tags = ", ".join(k.get("tags", []))
            title = k.get("title", "Untitled")
            display = f"{title}" + (f" [{tags}]" if tags else "")
            item = QListWidgetItem(display[:60])
            item.setData(Qt.ItemDataRole.UserRole, k)
            self.know_list.addItem(item)

    def _know_click(self, item, _prev):
        if not item:
            return
        k = item.data(Qt.ItemDataRole.UserRole)
        if k:
            tags = ", ".join(k.get("tags", []))
            self.detail.setText(
                f"<b style='font-size:20px;color:#0a84ff;'>{k.get('title', '')}</b>"
                f"<br><br><span style='color:rgba(255,255,255,0.4);'>Tags: {tags or 'none'}</span>"
                f"<br><br>{k.get('content', '')}"
            )

    def new_knowledge(self):
        title, ok = QInputDialog.getText(self, "Add Knowledge", "Title:")
        if not ok or not title.strip():
            return
        content, ok = QInputDialog.getMultiLineText(self, "Add Knowledge", "Content:")
        if not ok or not content.strip():
            return
        tags_str, ok = QInputDialog.getText(self, "Tags", "Tags (comma-separated):")
        tags = [t.strip() for t in tags_str.split(",") if t.strip()] if ok else []
        self._run("add_knowledge", {"title": title.strip(), "content": content.strip(), "tags": tags},
                  lambda r: self._load())
