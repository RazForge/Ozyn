<?php
/**
 * Ozayn Collaboration System
 * Real-time multi-user collaboration
 */

class CollaborationSystem {
    
    private $db;
    private $wsClients = [];

    public function __construct() {
        $this->db = \Database::getInstance();
        $this->ensureTables();
    }

    private function ensureTables() {
        $this->db->query("CREATE TABLE IF NOT EXISTS collaboration_sessions (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            owner_id INTEGER NOT NULL,
            type TEXT DEFAULT 'shared',
            max_users INTEGER DEFAULT 10,
            is_active INTEGER DEFAULT 1,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (owner_id) REFERENCES users(id)
        )");

        $this->db->query("CREATE TABLE IF NOT EXISTS collaboration_users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id TEXT NOT NULL,
            user_id INTEGER NOT NULL,
            role TEXT DEFAULT 'viewer',
            cursor_x REAL DEFAULT 0,
            cursor_y REAL DEFAULT 0,
            is_active INTEGER DEFAULT 1,
            joined_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (session_id) REFERENCES collaboration_sessions(id),
            FOREIGN KEY (user_id) REFERENCES users(id)
        )");

        $this->db->query("CREATE TABLE IF NOT EXISTS collaboration_events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id TEXT NOT NULL,
            user_id INTEGER NOT NULL,
            event_type TEXT NOT NULL,
            event_data TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (session_id) REFERENCES collaboration_sessions(id),
            FOREIGN KEY (user_id) REFERENCES users(id)
        )");

        $this->db->query("CREATE TABLE IF NOT EXISTS shared_documents (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id TEXT NOT NULL,
            title TEXT NOT NULL,
            content TEXT,
            version INTEGER DEFAULT 1,
            last_editor_id INTEGER,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (session_id) REFERENCES collaboration_sessions(id)
        )");
    }

    public function createSession($name, $ownerId, $type = 'shared', $maxUsers = 10) {
        $id = bin2hex(random_bytes(16));
        $this->db->insert('collaboration_sessions', [
            'id' => $id,
            'name' => $name,
            'owner_id' => $ownerId,
            'type' => $type,
            'max_users' => $maxUsers
        ]);
        
        $this->joinSession($id, $ownerId, 'owner');
        return $id;
    }

    public function joinSession($sessionId, $userId, $role = 'viewer') {
        $session = $this->db->fetch(
            "SELECT * FROM collaboration_sessions WHERE id = ? AND is_active = 1",
            [$sessionId]
        );
        
        if (!$session) {
            return ['error' => 'Session not found'];
        }

        $userCount = $this->db->fetch(
            "SELECT COUNT(*) as count FROM collaboration_users WHERE session_id = ? AND is_active = 1",
            [$sessionId]
        )['count'];

        if ($userCount >= $session['max_users']) {
            return ['error' => 'Session is full'];
        }

        $existing = $this->db->fetch(
            "SELECT id FROM collaboration_users WHERE session_id = ? AND user_id = ?",
            [$sessionId, $userId]
        );

        if ($existing) {
            $this->db->update('collaboration_users', 
                ['is_active' => 1, 'role' => $role], 
                'id = ?', [$existing['id']]);
        } else {
            $this->db->insert('collaboration_users', [
                'session_id' => $sessionId,
                'user_id' => $userId,
                'role' => $role
            ]);
        }

        $this->broadcastEvent($sessionId, $userId, 'user_joined', [
            'user_id' => $userId,
            'role' => $role
        ]);

        return ['success' => true];
    }

    public function leaveSession($sessionId, $userId) {
        $this->db->update('collaboration_users', 
            ['is_active' => 0], 
            'session_id = ? AND user_id = ?', [$sessionId, $userId]);
        
        $this->broadcastEvent($sessionId, $userId, 'user_left', [
            'user_id' => $userId
        ]);

        return ['success' => true];
    }

    public function updateCursor($sessionId, $userId, $x, $y) {
        $this->db->update('collaboration_users', 
            ['cursor_x' => $x, 'cursor_y' => $y], 
            'session_id = ? AND user_id = ?', [$sessionId, $userId]);
        
        $this->broadcastEvent($sessionId, $userId, 'cursor_move', [
            'user_id' => $userId,
            'x' => $x,
            'y' => $y
        ], false);

        return ['success' => true];
    }

    public function broadcastEvent($sessionId, $userId, $eventType, $data, $persist = true) {
        if ($persist) {
            $this->db->insert('collaboration_events', [
                'session_id' => $sessionId,
                'user_id' => $userId,
                'event_type' => $eventType,
                'event_data' => json_encode($data)
            ]);
        }

        $users = $this->db->fetchAll(
            "SELECT user_id FROM collaboration_users WHERE session_id = ? AND is_active = 1 AND user_id != ?",
            [$sessionId, $userId]
        );

        foreach ($users as $user) {
            $this->sendToUser($user['user_id'], [
                'type' => 'collaboration_event',
                'session_id' => $sessionId,
                'event' => $eventType,
                'data' => $data
            ]);
        }

        return ['success' => true];
    }

    public function sendToUser($userId, $data) {
        $this->wsClients[$userId] = $data;
    }

    public function getPendingEvents($userId) {
        $events = $this->wsClients[$userId] ?? [];
        unset($this->wsClients[$userId]);
        return $events;
    }

    public function getSessionUsers($sessionId) {
        return $this->db->fetchAll(
            "SELECT u.id, u.username, cu.role, cu.cursor_x, cu.cursor_y, cu.joined_at 
             FROM collaboration_users cu 
             JOIN users u ON cu.user_id = u.id 
             WHERE cu.session_id = ? AND cu.is_active = 1",
            [$sessionId]
        );
    }

    public function getUserSessions($userId) {
        return $this->db->fetchAll(
            "SELECT cs.*, 
                    (SELECT COUNT(*) FROM collaboration_users WHERE session_id = cs.id AND is_active = 1) as user_count
             FROM collaboration_sessions cs 
             JOIN collaboration_users cu ON cs.id = cu.session_id 
             WHERE cu.user_id = ? AND cs.is_active = 1",
            [$userId]
        );
    }

    public function createDocument($sessionId, $title, $content = '') {
        $this->db->insert('shared_documents', [
            'session_id' => $sessionId,
            'title' => $title,
            'content' => $content
        ]);
        return $this->db->getConnection()->lastInsertId();
    }

    public function updateDocument($docId, $content, $userId) {
        $this->db->query(
            "UPDATE shared_documents SET content = ?, version = version + 1, last_editor_id = ?, updated_at = datetime('now') WHERE id = ?",
            [$content, $userId, $docId]
        );
        return ['success' => true];
    }

    public function getDocument($docId) {
        return $this->db->fetch("SELECT * FROM shared_documents WHERE id = ?", [$docId]);
    }

    public function getSessionDocuments($sessionId) {
        return $this->db->fetchAll(
            "SELECT * FROM shared_documents WHERE session_id = ? ORDER BY updated_at DESC",
            [$sessionId]
        );
    }

    public function getEventHistory($sessionId, $limit = 50) {
        return $this->db->fetchAll(
            "SELECT ce.*, u.username 
             FROM collaboration_events ce 
             JOIN users u ON ce.user_id = u.id 
             WHERE ce.session_id = ? 
             ORDER BY ce.created_at DESC 
             LIMIT ?",
            [$sessionId, $limit]
        );
    }
}
