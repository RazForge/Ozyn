# Ozayn - Personal AI Digital Twin

Ozayn is the personal AI intelligence interface for the ARWE ecosystem. It provides a chat-based interface with memory, knowledge retrieval, and project management capabilities.

## Quick Start

### Prerequisites

- PHP 7.4+ with SQLite extension
- Web server (Apache/Nginx) or PHP built-in server

### Installation

1. Clone the repository:
```bash
git clone <repository-url>
cd Ozyn
```

2. Initialize the database:
```bash
php ozayn/install.php
```

3. Start the development server:
```bash
php -S localhost:8000 router.php
```

4. Open your browser and navigate to:
```
http://localhost:8000/ozayn
```

5. Register a new account and start chatting!

## Project Structure

```
ozyn/
├── ozayn/
│   ├── backend/
│   │   ├── api/          # API endpoints
│   │   ├── auth/         # Authentication system
│   │   ├── memory/       # Memory system
│   │   ├── knowledge/    # Knowledge retrieval
│   │   ├── projects/     # Project management
│   │   └── config/       # Configuration
│   ├── frontend/
│   │   ├── css/          # Styles
│   │   ├── js/           # JavaScript
│   │   └── index.html    # Main UI
│   ├── database/         # SQLite database
│   ├── logs/             # Application logs
│   └── install.php       # Installation script
├── router.php            # Request router
└── README.md
```

## Features

### Phase 1 (Current)
- User authentication (register/login)
- Chat interface
- Conversation history
- Basic memory system
- Knowledge base
- Project management
- Task tracking

### Phase 2 (Planned)
- AI model integration
- Voice interface
- ARWE system integration
- Computer control
- Advanced memory and context

## API Endpoints

### Authentication
- `POST /ozayn/backend/api/auth/register` - Register new user
- `POST /ozayn/backend/api/auth/login` - Login
- `POST /ozayn/backend/api/auth/logout` - Logout

### Chat
- `POST /ozayn/backend/api/chat/send` - Send message
- `GET /ozayn/backend/api/chat/list` - List conversations
- `GET /ozayn/backend/api/chat/history/{id}` - Get conversation history

### Projects
- `GET /ozayn/backend/api/projects/list` - List projects
- `POST /ozayn/backend/api/projects/create` - Create project

### Tasks
- `GET /ozayn/backend/api/tasks/list` - List tasks
- `POST /ozayn/backend/api/tasks/create` - Create task
- `PUT /ozayn/backend/api/tasks/{id}` - Update task

### Knowledge
- `GET /ozayn/backend/api/knowledge/list` - List knowledge entries
- `GET /ozayn/backend/api/knowledge/search?q=query` - Search knowledge
- `POST /ozayn/backend/api/knowledge/add` - Add knowledge entry

### Memory
- `GET /ozayn/backend/api/memory/recent` - Get recent memories
- `GET /ozayn/backend/api/memory/search?q=query` - Search memories
- `POST /ozayn/backend/api/memory/store` - Store memory

## Development

### Adding AI Integration

To integrate an AI model, edit `ozayn/backend/api/index.php` and modify the `generateResponse` method:

```php
private function generateResponse($message, $context, $history) {
    // Option 1: Local LLM
    // Option 2: OpenAI API
    // Option 3: Anthropic API
    // Option 4: Custom model
    
    // Example with OpenAI:
    // $response = $this->callOpenAI($message, $context, $history);
}
```

### Database

The application uses SQLite. The database file is located at:
```
ozayn/database/ozayn.db
```

To reset the database, delete it and run:
```bash
php ozayn/install.php
```

## License

ARWE Public Source License (ARWE-PSL) v1.0 - See LICENSE.md
