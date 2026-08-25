# Ozayn - Personal AI Digital Twin

Ozayn is the personal AI intelligence interface for the ARWE ecosystem. It provides a chat-based interface with memory, knowledge retrieval, project management, voice, vision, gesture recognition, and real-time collaboration.

## Quick Start

### Prerequisites

- PHP 7.4+ with SQLite extension
- Python 3.9+ with pip
- Node.js 16+ (optional, for desktop/mobile)
- Web server (Apache/Nginx) or PHP built-in server

### Development Installation

1. Clone the repository:
```bash
git clone <repository-url>
cd Ozyn
```

2. Initialize the database:
```bash
php ozayn/install.php
```

3. Install Python ML dependencies:
```bash
cd ozayn/ml
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

4. Start the ML server:
```bash
python ozayn/ml/server.py
```

5. Start the development server:
```bash
php -S localhost:8000 router.php
```

6. Open your browser and navigate to:
```
http://localhost:8000/ozayn
```

7. Register a new account and start chatting!

### Production Deployment

#### Option 1: Automated Script (Recommended)

```bash
sudo ./deploy.sh install
```

This installs and configures:
- Nginx web server with PHP-FPM
- Python ML server as systemd service
- WebSocket server for real-time updates
- Collaboration server for multi-user sessions
- Firewall rules

#### Option 2: Docker

```bash
docker-compose up -d
```

This runs:
- `ozayn-web`: PHP/Apache web server (port 80)
- `ozayn-ml`: Python ML server (port 8765)
- `ozayn-collab`: Collaboration WebSocket server (port 8082)

#### Option 3: Manual Deployment

```bash
# Install dependencies
sudo apt install php php-sqlite3 python3 python3-pip nginx

# Setup Python environment
cd ozayn/ml
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

# Initialize database
cd ../..
php ozayn/install.php

# Configure Nginx (copy config to /etc/nginx/sites-available/)
# Start services
python ozayn/ml/server.py &
php -S 0.0.0.0:9090 router.php &
```

## Project Structure

```
Ozyn/
├── ozayn/
│   ├── backend/
│   │   ├── api/              # API endpoints
│   │   ├── auth/             # Authentication
│   │   ├── memory/           # Memory system
│   │   ├── knowledge/        # Knowledge base
│   │   ├── projects/         # Project management
│   │   ├── ai/               # AI engine
│   │   ├── config/           # Configuration
│   │   └── database.php      # Database connection
│   ├── frontend/
│   │   ├── css/              # Styles
│   │   ├── js/
│   │   │   ├── app.js        # Main application
│   │   │   ├── tf-models.js  # TensorFlow.js models
│   │   │   ├── ml-pipeline.js # ML orchestration
│   │   │   ├── gesture-controller.js # Gesture recognition
│   │   │   ├── screen-analyzer.js    # Screen understanding
│   │   │   ├── vision-client.js      # Vision processing
│   │   │   ├── ml-client.js          # ML server client
│   │   │   ├── collaboration-client.js # Real-time collab
│   │   │   └── websocket.js          # WebSocket client
│   │   └── index.html        # Main UI
│   ├── ml/
│   │   ├── server.py         # ML WebSocket server
│   │   ├── vision_model.py   # Vision analysis
│   │   └── requirements.txt  # Python dependencies
│   ├── mobile/               # React Native app
│   ├── desktop/              # Electron app
│   ├── agents/               # AI agents
│   ├── tools/                # Tool modules
│   ├── voice/                # Voice system
│   ├── database/
│   │   ├── ozayn.db          # SQLite database
│   │   └── schema.sql        # Database schema
│   └── install.php           # Database installer
├── deploy.sh                 # Deployment script
├── docker-compose.yml        # Docker configuration
├── Dockerfile.web            # Web container
├── Dockerfile.ml             # ML container
└── router.php                # Request router
```

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                    OZAYN CLIENT                      │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐            │
│  │   Web    │ │ Desktop  │ │  Mobile  │            │
│  │ (React)  │ │(Electron)│ │(Expo/RN) │            │
│  └────┬─────┘ └────┬─────┘ └────┬─────┘            │
│       │            │            │                   │
│  ┌────┴────────────┴────────────┴─────┐             │
│  │         ML Pipeline (TF.js)        │             │
│  │  ┌─────────┐ ┌─────────┐ ┌──────┐ │             │
│  │  │ Gesture │ │ Vision  │ │Pose  │ │             │
│  │  │ Control │ │Analysis │ │Detect│ │             │
│  │  └─────────┘ └─────────┘ └──────┘ │             │
│  └────────────────┬───────────────────┘             │
└───────────────────┼─────────────────────────────────┘
                    │ WebSocket
┌───────────────────┼─────────────────────────────────┐
│                   │        OZAYN SERVER              │
│  ┌────────────────┴───────────────────┐             │
│  │         PHP API Server              │             │
│  │  ┌──────┐ ┌──────┐ ┌──────┐       │             │
│  │  │ Auth │ │ Chat │ │Memory│ ...    │             │
│  │  └──────┘ └──────┘ └──────┘       │             │
│  └────────────────┬───────────────────┘             │
│  ┌────────────────┴───────────────────┐             │
│  │       Python ML Server (WS)        │             │
│  │  ┌─────────┐ ┌─────────┐ ┌──────┐ │             │
│  │  │MediaPipe│ │ OpenCV  │ │Vision│ │             │
│  │  │ Hands   │ │ Cascade │ │Model │ │             │
│  │  └─────────┘ └─────────┘ └──────┘ │             │
│  └────────────────┬───────────────────┘             │
│  ┌────────────────┴───────────────────┐             │
│  │    Collaboration Server (WS)       │             │
│  │  ┌────────┐ ┌────────┐ ┌────────┐ │             │
│  │  │Sessions│ │ Cursors│ │Documents│ │             │
│  │  └────────┘ └────────┘ └────────┘ │             │
│  └────────────────────────────────────┘             │
│  ┌────────────────────────────────────┐             │
│  │         SQLite Database             │             │
│  │  Users, Sessions, Messages,        │             │
│  │  Memory, Knowledge, Projects       │             │
│  └────────────────────────────────────┘             │
└─────────────────────────────────────────────────────┘
```

