"""
Ozayn ARWE View — ARWE system status dashboard
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QFrame,
    QScrollArea, QPushButton
)
from PyQt6.QtCore import Qt


class ARWEView(QWidget):
    def __init__(self, worker_fn):
        super().__init__()
        self._run = worker_fn

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setStyleSheet("background:transparent;border:none;")

        container = QWidget()
        self.layout = QVBoxLayout(container)
        self.layout.setContentsMargins(16, 16, 16, 16)
        self.layout.setSpacing(12)

        header = QHBoxLayout()
        header.addWidget(QLabel("ARWE Systems"))
        header.addStretch()
        refresh_btn = QPushButton("Refresh")
        refresh_btn.setFixedHeight(32)
        refresh_btn.clicked.connect(self._load)
        header.addWidget(refresh_btn)
        self.layout.addLayout(header)

        self.status_area = QWidget()
        self.status_layout = QVBoxLayout(self.status_area)
        self.status_layout.setSpacing(8)
        scroll.setWidget(container)
        self.layout.addWidget(scroll, 1)

        self._load()

    def _load(self):
        self._run("arwe_status", callback=self._on_status)

    def _on_status(self, r):
        while self.status_layout.count():
            child = self.status_layout.takeAt(0)
            if child.widget():
                child.widget().deleteLater()

        systems = r.get("systems", r)
        if not systems or not isinstance(systems, dict):
            self.status_layout.addWidget(QLabel("No ARWE data available"))
            return

        for name, info in systems.items():
            if not isinstance(info, dict):
                continue
            card = QFrame()
            card.setStyleSheet(
                "background:rgba(28,28,56,0.55);border:1px solid rgba(255,255,255,0.1);"
                "border-radius:12px;padding:16px;margin-bottom:4px;"
            )
            cl = QVBoxLayout(card)

            hl = QHBoxLayout()
            hl.addWidget(QLabel(f"<b style='font-size:16px;'>{name.title()}</b>"))
            status = info.get("status", "unknown")
            color = "#30d158" if status == "online" else "#ff453a"
            lbl = QLabel(f"<span style='color:{color};font-weight:bold;'>{status}</span>")
            hl.addWidget(lbl)
            hl.addStretch()
            cl.addLayout(hl)

            details = info.get("details", {})
            if details:
                detail_text = " | ".join(f"{k}: {v}" for k, v in details.items())
                dl = QLabel(detail_text)
                dl.setWordWrap(True)
                dl.setStyleSheet("color:rgba(255,255,255,0.5);font-size:13px;margin-top:4px;")
                cl.addWidget(dl)

            self.status_layout.addWidget(card)

        # Load briefing
        self._run("arwe_briefing", callback=self._on_briefing)

    def _on_briefing(self, r):
        briefing = r.get("briefing", "")
        if briefing:
            lbl = QLabel(f"<b style='color:#0a84ff;'>Daily Briefing</b><br><pre style='color:rgba(255,255,255,0.7);'>{briefing}</pre>")
            lbl.setWordWrap(True)
            self.status_layout.addWidget(lbl)

        self.status_layout.addStretch()
