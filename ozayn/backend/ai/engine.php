<?php
/**
 * Ozayn AI Engine
 * Supports OpenAI-compatible APIs (OpenAI, Anthropic, Ollama, LM Studio, etc.)
 */

require_once __DIR__ . '/../config/config.php';
require_once __DIR__ . '/../memory/memory.php';
require_once __DIR__ . '/../knowledge/knowledge.php';

class AI {
    private $memory;
    private $knowledge;
    private $apiKey;
    private $baseUrl;
    private $model;

    public function __construct() {
        $this->memory = new Memory();
        $this->knowledge = new Knowledge();
        $this->loadConfig();
    }

    private function loadConfig() {
        $configPath = __DIR__ . '/../config/ai.json';
        if (file_exists($configPath)) {
            $config = json_decode(file_get_contents($configPath), true);
            $this->apiKey = $config['api_key'] ?? '';
            $this->baseUrl = $config['base_url'] ?? 'https://api.openai.com/v1';
            $this->model = $config['model'] ?? 'gpt-3.5-turbo';
        } else {
            // Default to local/demo mode
            $this->apiKey = '';
            $this->baseUrl = '';
            $this->model = 'demo';
        }
    }

    /**
     * Build system prompt with context
     */
    private function buildSystemPrompt($userId, $projectId = null) {
        $prompt = "You are Ozayn, an intelligent personal AI assistant and digital twin. ";
        $prompt .= "You are part of the ARWE ecosystem, built to assist with projects, knowledge, tasks, and decisions. ";
        $prompt .= "You have access to memory, knowledge base, and project management tools. ";
        $prompt .= "Be helpful, concise, and intelligent. Use markdown formatting when appropriate. ";
        $prompt .= "You can use tools to search knowledge, manage tasks, and interact with the system. ";

        // Add user preferences
        $prefs = $this->memory->getPreference($userId, 'response_style');
        if ($prefs) {
            $prompt .= "The user prefers responses that are: {$prefs}. ";
        }

        $name = $this->memory->getPreference($userId, 'display_name');
        if ($name) {
            $prompt .= "The user's name is {$name}. ";
        }

        // Add project context if available
        if ($projectId) {
            $project = $this->getProjectContext($userId, $projectId);
            if ($project) {
                $prompt .= "\nCurrent project: {$project['name']}\n";
                if (!empty($project['description'])) {
                    $prompt .= "Description: {$project['description']}\n";
                }
                if (!empty($project['tasks'])) {
                    $prompt .= "Active tasks: " . implode(', ', array_slice($project['tasks'], 0, 5)) . "\n";
                }
            }
        }

        return $prompt;
    }

    /**
     * Get project context
     */
    private function getProjectContext($userId, $projectId) {
        $db = $this->getDB();
        $stmt = $db->prepare("SELECT * FROM projects WHERE id = ? AND user_id = ?");
        $stmt->execute([$projectId, $userId]);
        $project = $stmt->fetch(PDO::FETCH_ASSOC);
        
        if (!$project) return null;

        // Get active tasks
        $stmt = $db->prepare("SELECT title FROM tasks WHERE project_id = ? AND status != 'completed' ORDER BY priority DESC LIMIT 5");
        $stmt->execute([$projectId]);
        $tasks = $stmt->fetchAll(PDO::FETCH_COLUMN);

        $project['tasks'] = $tasks;
        return $project;
    }

    /**
     * Get database connection
     */
    private function getDB() {
        $dbPath = __DIR__ . '/../database/ozayn.db';
        return new PDO('sqlite:' . $dbPath);
    }

    /**
     * Get relevant context from knowledge base
     */
    private function getKnowledgeContext($userId, $message, $projectId = null) {
        $context = $this->knowledge->getRelevantContext($userId, $message, $projectId);
        $contextStr = '';

        if (!empty($context['knowledge'])) {
            $contextStr .= "Relevant knowledge:\n";
            foreach ($context['knowledge'] as $entry) {
                $contextStr .= "- {$entry['title']}: " . substr($entry['content'], 0, 200) . "\n";
            }
        }

        if (!empty($context['project_knowledge'])) {
            $contextStr .= "\nProject-specific knowledge:\n";
            foreach ($context['project_knowledge'] as $entry) {
                $contextStr .= "- {$entry['title']}: " . substr($entry['content'], 0, 200) . "\n";
            }
        }

        return $contextStr;
    }

