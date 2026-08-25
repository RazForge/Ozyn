"""
Ozayn Dark Theme — Consistent styling across all views
"""

DARK_STYLE = """
QMainWindow, QWidget {
    background-color: #08081a;
    color: rgba(255,255,255,0.92);
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    font-size: 14px;
}

QFrame#sidebar {
    background-color: rgba(22,22,50,0.8);
    border-right: 1px solid rgba(255,255,255,0.1);
}

QFrame#header {
    background-color: rgba(16,16,36,0.9);
    border-bottom: 1px solid rgba(255,255,255,0.1);
}

QLabel#title {
    color: #0a84ff;
    font-size: 16px;
    font-weight: bold;
}

QLineEdit, QTextEdit#input {
    background-color: rgba(50,50,85,0.3);
    border: 1px solid rgba(255,255,255,0.1);
    border-radius: 12px;
    padding: 12px 16px;
    color: rgba(255,255,255,0.92);
    font-size: 14px;
    font-family: inherit;
    selection-background-color: #0a84ff;
}

QLineEdit:focus, QTextEdit#input:focus {
    border-color: #0a84ff;
}

QPushButton {
    background-color: #0a84ff;
    color: white;
    border: none;
    border-radius: 12px;
    padding: 12px 24px;
    font-size: 14px;
    font-weight: bold;
    font-family: inherit;
}

QPushButton:hover { background-color: #409cff; }
QPushButton:pressed { background-color: #0070e0; }
QPushButton:disabled {
    background-color: rgba(10,132,255,0.3);
    color: rgba(255,255,255,0.3);
}

QPushButton#secondary {
    background-color: rgba(50,50,85,0.3);
    border: 1px solid rgba(255,255,255,0.1);
    padding: 10px 16px;
    border-radius: 12px;
}
QPushButton#secondary:hover { background-color: rgba(255,255,255,0.08); }

QPushButton#danger {
    background-color: rgba(255,69,58,0.15);
    color: #ff453a;
    border: 1px solid rgba(255,69,58,0.3);
}

QPushButton#text-btn {
    background: transparent;
    color: #0a84ff;
    border: none;
    padding: 6px 12px;
    border-radius: 8px;
    font-size: 13px;
}
QPushButton#text-btn:hover { background-color: rgba(10,132,255,0.12); }

QPushButton#nav-btn {
    background: transparent;
    color: rgba(255,255,255,0.3);
    border: none;
    padding: 6px 0;
    border-radius: 0;
    font-size: 10px;
}
QPushButton#nav-btn:hover { color: rgba(255,255,255,0.6); }

QListWidget {
    background: transparent;
    border: none;
    outline: none;
}

QListWidget::item {
    padding: 10px 12px;
    border-radius: 8px;
    color: rgba(255,255,255,0.5);
    margin-bottom: 2px;
}
QListWidget::item:hover {
    background-color: rgba(255,255,255,0.06);
    color: rgba(255,255,255,0.92);
}
QListWidget::item:selected {
    background-color: rgba(10,132,255,0.12);
    color: #0a84ff;
}

QComboBox {
    background-color: rgba(50,50,85,0.3);
    border: 1px solid rgba(255,255,255,0.1);
    border-radius: 12px;
    padding: 10px 14px;
    color: rgba(255,255,255,0.92);
    font-size: 14px;
}
QComboBox::drop-down { border: none; width: 30px; }
QComboBox QAbstractItemView {
    background-color: #161632;
    color: rgba(255,255,255,0.92);
    border: 1px solid rgba(255,255,255,0.1);
    selection-background-color: rgba(10,132,255,0.3);
}

QScrollArea { border: none; background: transparent; }
QScrollBar:vertical {
    background: transparent;
    width: 6px;
}
QScrollBar::handle:vertical {
    background: rgba(255,255,255,0.1);
    border-radius: 3px;
    min-height: 30px;
}
QScrollBar::handle:vertical:hover { background: rgba(255,255,255,0.2); }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: transparent; height: 0; }

QCheckBox {
    color: rgba(255,255,255,0.7);
    spacing: 8px;
}
QCheckBox::indicator {
    width: 18px; height: 18px;
    border-radius: 4px;
    border: 2px solid rgba(255,255,255,0.3);
    background: transparent;
}
QCheckBox::indicator:checked {
    background: #0a84ff;
    border-color: #0a84ff;
}

QGroupBox {
    border: 1px solid rgba(255,255,255,0.1);
    border-radius: 12px;
    margin-top: 16px;
    padding: 16px;
}
QGroupBox::title {
    color: #0a84ff;
    font-weight: 600;
    subcontrol-origin: margin;
    left: 16px;
    padding: 0 8px;
}
"""
