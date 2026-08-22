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
require_once __DIR__ . '/../../tools/computer.php';
require_once __DIR__ . '/../../tools/monitor.php';

class API {
    private $auth;
    private $memory;
    private $knowledge;
    private $projects;
    private $ai;
    private $computer;
    private $monitor;
    private $userId;

    public function __construct() {
        $this->auth = new Auth();
        $this->memory = new Memory();
        $this->knowledge = new Knowledge();
        $this->projects = new Projects();
        $this->ai = new AI();
        $this->computer = new ComputerTools();
        $this->monitor = new SystemMonitor();
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

        // Tools routes
        if ($segments[0] === 'tools') {
            return $this->handleTools($method, $segments);
        }

        // System routes
        if ($segments[0] === 'system') {
            return $this->handleSystem($method, $segments);
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

            // System commands
            case 'system_overview':
                $overview = $this->monitor->getOverview();
                return $this->formatSystemOverview($overview);

            case 'cpu_info':
                $cpu = $this->monitor->getCpuInfo();
                return "**CPU Information**\n\n" .
                       "- Model: {$cpu['model']}\n" .
                       "- Cores: {$cpu['cores']}\n" .
                       "- Usage: {$cpu['usage']}%\n" .
                       "- Load: {$cpu['load_average']['1min']} / {$cpu['load_average']['5min']} / {$cpu['load_average']['15min']}";

            case 'memory_info':
                $mem = $this->monitor->getMemoryInfo();
                $usedGB = round($mem['used'] / 1073741824, 2);
                $totalGB = round($mem['total'] / 1073741824, 2);
                return "**Memory Information**\n\n" .
                       "- Used: {$usedGB} GB ({$mem['percent_used']}%)\n" .
                       "- Total: {$totalGB} GB\n" .
                       "- Available: " . round($mem['available'] / 1073741824, 2) . " GB";

            case 'disk_info':
                $disks = $this->monitor->getDiskInfo();
                $result = "**Disk Information**\n\n";
                foreach ($disks as $disk) {
                    $result .= "- **{$disk['mount_point']}**: {$disk['used']}/{$disk['size']} ({$disk['percent_used']})\n";
                }
                return $result;

            case 'processes':
                $procs = $this->monitor->getProcesses('cpu', 10);
                $result = "**Top Processes (by CPU)**\n\n";
                foreach ($procs['processes'] as $p) {
                    $cmd = substr($p['command'], 0, 50);
                    $result .= "- `{$p['pid']}` CPU: {$p['cpu']}% MEM: {$p['mem']}% - {$cmd}\n";
                }
                return $result;

            case 'top_cpu':
                $procs = $this->monitor->getTopCpu(10);
                $result = "**Top 10 CPU Processes**\n\n";
                foreach ($procs['processes'] as $p) {
                    $cmd = substr($p['command'], 0, 50);
                    $result .= "- `{$p['pid']}` {$p['cpu']}% - {$cmd}\n";
                }
                return $result;

            case 'top_memory':
                $procs = $this->monitor->getTopMemory(10);
                $result = "**Top 10 Memory Processes**\n\n";
                foreach ($procs['processes'] as $p) {
                    $cmd = substr($p['command'], 0, 50);
                    $result .= "- `{$p['pid']}` {$p['mem']}% - {$cmd}\n";
                }
                return $result;

            case 'network_info':
                $net = $this->monitor->getNetworkInfo();
                $result = "**Network Interfaces**\n\n";
                foreach ($net as $iface) {
                    $ips = implode(', ', $iface['ips'] ?? ['N/A']);
                    $result .= "- **{$iface['name']}**: {$ips}\n";
                    if (isset($iface['rx_mb'])) {
                        $result .= "  RX: {$iface['rx_mb']} MB / TX: {$iface['tx_mb']} MB\n";
                    }
                }
                return $result;

            case 'uptime':
                $up = $this->monitor->getUptime();
                return "**System Uptime**: {$up['formatted']}";

            case 'temperature':
                $temps = $this->monitor->getTemperature();
                if (empty($temps)) {
                    return "Temperature sensors not available on this system.";
                }
                $result = "**System Temperature**\n\n";
                foreach ($temps as $t) {
                    $result .= "- {$t['type']}: {$t['celsius']}°C ({$t['fahrenheit']}°F)\n";
                }
                return $result;

            // File commands
            case 'list_files':
                $files = $this->computer->listFiles($command['path']);
                if (isset($files['error'])) return "Error: {$files['error']}";
                $result = "**Files in {$files['path']}** ({$files['count']} items)\n\n";
                foreach ($files['items'] as $item) {
                    $icon = $item['type'] === 'directory' ? '📁' : '📄';
                    $size = $this->formatBytes($item['size']);
                    $result .= "{$icon} {$item['name']} ({$size})\n";
                }
                return $result;

            case 'read_file':
                $file = $this->computer->readFile($command['path']);
                if (isset($file['error'])) return "Error: {$file['error']}";
                $content = $file['truncated'] ? substr($file['content'], 0, 1000) . "\n... (truncated)" : $file['content'];
                return "**File: {$file['path']}**\n\n```\n{$content}\n```";

            case 'create_dir':
                $result = $this->computer->createDirectory($command['path']);
                if (isset($result['error'])) return "Error: {$result['error']}";
                return "Directory created: {$command['path']}";

            case 'current_dir':
                $dir = $this->computer->getCurrentDirectory();
                return "Current directory: `{$dir['path']}`";

            case 'find_files':
                $files = $this->computer->searchFiles($command['path'], $command['pattern']);
                if (isset($files['error'])) return "Error: {$files['error']}";
                if (empty($files['results'])) return "No files found matching: {$command['pattern']}";
                $result = "**Files matching {$command['pattern']}**\n\n";
                foreach ($files['results'] as $f) {
                    $result .= "- {$f['path']}\n";
                }
                return $result;

            default:
                return "Command executed.";
        }
    }

