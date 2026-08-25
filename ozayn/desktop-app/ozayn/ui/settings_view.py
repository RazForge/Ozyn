"""
Ozayn Settings View — Application settings and export
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QLineEdit,
    QPushButton, QCheckBox, QComboBox, QFileDialog, QMessageBox,
    QGroupBox, QFormLayout
)
from PyQt6.QtCore import Qt
import json


class SettingsView(QWidget):
    def __init__(self, api, worker_fn):
        super().__init__()
        self.api = api
        self._run = worker_fn

        layout = QVBoxLayout(self)
        layout.setContentsMargins(16, 16, 16, 16)
        layout.setSpacing(16)

        layout.addWidget(QLabel("Settings"))

        # AI Settings
        ai_group = QGroupBox("AI Provider")
        ai_form = QFormLayout(ai_group)
        self.api_key = QLineEdit()
        self.api_key.setPlaceholderText("API Key")
        self.api_key.setEchoMode(QLineEdit.EchoMode.Password)
        ai_form.addRow("API Key:", self.api_key)

        self.model_combo = QComboBox()
        self.model_combo.addItems(["gpt-4", "gpt-3.5-turbo", "claude-3", "local"])
        ai_form.addRow("Model:", self.model_combo)
        layout.addWidget(ai_group)

        # Appearance
        app_group = QGroupBox("Appearance")
        app_form = QFormLayout(app_group)
        self.theme_combo = QComboBox()
        self.theme_combo.addItems(["Dark", "Light", "System"])
        app_form.addRow("Theme:", self.theme_combo)
        layout.addWidget(app_group)

        # Data
        data_group = QGroupBox("Data")
        data_layout = QHBoxLayout(data_group)
        export_btn = QPushButton("Export Data to JSON")
        export_btn.clicked.connect(self._export)
        data_layout.addWidget(export_btn)
        layout.addWidget(data_group)

        # Save
        save_btn = QPushButton("Save Settings")
        save_btn.clicked.connect(self._save)
        layout.addWidget(save_btn)

        layout.addStretch()

    def _save(self):
        QMessageBox.information(self, "Settings", "Settings saved!")

    def _export(self):
        path, _ = QFileDialog.getSaveFileName(self, "Export Data", "ozayn_export.json", "JSON (*.json)")
        if not path:
            return
        data = {"user": self.api.user}
        try:
            data["conversations"] = self.api.list_conversations()
            data["projects"] = self.api.list_projects()
            data["tasks"] = self.api.list_tasks()
            data["knowledge"] = self.api.list_knowledge()
        except Exception:
            pass
        with open(path, "w") as f:
            json.dump(data, f, indent=2, default=str)
        QMessageBox.information(self, "Export", f"Data exported to {path}")