## Features

### Core
- User authentication (register/login)
- Chat interface with conversation history
- Memory system (short-term, project, long-term)
- Knowledge base with search
- Project management and task tracking
- Decision support system
- Audit logging

### AI & ML
- TensorFlow.js integration (client-side)
- Hand gesture recognition via webcam
- Screen understanding and analysis
- Face detection and recognition
- Object detection (COCO-SSD)
- Body pose estimation (MoveNet)
- Image classification (MobileNet)
- Python ML server with MediaPipe

### Collaboration
- Real-time multi-user sessions
- Cursor synchronization
- Shared document editing
- Event broadcasting

### Platforms
- **Web**: Full-featured SPA with responsive design
- **Desktop**: Electron app with system tray, screen capture, notifications
- **Mobile**: React Native (Expo) with camera, voice, push notifications

## Configuration

### AI Provider

Edit `ozayn/backend/config/ai.json`:
```json
{
    "provider": "openai",
    "api_key": "your-api-key",
    "model": "gpt-4",
    "max_tokens": 2048,
    "temperature": 0.7
}
```

### ML Server

Environment variables:
- `ML_HOST`: Bind address (default: localhost)
- `ML_PORT`: WebSocket port (default: 8765)

### Database

The application uses SQLite. Database location:
```
ozayn/database/ozayn.db
```

To reset: `php ozayn/install.php`

## API Endpoints

### Authentication
- `POST /ozayn/backend/api/auth/register` - Register
- `POST /ozayn/backend/api/auth/login` - Login
- `POST /ozayn/backend/api/auth/logout` - Logout

### Chat
- `POST /ozayn/backend/api/chat/send` - Send message
- `GET /ozayn/backend/api/chat/list` - List conversations
- `GET /ozayn/backend/api/chat/history/{id}` - Get history

### Projects
- `GET /ozayn/backend/api/projects/list` - List projects
- `POST /ozayn/backend/api/projects/create` - Create project

### Tasks
- `GET /ozayn/backend/api/tasks/list` - List tasks
- `POST /ozayn/backend/api/tasks/create` - Create task
- `PUT /ozayn/backend/api/tasks/{id}` - Update task

### Knowledge
- `GET /ozayn/backend/api/knowledge/list` - List entries
- `GET /ozayn/backend/api/knowledge/search?q=query` - Search
- `POST /ozayn/backend/api/knowledge/add` - Add entry

### Memory
- `GET /ozayn/backend/api/memory/recent` - Recent memories
- `GET /ozayn/backend/api/memory/search?q=query` - Search
- `POST /ozayn/backend/api/memory/store` - Store memory

### System
- `GET /ozayn/backend/api/system/status` - System status
- `GET /ozayn/backend/api/arwe/status` - ARWE status
- `POST /ozayn/backend/api/chat/send` (with commands) - Execute commands

## Service Management

```bash
# Check status
sudo ./deploy.sh status

# Restart services
sudo ./deploy.sh restart

# View logs
sudo ./deploy.sh logs

# Stop services
sudo ./deploy.sh stop

# Update and restart
sudo ./deploy.sh update
```

## Development

### Adding AI Integration

Edit `ozayn/backend/api/index.php` and modify the `generateResponse` method:

```php
private function generateResponse($message, $context, $history) {
    // Option 1: Local LLM
    // Option 2: OpenAI API
    // Option 3: Anthropic API
    // Option 4: Custom model
}
```

### Running Tests

```bash
# PHP syntax check
find ozayn -name "*.php" -exec php -l {} \;

# Python syntax check
python -m py_compile ozayn/ml/server.py

# JavaScript lint
npx eslint ozayn/frontend/js/
```

## License

ARWE Public Source License (ARWE-PSL) v1.0 - See LICENSE.md