    /**
     * Get conversation history in OpenAI format
     */
    private function formatHistory($history) {
        $messages = [];
        foreach ($history as $msg) {
            $messages[] = [
                'role' => $msg['role'],
                'content' => $msg['content']
            ];
        }
        return $messages;
    }

    /**
     * Generate response using AI
     */
    public function generateResponse($userId, $message, $conversationId = null, $projectId = null) {
        $systemPrompt = $this->buildSystemPrompt($userId, $projectId);
        $knowledgeContext = $this->getKnowledgeContext($userId, $message, $projectId);

        // Get conversation history
        $history = [];
        if ($conversationId) {
            $db = \Database::getInstance();
            $history = $db->fetchAll(
                "SELECT role, content FROM messages WHERE conversation_id = ? ORDER BY created_at ASC",
                [$conversationId]
            );
        }

        // Build messages array
        $messages = [['role' => 'system', 'content' => $systemPrompt]];

        if ($knowledgeContext) {
            $messages[] = ['role' => 'system', 'content' => $knowledgeContext];
        }

        $messages = array_merge($messages, $this->formatHistory($history));
        $messages[] = ['role' => 'user', 'content' => $message];

        // Try AI API, fallback to local processing
        if ($this->apiKey && $this->baseUrl) {
            return $this->callAPI($messages);
        }

        return $this->localProcess($message, $knowledgeContext, $userId);
    }

    /**
     * Call OpenAI-compatible API
     */
    private function callAPI($messages) {
        $payload = json_encode([
            'model' => $this->model,
            'messages' => $messages,
            'max_tokens' => MAX_TOKENS,
            'temperature' => TEMPERATURE
        ]);

        $ch = curl_init("{$this->baseUrl}/chat/completions");
        curl_setopt_array($ch, [
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_POST => true,
            CURLOPT_POSTFIELDS => $payload,
            CURLOPT_HTTPHEADER => [
                'Content-Type: application/json',
                "Authorization: Bearer {$this->apiKey}"
            ],
            CURLOPT_TIMEOUT => 60
        ]);

        $response = curl_exec($ch);
        $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
        curl_close($ch);

        if ($httpCode !== 200) {
            error_log("AI API error: {$httpCode} - {$response}");
            return $this->fallbackResponse($messages[count($messages)-1]['content'] ?? '');
        }

        $data = json_decode($response, true);
        return $data['choices'][0]['message']['content'] ?? 'No response generated.';
    }

    /**
     * Local processing when no API is configured
     */
    private function localProcess($message, $context, $userId) {
        $lower = strtolower($message);

        // Greeting
        if (preg_match('/^(hi|hello|hey|good morning|good afternoon|good evening)/', $lower)) {
            return "Hello! I'm Ozayn, your personal AI assistant. How can I help you today?";
        }

        // Help
        if (preg_match('/\b(help|what can you do|features|commands)\b/', $lower)) {
            return $this->getHelpText();
        }

        // Status
        if (preg_match('/\b(status|how are you|how\'s it going)\b/', $lower)) {
            return "I'm running and ready to assist. I have access to:\n\n" .
                   "- **Memory** - I remember our conversations\n" .
                   "- **Knowledge Base** - I can search your stored knowledge\n" .
                   "- **Projects** - I can help manage your projects\n" .
                   "- **Tasks** - I can track and organize tasks\n\n" .
                   "What would you like to work on?";
        }

        // Time
        if (preg_match('/\b(time|date|today|what time)\b/', $lower)) {
            return "Current time: " . date('Y-m-d H:i:s');
        }

        // Knowledge query
        if ($context) {
            return "Based on your knowledge base:\n\n{$context}\n\nHow can I help you with this?";
        }

        // Default
        return "I received your message: \"{$message}\"\n\n" .
               "To enable full AI responses, configure an API key in `ozayn/backend/config/ai.json`.\n\n" .
               "You can say **help** to see what I can do.";
    }

