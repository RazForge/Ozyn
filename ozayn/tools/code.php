<?php
/**
 * Ozayn Code Assistant Tools
 * Code analysis, generation, and debugging
 */

class CodeAssistant {
    
    private $db;

    public function __construct() {
        $this->db = \Database::getInstance();
    }

    /**
     * Analyze code
     */
    public function analyzeCode($code, $language = null) {
        $analysis = [
            'language' => $language ?? $this->detectLanguage($code),
            'lines' => count(explode("\n", $code)),
            'characters' => strlen($code),
            'issues' => [],
            'suggestions' => [],
            'complexity' => 'low'
        ];

        // Basic analysis
        $analysis['issues'] = $this->findIssues($code, $analysis['language']);
        $analysis['suggestions'] = $this->getSuggestions($code, $analysis['language']);
        $analysis['complexity'] = $this->calculateComplexity($code);

        return $analysis;
    }

    /**
     * Detect programming language
     */
    private function detectLanguage($code) {
        $patterns = [
            'php' => '/<\?php|<\?=|function\s+\w+\s*\(/',
            'javascript' => '/var\s+|let\s+|const\s+|function\s+\w+|=>/',
            'python' => '/def\s+\w+\s*\(|import\s+\w+|from\s+\w+\s+import/',
            'html' => '/<html|<div|<span|<p/',
            'css' => '/\{|:\s*\w+;|@media/',
            'sql' => '/SELECT\s+|INSERT\s+|UPDATE\s+|DELETE\s+|CREATE\s+TABLE/i',
            'c' => '/#include|printf\(|scanf\(/',
            'cpp' => '/#include|std::|cout\s*<</'
        ];

        foreach ($patterns as $lang => $pattern) {
            if (preg_match($pattern, $code)) {
                return $lang;
            }
        }

        return 'unknown';
    }

    /**
     * Find code issues
     */
    private function findIssues($code, $language) {
        $issues = [];

        // Common issues
        if (preg_match('/\beval\s*\(/', $code)) {
            $issues[] = [
                'type' => 'security',
                'message' => 'Use of eval() detected - potential security risk',
                'severity' => 'high'
            ];
        }

        if (preg_match('/\bexec\s*\(/', $code)) {
            $issues[] = [
                'type' => 'security',
                'message' => 'Use of exec() detected - ensure input is sanitized',
                'severity' => 'medium'
            ];
        }

        // PHP specific
        if ($language === 'php') {
            if (preg_match('/\$_GET|\$_POST|\$_REQUEST/', $code) && !preg_match('/htmlspecialchars|htmlentities|filter_/', $code)) {
                $issues[] = [
                    'type' => 'security',
                    'message' => 'User input used without sanitization',
                    'severity' => 'high'
                ];
            }

            if (preg_match('/==\s*null|!=\s*null/', $code)) {
                $issues[] = [
                    'type' => 'style',
                    'message' => 'Use === or !== for null comparison',
                    'severity' => 'low'
                ];
            }
        }

        // JavaScript specific
        if ($language === 'javascript') {
            if (preg_match('/==\s*[^=]|!=\s*[^=]/', $code) && !preg_match('/===|!==/', $code)) {
                $issues[] = [
                    'type' => 'style',
                    'message' => 'Use strict equality (=== or !==)',
                    'severity' => 'low'
                ];
            }
        }

        // General issues
        if (preg_match('/TODO|FIXME|HACK|XXX/', $code)) {
            $issues[] = [
                'type' => 'maintenance',
                'message' => 'Contains TODO/FIXME comments',
                'severity' => 'low'
            ];
        }

        return $issues;
    }

