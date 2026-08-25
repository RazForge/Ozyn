# Ozayn - Personal AI Digital Twin

Ozayn is the personal AI intelligence interface for the ARWE ecosystem. It provides chat, voice, vision, gesture recognition, project management, and real-time ARWE system orchestration.

## Architecture

Three native platforms sharing one PHP backend:

```
                    OZAYN
                      │
         ┌────────────┼────────────┐
         │            │            │
      WEB          DESKTOP      MOBILE
   (HTML/CSS/JS)  (PyQt6+C/C++) (Flutter)
         │            │            │
         └────────────┼────────────┘
                      │
              PHP API Server
                      │
               SQLite Database
```

- **Web**: Pure HTML/CSS/JavaScript SPA
- **Desktop**: Native PyQt6 with C/C++ core engine (ML, vision, system control, crypto)
- **Mobile**: Native Flutter (Dart) for Android

## Quick Start

### Prerequisites

- PHP 7.4+ with SQLite extension
- Python 3.9+ with pip
- GCC/G++ (for C core)
- OpenSSL dev headers (`libssl-dev` on Debian/Ubuntu)

### 1. Initialize Database

```bash
php ozayn/install.php
```

### 2. Start Web Server

```bash
./start.sh
# or manually:
php -S localhost:8000 router.php
```

### 3. Open Web App

```
http://localhost:8000/ozayn
```

Login: `demo` / `demo123`

### 4. Launch Desktop App

```bash
cd ozayn/desktop-app
./run.sh
```

This builds the C core library and launches the PyQt6 app.

### 5. Build Android App

```bash
cd ozayn/mobile-app
flutter pub get
flutter run
```

### 6. Start ML Server (Optional)

```bash
cd ozayn/ml
python3 -m venv venv && source venv/bin/activate
pip install -r requirements.txt
python server.py
```

## Project Structure

```
Ozyn/
├── ozayn/
│   ├── backend/              # PHP API server
│   │   ├── api/index.php     # REST router
│   │   ├── auth/             # Authentication
│   │   ├── memory/           # Memory system
│   │   ├── knowledge/        # Knowledge base
│   │   ├── projects/         # Projects & tasks
│   │   └── ai/               # AI engine
│   ├── frontend/             # Web app (HTML/CSS/JS)
│   ├── desktop-app/          # Native desktop app
│   │   ├── core/             # C/C++ core engine
│   │   │   ├── include/      # C headers
│   │   │   ├── src/          # C source
│   │   │   └── build/        # Compiled .so
│   │   ├── ozayn/            # Python package
│   │   │   ├── core_bindings.py  # ctypes → C core
│   │   │   ├── workers.py    # QThread pool
│   │   │   ├── ui/           # PyQt6 views
│   │   │   └── theme/        # Dark theme
│   │   └── main.py           # Entry point
│   ├── mobile-app/           # Flutter Android app
│   │   ├── lib/
│   │   │   ├── api/          # HTTP client
│   │   │   ├── screens/      # UI screens
│   │   │   ├── widgets/      # Reusable widgets
│   │   │   └── theme/        # Dark theme
│   │   └── android/          # Android config
│   ├── ml/                   # Python ML server
│   ├── tools/                # Tool modules
│   ├── agents/               # AI agents
│   └── install.php           # DB installer
├── router.php                # Static file router
├── start.sh                  # Dev server launcher
└── deploy.sh                 # Production deploy
```

## C/C++ Core Engine

The desktop app uses a C shared library (`libozayn_core.so`) for performance-critical operations:

| Module | Functions |
|--------|-----------|
| **ML Engine** | Model loading, inference, prediction |
| **Vision** | Screen capture, face detection, gesture recognition |
| **System** | CPU/memory/disk usage, process list, file ops, command execution |
| **Crypto** | SHA-256 hashing, AES-256 encryption, random generation |
| **Utility** | Timestamps, system info |

Build the core:

```bash
cd ozayn/desktop-app/core
make all
```

Test it:

```bash
cd ozayn/desktop-app
source venv/bin/activate
LD_LIBRARY_PATH=core/build python3 -c "
from ozayn.core_bindings import version, get_cpu_usage
print(f'Core v{version()}, CPU: {get_cpu_usage()}%')
"
```

## Desktop App (PyQt6 + C/C++)

Pure native desktop application with:

- **9 views**: Chat, Projects, Tasks, Knowledge, ARWE, Decisions, Audit, System Monitor, Settings
- **C core**: ML inference, vision processing, system monitoring via ctypes
- **Dark theme**: Consistent UI matching web/mobile
- **Keyboard shortcuts**: Ctrl+N (new chat), Ctrl+K (focus input)
- **Auto server**: Starts PHP server if not running

## Flutter Mobile App

Native Android app with:

- **5 tabs**: Chat, Projects, Tasks, ARWE, Settings
- **Dark theme**: Matching desktop and web
- **Local storage**: Session persistence
- **Real-time**: API polling for ARWE status

## API Endpoints

### Auth
- `POST /auth/register` - Register
- `POST /auth/login` - Login
- `POST /auth/logout` - Logout

### Chat
- `POST /chat/send` - Send message
- `GET /chat/list` - List conversations
- `GET /chat/history/{id}` - Get history

### Projects & Tasks
- `GET /projects/list` - List projects
- `POST /projects/create` - Create project
- `GET /tasks/list` - List tasks
- `POST /tasks/create` - Create task
- `PUT /tasks/{id}` - Update task

### Knowledge
- `GET /knowledge/list` - List entries
- `POST /knowledge/add` - Add entry

### ARWE
- `GET /arwe/status` - All system status
- `GET /arwe/briefing` - Daily briefing
- `GET /arwe/{system}` - Specific system

### Decisions & Audit
- `GET /decisions/list` - List decisions
- `POST /decisions/create` - Create decision
- `GET /audit/log` - Audit log

### System
- `GET /system/overview` - System overview
- `GET /system/cpu` - CPU info
- `GET /system/memory` - Memory info
- `GET /system/processes` - Process list

## Deployment

### Docker

```bash
docker-compose up -d
```

### Production Script

```bash
sudo ./deploy.sh install
```

### Manual

```bash
sudo apt install php php-sqlite3 nginx
php ozayn/install.php
```

## License

ARWE Public Source License (ARWE-PSL) v1.0
