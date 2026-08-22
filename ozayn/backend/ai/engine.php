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
               "**ARWE Integration:**\n" .
               "- `arwe status` - Check all ARWE systems\n" .
               "- `edunex status` - Check Edunex\n" .
               "- `govyx status` - Check Govyx\n" .
               "- `bilen status` - Check Bilen";
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

        return null;
    }
}
