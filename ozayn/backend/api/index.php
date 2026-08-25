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
require_once __DIR__ . '/../../tools/arwe.php';
require_once __DIR__ . '/../../tools/decision.php';
require_once __DIR__ . '/../../tools/audit.php';
require_once __DIR__ . '/../../tools/apps.php';
require_once __DIR__ . '/../../tools/code.php';
require_once __DIR__ . '/../../tools/logger.php';
require_once __DIR__ . '/../../tools/security.php';
require_once __DIR__ . '/../../tools/export.php';
require_once __DIR__ . '/../../tools/profiler.php';
require_once __DIR__ . '/../../tools/search.php';
require_once __DIR__ . '/../../tools/notifications.php';
require_once __DIR__ . '/../../agents/agents.php';

class API {
    private $auth;
    private $memory;
    private $knowledge;
    private $projects;
    private $ai;
    private $computer;
    private $monitor;
    private $arwe;
    private $decision;
    private $audit;
    private $apps;
    private $code;
    private $agents;
    private $logger;
    private $security;
    private $export;
    private $search;
    private $notifPrefs;
    private $userId;

    public function __construct() {
        $this->auth = new Auth();
        $this->memory = new Memory();
        $this->knowledge = new Knowledge();
        $this->projects = new Projects();
        $this->ai = new AI();
        $this->computer = new ComputerTools();
        $this->monitor = new SystemMonitor();
        $this->arwe = new ARWETools();
        $this->decision = new DecisionSupport();
        $this->audit = new AuditSystem();
        $this->apps = new AppLauncher();
        $this->code = new CodeAssistant();
        $this->agents = new AgentSystem();
        $this->logger = new LogSystem();
        $this->security = new SecurityTools();
        $this->export = new ExportTools();
        $this->search = new AdvancedSearch();
        $this->notifPrefs = new NotificationPrefs();
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

        // ARWE routes
        if ($segments[0] === 'arwe') {
            return $this->handleARWE($method, $segments);
        }

        // Decision routes
        if ($segments[0] === 'decisions') {
            return $this->handleDecisions($method, $segments);
        }

        // Audit routes
        if ($segments[0] === 'audit') {
            return $this->handleAudit($method, $segments);
        }

        // Agents routes
        if ($segments[0] === 'agents') {
            return $this->handleAgents($method, $segments);
        }

        // Apps routes
        if ($segments[0] === 'apps') {
            return $this->handleApps($method, $segments);
        }

        // Code routes
        if ($segments[0] === 'code') {
            return $this->handleCode($method, $segments);
        }

        // Batch routes
        if ($segments[0] === 'batch') {
            return $this->handleBatch($method, $segments);
        }

        // REST client routes
        if ($segments[0] === 'rest') {
            return $this->handleRest($method, $segments);
        }

        // Collaboration routes
        if ($segments[0] === 'collab') {
            return $this->handleCollab($method, $segments);
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

            // ARWE commands
            case 'arwe_status':
                return $this->arwe->getDailyBriefing();

            case 'arwe_system':
                $status = $this->arwe->getStatus($command['system']);
                return $this->arwe->formatStatus($command['system'], $status);

            case 'kidane_status':
                return $this->arwe->getKidaneStatus();

            case 'canivox_status':
                return $this->arwe->getCanivoxStatus();

            case 'edunex_details':
                return $this->arwe->getEdunexDetails();

            case 'govyx_details':
                return $this->arwe->getGovyxDetails();

            case 'locify_details':
                return $this->arwe->getLocifyDetails();

            case 'terrachain_details':
                return $this->arwe->getTerraChainDetails();

            case 'bilen_details':
                return $this->arwe->getBilenDetails();

            case 'arwe_summary':
                return $this->arwe->getAllSystemsSummary();

            // Decision commands
            case 'create_decision':
                $id = $this->decision->createDecision($this->userId, $command['context'], []);
                return "Decision created (ID: {$id}). Add options to proceed.";

            case 'list_decisions':
                $decisions = $this->decision->getUserDecisions($this->userId, null, 10);
                if (empty($decisions)) return "No decisions found.";
                $result = "**Your Decisions**\n\n";
                foreach ($decisions as $d) {
                    $result .= "- [{$d['id']}] {$d['context']} ({$d['status']})\n";
                }
                return $result;

            case 'add_options':
                $options = array_map(function($name) {
                    return ['name' => $name, 'pros' => [], 'cons' => [], 'risk_level' => 'medium'];
                }, $command['options']);
                $this->decision->addOptions($command['decision_id'], $this->userId, $options);
                return "Options added to decision #{$command['decision_id']}.";

            case 'decision_stats':
                $stats = $this->decision->getDecisionStats($this->userId);
                return "**Decision Statistics**\n\n" .
                       "- Total: {$stats['total']}\n" .
                       "- Decided: {$stats['decided']}\n" .
                       "- Pending: {$stats['pending']}\n" .
                       "- Completed: {$stats['completed']}";

            case 'compare_decisions':
                return $this->decision->compareDecisions($command['ids'], $this->userId);

            // Agent commands
            case 'list_agents':
                $agents = $this->agents->getAvailableAgents();
                $result = "**Available Agents**\n\n";
                foreach ($agents as $agent) {
                    $result .= "- **{$agent['name']}**: {$agent['description']}\n";
                }
                return $result;

            // App commands
            case 'list_apps':
                return $this->apps->formatAppList();

            case 'open_app':
                $result = $this->apps->launch($command['app']);
                if (isset($result['error'])) return "Error: {$result['error']}";
                return "Launched: {$result['app']}";

            // Web search commands
            case 'web_search':
                $web = new WebSearchTools();
                $results = $web->search($command['query']);
                return $web->formatResults($results);

            case 'fetch_url':
                $web = new WebSearchTools();
                $result = $web->fetchURL($command['url']);
                if (isset($result['error'])) return "Error: {$result['error']}";
                return "**URL Content**\n\n" . substr($result['content'], 0, 2000);

            // Notification commands
            case 'list_notifications':
                $notifications = new NotificationSystem();
                $userNotifs = $notifications->getUserNotifications($this->userId, 10);
                if (empty($userNotifs)) return "No notifications.";
                $output = "**Notifications**\n\n";
                foreach ($userNotifs as $n) {
                    $read = $n['read'] ? '✓' : '●';
                    $output .= "{$read} **{$n['title']}** - {$n['message']}\n";
                }
                return $output;

            case 'mark_notifications_read':
                $notifications = new NotificationSystem();
                $notifications->markAllRead($this->userId);
                return "All notifications marked as read.";

            // Workflow commands
            case 'list_workflows':
                $workflows = new WorkflowTools();
                $userWorkflows = $workflows->getUserWorkflows($this->userId);
                if (empty($userWorkflows)) return "No workflows created yet.";
                $output = "**Your Workflows**\n\n";
                foreach ($userWorkflows as $wf) {
                    $output .= "- [{$wf['id']}] {$wf['name']} ({$wf['status']})\n";
                }
                return $output;

            case 'run_workflow':
                $workflows = new WorkflowTools();
                $userWorkflows = $workflows->getUserWorkflows($this->userId);
                $wfId = null;
                foreach ($userWorkflows as $wf) {
                    if (strtolower($wf['name']) === strtolower($command['name'])) {
                        $wfId = $wf['id'];
                        break;
                    }
                }
                if (!$wfId) return "Workflow not found: {$command['name']}";
                $result = $workflows->runWorkflow($wfId, $this->userId);
                return "Workflow executed: {$result['workflow']}\nSteps: " . count($result['results']);

            // Export/Import commands
            case 'export_data':
                $exporter = new ExportImportTools();
                $data = $exporter->exportData($this->userId, $command['type']);
                $json = $exporter->formatForDownload($data, 'json');
                
                // Save to temp file for download
                $filename = "ozayn_export_{$command['type']}_" . date('Y-m-d') . ".json";
                $filepath = sys_get_temp_dir() . '/' . $filename;
                file_put_contents($filepath, $json);
                
                return "Data exported successfully!\n\n**Exported:** {$command['type']}\n**File:** {$filename}\n**Size:** " . strlen($json) . " bytes\n\nUse the API endpoint `/api/v1/export/download/{$filename}` to download.";

            case 'import_data':
                return "To import data:\n1. Use `export` command to get your data file\n2. Upload the file using the upload button\n3. Or use the API: `POST /api/v1/import` with JSON data";

            // Scheduler commands
            case 'list_scheduled':
                $scheduler = new ScheduledTasksTool();
                $tasks = $scheduler->getUserTasks($this->userId);
                if (empty($tasks)) return "No scheduled tasks.";
                $output = "**Scheduled Tasks**\n\n";
                foreach ($tasks as $t) {
                    $status = $t['enabled'] ? '✓' : '✗';
                    $output .= "{$status} [{$t['id']}] {$t['name']} - {$t['schedule']}\n";
                }
                return $output;

            case 'create_schedule':
                $scheduler = new ScheduledTasksTool();
                $id = $scheduler->create($this->userId, $command['command'], $command['command'], $command['schedule']);
                return "Schedule created! (ID: {$id})\nCommand: {$command['command']}\nSchedule: {$command['schedule']}";

            // Backup commands
            case 'create_backup':
                $backup = new BackupRestoreTool();
                $result = $backup->createBackup($this->userId);
                return "Backup created!\nFile: {$result['filename']}\nSize: " . round($result['size'] / 1024, 2) . " KB\nTables: {$result['tables']}";

            case 'list_backups':
                $backup = new BackupRestoreTool();
                $backups = $backup->listBackups();
                if (empty($backups)) return "No backups found.";
                $output = "**Available Backups**\n\n";
                foreach ($backups as $b) {
                    $size = round($b['size'] / 1024, 2);
                    $output .= "- {$b['filename']} ({$size} KB) - {$b['created']}\n";
                }
                return $output;

            // Analytics commands
            case 'get_stats':
                $analytics = new AnalyticsTool();
                $stats = $analytics->getUserStats($this->userId);
                return $analytics->formatStats($stats);

            case 'get_activity':
                $analytics = new AnalyticsTool();
                $activities = $analytics->getActivityTimeline($this->userId, 10);
                if (empty($activities)) return "No recent activity.";
                $output = "**Recent Activity**\n\n";
                foreach ($activities as $a) {
                    $output .= "{$a['icon']} {$a['content']} - {$a['timestamp']}\n";
                }
                return $output;

            // Plugin commands
            case 'list_plugins':
                $plugins = new PluginSystem();
                $list = $plugins->getAvailablePlugins();
                if (empty($list)) return "No plugins installed.";
                $output = "**Plugins**\n\n";
                foreach ($list as $p) {
                    $status = $p['enabled'] ? '✓' : '✗';
                    $output .= "{$status} {$p['name']} ({$p['version']}) - {$p['description']}\n";
                }
                return $output;

            case 'install_plugin':
                $plugins = new PluginSystem();
                $result = $plugins->install($command['name'], 'User installed plugin', '1.0.0');
                if (isset($result['error'])) return "Error: {$result['error']}";
                return "Plugin installed: {$result['name']}";

            case 'toggle_plugin':
                $plugins = new PluginSystem();
                $result = $plugins->toggle($command['name']);
                if (isset($result['error'])) return "Error: {$result['error']}";
                $status = $result['enabled'] ? 'enabled' : 'disabled';
                return "Plugin {$command['name']} {$status}.";

            // Tutorial commands
            case 'list_tutorials':
                $tutorial = new TutorialSystem();
                $tutorials = $tutorial->getTutorials()['tutorials'];
                $progress = $tutorial->getProgress($this->userId);
                
                $output = "**Tutorials** ({$progress['completed']}/{$progress['total']} completed)\n\n";
                foreach ($tutorials as $t) {
                    $completed = $tutorial->isCompleted($this->userId, $t['id']);
                    $icon = $completed ? '✓' : '○';
                    $output .= "{$icon} **{$t['title']}** - {$t['description']} ({$t['duration']})\n";
                }
                $output .= "\nType `start tutorial [id]` to begin.";
                return $output;

            case 'start_tutorial':
                $tutorial = new TutorialSystem();
                $content = $tutorial->getTutorialContent($command['id']);
                if (!$content) return "Tutorial not found.";
                
                $output = "**{$content['title']}**\n\n";
                foreach ($content['steps'] as $i => $step) {
                    $output .= "**Step " . ($i + 1) . ": {$step['title']}**\n";
                    $output .= "{$step['content']}\n";
                    $output .= "💡 Tip: {$step['tip']}\n\n";
                }
                $output .= "Type `progress` to see your tutorial progress.";
                return $output;

            case 'tutorial_progress':
                $tutorial = new TutorialSystem();
                $progress = $tutorial->getProgress($this->userId);
                return "**Tutorial Progress**\n\n" .
                       "Completed: {$progress['completed']}/{$progress['total']}\n" .
                       "Progress: {$progress['percentage']}%\n" .
                       "Remaining: {$progress['remaining']}";

            case 'log':
            case 'logs':
                $level = $command['level'] ?? 'info';
                $logs = $this->logger->getRecent($level, 20);
                $output = "**Recent Logs** ({$level})\n\n";
                foreach ($logs as $log) {
                    $output .= "- [{$log['timestamp']}] {$log['component']}: {$log['message']}\n";
                }
                return $output ?: "No logs found";

            case 'log_stats':
                $stats = $this->logger->getStats();
                return "**Log Statistics**\n\n" .
                       "Files: {$stats['total_files']}\n" .
                       "Size: {$stats['total_size_formatted']}\n" .
                       "By Level: " . json_encode($stats['by_level']);

            case 'sanitize':
                $input = $command['input'] ?? '';
                $type = $command['type'] ?? 'text';
                $clean = $this->security->sanitize($input, $type);
                return "**Sanitized**\n\nInput: {$input}\nType: {$type}\nClean: {$clean}";

            case 'check_xss':
                $input = $command['input'] ?? '';
                $detected = $this->security->detectXSS($input);
                return $detected ? "XSS detected in input" : "No XSS detected";

            case 'check_sql':
                $input = $command['input'] ?? '';
                $detected = $this->security->detectSQLInjection($input);
                return $detected ? "SQL injection detected" : "No SQL injection detected";

            case 'export_data_type':
                $type = $command['type'] ?? 'conversations';
                $format = $command['format'] ?? 'json';
                $result = $this->export->export($type, $format, ['limit' => 100]);
                if ($result['success']) {
                    $save = $this->export->saveToFile($result['content'], "export_{$type}", $format);
                    return "**Exported** {$result['count']} records to {$format}\nSaved: {$save['path']}";
                }
                return "Export failed: " . ($result['error'] ?? 'Unknown error');

            case 'export_preview':
                $type = $command['type'] ?? 'conversations';
                $format = $command['format'] ?? 'markdown';
                $result = $this->export->export($type, $format, ['limit' => 10]);
                if ($result['success']) {
                    return "**Preview** (first 10 rows)\n\n" . substr($result['content'], 0, 1500);
                }
                return "Preview failed";

            case 'export_types':
                return "**Export Types**\n\n" . implode("\n", $this->export->getSupportedTypes());

            case 'export_formats':
                return "**Export Formats**\n\n" . implode("\n", $this->export->getSupportedFormats());

            case 'profile':
                $info = Profiler::getServerInfo();
                $mem = Profiler::getMemoryUsage();
                return "**Profiler**\n\n" .
                       "PHP: {$info['php_version']}\n" .
                       "Server: {$info['server_software']}\n" .
                       "Memory Limit: {$info['memory_limit']}\n" .
                       "Current Memory: {$mem['current']}\n" .
                       "Peak Memory: {$mem['peak']}";

            case 'benchmark':
                $iterations = $command['iterations'] ?? 100;
                $result = Profiler::benchmark(function() { $x = 0; for($i=0;$i<1000;$i++) $x+=$i; }, $iterations);
                return "**Benchmark** ({$iterations} iterations)\n\n" .
                       "Avg: {$result['avg_ms']}ms\n" .
                       "Min: {$result['min_ms']}ms\n" .
                       "Max: {$result['max_ms']}ms\n" .
                       "Median: {$result['median_ms']}ms\n" .
                       "P95: {$result['p95_ms']}ms";

            case 'search_data':
                $query = $command['query'] ?? '';
                $type = $command['type'] ?? 'all';
                $results = $this->search->search($query, ['type' => $type, 'user_id' => $this->userId]);
                $output = "**Search Results** for \"{$query}\" ({$results['total']} found)\n\n";
                foreach ($results['results'] as $t => $items) {
                    if (!empty($items)) {
                        $output .= "**{$t}**: " . count($items) . " found\n";
                    }
                }
                return $output ?: "No results found";

            case 'search_stats':
                $stats = $this->search->searchStats($this->userId);
                $output = "**Data Statistics**\n\n";
                foreach ($stats as $type => $count) {
                    if ($type !== 'total') {
                        $output .= "- {$type}: {$count}\n";
                    }
                }
                $output .= "\n**Total**: {$stats['total']} records";
                return $output;

            case 'notif_prefs':
                $prefs = $this->notifPrefs->get($this->userId);
                $output = "**Notification Preferences**\n\n";
                foreach ($prefs as $key => $value) {
                    $label = str_replace(['notif_', '_'], ['', ' '], $key);
                    $output .= "- {$label}: " . ($value ? 'ON' : 'OFF') . "\n";
                }
                return $output;

            case 'set_notif':
                $key = $command['key'] ?? '';
                $value = $command['value'] ?? 1;
                $this->notifPrefs->set($this->userId, 'notif_' . $key, $value);
                return "**Notification Updated**: {$key} = " . ($value ? 'ON' : 'OFF');

            case 'notif_channels':
                $channels = $this->notifPrefs->getChannels($this->userId);
                return "**Notification Channels**\n\n" . implode("\n", array_map(function($c) { return "- {$c}"; }, $channels));

            case 'vision_start':
                return "**Vision System**\n\n" .
                       "To use computer vision:\n" .
                       "1. Start the ML server: `python ozayn/ml/server.py`\n" .
                       "2. Open the 3D interface: `/ozayn/3d`\n" .
                       "3. Click 'Voice' to enable camera\n\n" .
                       "The ML server provides:\n" .
                       "- Face detection\n" .
                       "- Object detection\n" .
                       "- Screen analysis\n" .
                       "- Gesture recognition";

            case 'analyze_screen':
                return "**Screen Analysis**\n\n" .
                       "Connect to the ML server to analyze screen content.\n" .
                       "Run: `python ozayn/ml/server.py`\n" .
                       "Then use the JavaScript client in your browser.";

            case 'gesture_start':
                return "**Gesture Control**\n\n" .
                       "Gesture recognition requires:\n" .
                       "1. Camera access\n" .
                       "2. ML server running\n" .
                       "3. WebSocket connection\n\n" .
                       "Supported gestures:\n" .
                       "- Swipe left/right\n" .
                       "- Swipe up/down\n" .
                       "- Motion detection";

            case 'open_3d':
                return "**3D Interface**\n\n" .
                       "Open the 3D spatial interface at: `/ozayn/3d`\n\n" .
                       "Features:\n" .
                       "- ARWE system visualization\n" .
                       "- Interactive node graph\n" .
                       "- Real-time status monitoring\n" .
                       "- Voice control integration";

            case 'ml_status':
                return "**ML Server Status**\n\n" .
                       "The Python ML server provides computer vision and gesture recognition.\n\n" .
                       "To start: `python ozayn/ml/server.py`\n" .
                       "Endpoint: `ws://localhost:8765`\n\n" .
                       "Capabilities:\n" .
                       "- Face detection (OpenCV)\n" .
                       "- Object detection (color analysis)\n" .
                       "- Screen analysis\n" .
                       "- Gesture recognition (motion)";

            case 'kidane_command':
                $cmd = $command['command'] ?? '';
                return "**Kidane Command Sent**\n\n" .
                       "Command: {$cmd}\n" .
                       "Status: Waiting for real API connection\n" .
                       "Configure API at: /ozayn/config";

            case 'canivox_command':
                $cmd = $command['command'] ?? '';
                return "**Canivox Command Sent**\n\n" .
                       "Command: {$cmd}\n" .
                       "Status: Waiting for real API connection\n" .
                       "Configure API at: /ozayn/config";

            case 'health':
            case 'health check':
                require_once __DIR__ . '/../../tools/health.php';
                $health = new HealthMonitor();
                $results = $health->runAllChecks();
                $output = "**System Health** ({$results['overall']})\n\n";
                foreach ($results['checks'] as $name => $check) {
                    $icon = $check['status'] === 'healthy' ? '✅' : ($check['status'] === 'warning' ? '⚠️' : '❌');
                    $output .= "{$icon} **{$name}**: {$check['message']}\n";
                }
                return $output;

            case 'alerts':
                require_once __DIR__ . '/../../tools/health.php';
                $health = new HealthMonitor();
                $alerts = $health->getAlerts();
                if (empty($alerts)) {
                    return "**No Alerts** - All systems healthy";
                }
                $output = "**Active Alerts** (" . count($alerts) . ")\n\n";
                foreach ($alerts as $alert) {
                    $icon = $alert['status'] === 'critical' ? '❌' : '⚠️';
                    $output .= "{$icon} **{$alert['check']}**: {$alert['message']}\n";
                }
                return $output;

            case 'dashboard':
                return "**ARWE Dashboard**\n\n" .
                       "Open the full dashboard at: `/ozayn/dashboard`\n\n" .
                       "Features:\n" .
                       "- Real-time system monitoring\n" .
                       "- API connection testing\n" .
                       "- Activity logs\n" .
                       "- Quick actions";

            case 'batch_example':
                require_once __DIR__ . '/../../tools/batch.php';
                $batch = new BatchOperations();
                $example = $batch->getExample();
                $types = $batch->getSupportedTypes();
                return "**Batch Operations**\n\n" .
                       "Execute multiple operations in one request.\n\n" .
                       "**Supported Types:**\n" . implode("\n", array_map(fn($t) => "- {$t}", $types)) . "\n\n" .
                       "**Example:**\n```json\n" . json_encode($example, JSON_PRETTY_PRINT) . "\n```";

            case 'api_get':
                require_once __DIR__ . '/../../tools/rest_client.php';
                $client = new RestClient();
                $result = $client->get($command['url']);
                $output = "**GET {$command['url']}**\n\n";
                $output .= "Status: {$result['http_code']}\n";
                $output .= "Duration: {$result['duration_ms']}ms\n\n";
                $output .= "```json\n" . json_encode($result['data'], JSON_PRETTY_PRINT) . "\n```";
                return $output;

            case 'api_post':
                require_once __DIR__ . '/../../tools/rest_client.php';
                $client = new RestClient();
                $data = json_decode($command['data'], true) ?? $command['data'];
                $result = $client->post($command['url'], $data);
                $output = "**POST {$command['url']}**\n\n";
                $output .= "Status: {$result['http_code']}\n";
                $output .= "Duration: {$result['duration_ms']}ms\n\n";
                $output .= "```json\n" . json_encode($result['data'], JSON_PRETTY_PRINT) . "\n```";
                return $output;

            case 'git_status':
                require_once __DIR__ . '/../../tools/git.php';
                $git = new GitIntegration();
                if (!$git->isGitRepo()) return "Not a git repository";
                $status = $git->getStatus();
                $output = "**Git Status**\n\n";
                if (!empty($status['modified'])) $output .= "Modified: " . implode(", ", $status['modified']) . "\n";
                if (!empty($status['added'])) $output .= "Added: " . implode(", ", $status['added']) . "\n";
                if (!empty($status['deleted'])) $output .= "Deleted: " . implode(", ", $status['deleted']) . "\n";
                if (!empty($status['untracked'])) $output .= "Untracked: " . implode(", ", $status['untracked']) . "\n";
                return $output ?: "Clean working tree";

            case 'git_log':
                require_once __DIR__ . '/../../tools/git.php';
                $git = new GitIntegration();
                if (!$git->isGitRepo()) return "Not a git repository";
                $limit = $command['limit'] ?? 10;
                $commits = $git->getLog($limit);
                $output = "**Git Log** (last {$limit})\n\n";
                foreach ($commits as $c) {
                    $hash = substr($c['hash'], 0, 7);
                    $output .= "`{$hash}` {$c['message']} ({$c['author']})\n";
                }
                return $output;

            case 'git_branches':
                require_once __DIR__ . '/../../tools/git.php';
                $git = new GitIntegration();
                if (!$git->isGitRepo()) return "Not a git repository";
                $branches = $git->getBranches();
                $output = "**Git Branches**\n\n";
                $output .= "Current: `{$branches['current']}`\n\n";
                $output .= "Local: " . implode(", ", $branches['local']) . "\n";
                if (!empty($branches['remote'])) $output .= "Remote: " . implode(", ", $branches['remote']) . "\n";
                return $output;

            case 'git_diff':
                require_once __DIR__ . '/../../tools/git.php';
                $git = new GitIntegration();
                if (!$git->isGitRepo()) return "Not a git repository";
                $diff = $git->diff($command['file']);
                return "**Git Diff**\n\n```\n{$diff}\n```";

            case 'git_add':
                require_once __DIR__ . '/../../tools/git.php';
                $git = new GitIntegration();
                if (!$git->isGitRepo()) return "Not a git repository";
                $result = $git->add($command['files']);
                return "Files added: {$command['files']}";

            case 'git_commit':
                require_once __DIR__ . '/../../tools/git.php';
                $git = new GitIntegration();
                if (!$git->isGitRepo()) return "Not a git repository";
                $result = $git->commit($command['message']);
                return "**Committed**: {$command['message']}";

            case 'git_push':
                require_once __DIR__ . '/../../tools/git.php';
                $git = new GitIntegration();
                if (!$git->isGitRepo()) return "Not a git repository";
                $result = $git->push();
                return "**Pushed to remote**\n\n{$result['output']}";

            case 'git_pull':
                require_once __DIR__ . '/../../tools/git.php';
                $git = new GitIntegration();
                if (!$git->isGitRepo()) return "Not a git repository";
                $result = $git->pull();
                return "**Pulled from remote**\n\n{$result['output']}";

            case 'file_diff':
                require_once __DIR__ . '/../../tools/filediff.php';
                $diff = new FileDiff();
                $result = $diff->diffFiles($command['file1'], $command['file2']);
                if (isset($result['error'])) return "Error: {$result['error']}";
                $output = "**File Diff**\n\n";
                $output .= "Same: {$result['stats']['same']} | Added: {$result['stats']['added']} | Removed: {$result['stats']['removed']} | Changed: {$result['stats']['changed']}\n\n";
                $output .= $diff->formatDiff($result);
                return $output;

            case 'file_compare':
                require_once __DIR__ . '/../../tools/filediff.php';
                $diff = new FileDiff();
                $content1 = file_get_contents($command['file1']);
                $content2 = file_get_contents($command['file2']);
                if ($content1 === false) return "Error reading: {$command['file1']}";
                if ($content2 === false) return "Error reading: {$command['file2']}";
                $result = $diff->findSimilar($content1, $content2);
                return "**File Comparison**\n\n" .
                       "Similarity: {$result['similarity']}%\n" .
                       "Common words: {$result['common_words']}\n" .
                       "Total unique words: {$result['total_words']}";

            case 'list_sessions':
                require_once __DIR__ . '/../../tools/collaboration.php';
                $collab = new CollaborationSystem();
                $sessions = $collab->getUserSessions($this->userId);
                if (empty($sessions)) return "**No Active Sessions**\n\nCreate one with: `create session [name]`";
                $output = "**Your Sessions**\n\n";
                foreach ($sessions as $s) {
                    $output .= "- **{$s['name']}** ({$s['user_count']} users) - ID: `{$s['id']}`\n";
                }
                return $output;

            case 'create_session':
                require_once __DIR__ . '/../../tools/collaboration.php';
                $collab = new CollaborationSystem();
                $id = $collab->createSession($command['name'], $this->userId);
                return "**Session Created**\n\nName: {$command['name']}\nID: `{$id}`\n\nShare this ID with others to collaborate.";

            case 'join_session':
                require_once __DIR__ . '/../../tools/collaboration.php';
                $collab = new CollaborationSystem();
                $result = $collab->joinSession($command['id'], $this->userId);
                if (isset($result['error'])) return "Error: {$result['error']}";
                $users = $collab->getSessionUsers($command['id']);
                $output = "**Joined Session**\n\nUsers online:\n";
                foreach ($users as $u) {
                    $output .= "- {$u['username']} ({$u['role']})\n";
                }
                return $output;

            case 'invite_user':
                require_once __DIR__ . '/../../tools/collaboration.php';
                $collab = new CollaborationSystem();
                return "**Invite Sent**\n\nUser @{$command['username']} has been invited to collaborate.";

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

    /**
     * Handle ARWE routes
     */
    private function handleARWE($method, $segments) {
        $action = $segments[1] ?? null;
        $subAction = $segments[2] ?? null;

        // ARWE Config endpoints
        if ($action === 'config') {
            require_once __DIR__ . '/../../tools/arwe_config.php';
            $arweConfig = new ARWEConfig();

            if ($method === 'GET' && $subAction === 'systems') {
                $this->response(['systems' => $arweConfig->getSystems()]);
            }

            if ($method === 'GET' && $subAction === 'list') {
                $this->response(['configs' => $arweConfig->getAll()]);
            }

            if ($method === 'POST' && $subAction === 'save') {
                $input = $this->getInput();
                $arweConfig->save($input['system'], $input['config']);
                $this->response(['success' => true]);
            }

            if ($method === 'POST' && $subAction === 'test') {
                $input = $this->getInput();
                $result = $arweConfig->testConnection($input['system']);
                $this->response($result);
            }

            if ($method === 'POST' && $subAction === 'delete') {
                $input = $this->getInput();
                $arweConfig->delete($input['system']);
                $this->response(['success' => true]);
            }

            if ($method === 'GET' && $subAction === 'export') {
                $this->response(['config' => $arweConfig->exportConfig()]);
            }

            if ($method === 'POST' && $subAction === 'import') {
                $input = $this->getInput();
                $arweConfig->importConfig($input['config']);
                $this->response(['success' => true]);
            }
        }

        // Get all status
        if ($method === 'GET' && ($action === 'status' || $action === null)) {
            $result = $this->arwe->getAllStatus();
            $this->response($result);
        }

        // Get daily briefing
        if ($method === 'GET' && $action === 'briefing') {
            $result = $this->arwe->getDailyBriefing();
            $this->response(['briefing' => $result]);
        }

        // Get specific system status
        if ($method === 'GET' && isset($segments[1]) && $segments[1] !== 'config') {
            $result = $this->arwe->getStatus($segments[1]);
            $this->response($result);
        }

        $this->error('Invalid ARWE action', 400);
    }

    /**
     * Handle decisions routes
     */
    private function handleDecisions($method, $segments) {
        $action = $segments[1] ?? null;

        // List decisions
        if ($method === 'GET' && ($action === 'list' || $action === null)) {
            $status = $_GET['status'] ?? null;
            $result = $this->decision->getUserDecisions($this->userId, $status);
            $this->response(['decisions' => $result]);
        }

        // Create decision
        if ($method === 'POST' && $action === 'create') {
            $input = $this->getInput();
            $id = $this->decision->createDecision(
                $this->userId,
                $input['context'] ?? '',
                $input['options'] ?? [],
                $input['project_id'] ?? null
            );
            $this->response(['success' => true, 'id' => $id], 201);
        }

        // Get decision
        if ($method === 'GET' && isset($segments[2])) {
            $result = $this->decision->getDecision($segments[2], $this->userId);
            if (!$result) $this->error('Decision not found', 404);
            $this->response($result);
        }

        // Make decision
        if ($method === 'PUT' && isset($segments[2]) && $segments[3] === 'decide') {
            $input = $this->getInput();
            $this->decision->makeDecision(
                $segments[2],
                $this->userId,
                $input['chosen_option'] ?? '',
                $input['reasoning'] ?? null
            );
            $this->response(['success' => true]);
        }

        $this->error('Invalid decision action', 400);
    }

    /**
     * Handle audit routes
     */
    private function handleAudit($method, $segments) {
        $action = $segments[1] ?? null;

        // Get user log
        if ($method === 'GET' && ($action === 'log' || $action === null)) {
            $limit = $_GET['limit'] ?? 100;
            $result = $this->audit->getUserLog($this->userId, $limit);
            $this->response(['log' => $result]);
        }

        // Get recent actions
        if ($method === 'GET' && $action === 'recent') {
            $limit = $_GET['limit'] ?? 50;
            $result = $this->audit->getRecentActions($limit);
            $this->response(['actions' => $result]);
        }

        // Search audit log
        if ($method === 'GET' && $action === 'search') {
            $query = $_GET['q'] ?? '';
            $result = $this->audit->search($query, $this->userId);
            $this->response(['results' => $result]);
        }

        $this->error('Invalid audit action', 400);
    }

    /**
     * Handle agents routes
     */
    private function handleAgents($method, $segments) {
        $action = $segments[1] ?? null;

        // List agents
        if ($method === 'GET' && ($action === 'list' || $action === null)) {
            $result = $this->agents->getAvailableAgents();
            $this->response(['agents' => $result]);
        }

        // Route task
        if ($method === 'POST' && $action === 'route') {
            $input = $this->getInput();
            $result = $this->agents->routeTask(
                $input['task'] ?? '',
                $input['context'] ?? []
            );
            $this->response($result);
        }

        $this->error('Invalid agent action', 400);
    }

    /**
     * Handle apps routes
     */
    private function handleApps($method, $segments) {
        $action = $segments[1] ?? null;

        // List apps
        if ($method === 'GET' && ($action === 'list' || $action === null)) {
            $result = $this->apps->listApps();
            $this->response(['apps' => $result]);
        }

        // Launch app
        if ($method === 'POST' && $action === 'launch') {
            $input = $this->getInput();
            $result = $this->apps->launch(
                $input['app'] ?? '',
                $input['args'] ?? []
            );
            $this->response($result);
        }

        // Open file
        if ($method === 'POST' && $action === 'open-file') {
            $input = $this->getInput();
            $result = $this->apps->openFile($input['path'] ?? '');
            $this->response($result);
        }

        // Open URL
        if ($method === 'POST' && $action === 'open-url') {
            $input = $this->getInput();
            $result = $this->apps->openURL($input['url'] ?? '');
            $this->response($result);
        }

        $this->error('Invalid app action', 400);
    }

    /**
     * Handle code routes
     */
    private function handleCode($method, $segments) {
        $action = $segments[1] ?? null;

        // Analyze code
        if ($method === 'POST' && $action === 'analyze') {
            $input = $this->getInput();
            $result = $this->code->analyzeCode(
                $input['code'] ?? '',
                $input['language'] ?? null
            );
            $this->response($result);
        }

        // Generate skeleton
        if ($method === 'POST' && $action === 'generate') {
            $input = $this->getInput();
            $result = $this->code->generateSkeleton(
                $input['type'] ?? 'function',
                $input['name'] ?? 'MyFunction',
                $input['language'] ?? 'php'
            );
            $this->response(['code' => $result]);
        }

        $this->error('Invalid code action', 400);
    }

    /**
     * Handle batch operations
     */
    private function handleBatch($method, $segments) {
        if ($method === 'POST') {
            $input = $this->getInput();
            $operations = $input['operations'] ?? [];
            if (empty($operations)) {
                $this->error('No operations provided', 400);
            }
            require_once __DIR__ . '/../../tools/batch.php';
            $batch = new BatchOperations();
            $results = [];
            foreach ($operations as $op) {
                $action = $op['action'] ?? '';
                try {
                    switch ($action) {
                        case 'get_memories':
                            $results[] = ['action' => $action, 'result' => $this->memory->getByUser($this->userId)];
                            break;
                        case 'get_stats':
                            $results[] = ['action' => $action, 'result' => $this->memory->getStats($this->userId)];
                            break;
                        case 'get_projects':
                            $results[] = ['action' => $action, 'result' => $this->projects->getByUser($this->userId)];
                            break;
                        case 'get_knowledge':
                            $results[] = ['action' => $action, 'result' => $this->knowledge->getByUser($this->userId)];
                            break;
                        default:
                            $results[] = ['action' => $action, 'error' => 'Unknown action'];
                    }
                } catch (Exception $e) {
                    $results[] = ['action' => $action, 'error' => $e->getMessage()];
                }
            }
            $this->response($results);
        }
        $this->error('Method not allowed', 405);
    }

    /**
     * Handle REST client requests
     */
    private function handleRest($method, $segments) {
        $action = $segments[1] ?? null;
        if ($method === 'POST' && $action === 'request') {
            $input = $this->getInput();
            $url = $input['url'] ?? '';
            $reqMethod = $input['method'] ?? 'GET';
            $headers = $input['headers'] ?? [];
            $body = $input['body'] ?? null;
            $timeout = $input['timeout'] ?? 30;

            $ch = curl_init();
            curl_setopt($ch, CURLOPT_URL, $url);
            curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
            curl_setopt($ch, CURLOPT_TIMEOUT, $timeout);
            curl_setopt($ch, CURLOPT_FOLLOWLOCATION, true);
            curl_setopt($ch, CURLOPT_CUSTOMREQUEST, strtoupper($reqMethod));

            if (!empty($headers)) {
                $curlHeaders = [];
                foreach ($headers as $k => $v) {
                    $curlHeaders[] = "$k: $v";
                }
                curl_setopt($ch, CURLOPT_HTTPHEADER, $curlHeaders);
            }

            if ($body && in_array(strtoupper($reqMethod), ['POST', 'PUT', 'PATCH'])) {
                curl_setopt($ch, CURLOPT_POSTFIELDS, is_string($body) ? $body : json_encode($body));
            }

            $response = curl_exec($ch);
            $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
            $error = curl_error($ch);
            $info = curl_getinfo($ch);
            curl_close($ch);

            if ($error) {
                $this->response(['error' => $error, 'url' => $url], 500);
            }

            $this->response([
                'status' => $httpCode,
                'headers' => $info['response_headers'] ?? [],
                'body' => $response,
                'time' => $info['total_time']
            ]);
        }
        $this->error('Invalid REST action', 400);
    }

    /**
     * Handle collaboration routes
     */
    private function handleCollab($method, $segments) {
        $action = $segments[1] ?? null;
        require_once __DIR__ . '/../../tools/collaboration.php';
        $collab = new CollaborationSystem();

        if ($method === 'GET' && $action === 'sessions') {
            $sessions = $collab->getUserSessions($this->userId);
            $this->response(['sessions' => $sessions]);
        }

        if ($method === 'POST' && $action === 'create') {
            $input = $this->getInput();
            $id = $collab->createSession($input['name'] ?? 'Untitled', $this->userId);
            $this->response(['id' => $id, 'name' => $input['name'] ?? 'Untitled']);
        }

        if ($method === 'POST' && $action === 'join') {
            $input = $this->getInput();
            $result = $collab->joinSession($input['session_id'] ?? '', $this->userId);
            $this->response($result);
        }

        if ($method === 'GET' && $action === 'users') {
            $sessionId = $segments[2] ?? '';
            $users = $collab->getSessionUsers($sessionId);
            $this->response(['users' => $users]);
        }

        if ($method === 'POST' && $action === 'cursor') {
            $input = $this->getInput();
            $collab->updateCursor($input['session_id'] ?? '', $this->userId, $input['x'] ?? 0, $input['y'] ?? 0, $input['file'] ?? '');
            $this->response(['success' => true]);
        }

        if ($method === 'POST' && $action === 'event') {
            $input = $this->getInput();
            $collab->addEvent($input['session_id'] ?? '', $this->userId, $input['type'] ?? 'message', $input['data'] ?? '');
            $this->response(['success' => true]);
        }

        $this->error('Invalid collab action', 400);
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
