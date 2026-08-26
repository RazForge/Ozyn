# OZAYN — AI Digital Twin

Pure native desktop application. No web server, no PHP, no network dependency.

## Architecture

```
ozayn/
├── desktop-app/          # PyQt6 desktop application
│   ├── main.py           # Entry point
│   ├── native_api.py     # Pure Python API (SQLite)
│   ├── database.py       # SQLite persistence layer
│   ├── run.sh            # Build C core + launch
│   ├── core/             # C/C++ performance engine
│   │   ├── include/      # C headers
│   │   ├── src/          # C source files
│   │   └── build/        # Compiled .so
│   └── ozayn/            # Python package
│       ├── ui/           # All PyQt6 views
│       │   ├── dashboard_view.py   # Intelligence Command Center
│       │   ├── login_window.py     # Multimodal auth
│       │   ├── main_window.py      # Navigation
│       │   ├── chat_view.py        # AI Chat
│       │   ├── projects_view.py    # Projects
│       │   ├── tasks_view.py       # Tasks
│       │   ├── knowledge_view.py   # Knowledge base
│       │   ├── arwe_view.py        # ARWE systems
│       │   ├── decisions_view.py   # Decision center
│       │   ├── audit_view.py       # Audit log
│       │   ├── system_view.py      # System monitor (C core)
│       │   └── settings_view.py    # Settings
│       ├── workers.py    # QThread async workers
│       ├── core_bindings.py  # C core ctypes bindings
│       └── theme/        # Dark theme
└── README.md
```

## Quick Start

```bash
cd ozayn/desktop-app
./run.sh
```

Login: `demo` / `demo123`

## Features

- **Intelligence Command Center** — AI-powered dashboard
- **Multimodal Authentication** — Text, Voice, Face, Passkey, Virtual Keyboard, 2FA
- **Chat** — AI conversation interface
- **Projects & Tasks** — Project management
- **Knowledge Base** — Information storage
- **ARWE Network** — 6-system intelligence monitoring
- **Decision Center** — AI-assisted decision making
- **System Monitor** — Real-time CPU, memory, disk, processes (via C core)
- **Audit Log** — Activity tracking

## Tech Stack

- **Python 3** — Application logic
- **PyQt6** — Native desktop UI
- **SQLite** — Local data persistence
- **C/C++** — Performance-critical operations (system monitor, vision, crypto)
