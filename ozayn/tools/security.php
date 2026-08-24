<?php
/**
 * Ozayn Security Tools
 * Input sanitization and XSS protection
 */

class SecurityTools {
    
    private $db;

    public function __construct() {
        $this->db = \Database::getInstance();
    }

    public function sanitize($input, $type = 'text') {
        if (is_array($input)) {
            return array_map([$this, 'sanitize'], $input);
        }
        $input = str_replace(chr(0), '', $input);
        $input = strip_tags($input);
        $input = htmlspecialchars($input, ENT_QUOTES, 'UTF-8');
        switch ($type) {
            case 'email': $input = filter_var($input, FILTER_SANITIZE_EMAIL); break;
            case 'url': $input = filter_var($input, FILTER_SANITIZE_URL); break;
            case 'int': $input = filter_var($input, FILTER_SANITIZE_NUMBER_INT); break;
            case 'float': $input = filter_var($input, FILTER_SANITIZE_NUMBER_FLOAT, FILTER_FLAG_ALLOW_FRACTION); break;
            case 'filename': $input = preg_replace('/[^a-zA-Z0-9_\-\.]/', '_', $input); break;
            case 'username': $input = preg_replace('/[^a-zA-Z0-9_\-]/', '', $input); break;
        }
        return $input;
    }

    public function validate($input, $rules) {
        $errors = [];
        foreach ($rules as $field => $rule) {
            $value = $input[$field] ?? null;
            if (!empty($rule['required']) && empty($value)) {
                $errors[$field] = "{$field} is required";
                continue;
            }
            if (empty($value)) continue;
            if (isset($rule['min']) && strlen($value) < $rule['min']) {
                $errors[$field] = "{$field} must be at least {$rule['min']} characters";
            }
            if (isset($rule['max']) && strlen($value) > $rule['max']) {
                $errors[$field] = "{$field} must be at most {$rule['max']} characters";
            }
            if (!empty($rule['email']) && !filter_var($value, FILTER_VALIDATE_EMAIL)) {
                $errors[$field] = "{$field} must be a valid email";
            }
            if (!empty($rule['numeric']) && !is_numeric($value)) {
                $errors[$field] = "{$field} must be numeric";
            }
        }
        return ['valid' => empty($errors), 'errors' => $errors];
    }

    public function generateCSRFToken($userId) {
        $token = bin2hex(random_bytes(32));
        $this->db->insert('csrf_tokens', [
            'user_id' => $userId,
            'token' => $token,
            'expires_at' => date('Y-m-d H:i:s', time() + 3600)
        ]);
        return $token;
    }

    public function validateCSRFToken($userId, $token) {
        $result = $this->db->fetch(
            "SELECT id FROM csrf_tokens WHERE user_id = ? AND token = ? AND expires_at > ?",
            [$userId, $token, date('Y-m-d H:i:s')]
        );
        if ($result) {
            $this->db->delete('csrf_tokens', 'id = ?', [$result['id']]);
            return true;
        }
        return false;
    }

    public function detectSQLInjection($input) {
        $patterns = [
            '/\b(union|select|insert|update|delete|drop|truncate|exec|execute)\b/i',
            '/(--|;|\'|"|\b(or|and)\b\s+\d+\s*=\s*\d+)/i'
        ];
        foreach ($patterns as $pattern) {
            if (preg_match($pattern, $input)) return true;
        }
        return false;
    }

    public function detectXSS($input) {
        $patterns = [
            '/<script\b[^>]*>(.*?)<\/script>/is',
            '/javascript:/i',
            '/on\w+\s*=/i',
            '/<iframe\b[^>]*>/i'
        ];
        foreach ($patterns as $pattern) {
            if (preg_match($pattern, $input)) return true;
        }
        return false;
    }

    public function checkSuspiciousActivity($userId, $action) {
        $result = $this->db->fetch(
            "SELECT COUNT(*) as count FROM audit_log WHERE user_id = ? AND action = ? AND created_at > datetime('now', '-5 minutes')",
            [$userId, $action]
        );
        return $result && $result['count'] > 10;
    }

    public function getSecurityHeaders() {
        return [
            'X-Content-Type-Options' => 'nosniff',
            'X-Frame-Options' => 'SAMEORIGIN',
            'X-XSS-Protection' => '1; mode=block',
            'Content-Security-Policy' => "default-src 'self'",
            'Referrer-Policy' => 'strict-origin-when-cross-origin'
        ];
    }
}
