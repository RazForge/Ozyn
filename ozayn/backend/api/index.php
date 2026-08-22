<?php
/**
 * Ozayn API Router
 */

header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type, Authorization, X-Session-ID');

// Handle preflight requests
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit();
}

require_once __DIR__ . '/../auth/auth.php';
require_once __DIR__ . '/../memory/memory.php';
require_once __DIR__ . '/../knowledge/knowledge.php';
require_once __DIR__ . '/../projects/projects.php';
require_once __DIR__ . '/../ai/engine.php';

class API {
    private $auth;
    private $memory;
    private $knowledge;
    private $projects;
    private $ai;
    private $userId;

    public function __construct() {
        $this->auth = new Auth();
        $this->memory = new Memory();
        $this->knowledge = new Knowledge();
        $this->projects = new Projects();
        $this->ai = new AI();
    }

    /**
     * Get request data
     */
    private function getInput() {
        $input = json_decode(file_get_contents('php://input'), true);
        return $input ?: [];
    }

    /**
     * Send JSON response
     */
    private function response($data, $statusCode = 200) {
        http_response_code($statusCode);
        echo json_encode($data);
        exit();
    }

    /**
     * Send error response
     */
    private function error($message, $statusCode = 400) {
        $this->response(['error' => $message], $statusCode);
    }

    /**
     * Authenticate request
     */
    private function authenticate() {
        $sessionId = $_SERVER['HTTP_X_SESSION_ID'] ?? null;
        
        if (!$sessionId) {
            $this->error('Authentication required', 401);
        }

        $user = $this->auth->getUserFromSession($sessionId);
        if (!$user) {
            $this->error('Invalid session', 401);
        }

        $this->userId = $user['id'];
        return $user;
    }

    /**
     * Route request
     */
    public function route($method, $path) {
        $segments = explode('/', trim($path, '/'));
        
        // Auth routes
        if ($segments[0] === 'auth') {
            return $this->handleAuth($method, $segments);
        }

        // Protected routes require authentication
        $user = $this->authenticate();

        // Chat routes
        if ($segments[0] === 'chat') {
            return $this->handleChat($method, $segments);
        }

        // Memory routes
        if ($segments[0] === 'memory') {
            return $this->handleMemory($method, $segments);
        }

        // Knowledge routes
        if ($segments[0] === 'knowledge') {
            return $this->handleKnowledge($method, $segments);
        }

        // Project routes
        if ($segments[0] === 'projects') {
            return $this->handleProjects($method, $segments);
        }

        // Task routes
        if ($segments[0] === 'tasks') {
            return $this->handleTasks($method, $segments);
        }

        $this->error('Endpoint not found', 404);
    }

    /**
     * Handle auth routes
     */
    private function handleAuth($method, $segments) {
        $action = $segments[1] ?? null;

        if ($method === 'POST' && $action === 'register') {
            $input = $this->getInput();
            $result = $this->auth->register(
                $input['username'] ?? '',
                $input['password'] ?? '',
                $input['email'] ?? null,
                $input['full_name'] ?? null
            );
            $this->response($result, $result['success'] ? 201 : 400);
        }

        if ($method === 'POST' && $action === 'login') {
            $input = $this->getInput();
            $result = $this->auth->login(
                $input['username'] ?? '',
                $input['password'] ?? ''
            );
            $this->response($result, $result['success'] ? 200 : 401);
        }

        if ($method === 'POST' && $action === 'logout') {
            $sessionId = $_SERVER['HTTP_X_SESSION_ID'] ?? null;
            $this->auth->logout($sessionId);
            $this->response(['success' => true]);
        }

        $this->error('Invalid auth action', 400);
    }

    /**
     * Handle chat routes
     */
    private function handleChat($method, $segments) {
        $action = $segments[1] ?? null;

        if ($method === 'POST' && $action === 'send') {
            $input = $this->getInput();
            $message = $input['message'] ?? '';
            $conversationId = $input['conversation_id'] ?? null;
            $projectId = $input['project_id'] ?? null;

            // Get or create conversation
            if (!$conversationId) {
                $conversationId = $this->projects->createConversation($this->userId, $projectId);
            }

            // Save user message
            $this->projects->addMessage($conversationId, 'user', $message);

            // Check for commands first
            $command = $this->ai->processCommand($this->userId, $message, $conversationId, $projectId);
            
            if ($command) {
                $aiResponse = $this->executeCommand($command);
            } else {
                // Generate AI response
                $aiResponse = $this->ai->generateResponse($this->userId, $message, $conversationId, $projectId);
            }

            // Save AI response
            $this->projects->addMessage($conversationId, 'assistant', $aiResponse);

            // Store in memory
            $this->memory->store($this->userId, "last_query", $message, 'short_term');

            $this->response([
                'success' => true,
                'conversation_id' => $conversationId,
                'response' => $aiResponse
            ]);
        }

        if ($method === 'GET' && $action === 'history') {
            $conversationId = $segments[2] ?? null;
            if (!$conversationId) {
                $this->error('Conversation ID required');
            }

            $messages = $this->projects->getMessages($conversationId);
            $this->response(['messages' => $messages]);
        }

        if ($method === 'GET' && $action === 'list') {
            $conversations = $this->projects->getUserConversations($this->userId);
            $this->response(['conversations' => $conversations]);
        }

        $this->error('Invalid chat action', 400);
    }