    /**
     * Get code suggestions
     */
    private function getSuggestions($code, $language) {
        $suggestions = [];

        // Long functions
        $lines = explode("\n", $code);
        if (count($lines) > 50) {
            $suggestions[] = 'Consider breaking this into smaller functions';
        }

        // Deep nesting
        $nesting = 0;
        $maxNesting = 0;
        foreach ($lines as $line) {
            $nesting += substr_count($line, '{') - substr_count($line, '}');
            $maxNesting = max($maxNesting, $nesting);
        }
        if ($maxNesting > 4) {
            $suggestions[] = 'Deep nesting detected - consider refactoring';
        }

        // Magic numbers
        if (preg_match('/\b\d{3,}\b/', $code) && !preg_match('/\b(0x|0b)\d/', $code)) {
            $suggestions[] = 'Consider using named constants for magic numbers';
        }

        // Missing comments for long code
        if (count($lines) > 30 && !preg_match('/\/\*|\/\/|#/', $code)) {
            $suggestions[] = 'Consider adding comments for documentation';
        }

        return $suggestions;
    }

    /**
     * Calculate code complexity
     */
    private function calculateComplexity($code) {
        $complexity = 0;
        
        // Count control structures
        $patterns = ['if\s*\(', 'else\s*if', 'for\s*\(', 'foreach\s*\(', 'while\s*\(', 'switch\s*\(', 'case\s+'];
        foreach ($patterns as $pattern) {
            $complexity += preg_match_all("/{$pattern}/", $code);
        }

        if ($complexity > 20) return 'very_high';
        if ($complexity > 15) return 'high';
        if ($complexity > 10) return 'medium';
        if ($complexity > 5) return 'low';
        return 'very_low';
    }

    /**
     * Generate code skeleton
     */
    public function generateSkeleton($type, $name, $language = 'php') {
        $templates = [
            'php' => [
                'function' => "<?php\n\nfunction {$name}(\$params) {\n    // TODO: Implement\n    return null;\n}\n",
                'class' => "<?php\n\nclass {$name} {\n    private \$db;\n\n    public function __construct() {\n        // Initialize\n    }\n\n    public function method() {\n        // TODO: Implement\n    }\n}\n",
                'api' => "<?php\n\nheader('Content-Type: application/json');\n\n\$method = \$_SERVER['REQUEST_METHOD'];\n\nswitch (\$method) {\n    case 'GET':\n        // Handle GET\n        break;\n    case 'POST':\n        // Handle POST\n        break;\n    default:\n        http_response_code(405);\n        echo json_encode(['error' => 'Method not allowed']);\n}\n"
            ],
            'javascript' => [
                'function' => "function {$name}(params) {\n    // TODO: Implement\n    return null;\n}\n",
                'class' => "class {$name} {\n    constructor() {\n        // Initialize\n    }\n\n    method() {\n        // TODO: Implement\n    }\n}\n",
                'module' => "// {$name} Module\n\nexport default function {$name}() {\n    // TODO: Implement\n}\n"
            ],
            'python' => [
                'function' => "def {$name}(params):\n    \"\"\"TODO: Implement\"\"\"\n    pass\n",
                'class' => "class {$name}:\n    def __init__(self):\n        pass\n\n    def method(self):\n        pass\n"
            ]
        ];

        if (isset($templates[$language][$type])) {
            return $templates[$language][$type];
        }

        return "Template not available for {$language}/{$type}";
    }

    /**
     * Format analysis for display
     */
    public function formatAnalysis($analysis) {
        $output = "**Code Analysis**\n\n";
        $output .= "- Language: {$analysis['language']}\n";
        $output .= "- Lines: {$analysis['lines']}\n";
        $output .= "- Characters: {$analysis['characters']}\n";
        $output .= "- Complexity: " . ucfirst(str_replace('_', ' ', $analysis['complexity'])) . "\n\n";

        if (!empty($analysis['issues'])) {
            $output .= "**Issues Found:**\n";
            foreach ($analysis['issues'] as $issue) {
                $icon = $issue['severity'] === 'high' ? '🔴' : ($issue['severity'] === 'medium' ? '🟡' : '🟢');
                $output .= "{$icon} [{$issue['type']}] {$issue['message']}\n";
            }
            $output .= "\n";
        }

        if (!empty($analysis['suggestions'])) {
            $output .= "**Suggestions:**\n";
            foreach ($analysis['suggestions'] as $suggestion) {
                $output .= "- 💡 {$suggestion}\n";
            }
        }

        return $output;
    }
}
