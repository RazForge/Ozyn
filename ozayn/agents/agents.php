<?php
/**
 * Ozayn Agent Architecture
 * Specialized agents for different tasks
 */

class AgentSystem {
    
    private $db;
    private $agents = [];

    public function __construct() {
        $this->db = \Database::getInstance();
        $this->initAgents();
    }

    /**
     * Initialize available agents
     */
    private function initAgents() {
        $this->agents = [
            'coding' => new CodingAgent(),
            'research' => new ResearchAgent(),
            'arwe' => new ARWEAgent(),
            'file' => new FileAgent(),
            'system' => new SystemAgent()
        ];
    }

    /**
     * Get available agents
     */
    public function getAvailableAgents() {
        return array_map(function($agent) {
            return $agent->getDescription();
        }, $this->agents);
    }

    /**
     * Route task to appropriate agent
     */
    public function routeTask($task, $context = []) {
        $agent = $this->selectAgent($task, $context);
        
        if (!$agent) {
            return ['error' => 'No suitable agent found'];
        }

        return $agent->execute($task, $context);
    }

    /**
     * Select best agent for task
     */
    private function selectAgent($task, $context) {
        $lower = strtolower($task);
        
        // Coding tasks
        if (preg_match('/\b(code|program|function|debug|php|python|javascript|html|css|sql|api|file|write|read)\b/i', $lower)) {
            return $this->agents['coding'];
        }
        
        // Research tasks
        if (preg_match('/\b(research|search|find|analyze|learn|explain|what|how|why)\b/i', $lower)) {
            return $this->agents['research'];
        }
        
        // ARWE tasks
        if (preg_match('/\b(arwe|edunex|govyx|locify|terrachain|bilen|kidane|canivox|drone|robot)\b/i', $lower)) {
            return $this->agents['arwe'];
        }
        
        // System tasks
        if (preg_match('/\b(system|cpu|memory|disk|process|monitor|status|uptime)\b/i', $lower)) {
            return $this->agents['system'];
        }
        
        // File tasks
        if (preg_match('/\b(file|folder|directory|ls|mkdir|delete|copy|move)\b/i', $lower)) {
            return $this->agents['file'];
        }
        
        // Default to research
        return $this->agents['research'];
    }
}

/**
 * Base Agent Class
 */
abstract class BaseAgent {
    protected $name;
    protected $description;
    protected $capabilities;

    public function getDescription() {
        return [
            'name' => $this->name,
            'description' => $this->description,
            'capabilities' => $this->capabilities
        ];
    }

    abstract public function execute($task, $context = []);
}

/**
 * Coding Agent
 */
class CodingAgent extends BaseAgent {
    public function __construct() {
        $this->name = 'Coding Agent';
        $this->description = 'Helps with code writing, debugging, and analysis';
        $this->capabilities = [
            'code_generation',
            'code_review',
            'debugging',
            'refactoring',
            'documentation'
        ];
    }

    public function execute($task, $context = []) {
        $lower = strtolower($task);
        
        // Generate code
        if (preg_match('/\b(generate|create|write)\b.*\b(code|function|class|api)\b/i', $lower)) {
            return $this->generateCode($task, $context);
        }
        
        // Review code
        if (preg_match('/\b(review|analyze|check)\b.*\b(code|file)\b/i', $lower)) {
            return $this->reviewCode($task, $context);
        }
        
        // Explain code
        if (preg_match('/\b(explain|what does|how does)\b/i', $lower)) {
            return $this->explainCode($task, $context);
        }
        
        return [
            'agent' => $this->name,
            'response' => "I can help with coding tasks. Try asking me to:\n" .
                         "- Generate code\n" .
                         "- Review code\n" .
                         "- Explain code\n" .
                         "- Debug issues"
        ];
    }

    private function generateCode($task, $context) {
        // Basic code generation
        return [
            'agent' => $this->name,
            'type' => 'code_generation',
            'response' => "I can generate code for you. Please specify:\n" .
                         "- Programming language\n" .
                         "- What the code should do\n" .
                         "- Any specific requirements"
        ];
    }

    private function reviewCode($task, $context) {
        return [
            'agent' => $this->name,
            'type' => 'code_review',
            'response' => "I can review code for you. Please provide the code you'd like me to analyze."
        ];
    }

    private function explainCode($task, $context) {
        return [
            'agent' => $this->name,
            'type' => 'code_explanation',
            'response' => "I can explain code. Please share the code you'd like me to explain."
        ];
    }
}

/**
 * Research Agent
 */
class ResearchAgent extends BaseAgent {
    public function __construct() {
        $this->name = 'Research Agent';
        $this->description = 'Helps with research, analysis, and knowledge retrieval';
        $this->capabilities = [
            'knowledge_search',
            'analysis',
            'summarization',
            'explanation'
        ];
    }

    public function execute($task, $context = []) {
        $knowledge = new Knowledge();
        
        // Search knowledge base
        $results = $knowledge->search(1, $task, 5);
        
        if (!empty($results)) {
            $response = "I found relevant information:\n\n";
            foreach ($results as $result) {
                $response .= "**{$result['title']}**\n";
                $response .= substr($result['content'], 0, 200) . "...\n\n";
            }
            return [
                'agent' => $this->name,
                'type' => 'knowledge_retrieval',
                'results' => $results,
                'response' => $response
            ];
        }
        
        return [
            'agent' => $this->name,
            'type' => 'research',
            'response' => "I can help with research. What would you like to know?"
        ];
    }
}