    /**
     * Format system overview
     */
    private function formatSystemOverview($overview) {
        $memUsed = round($overview['memory']['used'] / 1073741824, 2);
        $memTotal = round($overview['memory']['total'] / 1073741824, 2);
        
        return "**System Overview**\n\n" .
               "**Hostname**: {$overview['hostname']}\n" .
               "**OS**: {$overview['os']}\n" .
               "**Architecture**: {$overview['architecture']}\n" .
               "**Uptime**: {$overview['uptime']['formatted']}\n\n" .
               "**CPU**\n" .
               "- Model: {$overview['cpu']['model']}\n" .
               "- Cores: {$overview['cpu']['cores']}\n" .
               "- Usage: {$overview['cpu']['usage']}%\n" .
               "- Load: {$overview['load_average']['1min']} / {$overview['load_average']['5min']} / {$overview['load_average']['15min']}\n\n" .
               "**Memory**\n" .
               "- Used: {$memUsed} GB / {$memTotal} GB ({$overview['memory']['percent_used']}%)\n\n" .
               "**Disk**\n" .
               "- " . count($overview['disk']) . " disk(s) mounted\n\n" .
               "**PHP**: {$overview['php_version']}\n" .
               "**Time**: {$overview['timestamp']}";
    }

    /**
     * Format bytes
     */
    private function formatBytes($bytes) {
        $units = ['B', 'KB', 'MB', 'GB', 'TB'];
        $bytes = max($bytes, 0);
        $pow = floor(($bytes ? log($bytes) : 0) / log(1024));
        $pow = min($pow, count($units) - 1);
        $bytes /= pow(1024, $pow);
        return round($bytes, 2) . ' ' . $units[$pow];
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

    /**
     * Handle tools routes (computer control)
     */
    private function handleTools($method, $segments) {
        $action = $segments[1] ?? null;

        // Files - list
        if ($method === 'GET' && $action === 'files') {
            $path = $_GET['path'] ?? '.';
            $recursive = isset($_GET['recursive']);
            $result = $this->computer->listFiles($path, $recursive);
            $this->response($result);
        }

        // Files - read
        if ($method === 'GET' && $action === 'read') {
            $path = $_GET['path'] ?? '';
            if (!$path) $this->error('Path required');
            $result = $this->computer->readFile($path);
            $this->response($result);
        }

        // Files - write
        if ($method === 'POST' && $action === 'write') {
            $input = $this->getInput();
            $result = $this->computer->writeFile(
                $input['path'] ?? '',
                $input['content'] ?? '',
                $input['append'] ?? false
            );
            $this->response($result);
        }

        // Files - mkdir
        if ($method === 'POST' && $action === 'mkdir') {
            $input = $this->getInput();
            $result = $this->computer->createDirectory($input['path'] ?? '');
            $this->response($result);
        }

        // Files - delete
        if ($method === 'POST' && $action === 'delete') {
            $input = $this->getInput();
            $result = $this->computer->delete($input['path'] ?? '');
            $this->response($result);
        }

        // Files - copy
        if ($method === 'POST' && $action === 'copy') {
            $input = $this->getInput();
            $result = $this->computer->copyFile(
                $input['source'] ?? '',
                $input['destination'] ?? ''
            );
            $this->response($result);
        }

        // Files - move
        if ($method === 'POST' && $action === 'move') {
            $input = $this->getInput();
            $result = $this->computer->moveFile(
                $input['source'] ?? '',
                $input['destination'] ?? ''
            );
            $this->response($result);
        }

        // Files - search
        if ($method === 'GET' && $action === 'search') {
            $path = $_GET['path'] ?? '.';
            $pattern = $_GET['pattern'] ?? '*';
            $result = $this->computer->searchFiles($path, $pattern);
            $this->response($result);
        }

        // Files - grep
        if ($method === 'GET' && $action === 'grep') {
            $path = $_GET['path'] ?? '.';
            $pattern = $_GET['pattern'] ?? '';
            if (!$pattern) $this->error('Pattern required');
            $result = $this->computer->grep($path, $pattern);
            $this->response($result);
        }

        // Files - stat
        if ($method === 'GET' && $action === 'stat') {
            $path = $_GET['path'] ?? '';
            if (!$path) $this->error('Path required');
            $result = $this->computer->stat($path);
            $this->response($result);
        }

        // Run command
        if ($method === 'POST' && $action === 'run') {
            $input = $this->getInput();
            $result = $this->computer->runCommand(
                $input['command'] ?? '',
                $input['timeout'] ?? 30
            );
            $this->response($result);
        }

        // Current directory
        if ($method === 'GET' && $action === 'pwd') {
            $result = $this->computer->getCurrentDirectory();
            $this->response($result);
        }

        $this->error('Invalid tool action', 400);
    }

    /**
     * Handle system monitoring routes
     */
    private function handleSystem($method, $segments) {
        $action = $segments[1] ?? null;

        // Overview
        if ($method === 'GET' && ($action === 'overview' || $action === null)) {
            $result = $this->monitor->getOverview();
            $this->response($result);
        }

        // CPU
        if ($method === 'GET' && $action === 'cpu') {
            $result = $this->monitor->getCpuInfo();
            $this->response($result);
        }

        // CPU usage
        if ($method === 'GET' && $action === 'cpu-usage') {
            $result = ['usage' => $this->monitor->getCpuUsage()];
            $this->response($result);
        }

        // Memory
        if ($method === 'GET' && $action === 'memory') {
            $result = $this->monitor->getMemoryInfo();
            $this->response($result);
        }

        // Disk
        if ($method === 'GET' && $action === 'disk') {
            $result = $this->monitor->getDiskInfo();
            $this->response($result);
        }

        // Load average
        if ($method === 'GET' && $action === 'load') {
            $result = $this->monitor->getLoadAverage();
            $this->response($result);
        }

        // Processes
        if ($method === 'GET' && $action === 'processes') {
            $sortBy = $_GET['sort'] ?? 'cpu';
            $limit = $_GET['limit'] ?? 20;
            $result = $this->monitor->getProcesses($sortBy, $limit);
            $this->response($result);
        }

        // Top CPU
        if ($method === 'GET' && $action === 'top-cpu') {
            $limit = $_GET['limit'] ?? 10;
            $result = $this->monitor->getTopCpu($limit);
            $this->response($result);
        }

        // Top Memory
        if ($method === 'GET' && $action === 'top-memory') {
            $limit = $_GET['limit'] ?? 10;
            $result = $this->monitor->getTopMemory($limit);
            $this->response($result);
        }

        // Kill process
        if ($method === 'POST' && $action === 'kill') {
            $input = $this->getInput();
            $result = $this->monitor->killProcess(
                $input['pid'] ?? 0,
                $input['signal'] ?? 15
            );
            $this->response($result);
        }

        // Network
        if ($method === 'GET' && $action === 'network') {
            $result = $this->monitor->getNetworkInfo();
            $this->response(['interfaces' => $result]);
        }

        // Temperature
        if ($method === 'GET' && $action === 'temperature') {
            $result = $this->monitor->getTemperature();
            $this->response(['sensors' => $result]);
        }

        // Uptime
        if ($method === 'GET' && $action === 'uptime') {
            $result = $this->monitor->getUptime();
            $this->response($result);
        }

        // Logged users
        if ($method === 'GET' && $action === 'users') {
            $result = $this->monitor->getLoggedUsers();
            $this->response(['users' => $result]);
        }

        // Directory size
        if ($method === 'GET' && $action === 'dirsize') {
            $path = $_GET['path'] ?? '.';
            $result = $this->monitor->getDirectorySize($path);
            $this->response($result);
        }

        $this->error('Invalid system action', 400);
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
