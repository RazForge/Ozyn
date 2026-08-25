"""
Ozayn Chat View — Conversation interface
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QLineEdit,
    QPushButton, QTextEdit, QListWidget, QListWidgetItem, QFrame, QSplitter
)
from PyQt6.QtCore import Qt


class ChatView(QWidget):
    def __init__(self, worker_fn):
        super().__init__()
        self._run = worker_fn
        self._conv_id = None
        self._project_id = None

        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # Conversations sidebar
        sidebar = QFrame()
        sidebar.setFixedWidth(240)
        sidebar.setStyleSheet("background:rgba(22,22,50,0.6);border-right:1px solid rgba(255,255,255,0.1);")
        sl = QVBoxLayout(sidebar)
        sl.setContentsMargins(12, 16, 12, 12)

        header = QHBoxLayout()
        header.addWidget(QLabel("Conversations"))
        new_btn = QPushButton("+")
        new_btn.setFixedSize(28, 28)
        new_btn.setStyleSheet("font-size:16px;padding:2px;border-radius:14px;")
        new_btn.clicked.connect(self.new_chat)
        header.addWidget(new_btn)
        header.addStretch()
        sl.addLayout(header)

        self.conv_list = QListWidget()
        self.conv_list.itemClicked.connect(self._conv_click)
        sl.addWidget(self.conv_list)

        layout.addWidget(sidebar)

        # Main chat area
        chat_widget = QWidget()
        cl = QVBoxLayout(chat_widget)
        cl.setContentsMargins(16, 16, 16, 12)
        cl.setSpacing(0)

        self.chat_header = QLabel("New Chat")
        self.chat_header.setStyleSheet("font-size:16px;font-weight:bold;color:#0a84ff;padding-bottom:12px;border-bottom:1px solid rgba(255,255,255,0.1);")
        cl.addWidget(self.chat_header)

        self.messages = QTextEdit()
        self.messages.setReadOnly(True)
        self.messages.setObjectName("input")
        self.messages.setStyleSheet("background:transparent;border:none;font-size:14px;padding:8px;")
        cl.addWidget(self.messages, 1)

        # Input area
        input_frame = QFrame()
        input_frame.setStyleSheet("background:transparent;")
        il = QHBoxLayout(input_frame)
        il.setContentsMargins(0, 8, 0, 0)

        self.chat_input = QLineEdit()
        self.chat_input.setPlaceholderText("Type a message...")
        self.chat_input.setObjectName("input")
        self.chat_input.returnPressed.connect(self.send_message)
        il.addWidget(self.chat_input)

        send_btn = QPushButton("Send")
        send_btn.setFixedWidth(80)
        send_btn.clicked.connect(self.send_message)
        il.addWidget(send_btn)

        cl.addWidget(input_frame)
        layout.addWidget(chat_widget, 1)

        self._load_conversations()

    def _load_conversations(self):
        self._run("conversations", callback=self._on_convs)

    def _on_convs(self, r):
        self.conv_list.clear()
        convs = r.get("conversations", [])
        if not convs:
            self.conv_list.addItem(QListWidgetItem("No conversations yet"))
            return
        for c in convs:
            title = c.get("title") or c.get("name") or f"Chat {c.get('id', '')}"
            item = QListWidgetItem(title[:50])
            item.setData(Qt.ItemDataRole.UserRole, c)
            self.conv_list.addItem(item)

    def _conv_click(self, item):
        c = item.data(Qt.ItemDataRole.UserRole)
        if c:
            self._conv_id = c.get("id")
            self.chat_header.setText(c.get("title") or c.get("name") or "Chat")
            self._run("history", {"conversation_id": self._conv_id}, self._on_hist)

    def _on_hist(self, r):
        self.messages.clear()
        msgs = r.get("messages", [])
        for m in msgs:
            role = m.get("role", "user")
            content = m.get("content", "")
            prefix = "You" if role == "user" else "Ozayn"
            self.messages.append(f"<b>{prefix}:</b> {content}<br>")

    def send_message(self):
        msg = self.chat_input.text().strip()
        if not msg:
            return
        self.chat_input.clear()
        self.messages.append(f"<b>You:</b> {msg}<br>")
        data = {"message": msg}
        if self._conv_id:
            data["conversation_id"] = self._conv_id
        if self._project_id:
            data["project_id"] = self._project_id
        self._run("chat", data, self._on_chat)

    def _on_chat(self, r):
        resp = r.get("response", "")
        if resp:
            self.messages.append(f"<b>Ozayn:</b> {resp}<br>")
        conv_id = r.get("conversation_id")
        if conv_id:
            self._conv_id = conv_id
        self._load_conversations()

    def new_chat(self):
        self._conv_id = None
        self.messages.clear()
        self.chat_header.setText("New Chat")
        self.chat_input.setFocus()