/**
 * ARWE Agent
 */
class ARWEAgent extends BaseAgent {
    public function __construct() {
        $this->name = 'ARWE Agent';
        $this->description = 'Manages ARWE system integrations';
        $this->capabilities = [
            'system_status',
            'arwe_queries',
            'fleet_monitoring',
            'alerts'
        ];
    }

    public function execute($task, $context = []) {
        $arwe = new ARWETools();
        $lower = strtolower($task);
        
        // Check all systems
        if (preg_match('/\b(all|status|overview)\b/i', $lower)) {
            $status = $arwe->getAllStatus();
            return [
                'agent' => $this->name,
                'type' => 'system_status',
                'status' => $status,
                'response' => $arwe->getDailyBriefing()
            ];
        }
        
        // Check specific system
        $systems = ['edunex', 'govyx', 'locify', 'terrachain', 'bilen', 'kidane', 'canivox'];
        foreach ($systems as $system) {
            if (strpos($lower, $system) !== false) {
                $status = $arwe->getStatus($system);
                return [
                    'agent' => $this->name,
                    'type' => 'system_status',
                    'system' => $system,
                    'status' => $status,
                    'response' => $arwe->formatStatus($system, $status)
                ];
            }
        }
        
        return [
            'agent' => $this->name,
            'type' => 'arwe',
            'response' => "I can help with ARWE systems. Try:\n" .
                         "- Check ARWE status\n" .
                         "- Check Edunex status\n" .
                         "- Check Kidane fleet"
        ];
    }
}

/**
 * File Agent
 */
class FileAgent extends BaseAgent {
    public function __construct() {
        $this->name = 'File Agent';
        $this->description = 'Manages file operations';
        $this->capabilities = [
            'file_list',
            'file_read',
            'file_write',
            'file_search',
            'directory_management'
        ];
    }

    public function execute($task, $context = []) {
        $computer = new ComputerTools();
        $lower = strtolower($task);
        
        // List files
        if (preg_match('/\b(ls|list|dir|files)\b/i', $lower)) {
            $path = $context['path'] ?? '.';
            $result = $computer->listFiles($path);
            return [
                'agent' => $this->name,
                'type' => 'file_list',
                'result' => $result,
                'response' => "Found {$result['count']} items in {$result['path']}"
            ];
        }
        
        // Read file
        if (preg_match('/\b(read|open|cat)\b/i', $lower)) {
            $path = $context['path'] ?? '';
            if ($path) {
                $result = $computer->readFile($path);
                return [
                    'agent' => $this->name,
                    'type' => 'file_read',
                    'result' => $result,
                    'response' => isset($result['error']) ? $result['error'] : "File contents loaded"
                ];
            }
        }
        
        return [
            'agent' => $this->name,
            'type' => 'file',
            'response' => "I can help with file operations. Try:\n" .
                         "- List files\n" .
                         "- Read file [path]\n" .
                         "- Search files [pattern]"
        ];
    }
}

/**
 * System Agent
 */
class SystemAgent extends BaseAgent {
    public function __construct() {
        $this->name = 'System Agent';
        $this->description = 'Monitors system resources and processes';
        $this->capabilities = [
            'system_monitoring',
            'process_management',
            'resource_tracking',
            'performance_analysis'
        ];
    }

    public function execute($task, $context = []) {
        $monitor = new SystemMonitor();
        $lower = strtolower($task);
        
        // System overview
        if (preg_match('/\b(overview|status|info)\b/i', $lower)) {
            $overview = $monitor->getOverview();
            return [
                'agent' => $this->name,
                'type' => 'system_overview',
                'overview' => $overview,
                'response' => "System: {$overview['hostname']}\n" .
                             "OS: {$overview['os']}\n" .
                             "Uptime: {$overview['uptime']['formatted']}"
            ];
        }
        
        // CPU info
        if (preg_match('/\b(cpu|processor)\b/i', $lower)) {
            $cpu = $monitor->getCpuInfo();
            return [
                'agent' => $this->name,
                'type' => 'cpu_info',
                'cpu' => $cpu,
                'response' => "CPU: {$cpu['model']}\nCores: {$cpu['cores']}\nUsage: {$cpu['usage']}%"
            ];
        }
        
        // Memory info
        if (preg_match('/\b(memory|mem|ram)\b/i', $lower)) {
            $mem = $monitor->getMemoryInfo();
            $usedGB = round($mem['used'] / 1073741824, 2);
            $totalGB = round($mem['total'] / 1073741824, 2);
            return [
                'agent' => $this->name,
                'type' => 'memory_info',
                'memory' => $mem,
                'response' => "Memory: {$usedGB} GB / {$totalGB} GB ({$mem['percent_used']}%)"
            ];
        }
        
        // Processes
        if (preg_match('/\b(process|ps|running)\b/i', $lower)) {
            $procs = $monitor->getProcesses('cpu', 10);
            return [
                'agent' => $this->name,
                'type' => 'processes',
                'processes' => $procs,
                'response' => "Top " . count($procs['processes']) . " processes loaded"
            ];
        }
        
        return [
            'agent' => $this->name,
            'type' => 'system',
            'response' => "I can help with system monitoring. Try:\n" .
                         "- System overview\n" .
                         "- CPU info\n" .
                         "- Memory info\n" .
                         "- Running processes"
        ];
    }
}
