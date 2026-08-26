"""
Ozayn Intelligence Command Center — Main Dashboard
The central intelligence layer of the ARWE ecosystem.
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
    QFrame, QScrollArea, QLineEdit, QTextEdit, QGridLayout,
    QSizePolicy, QSpacerItem
)
from PyQt6.QtCore import Qt, QTimer, QSize
from PyQt6.QtGui import QFont


# ─── Style Constants ────────────────────────────────────────────────────────

CARD_STYLE = """
    QFrame#card {
        background: rgba(22, 22, 50, 0.65);
        border: 1px solid rgba(255, 255, 255, 0.08);
        border-radius: 14px;
        padding: 16px;
    }
    QFrame#card:hover {
        border-color: rgba(10, 132, 255, 0.3);
    }
"""

CARD_TITLE = "font-size:11px;font-weight:600;color:rgba(255,255,255,0.45);letter-spacing:1px;text-transform:uppercase;"
CARD_VALUE = "font-size:22px;font-weight:bold;color:#ffffff;"
CARD_SUB = "font-size:11px;color:rgba(255,255,255,0.4);"

STATUS_ONLINE = "font-size:13px;font-weight:bold;color:#30d158;"
STATUS_WARNING = "font-size:13px;font-weight:bold;color:#ff9f0a;"
STATUS_DANGER = "font-size:13px;font-weight:bold;color:#ff453a;"
STATUS_IDLE = "font-size:13px;font-weight:bold;color:rgba(255,255,255,0.5);"


def _make_card(title, value, sub="", status_color=None):
    """Create a reusable stats card."""
    card = QFrame()
    card.setObjectName("card")
    card.setStyleSheet(CARD_STYLE)
    card.setFixedHeight(100)
    card.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

    layout = QVBoxLayout(card)
    layout.setContentsMargins(16, 12, 16, 12)
    layout.setSpacing(4)

    t = QLabel(title.upper())
    t.setStyleSheet(CARD_TITLE)
    layout.addWidget(t)

    v = QLabel(value)
    v.setStyleSheet(status_color or CARD_VALUE)
    layout.addWidget(v)

    if sub:
        s = QLabel(sub)
        s.setStyleSheet(CARD_SUB)
        layout.addWidget(s)

    return card


def _make_section_header(title, action_text=None):
    """Create a section header with optional action button."""
    widget = QWidget()
    layout = QHBoxLayout(widget)
    layout.setContentsMargins(0, 0, 0, 0)

    lbl = QLabel(title)
    lbl.setStyleSheet("font-size:14px;font-weight:bold;color:#ffffff;letter-spacing:1px;")
    layout.addWidget(lbl)
    layout.addStretch()

    if action_text:
        btn = QPushButton(action_text)
        btn.setStyleSheet("""
            QPushButton {
                background:rgba(10,132,255,0.15);color:#0a84ff;border:1px solid rgba(10,132,255,0.3);
                border-radius:8px;padding:6px 14px;font-size:12px;font-weight:600;
            }
            QPushButton:hover { background:rgba(10,132,255,0.25); }
        """)
        layout.addWidget(btn)

    return widget


# ─── Dashboard View ─────────────────────────────────────────────────────────

class DashboardView(QWidget):
    """Intelligence Command Center — the Ozayn main page."""

    def __init__(self, run_fn, api):
        super().__init__()
        self._run = run_fn
        self.api = api
        self._build_ui()
        self._load_live_data()

    def _build_ui(self):
        outer = QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(0)

        # Top bar
        outer.addWidget(self._build_top_bar())

        # Scrollable content
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        scroll.setStyleSheet("background:transparent;border:none;")

        content = QWidget()
        content.setStyleSheet("background:transparent;")
        cl = QVBoxLayout(content)
        cl.setContentsMargins(24, 20, 24, 24)
        cl.setSpacing(20)

        # AI Core Interaction
        cl.addWidget(self._build_ai_core())

        # Intelligence Overview Cards
        cl.addWidget(self._build_overview_cards())

        # AI Insights + Decision Center + Simulation (3 columns)
        cl.addWidget(self._build_three_columns())

        # Digital Twin + ARWE Network (2 columns)
        cl.addWidget(self._build_bottom_row())

        cl.addStretch()

        scroll.setWidget(content)
        outer.addWidget(scroll, 1)

    # ─── Top Bar ────────────────────────────────────────────────────────

    def _build_top_bar(self):
        bar = QFrame()
        bar.setFixedHeight(56)
        bar.setStyleSheet("""
            QFrame {
                background:rgba(16,16,36,0.9);
                border-bottom:1px solid rgba(255,255,255,0.06);
            }
        """)
        layout = QHBoxLayout(bar)
        layout.setContentsMargins(24, 0, 24, 0)

        # Left: OZAYN branding
        ozayn = QLabel("OZAYN")
        ozayn.setStyleSheet("font-size:18px;font-weight:bold;color:#0a84ff;letter-spacing:3px;")
        layout.addWidget(ozayn)

        sub = QLabel("Intelligence Command Center")
        sub.setStyleSheet("font-size:11px;color:rgba(255,255,255,0.35);margin-left:12px;letter-spacing:1px;")
        layout.addWidget(sub)

        layout.addStretch()

        # Right: status icons
        for icon, color, tooltip in [
            ("◉", "#30d158", "AI Core Active"),
            ("🔔", "#ff9f0a", "3 Notifications"),
            ("🎙", "#0a84ff", "Voice Control"),
            ("🧠", "#5e5ce6", "AI Status"),
        ]:
            btn = QPushButton(icon)
            btn.setToolTip(tooltip)
            btn.setFixedSize(36, 36)
            btn.setStyleSheet(f"""
                QPushButton {{
                    background:transparent;color:{color};border:none;
                    font-size:16px;border-radius:18px;
                }}
                QPushButton:hover {{ background:rgba(255,255,255,0.06); }}
            """)
            layout.addWidget(btn)

        # User avatar
        username = str(self.api.user.get("username", "U"))[:1].upper()
        avatar = QLabel(username)
        avatar.setAlignment(Qt.AlignmentFlag.AlignCenter)
        avatar.setFixedSize(36, 36)
        avatar.setStyleSheet(f"""
            font-size:14px;font-weight:bold;color:#0a84ff;
            background:rgba(10,132,255,0.15);border-radius:18px;
        """)
        layout.addWidget(avatar)

        return bar

    # ─── AI Core Interaction ────────────────────────────────────────────

    def _build_ai_core(self):
        card = QFrame()
        card.setObjectName("card")
        card.setStyleSheet(CARD_STYLE)
        card.setMinimumHeight(200)

        layout = QVBoxLayout(card)
        layout.setContentsMargins(32, 24, 32, 24)
        layout.setSpacing(12)

        # AI Status indicator
        status_row = QHBoxLayout()
        status_row.addStretch()
        dot = QLabel("◉ AI CORE ACTIVE")
        dot.setStyleSheet("font-size:12px;font-weight:bold;color:#30d158;letter-spacing:2px;")
        status_row.addWidget(dot)
        status_row.addStretch()
        layout.addLayout(status_row)

        # Greeting
        greeting = QLabel("What would you like to analyze?")
        greeting.setAlignment(Qt.AlignmentFlag.AlignCenter)
        greeting.setStyleSheet("font-size:20px;font-weight:300;color:#ffffff;font-style:italic;")
        layout.addWidget(greeting)

        layout.addSpacing(8)

        # Interaction buttons
        btn_row = QHBoxLayout()
        btn_row.setSpacing(12)
        btn_row.setAlignment(Qt.AlignmentFlag.AlignCenter)

        for icon, label in [("🎙", "Speak"), ("⌨", "Type"), ("✋", "Gesture"), ("📷", "Vision")]:
            btn = QPushButton(f"{icon}  {label}")
            btn.setFixedHeight(40)
            btn.setStyleSheet(f"""
                QPushButton {{
                    background:rgba(255,255,255,0.04);color:rgba(255,255,255,0.6);
                    border:1px solid rgba(255,255,255,0.08);border-radius:20px;
                    font-size:13px;padding:0 20px;
                }}
                QPushButton:hover {{
                    background:rgba(10,132,255,0.15);color:#ffffff;
                    border-color:rgba(10,132,255,0.4);
                }}
            """)
            btn_row.addWidget(btn)
        layout.addLayout(btn_row)

        layout.addSpacing(8)

        # Search / Ask input
        search_row = QHBoxLayout()
        search_row.setSpacing(0)
        search_input = QLineEdit()
        search_input.setPlaceholderText("Ask Ozayn anything...")
        search_input.setFixedHeight(48)
        search_input.setStyleSheet("""
            QLineEdit {
                background:rgba(255,255,255,0.06);color:#ffffff;
                border:1px solid rgba(255,255,255,0.1);
                border-radius:24px 0 0 24px;padding:0 20px;font-size:15px;
            }
            QLineEdit:focus { border-color:rgba(10,132,255,0.5); }
        """)
        search_row.addWidget(search_input)

        ask_btn = QPushButton("Ask Ozayn →")
        ask_btn.setFixedSize(140, 48)
        ask_btn.setStyleSheet("""
            QPushButton {
                background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #0a84ff, stop:1 #5e5ce6);
                color:white;border:none;border-radius:0 24px 24px 0;
                font-size:14px;font-weight:bold;
            }
            QPushButton:hover { background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #409cff, stop:1 #7a78ff); }
        """)
        search_row.addWidget(ask_btn)
        layout.addLayout(search_row)

        return card

    # ─── Intelligence Overview Cards ────────────────────────────────────

    def _build_overview_cards(self):
        widget = QWidget()
        grid = QGridLayout(widget)
        grid.setContentsMargins(0, 0, 0, 0)
        grid.setSpacing(12)

        cards_data = [
            ("AI STATUS", "● ACTIVE", "", STATUS_ONLINE),
            ("RISK LEVEL", "LOW", "All systems nominal", STATUS_ONLINE),
            ("DECISIONS", "12 ACTIVE", "3 require attention", STATUS_WARNING),
            ("ARWE SYSTEMS", "6/6 ONLINE", "All networks healthy", STATUS_ONLINE),
            ("ALERTS", "3 NEW", "2 medium · 1 high", STATUS_WARNING),
            ("SIMULATIONS", "8 RUNNING", "2 completing soon", STATUS_IDLE),
        ]

        for i, (title, value, sub, color) in enumerate(cards_data):
            row, col = divmod(i, 3)
            card = _make_card(title, value, sub, color)
            grid.addWidget(card, row, col)

        return widget

    # ─── Three Columns: Insights + Decisions + Simulation ───────────────

    def _build_three_columns(self):
        widget = QWidget()
        layout = QHBoxLayout(widget)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(16)

        # AI Insights
        layout.addWidget(self._build_insights_panel(), 2)

        # Decision Center
        layout.addWidget(self._build_decisions_panel(), 1)

        # Simulation Lab
        layout.addWidget(self._build_simulations_panel(), 1)

        return widget

    def _build_insights_panel(self):
        card = QFrame()
        card.setObjectName("card")
        card.setStyleSheet(CARD_STYLE)

        layout = QVBoxLayout(card)
        layout.setContentsMargins(16, 14, 16, 14)
        layout.setSpacing(10)

        layout.addWidget(_make_section_header("AI INSIGHTS", "View All"))

        insights = [
            ("⚠", "Procurement anomaly detected", "TerraChain → Addis region", "Confidence: 87%", "#ff9f0a"),
            ("📊", "Education performance changing", "Edunex → Grade 12", "Trend: +12%", "#0a84ff"),
            ("🛡", "Security pattern detected", "Bilen → Network activity", "Risk: Medium", "#ff9f0a"),
            ("🏛", "Government workflow delayed", "Govyx → 4 tasks", "Priority: High", "#ff453a"),
        ]

        for icon, title, source, detail, color in insights:
            row = QFrame()
            row.setStyleSheet("""
                QFrame { background:rgba(255,255,255,0.03);border-radius:10px;padding:10px; }
                QFrame:hover { background:rgba(255,255,255,0.06); }
            """)
            rl = QVBoxLayout(row)
            rl.setContentsMargins(12, 8, 12, 8)
            rl.setSpacing(2)

            top = QHBoxLayout()
            icon_lbl = QLabel(icon)
            icon_lbl.setStyleSheet("font-size:16px;")
            top.addWidget(icon_lbl)

            title_lbl = QLabel(title)
            title_lbl.setStyleSheet(f"font-size:13px;font-weight:600;color:{color};")
            top.addWidget(title_lbl)
            top.addStretch()
            rl.addLayout(top)

            src = QLabel(f"{source}  ·  {detail}")
            src.setStyleSheet("font-size:11px;color:rgba(255,255,255,0.4);")
            rl.addWidget(src)

            layout.addWidget(row)

        return card

    def _build_decisions_panel(self):
        card = QFrame()
        card.setObjectName("card")
        card.setStyleSheet(CARD_STYLE)

        layout = QVBoxLayout(card)
        layout.setContentsMargins(16, 14, 16, 14)
        layout.setSpacing(10)

        layout.addWidget(_make_section_header("DECISION CENTER", "View All"))

        notice = QLabel("3 decisions require attention")
        notice.setStyleSheet("font-size:12px;color:#ff9f0a;font-weight:600;")
        layout.addWidget(notice)

        layout.addSpacing(4)

        decisions = [
            ("01", "Infrastructure investment", "4 possible outcomes"),
            ("02", "Education resource allocation", "3 possible outcomes"),
            ("03", "Security response", "5 possible outcomes"),
        ]

        for num, title, outcomes in decisions:
            row = QFrame()
            row.setStyleSheet("""
                QFrame { background:rgba(255,255,255,0.03);border-radius:8px;padding:8px; }
                QFrame:hover { background:rgba(255,255,255,0.06); }
            """)
            rl = QHBoxLayout(row)
            rl.setContentsMargins(12, 8, 12, 8)

            n = QLabel(num)
            n.setStyleSheet("font-size:16px;font-weight:bold;color:#0a84ff;")
            n.setFixedWidth(28)
            rl.addWidget(n)

            info = QVBoxLayout()
            info.setSpacing(1)
            t = QLabel(title)
            t.setStyleSheet("font-size:13px;color:#ffffff;font-weight:500;")
            info.addWidget(t)
            o = QLabel(outcomes)
            o.setStyleSheet("font-size:11px;color:rgba(255,255,255,0.4);")
            info.addWidget(o)
            rl.addLayout(info, 1)

            layout.addWidget(row)

        layout.addStretch()

        create_btn = QPushButton("+ New Decision")
        create_btn.setStyleSheet("""
            QPushButton {
                background:rgba(10,132,255,0.12);color:#0a84ff;
                border:1px solid rgba(10,132,255,0.25);border-radius:8px;
                padding:10px;font-size:13px;font-weight:600;
            }
            QPushButton:hover { background:rgba(10,132,255,0.2); }
        """)
        layout.addWidget(create_btn)

        return card

    def _build_simulations_panel(self):
        card = QFrame()
        card.setObjectName("card")
        card.setStyleSheet(CARD_STYLE)

        layout = QVBoxLayout(card)
        layout.setContentsMargins(16, 14, 16, 14)
        layout.setSpacing(10)

        layout.addWidget(_make_section_header("SIMULATION LAB"))

        sims = [
            ("Economic Policy", "RUNNING", "#0a84ff"),
            ("School Expansion", "COMPLETE", "#30d158"),
            ("Network Expansion", "RUNNING", "#0a84ff"),
        ]

        for name, status, color in sims:
            row = QFrame()
            row.setStyleSheet("""
                QFrame { background:rgba(255,255,255,0.03);border-radius:8px;padding:8px; }
                QFrame:hover { background:rgba(255,255,255,0.06); }
            """)
            rl = QHBoxLayout(row)
            rl.setContentsMargins(12, 8, 12, 8)

            n = QLabel(name)
            n.setStyleSheet("font-size:13px;color:#ffffff;")
            rl.addWidget(n)
            rl.addStretch()

            s = QLabel(f"● {status}")
            s.setStyleSheet(f"font-size:11px;font-weight:bold;color:{color};")
            rl.addWidget(s)

            layout.addWidget(row)

        layout.addStretch()

        create_btn = QPushButton("+ Create Simulation")
        create_btn.setStyleSheet("""
            QPushButton {
                background:rgba(94,92,230,0.12);color:#5e5ce6;
                border:1px solid rgba(94,92,230,0.25);border-radius:8px;
                padding:10px;font-size:13px;font-weight:600;
            }
            QPushButton:hover { background:rgba(94,92,230,0.2); }
        """)
        layout.addWidget(create_btn)

        return card

    # ─── Bottom Row: Digital Twin + ARWE Network ────────────────────────

    def _build_bottom_row(self):
        widget = QWidget()
        layout = QHBoxLayout(widget)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(16)

        layout.addWidget(self._build_digital_twin_card(), 1)
        layout.addWidget(self._build_arwe_network_card(), 2)

        return widget

    def _build_digital_twin_card(self):
        card = QFrame()
        card.setObjectName("card")
        card.setStyleSheet(CARD_STYLE)

        layout = QVBoxLayout(card)
        layout.setContentsMargins(16, 14, 16, 14)
        layout.setSpacing(8)

        layout.addWidget(_make_section_header("DIGITAL TWIN", "Configure"))

        components = [
            ("Identity", "● Synchronized", "#30d158"),
            ("Knowledge", "● Updated", "#30d158"),
            ("Preferences", "● Updated", "#30d158"),
            ("Behavior Model", "● Active", "#0a84ff"),
            ("Decision Model", "● Active", "#0a84ff"),
        ]

        for name, status, color in components:
            row = QHBoxLayout()
            n = QLabel(name)
            n.setStyleSheet("font-size:12px;color:rgba(255,255,255,0.6);")
            row.addWidget(n)
            row.addStretch()
            s = QLabel(status)
            s.setStyleSheet(f"font-size:12px;font-weight:600;color:{color};")
            row.addWidget(s)
            layout.addLayout(row)

        layout.addSpacing(8)

        sync = QLabel("Last sync: 26 Aug 2026 · 04:42")
        sync.setStyleSheet("font-size:10px;color:rgba(255,255,255,0.3);")
        layout.addWidget(sync)

        return card

    def _build_arwe_network_card(self):
        card = QFrame()
        card.setObjectName("card")
        card.setStyleSheet(CARD_STYLE)

        layout = QVBoxLayout(card)
        layout.setContentsMargins(16, 14, 16, 14)
        layout.setSpacing(8)

        layout.addWidget(_make_section_header("ARWE INTELLIGENCE NETWORK", "View All"))

        systems = [
            ("◉ Govyx", "Government", "#30d158"),
            ("◉ Edunex", "Education", "#30d158"),
            ("◉ Locify", "Identity", "#30d158"),
            ("◉ TerraChain", "Land & Procurement", "#30d158"),
            ("◉ Bilen", "Security", "#30d158"),
            ("◉ Kidane", "Drone Intelligence", "#30d158"),
        ]

        grid = QGridLayout()
        grid.setSpacing(8)

        for i, (name, desc, color) in enumerate(systems):
            row, col = divmod(i, 3)

            sys_frame = QFrame()
            sys_frame.setStyleSheet("""
                QFrame { background:rgba(255,255,255,0.03);border-radius:10px;padding:10px; }
                QFrame:hover { background:rgba(255,255,255,0.07);border:1px solid rgba(10,132,255,0.2); }
            """)
            fl = QVBoxLayout(sys_frame)
            fl.setContentsMargins(10, 8, 10, 8)
            fl.setSpacing(2)

            nl = QLabel(name)
            nl.setStyleSheet(f"font-size:13px;font-weight:600;color:{color};")
            fl.addWidget(nl)

            dl = QLabel(desc)
            dl.setStyleSheet("font-size:10px;color:rgba(255,255,255,0.4);")
            fl.addWidget(dl)

            grid.addWidget(sys_frame, row, col)

        layout.addLayout(grid)

        return card

    # ─── Load Live Data ─────────────────────────────────────────────────

    def _load_live_data(self):
        """Fetch live data from API to populate cards."""
        self._run("arwe_status", callback=self._on_arwe_status)
        self._run("decisions", callback=self._on_decisions)
        self._run("system_overview", callback=self._on_system)

    def _on_arwe_status(self, r):
        systems = r.get("systems", r)
        if isinstance(systems, dict):
            online = sum(1 for s in systems.values()
                         if isinstance(s, dict) and s.get("status") == "online")
            total = len(systems)
            # Could update card values here with live data

    def _on_decisions(self, r):
        decisions = r.get("decisions", [])
        if isinstance(decisions, list):
            active = len([d for d in decisions if d.get("status") == "pending"])
            # Could update decision card

    def _on_system(self, r):
        # Could update system overview cards
        pass