    /**
     * Get help text
     */
    private function getHelpText() {
        return "**Ozayn - Available Commands**\n\n" .
               "**General:**\n" .
               "- `help` - Show this help\n" .
               "- `status` - Check system status\n" .
               "- `time` - Get current time\n\n" .
               "**Projects:**\n" .
               "- `create project [name]` - Create a new project\n" .
               "- `list projects` - Show all projects\n" .
               "- `select project [id]` - Switch to a project\n\n" .
               "**Tasks:**\n" .
               "- `add task [title]` - Create a new task\n" .
               "- `list tasks` - Show pending tasks\n" .
               "- `complete task [id]` - Mark task as done\n\n" .
               "**Knowledge:**\n" .
               "- `search [query]` - Search knowledge base\n" .
               "- `add knowledge [title]` - Add knowledge entry\n\n" .
               "**Memory:**\n" .
               "- `remember [key]: [value]` - Store information\n" .
               "- `recall [key]` - Retrieve information\n\n" .
               "**System Monitoring:**\n" .
               "- `system` or `sysinfo` - Full system overview\n" .
               "- `cpu` - CPU information and usage\n" .
               "- `memory` or `mem` - Memory usage\n" .
               "- `disk` - Disk usage\n" .
               "- `processes` or `ps` - Running processes\n" .
               "- `top cpu` - Top processes by CPU\n" .
               "- `top memory` - Top processes by memory\n" .
               "- `network` - Network interfaces\n" .
               "- `uptime` - System uptime\n" .
               "- `temperature` or `temp` - System temperature\n\n" .
               "**ARWE Integration:**\n" .
               "- `arwe` - Check all ARWE systems\n" .
               "- `edunex` - Check Edunex\n" .
               "- `govyx` - Check Govyx\n" .
               "- `locify` - Check Locify\n" .
               "- `terrachain` - Check TerraChain\n" .
               "- `bilen` - Check Bilen\n" .
               "- `kidane` - Check Kidane fleet\n" .
               "- `canivox` - Check Canivox fleet\n\n" .
               "**Decision Support:**\n" .
               "- `decide [context]` - Create a decision\n" .
               "- `decisions` - List your decisions\n\n" .
               "**File Operations:**\n" .
               "- `ls [path]` - List files\n" .
               "- `read [path]` - Read file contents\n" .
               "- `write [path] [content]` - Write to file\n" .
               "- `mkdir [path]` - Create directory\n" .
               "- `delete [path]` - Delete file/directory\n" .
               "- `find [pattern]` - Search files\n" .
               "- `pwd` - Current directory\n\n" .
               "**Applications:**\n" .
               "- `apps` - List available apps\n" .
               "- `open [app]` - Launch application\n\n" .
                "**Code Assistant:**\n" .
                "- `analyze code` - Analyze code\n" .
                "- `generate [function|class|api] [name]` - Generate code\n\n" .
                "**ARWE Systems:**\n" .
                "- `arwe` - All systems overview\n" .
                "- `edunex` - Education platform\n" .
                "- `govyx` - Government services\n" .
                "- `locify` - Digital identity\n" .
                "- `terrachain` - Land transparency\n" .
                "- `bilen` - Security intelligence\n" .
                "- `kidane` - Aerial robotics fleet\n" .
                "- `canivox` - Ground robotics fleet\n" .
                "- `summary` - All systems summary\n\n" .
                "**Web Search:**\n" .
                "- `search [query]` - Search the web\n" .
                "- `fetch [url]` - Fetch URL content\n\n" .
                "**Notifications:**\n" .
                "- `notifications` - View notifications\n" .
                "- `mark read` - Mark all as read\n\n" .
                "**Workflows:**\n" .
                "- `workflows` - List your workflows\n" .
                "- `run workflow [name]` - Run a workflow\n\n" .
                "**Scheduler:**\n" .
                "- `scheduled` - List scheduled tasks\n" .
                "- `schedule [command] [frequency]` - Create schedule\n\n" .
                "**Backup:**\n" .
                "- `backup` - Create backup\n" .
                "- `backups` - List backups\n\n" .
                "**Data:**\n" .
                "- `export [type]` - Export data\n" .
                "- `import` - Import data";
    }

    /**
     * Fallback response
     */
    private function fallbackResponse($message) {
        return "I'm having trouble connecting to my AI service right now. " .
               "Please check the configuration and try again.\n\n" .
               "You said: \"{$message}\"";
    }

