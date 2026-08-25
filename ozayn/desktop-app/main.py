#!/usr/bin/env python3
"""
Ozayn Desktop — Native PyQt6 Application Entry Point
Pure desktop app with C/C++ core engine, no web wrappers
"""

import sys
import os
import signal

# Allow Ctrl+C to kill the app
signal.signal(signal.SIGINT, signal.SIG_DFL)

# Ensure parent dir is importable
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from PyQt6.QtWidgets import QApplication
from PyQt6.QtCore import Qt

from ozayn.theme.dark import DARK_STYLE
from ozayn.ui.login_window import LoginWindow
from ozayn.ui.main_window import MainWindow

# Import api_client from root
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__))))
from api_client import OzaynAPI


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("Ozayn")
    app.setOrganizationName("ARWE")
    app.setStyle("Fusion")
    app.setStyleSheet(DARK_STYLE)

    api = OzaynAPI()

    def show_login():
        win = LoginWindow(api, show_main)
        win.show()
        return win

    def show_main():
        win = MainWindow(api, show_login)
        win.show()
        return win

    login_win = show_login()
    app.exec()


if __name__ == "__main__":
    main()
