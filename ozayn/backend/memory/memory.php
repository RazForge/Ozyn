<?php
/**
 * Ozayn Memory System
 */

require_once __DIR__ . '/../database.php';

class Memory {
    private $db;

    public function __construct() {
        $this->db = Database::getInstance();
    }

    /**
     * Store memory
     */
    public function store($userId, $key, $value, $type = 'short_term', $projectId = null, $context = null, $importance = 0.5) {
        return $this->db->insert('memory', [
            'user_id' => $userId,
            'project_id' => $projectId,
            'memory_type' => $type,
            'key' => $key,
            'value' => $value,
            'context' => $context,
            'importance' => $importance
        ]);
    }

    /**
     * Retrieve memory by key
     */
    public function retrieve($userId, $key, $type = null) {
        $sql = "SELECT * FROM memory WHERE user_id = ? AND key = ?";
        $params = [$userId, $key];

        if ($type) {
            $sql .= " AND memory_type = ?";
            $params[] = $type;
        }

        $sql .= " ORDER BY importance DESC, accessed_at DESC LIMIT 1";

        $memory = $this->db->fetch($sql, $params);

        if ($memory) {
            // Update accessed_at
            $this->db->update('memory', 
                ['accessed_at' => date('Y-m-d H:i:s')],
                'id = ?',
                [$memory['id']]
            );
        }

        return $memory;
    }

    /**
     * Search memory by value
     */
    public function search($userId, $query, $type = null, $limit = 10) {
        $sql = "SELECT * FROM memory WHERE user_id = ? AND value LIKE ?";
        $params = [$userId, "%{$query}%"];

        if ($type) {
            $sql .= " AND memory_type = ?";
            $params[] = $type;
        }

        $sql .= " ORDER BY importance DESC, accessed_at DESC LIMIT ?";
        $params[] = $limit;

        return $this->db->fetchAll($sql, $params);
    }

    /**
     * Get project memories
     */
    public function getProjectMemories($userId, $projectId, $type = null) {
        $sql = "SELECT * FROM memory WHERE user_id = ? AND project_id = ?";
        $params = [$userId, $projectId];

        if ($type) {
            $sql .= " AND memory_type = ?";
            $params[] = $type;
        }

        $sql .= " ORDER BY importance DESC, created_at DESC";

        return $this->db->fetchAll($sql, $params);
    }

    /**
     * Get recent memories
     */
    public function getRecent($userId, $limit = 10, $type = null) {
        $sql = "SELECT * FROM memory WHERE user_id = ?";
        $params = [$userId];

        if ($type) {
            $sql .= " AND memory_type = ?";
            $params[] = $type;
        }

        $sql .= " ORDER BY accessed_at DESC LIMIT ?";
        $params[] = $limit;

        return $this->db->fetchAll($sql, $params);
    }

    /**
     * Update memory
     */
    public function update($id, $data) {
        return $this->db->update('memory', $data, 'id = ?', [$id]);
    }

    /**
     * Delete memory
     */
    public function delete($id) {
        return $this->db->delete('memory', 'id = ?', [$id]);
    }

    /**
     * Store user preference
     */
    public function setPreference($userId, $key, $value) {
        // Check if preference exists
        $existing = $this->db->fetch(
            "SELECT id FROM memory WHERE user_id = ? AND memory_type = 'preference' AND key = ?",
            [$userId, $key]
        );

        if ($existing) {
            return $this->db->update('memory', 
                ['value' => $value, 'accessed_at' => date('Y-m-d H:i:s')],
                'id = ?',
                [$existing['id']]
            );
        }

        return $this->store($userId, $key, $value, 'preference');
    }

    /**
     * Get user preference
     */
    public function getPreference($userId, $key, $default = null) {
        $memory = $this->retrieve($userId, $key, 'preference');
        return $memory ? $memory['value'] : $default;
    }

    /**
     * Store conversation context
     */
    public function storeContext($userId, $conversationId, $context) {
        return $this->store($userId, "conversation_{$conversationId}_context", json_encode($context), 'short_term', null, null, 0.7);
    }

    /**
     * Get conversation context
     */
    public function getContext($userId, $conversationId) {
        $memory = $this->retrieve($userId, "conversation_{$conversationId}_context", 'short_term');
        return $memory ? json_decode($memory['value'], true) : null;
    }

    /**
     * Cleanup old short-term memories
     */
    public function cleanup($userId, $maxAge = 3600 * 24 * 7) { // 7 days
        $cutoff = date('Y-m-d H:i:s', time() - $maxAge);
        
        return $this->db->delete('memory', 
            'user_id = ? AND memory_type = ? AND accessed_at < ? AND importance < 0.5',
            [$userId, 'short_term', $cutoff]
        );
    }
}
