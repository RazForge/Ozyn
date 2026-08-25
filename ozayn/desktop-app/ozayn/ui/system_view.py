"""
Ozayn System View — Real-time system monitoring via C core
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QFrame,
    QScrollArea, QPushButton, QGridLayout
)
from PyQt6.QtCore import Qt, QTimer

from ozayn import core_bindings as core


class SystemView(QWidget):
    def __init__(self):
        super().__init__()
        self._setup_ui()
        self._refresh()
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._refresh)
        self._timer.start(3000)

    def _setup_ui(self):
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setStyleSheet("background:transparent;border:none;")

        container = QWidget()
        gl = QGridLayout(container)
        gl.setContentsMargins(16, 16, 16, 16)
        gl.setSpacing(12)

        header = QHBoxLayout()
        header.addWidget(QLabel("System Monitor"))
        header.addStretch()

        try:
            ver = core.version()
            header.addWidget(QLabel(f"<span style='color:rgba(255,255,255,0.4);'>Core v{ver}</span>"))
        except Exception:
            pass

        refresh_btn = QPushButton("Refresh")
        refresh_btn.setFixedHeight(32)
        refresh_btn.clicked.connect(self._refresh)
        header.addWidget(refresh_btn)
        gl.addLayout(header, 0, 0, 1, 3)

        # CPU card
        self.cpu_card = self._make_card("CPU", "---")
        gl.addWidget(self.cpu_card, 1, 0)

        # Memory card
        self.mem_card = self._make_card("Memory", "---")
        gl.addWidget(self.mem_card, 1, 1)

        # Disk card
        self.disk_card = self._make_card("Disk", "---")
        gl.addWidget(self.disk_card, 1, 2)

        # Hostname / Uptime
        self.info_card = self._make_card("System", "---")
        gl.addWidget(self.info_card, 2, 0, 1, 2)

        # Processes
        self.proc_card = self._make_card("Top Processes", "---")
        gl.addWidget(self.proc_card, 2, 2)

        scroll.setWidget(container)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(scroll)

    def _make_card(self, title, value):
        card = QFrame()
        card.setStyleSheet(
            "background:rgba(28,28,56,0.55);border:1px solid rgba(255,255,255,0.1);"
            "border-radius:12px;padding:16px;"
        )
        cl = QVBoxLayout(card)
        tl = QLabel(f"<span style='color:rgba(255,255,255,0.5);font-size:12px;'>{title}</span>")
        cl.addWidget(tl)
        val = QLabel(value)
        val.setObjectName("title")
        val.setStyleSheet("font-size:24px;font-weight:bold;color:#0a84ff;")
        val.setWordWrap(True)
        cl.addWidget(val)
        card._value_label = val
        return card

    def _refresh(self):
        try:
            cpu = core.get_cpu_usage()
            self.cpu_card._value_label.setText(f"{cpu}%")
            color = "#30d158" if cpu < 70 else "#ff9f0a" if cpu < 90 else "#ff453a"
            self.cpu_card._value_label.setStyleSheet(f"font-size:24px;font-weight:bold;color:{color};")
        except Exception:
            self.cpu_card._value_label.setText("Error")

        try:
            mem = core.get_memory_usage()
            used_gb = mem["used"] / (1024**3)
            total_gb = mem["total"] / (1024**3)
            pct = (mem["used"] / mem["total"] * 100) if mem["total"] > 0 else 0
            self.mem_card._value_label.setText(f"{used_gb:.1f} / {total_gb:.1f} GB\n({pct:.0f}%)")
        except Exception:
            self.mem_card._value_label.setText("Error")

        try:
            disk = core.get_disk_usage("/")
            used_gb = disk["used"] / (1024**3)
            total_gb = disk["total"] / (1024**3)
            pct = (disk["used"] / disk["total"] * 100) if disk["total"] > 0 else 0
            self.disk_card._value_label.setText(f"{used_gb:.1f} / {total_gb:.1f} GB\n({pct:.0f}%)")
        except Exception:
            self.disk_card._value_label.setText("Error")

        try:
            hostname = core.get_hostname()
            uptime = core.get_uptime()
            hours = uptime // 3600
            mins = (uptime % 3600) // 60
            self.info_card._value_label.setText(f"{hostname}\nUptime: {hours}h {mins}m")
        except Exception:
            self.info_card._value_label.setText("Error")

        try:
            procs = core.list_processes("cpu", 5)
            lines = []
            for p in procs[:5]:
                lines.append(f"{p.get('command', '?')[:25]}  CPU:{p.get('cpu', 0)}%")
            self.proc_card._value_label.setText("\n".join(lines) if lines else "No processes")
            self.proc_card._value_label.setStyleSheet("font-size:12px;font-weight:normal;color:rgba(255,255,255,0.8);")
        except Exception:
            self.proc_card._value_label.setText("Error")
