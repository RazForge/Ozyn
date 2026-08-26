#!/usr/bin/env python3
"""
Ozayn Desktop — Pure Native Desktop Application
No PHP backend, no web server, no network dependency.
All data stored locally in SQLite via Python.
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

# Import native API (no HTTP, no PHP)
from native_api import OzaynAPI


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("Ozayn")
    app.setOrganizationName("ARWE")
    app.setStyle("Fusion")
    app.setStyleSheet(DARK_STYLE)

    api = OzaynAPI()
    current_login = [None]
    current_main = [None]

    def show_main():
        old = current_login[0]
        current_login[0] = None
        current_main[0] = MainWindow(api, show_login)
        current_main[0].show()
        current_main[0].raise_()
        current_main[0].activateWindow()
        if old:
            old.hide()
            old.close()
            old.deleteLater()

    def show_login():
        old = current_main[0]
        current_main[0] = None
        current_login[0] = LoginWindow(api, show_main)
        current_login[0].show()
        current_login[0].raise_()
        current_login[0].activateWindow()
        if old:
            old.hide()
            old.close()
            old.deleteLater()

    show_login()
    app.exec()


if __name__ == "__main__":
    main()