    /**
     * Execute a command
     */
    private function executeCommand($command) {
        switch ($command['action']) {
            case 'create_project':
                $id = $this->projects->create($this->userId, $command['name']);
                return "Project \"{$command['name']}\" created successfully! (ID: {$id})";

            case 'create_task':
                $id = $this->projects->createTask($this->userId, $command['title']);
                return "Task \"{$command['title']}\" created successfully! (ID: {$id})";

            case 'store_memory':
                $this->memory->store($this->userId, $command['key'], $command['value'], 'long_term', null, null, 0.8);
                return "Remembered: \"{$command['key']}\" = \"{$command['value']}\"";

            case 'recall':
                if ($command['value']) {
                    return "I remember \"{$command['key']}\": {$command['value']}";
                }
                return "I don't have anything stored for \"{$command['key']}\".";

            case 'search':
                if (empty($command['results'])) {
                    return "No results found for \"{$command['query']}\".";
                }
                $results = "Search results for \"{$command['query']}\":\n\n";
                foreach ($command['results'] as $r) {
                    $results .= "- **{$r['title']}**: " . substr($r['content'], 0, 100) . "...\n";
                }
                return $results;

            default:
                return "Command executed.";
        }
    }

    /**
     * Handle memory routes
     */
    private function handleMemory($method, $segments) {
        $action = $segments[1] ?? null;

        if ($method === 'GET' && $action === 'search') {
            $query = $_GET['q'] ?? '';
            $results = $this->memory->search($this->userId, $query);
            $this->response(['results' => $results]);
        }

        if ($method === 'GET' && $action === 'recent') {
            $results = $this->memory->getRecent($this->userId);
            $this->response(['results' => $results]);
        }

        if ($method === 'POST' && $action === 'store') {
            $input = $this->getInput();
            $id = $this->memory->store(
                $this->userId,
                $input['key'] ?? '',
                $input['value'] ?? '',
                $input['type'] ?? 'short_term',
                $input['project_id'] ?? null,
                $input['context'] ?? null,
                $input['importance'] ?? 0.5
            );
            $this->response(['success' => true, 'id' => $id], 201);
        }

        $this->error('Invalid memory action', 400);
    }

    /**
     * Handle knowledge routes
     */
    private function handleKnowledge($method, $segments) {
        $action = $segments[1] ?? null;

        if ($method === 'GET' && $action === 'search') {
            $query = $_GET['q'] ?? '';
            $results = $this->knowledge->search($this->userId, $query);
            $this->response(['results' => $results]);
        }

        if ($method === 'GET' && $action === 'list') {
            $results = $this->knowledge->getAll($this->userId);
            $this->response(['results' => $results]);
        }

        if ($method === 'POST' && $action === 'add') {
            $input = $this->getInput();
            $id = $this->knowledge->add(
                $this->userId,
                $input['title'] ?? '',
                $input['content'] ?? '',
                $input['source'] ?? null,
                $input['tags'] ?? [],
                $input['project_id'] ?? null
            );
            $this->response(['success' => true, 'id' => $id], 201);
        }

        $this->error('Invalid knowledge action', 400);
    }

    /**
     * Handle project routes
     */
    private function handleProjects($method, $segments) {
        $action = $segments[1] ?? null;

        if ($method === 'GET' && $action === 'list') {
            $projects = $this->projects->getUserProjects($this->userId);
            $this->response(['projects' => $projects]);
        }

        if ($method === 'GET' && isset($segments[2])) {
            $project = $this->projects->getWithContext($segments[2], $this->userId);
            if (!$project) {
                $this->error('Project not found', 404);
            }
            $this->response($project);
        }

        if ($method === 'POST' && $action === 'create') {
            $input = $this->getInput();
            $id = $this->projects->create(
                $this->userId,
                $input['name'] ?? '',
                $input['description'] ?? null
            );
            $this->response(['success' => true, 'id' => $id], 201);
        }

        $this->error('Invalid project action', 400);
    }

    /**
     * Handle task routes
     */
    private function handleTasks($method, $segments) {
        $action = $segments[1] ?? null;

        if ($method === 'GET' && $action === 'list') {
            $status = $_GET['status'] ?? null;
            $projectId = $_GET['project_id'] ?? null;
            $tasks = $this->projects->getUserTasks($this->userId, $status, $projectId);
            $this->response(['tasks' => $tasks]);
        }

        if ($method === 'POST' && $action === 'create') {
            $input = $this->getInput();
            $id = $this->projects->createTask(
                $this->userId,
                $input['title'] ?? '',
                $input['description'] ?? null,
                $input['project_id'] ?? null,
                $input['priority'] ?? 'medium',
                $input['due_date'] ?? null
            );
            $this->response(['success' => true, 'id' => $id], 201);
        }

        if ($method === 'PUT' && isset($segments[2])) {
            $input = $this->getInput();
            $this->projects->updateTask($segments[2], $this->userId, $input);
            $this->response(['success' => true]);
        }

        $this->error('Invalid task action', 400);
    }

}

// Parse request
$method = $_SERVER['REQUEST_METHOD'];
$path = $_SERVER['REQUEST_URI'];
$path = parse_url($path, PHP_URL_PATH);

// Remove base path
$path = preg_replace('#^/ozayn/backend/api#', '', $path);

// Route request
$api = new API();
$api->route($method, $path);