    /**
     * Process tool commands
     */
    public function processCommand($userId, $message, $conversationId, $projectId) {
        $lower = strtolower(trim($message));

        // Parse commands
        if (preg_match('/^create project\s+(.+)/i', $message, $matches)) {
            return ['action' => 'create_project', 'name' => trim($matches[1])];
        }

        if (preg_match('/^add task\s+(.+)/i', $message, $matches)) {
            return ['action' => 'create_task', 'title' => trim($matches[1])];
        }

        if (preg_match('/^remember\s+(.+?):\s*(.+)/i', $message, $matches)) {
            return ['action' => 'store_memory', 'key' => trim($matches[1]), 'value' => trim($matches[2])];
        }

        if (preg_match('/^recall\s+(.+)/i', $message, $matches)) {
            $key = trim($matches[1]);
            $result = $this->memory->retrieve($userId, $key);
            return ['action' => 'recall', 'key' => $key, 'value' => $result['value'] ?? null];
        }

        if (preg_match('/^search\s+(.+)/i', $message, $matches)) {
            $query = trim($matches[1]);
            $results = $this->knowledge->search($userId, $query, 5);
            return ['action' => 'search', 'query' => $query, 'results' => $results];
        }

        // System commands
        if (preg_match('/^(system|sysinfo|system info)$/i', $lower)) {
            return ['action' => 'system_overview'];
        }

        if (preg_match('/^(cpu|cpu info)$/i', $lower)) {
            return ['action' => 'cpu_info'];
        }

        if (preg_match('/^(memory|mem|ram|memory info)$/i', $lower)) {
            return ['action' => 'memory_info'];
        }

        if (preg_match('/^(disk|disks|disk info)$/i', $lower)) {
            return ['action' => 'disk_info'];
        }

        if (preg_match('/^(processes|ps|running processes)$/i', $lower)) {
            return ['action' => 'processes'];
        }

        if (preg_match('/^top\s+cpu$/i', $lower)) {
            return ['action' => 'top_cpu'];
        }

        if (preg_match('/^top\s+(memory|mem)$/i', $lower)) {
            return ['action' => 'top_memory'];
        }

        if (preg_match('/^(network|net|interfaces)$/i', $lower)) {
            return ['action' => 'network_info'];
        }

        if (preg_match('/^(uptime|up time)$/i', $lower)) {
            return ['action' => 'uptime'];
        }

        if (preg_match('/^(temperature|temp|temps)$/i', $lower)) {
            return ['action' => 'temperature'];
        }

        // ARWE commands
        if (preg_match('/^(arwe|arwe status|arwe overview)$/i', $lower)) {
            return ['action' => 'arwe_status'];
        }

        if (preg_match('/^(edunex|edunex status)$/i', $lower)) {
            return ['action' => 'arwe_system', 'system' => 'edunex'];
        }

        if (preg_match('/^(govyx|govyx status)$/i', $lower)) {
            return ['action' => 'arwe_system', 'system' => 'govyx'];
        }

        if (preg_match('/^(locify|locify status)$/i', $lower)) {
            return ['action' => 'arwe_system', 'system' => 'locify'];
        }

        if (preg_match('/^(terrachain|terrachain status)$/i', $lower)) {
            return ['action' => 'arwe_system', 'system' => 'terrachain'];
        }

        if (preg_match('/^(bilen|bilen status)$/i', $lower)) {
            return ['action' => 'arwe_system', 'system' => 'bilen'];
        }

        if (preg_match('/^(kidane|kidane status|kidane fleet)$/i', $lower)) {
            return ['action' => 'kidane_status'];
        }

        if (preg_match('/^(canivox|canivox status|canivox fleet)$/i', $lower)) {
            return ['action' => 'canivox_status'];
        }

        if (preg_match('/^(edunex details|edunex info)$/i', $lower)) {
            return ['action' => 'edunex_details'];
        }

        if (preg_match('/^(govyx details|govyx info)$/i', $lower)) {
            return ['action' => 'govyx_details'];
        }

        if (preg_match('/^(locify details|locify info)$/i', $lower)) {
            return ['action' => 'locify_details'];
        }

        if (preg_match('/^(terrachain details|terrachain info)$/i', $lower)) {
            return ['action' => 'terrachain_details'];
        }

        if (preg_match('/^(bilen details|bilen info)$/i', $lower)) {
            return ['action' => 'bilen_details'];
        }

        if (preg_match('/^(summary|arwe summary|all systems)$/i', $lower)) {
            return ['action' => 'arwe_summary'];
        }

        // Decision commands
        if (preg_match('/^decide\s+(.+)/i', $message, $matches)) {
            return ['action' => 'create_decision', 'context' => trim($matches[1])];
        }

        if (preg_match('/^(decisions|my decisions)$/i', $lower)) {
            return ['action' => 'list_decisions'];
        }

        if (preg_match('/^add options\s+(\d+)\s+(.+)/i', $message, $matches)) {
            $options = array_map('trim', explode(',', $matches[2]));
            return ['action' => 'add_options', 'decision_id' => (int)$matches[1], 'options' => $options];
        }

        if (preg_match('/^(stats|decision stats|my stats)$/i', $lower)) {
            return ['action' => 'decision_stats'];
        }

        if (preg_match('/^compare\s+(.+)/i', $message, $matches)) {
            $ids = array_map('intval', explode(',', $matches[1]));
            return ['action' => 'compare_decisions', 'ids' => $ids];
        }

        // Agent commands
        if (preg_match('/^(agents|available agents)$/i', $lower)) {
            return ['action' => 'list_agents'];
        }

        // App launcher commands
        if (preg_match('/^open\s+(.+)/i', $message, $matches)) {
            return ['action' => 'open_app', 'app' => trim($matches[1])];
        }

        if (preg_match('/^(apps|applications|launchers)$/i', $lower)) {
            return ['action' => 'list_apps'];
        }

        // Code commands
        if (preg_match('/^analyze\s+code$/i', $lower)) {
            return ['action' => 'analyze_code'];
        }

        if (preg_match('/^generate\s+(function|class|api)\s+(.+)/i', $message, $matches)) {
            return ['action' => 'generate_code', 'type' => $matches[1], 'name' => $matches[2]];
        }

        // File commands
        if (preg_match('/^ls\s*(.*)/i', $message, $matches)) {
            $path = trim($matches[1]) ?: '.';
            return ['action' => 'list_files', 'path' => $path];
        }

        if (preg_match('/^pwd$/i', $lower)) {
            return ['action' => 'current_dir'];
        }

        if (preg_match('/^read\s+(.+)/i', $message, $matches)) {
            return ['action' => 'read_file', 'path' => trim($matches[1])];
        }

        if (preg_match('/^mkdir\s+(.+)/i', $message, $matches)) {
            return ['action' => 'create_dir', 'path' => trim($matches[1])];
        }

        if (preg_match('/^(find|search files?)\s+(.+)/i', $message, $matches)) {
            return ['action' => 'find_files', 'path' => '.', 'pattern' => trim($matches[2])];
        }

        // Web search commands
        if (preg_match('/^(search|google|web)\s+(.+)/i', $message, $matches)) {
            return ['action' => 'web_search', 'query' => trim($matches[2])];
        }

        if (preg_match('/^(fetch|get url|open url)\s+(.+)/i', $message, $matches)) {
            return ['action' => 'fetch_url', 'url' => trim($matches[2])];
        }

        // Notification commands
        if (preg_match('/^(notifications|unread)$/i', $lower)) {
            return ['action' => 'list_notifications'];
        }

        if (preg_match('/^(mark read|read all)$/i', $lower)) {
            return ['action' => 'mark_notifications_read'];
        }

        // Workflow commands
        if (preg_match('/^(workflows|my workflows)$/i', $lower)) {
            return ['action' => 'list_workflows'];
        }

        if (preg_match('/^(run workflow)\s+(.+)/i', $message, $matches)) {
            return ['action' => 'run_workflow', 'name' => trim($matches[1])];
        }

        // Export/Import commands
        if (preg_match('/^export\s+(all|profile|projects|tasks|knowledge|decisions|conversations)$/i', $message, $matches)) {
            return ['action' => 'export_data', 'type' => strtolower($matches[1])];
        }

        if (preg_match('/^(import|upload data)$/i', $lower)) {
            return ['action' => 'import_data'];
        }

        // Scheduler commands
        if (preg_match('/^(scheduled|schedule|cron|tasks scheduled)$/i', $lower)) {
            return ['action' => 'list_scheduled'];
        }

        if (preg_match('/^schedule\s+(.+)\s+(hourly|daily|weekly|monthly)$/i', $message, $matches)) {
            return ['action' => 'create_schedule', 'command' => trim($matches[1]), 'schedule' => $matches[2]];
        }

        // Backup commands
        if (preg_match('/^(backup|create backup)$/i', $lower)) {
            return ['action' => 'create_backup'];
        }

        if (preg_match('/^(backups|list backups)$/i', $lower)) {
            return ['action' => 'list_backups'];
        }

        // Analytics commands
        if (preg_match('/^(stats|statistics|my stats)$/i', $lower)) {
            return ['action' => 'get_stats'];
        }

        if (preg_match('/^(activity|recent activity)$/i', $lower)) {
            return ['action' => 'get_activity'];
        }

        // Plugin commands
        if (preg_match('/^(plugins|list plugins)$/i', $lower)) {
            return ['action' => 'list_plugins'];
        }

        if (preg_match('/^install plugin\s+(.+)/i', $message, $matches)) {
            return ['action' => 'install_plugin', 'name' => trim($matches[1])];
        }

        if (preg_match('/^enable plugin\s+(.+)/i', $message, $matches)) {
            return ['action' => 'toggle_plugin', 'name' => trim($matches[1])];
        }

        // Tutorial commands
        if (preg_match('/^(tutorial|tutorials|learn|onboarding)$/i', $lower)) {
            return ['action' => 'list_tutorials'];
        }

        if (preg_match('/^start tutorial\s+(.+)/i', $message, $matches)) {
            return ['action' => 'start_tutorial', 'id' => trim($matches[1])];
        }

        if (preg_match('/^(progress|my progress)$/i', $lower)) {
            return ['action' => 'tutorial_progress'];
        }

        // Profiler commands
        if (preg_match('/^(profile|profiler)$/i', $lower)) {
            return ['action' => 'profile'];
        }

        if (preg_match('/^benchmark\s*(\d+)?$/i', $message, $matches)) {
            return ['action' => 'benchmark', 'iterations' => (int)($matches[1] ?? 100)];
        }

        // Search commands
        if (preg_match('/^searchdata\s+(.+?)(?:\s+(conversations|messages|tasks|knowledge|memory|decisions))?$/i', $message, $matches)) {
            return ['action' => 'search_data', 'query' => trim($matches[1]), 'type' => $matches[2] ?? 'all'];
        }

        if (preg_match('/^(searchstats|data stats)$/i', $lower)) {
            return ['action' => 'search_stats'];
        }

        // Security commands
        if (preg_match('/^sanitize\s+(.+?)(?:\s+(text|email|url|int|float|filename|username))?$/i', $message, $matches)) {
            return ['action' => 'sanitize', 'input' => trim($matches[1]), 'type' => $matches[2] ?? 'text'];
        }

        if (preg_match('/^checkxss\s+(.+)/i', $message, $matches)) {
            return ['action' => 'check_xss', 'input' => trim($matches[1])];
        }

        if (preg_match('/^checksql\s+(.+)/i', $message, $matches)) {
            return ['action' => 'check_sql', 'input' => trim($matches[1])];
        }

        // Logging commands
        if (preg_match('/^logs?\s*(debug|info|warning|error)?$/i', $message, $matches)) {
            return ['action' => 'logs', 'level' => $matches[1] ?? 'info'];
        }

        if (preg_match('/^(logstats|log stats)$/i', $lower)) {
            return ['action' => 'log_stats'];
        }

        // Export commands
        if (preg_match('/^export\s+(conversations|messages|tasks|decisions|projects|audit|knowledge)\s*(json|csv|markdown|xml)?$/i', $message, $matches)) {
            return ['action' => 'export_data_type', 'type' => $matches[1], 'format' => $matches[2] ?? 'json'];
        }

        if (preg_match('/^exportpreview\s+(conversations|messages|tasks|decisions|projects|audit|knowledge)\s*(json|csv|markdown|xml)?$/i', $message, $matches)) {
            return ['action' => 'export_preview', 'type' => $matches[1], 'format' => $matches[2] ?? 'markdown'];
        }

        if (preg_match('/^(exporttypes|export types)$/i', $lower)) {
            return ['action' => 'export_types'];
        }

        if (preg_match('/^(exportformats|export formats)$/i', $lower)) {
            return ['action' => 'export_formats'];
        }

        // Notification preference commands
        if (preg_match('/^(notifprefs|notification prefs|notification settings)$/i', $lower)) {
            return ['action' => 'notif_prefs'];
        }

        if (preg_match('/^setnotif\s+(\w+)\s+(on|off|1|0)$/i', $message, $matches)) {
            $key = $matches[1];
            $value = in_array(strtolower($matches[2]), ['on', '1']) ? 1 : 0;
            return ['action' => 'set_notif', 'key' => $key, 'value' => $value];
        }

        if (preg_match('/^(notifchannels|notification channels)$/i', $lower)) {
            return ['action' => 'notif_channels'];
        }

        // Vision commands
        if (preg_match('/^(vision|camera|see|detect faces?)$/i', $lower)) {
            return ['action' => 'vision_start'];
        }

        if (preg_match('/^(analyze screen|screen analysis|what.+on screen)$/i', $lower)) {
            return ['action' => 'analyze_screen'];
        }

        // Gesture commands
        if (preg_match('/^(gesture|gestures|hand tracking)$/i', $lower)) {
            return ['action' => 'gesture_start'];
        }

        // 3D commands
        if (preg_match('/^(3d|3d view|spatial|三维)$/i', $lower)) {
            return ['action' => 'open_3d'];
        }

        // ML server commands
        if (preg_match('/^(ml server|ml status|ml status)$/i', $lower)) {
            return ['action' => 'ml_status'];
        }

        // Robotics commands
        if (preg_match('/^(kidane status|kidane info|kidane battery)$/i', $lower)) {
            return ['action' => 'kidane_status'];
        }

        if (preg_match('/^(canivox status|canivox info|canivox battery)$/i', $lower)) {
            return ['action' => 'canivox_status'];
        }

        if (preg_match('/^(kidane command|kidane go)\s+(.+)/i', $message, $matches)) {
            return ['action' => 'kidane_command', 'command' => trim($matches[1])];
        }

        if (preg_match('/^(canivox command|canivox go)\s+(.+)/i', $message, $matches)) {
            return ['action' => 'canivox_command', 'command' => trim($matches[1])];
        }

        // Health commands
        if (preg_match('/^(health|health check|system health)$/i', $lower)) {
            return ['action' => 'health'];
        }

        if (preg_match('/^(alerts|system alerts|warnings)$/i', $lower)) {
            return ['action' => 'alerts'];
        }

        if (preg_match('/^(dashboard|arwe dashboard)$/i', $lower)) {
            return ['action' => 'dashboard'];
        }

        // Batch operation commands
        if (preg_match('/^(batch|batch ops|batch example)$/i', $lower)) {
            return ['action' => 'batch_example'];
        }

        // API commands
        if (preg_match('/^api get\s+(.+)/i', $message, $matches)) {
            return ['action' => 'api_get', 'url' => trim($matches[1])];
        }

        if (preg_match('/^api post\s+(.+?)\s+(\{.+\})/i', $message, $matches)) {
            return ['action' => 'api_post', 'url' => trim($matches[1]), 'data' => trim($matches[2])];
        }

        // Git commands
        if (preg_match('/^(git status|git st)$/i', $lower)) {
            return ['action' => 'git_status'];
        }

        if (preg_match('/^(git log|git history)(?:\s+(\d+))?$/i', $message, $matches)) {
            return ['action' => 'git_log', 'limit' => (int)($matches[2] ?? 10)];
        }

        if (preg_match('/^(git branches|git branch)$/i', $lower)) {
            return ['action' => 'git_branches'];
        }

        if (preg_match('/^git diff(?:\s+(.+))?$/i', $message, $matches)) {
            return ['action' => 'git_diff', 'file' => $matches[1] ?? null];
        }

        if (preg_match('/^git add\s+(.+)/i', $message, $matches)) {
            return ['action' => 'git_add', 'files' => trim($matches[1])];
        }

        if (preg_match('/^git commit\s+(.+)/i', $message, $matches)) {
            return ['action' => 'git_commit', 'message' => trim($matches[1])];
        }

        if (preg_match('/^(git push|git sync)$/i', $lower)) {
            return ['action' => 'git_push'];
        }

        if (preg_match('/^(git pull|git fetch)$/i', $lower)) {
            return ['action' => 'git_pull'];
        }

        // File diff commands
        if (preg_match('/^diff\s+(.+?)\s+(.+)/i', $message, $matches)) {
            return ['action' => 'file_diff', 'file1' => trim($matches[1]), 'file2' => trim($matches[2])];
        }

        if (preg_match('/^(compare|similarity)\s+(.+?)\s+(.+)/i', $message, $matches)) {
            return ['action' => 'file_compare', 'file1' => trim($matches[1]), 'file2' => trim($matches[2])];
        }

        return null;
    }
}
