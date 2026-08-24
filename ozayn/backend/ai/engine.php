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

        return $prompt;
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
               "- `generate [function|class|api] [name]` - Generate code";
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

        // Decision commands
        if (preg_match('/^decide\s+(.+)/i', $message, $matches)) {
            return ['action' => 'create_decision', 'context' => trim($matches[1])];
        }

        if (preg_match('/^(decisions|my decisions)$/i', $lower)) {
            return ['action' => 'list_decisions'];
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

        return null;
    }
}
