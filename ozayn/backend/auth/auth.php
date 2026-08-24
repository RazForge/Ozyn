<?php
/**
 * Ozayn Authentication System
 */

require_once __DIR__ . '/../database.php';

class Auth {
    private $db;

    public function __construct() {
        $this->db = Database::getInstance();
    }

    /**
     * Register a new user
     */
    public function register($username, $password, $email = null, $fullName = null) {
        // Check if username exists
        $existing = $this->db->fetch(
            "SELECT id FROM users WHERE username = ?",
            [$username]
        );
        
        if ($existing) {
            return ['success' => false, 'error' => 'Username already exists'];
        }

        // Hash password
        $passwordHash = password_hash($password, PASSWORD_BCRYPT, ['cost' => 12]);

        // Insert user
        $userId = $this->db->insert('users', [
            'username' => $username,
            'password_hash' => $passwordHash,
            'email' => $email,
            'full_name' => $fullName,
            'role' => 'user'
        ]);

        return ['success' => true, 'user_id' => $userId];
    }

    /**
     * Login user
     */
    public function login($username, $password) {
        $user = $this->db->fetch(
            "SELECT * FROM users WHERE username = ?",
            [$username]
        );

        if (!$user || !password_verify($password, $user['password_hash'])) {
            return ['success' => false, 'error' => 'Invalid credentials'];
        }

        // Update last login
        $this->db->update('users', 
            ['last_login' => date('Y-m-d H:i:s')],
            'id = ?',
            [$user['id']]
        );

        // Create session
        $sessionId = $this->createSession($user['id']);

        return [
            'success' => true,
            'session_id' => $sessionId,
            'user' => [
                'id' => $user['id'],
                'username' => $user['username'],
                'email' => $user['email'],
                'full_name' => $user['full_name'],
                'role' => $user['role']
            ]
        ];
    }

    /**
     * Create session
     */
    private function createSession($userId) {
        $sessionId = bin2hex(random_bytes(32));
        $expiresAt = date('Y-m-d H:i:s', time() + SESSION_LIFETIME);

        $this->db->insert('sessions', [
            'id' => $sessionId,
            'user_id' => $userId,
            'expires_at' => $expiresAt
        ]);

        return $sessionId;
    }

    /**
     * Validate session
     */
    public function validateSession($sessionId) {
        if (!$sessionId) {
            return false;
        }

        $session = $this->db->fetch(
            "SELECT * FROM sessions WHERE id = ? AND expires_at > datetime('now')",
            [$sessionId]
        );

        return $session ? $session['user_id'] : false;
    }

    /**
     * Get user from session
     */
    public function getUserFromSession($sessionId) {
        $userId = $this->validateSession($sessionId);
        if (!$userId) {
            return null;
        }

        return $this->db->fetch(
            "SELECT id, username, email, full_name, role FROM users WHERE id = ?",
            [$userId]
        );
    }

    /**
     * Logout
     */
    public function logout($sessionId) {
        $this->db->delete('sessions', 'id = ?', [$sessionId]);
        return true;
    }

    /**
     * Generate CSRF token
     */
    public function generateCSRFToken() {
        if (session_status() === PHP_SESSION_NONE) {
            session_start();
        }
        
        if (!isset($_SESSION[CSRF_TOKEN_NAME])) {
            $_SESSION[CSRF_TOKEN_NAME] = bin2hex(random_bytes(32));
        }
        
        return $_SESSION[CSRF_TOKEN_NAME];
    }

    /**
     * Verify CSRF token
     */
    public function verifyCSRFToken($token) {
        if (session_status() === PHP_SESSION_NONE) {
            session_start();
        }
        
        return isset($_SESSION[CSRF_TOKEN_NAME]) && 
               hash_equals($_SESSION[CSRF_TOKEN_NAME], $token);
    }
}
