<?php
/**
 * Ozayn Advanced Search
 * Full-text search with filters across all data types
 */

class AdvancedSearch {
    
    private $db;

    public function __construct() {
        $this->db = \Database::getInstance();
    }

    public function search($query, $options = []) {
        $type = $options['type'] ?? 'all';
        $limit = $options['limit'] ?? 50;
        $offset = $options['offset'] ?? 0;
        $dateFrom = $options['date_from'] ?? null;
        $dateTo = $options['date_to'] ?? null;
        $userId = $options['user_id'] ?? null;

        $results = [];

        if ($type === 'all' || $type === 'conversations') {
            $results['conversations'] = $this->searchConversations($query, $limit, $offset, $dateFrom, $dateTo, $userId);
        }
        if ($type === 'all' || $type === 'messages') {
            $results['messages'] = $this->searchMessages($query, $limit, $offset, $dateFrom, $dateTo, $userId);
        }
        if ($type === 'all' || $type === 'tasks') {
            $results['tasks'] = $this->searchTasks($query, $limit, $offset, $dateFrom, $dateTo, $userId);
        }
        if ($type === 'all' || $type === 'knowledge') {
            $results['knowledge'] = $this->searchKnowledge($query, $limit, $offset, $dateFrom, $dateTo, $userId);
        }
        if ($type === 'all' || $type === 'memory') {
            $results['memory'] = $this->searchMemory($query, $limit, $offset, $dateFrom, $dateTo, $userId);
        }
        if ($type === 'all' || $type === 'decisions') {
            $results['decisions'] = $this->searchDecisions($query, $limit, $offset, $dateFrom, $dateTo, $userId);
        }

        $total = 0;
        foreach ($results as $items) {
            $total += count($items);
        }

        return [
            'query' => $query,
            'total' => $total,
            'results' => $results
        ];
    }

    private function searchConversations($query, $limit, $offset, $dateFrom, $dateTo, $userId) {
        $sql = "SELECT * FROM conversations WHERE (title LIKE ? OR context LIKE ?)";
        $params = ["%{$query}%", "%{$query}%"];
        if ($userId) { $sql .= " AND user_id = ?"; $params[] = $userId; }
        if ($dateFrom) { $sql .= " AND created_at >= ?"; $params[] = $dateFrom; }
        if ($dateTo) { $sql .= " AND created_at <= ?"; $params[] = $dateTo; }
        $sql .= " ORDER BY created_at DESC LIMIT {$limit} OFFSET {$offset}";
        return $this->db->fetchAll($sql, $params);
    }

    private function searchMessages($query, $limit, $offset, $dateFrom, $dateTo, $userId) {
        $sql = "SELECT m.*, c.title as conversation_title FROM messages m LEFT JOIN conversations c ON m.conversation_id = c.id WHERE (m.content LIKE ? OR m.context LIKE ?)";
        $params = ["%{$query}%", "%{$query}%"];
        if ($userId) { $sql .= " AND m.user_id = ?"; $params[] = $userId; }
        if ($dateFrom) { $sql .= " AND m.created_at >= ?"; $params[] = $dateFrom; }
        if ($dateTo) { $sql .= " AND m.created_at <= ?"; $params[] = $dateTo; }
        $sql .= " ORDER BY m.created_at DESC LIMIT {$limit} OFFSET {$offset}";
        return $this->db->fetchAll($sql, $params);
    }

    private function searchTasks($query, $limit, $offset, $dateFrom, $dateTo, $userId) {
        $sql = "SELECT * FROM tasks WHERE (title LIKE ? OR description LIKE ?)";
        $params = ["%{$query}%", "%{$query}%"];
        if ($userId) { $sql .= " AND user_id = ?"; $params[] = $userId; }
        if ($dateFrom) { $sql .= " AND created_at >= ?"; $params[] = $dateFrom; }
        if ($dateTo) { $sql .= " AND created_at <= ?"; $params[] = $dateTo; }
        $sql .= " ORDER BY created_at DESC LIMIT {$limit} OFFSET {$offset}";
        return $this->db->fetchAll($sql, $params);
    }

    private function searchKnowledge($query, $limit, $offset, $dateFrom, $dateTo, $userId) {
        $sql = "SELECT * FROM knowledge WHERE (title LIKE ? OR content LIKE ? OR tags LIKE ?)";
        $params = ["%{$query}%", "%{$query}%", "%{$query}%"];
        if ($userId) { $sql .= " AND user_id = ?"; $params[] = $userId; }
        if ($dateFrom) { $sql .= " AND created_at >= ?"; $params[] = $dateFrom; }
        if ($dateTo) { $sql .= " AND created_at <= ?"; $params[] = $dateTo; }
        $sql .= " ORDER BY created_at DESC LIMIT {$limit} OFFSET {$offset}";
        return $this->db->fetchAll($sql, $params);
    }

    private function searchMemory($query, $limit, $offset, $dateFrom, $dateTo, $userId) {
        $sql = "SELECT * FROM memory WHERE (`key` LIKE ? OR `value` LIKE ?)";
        $params = ["%{$query}%", "%{$query}%"];
        if ($userId) { $sql .= " AND user_id = ?"; $params[] = $userId; }
        if ($dateFrom) { $sql .= " AND created_at >= ?"; $params[] = $dateFrom; }
        if ($dateTo) { $sql .= " AND created_at <= ?"; $params[] = $dateTo; }
        $sql .= " ORDER BY created_at DESC LIMIT {$limit} OFFSET {$offset}";
        return $this->db->fetchAll($sql, $params);
    }

    private function searchDecisions($query, $limit, $offset, $dateFrom, $dateTo, $userId) {
        $sql = "SELECT * FROM decisions WHERE (title LIKE ? OR description LIKE ? OR recommendation LIKE ?)";
        $params = ["%{$query}%", "%{$query}%", "%{$query}%"];
        if ($userId) { $sql .= " AND user_id = ?"; $params[] = $userId; }
        if ($dateFrom) { $sql .= " AND created_at >= ?"; $params[] = $dateFrom; }
        if ($dateTo) { $sql .= " AND created_at <= ?"; $params[] = $dateTo; }
        $sql .= " ORDER BY created_at DESC LIMIT {$limit} OFFSET {$offset}";
        return $this->db->fetchAll($sql, $params);
    }

    public function searchStats($userId = null) {
        $stats = [];
        $tables = ['conversations', 'messages', 'tasks', 'knowledge', 'memory', 'decisions'];
        foreach ($tables as $table) {
            $sql = "SELECT COUNT(*) as count FROM {$table}";
            $params = [];
            if ($userId && $table !== 'messages') {
                $sql .= " WHERE user_id = ?";
                $params[] = $userId;
            }
            $result = $this->db->fetch($sql, $params);
            $stats[$table] = $result['count'] ?? 0;
        }
        $stats['total'] = array_sum($stats);
        return $stats;
    }
}
