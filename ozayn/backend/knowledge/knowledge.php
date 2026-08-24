<?php
/**
 * Ozayn Knowledge Retrieval System
 */

require_once __DIR__ . '/../database.php';

class Knowledge {
    private $db;

    public function __construct() {
        $this->db = Database::getInstance();
    }

    /**
     * Add knowledge entry
     */
    public function add($userId, $title, $content, $source = null, $tags = [], $projectId = null) {
        return $this->db->insert('knowledge', [
            'user_id' => $userId,
            'project_id' => $projectId,
            'title' => $title,
            'content' => $content,
            'source' => $source,
            'tags' => json_encode($tags)
        ]);
    }

    /**
     * Search knowledge by keyword
     */
    public function search($userId, $query, $limit = 10) {
        $sql = "SELECT * FROM knowledge WHERE user_id = ? AND (title LIKE ? OR content LIKE ?)";
        $params = [$userId, "%{$query}%", "%{$query}%"];
        
        $sql .= " ORDER BY created_at DESC LIMIT ?";
        $params[] = $limit;

        return $this->db->fetchAll($sql, $params);
    }

    /**
     * Get knowledge by tag
     */
    public function getByTag($userId, $tag, $limit = 10) {
        $sql = "SELECT * FROM knowledge WHERE user_id = ? AND tags LIKE ?";
        $params = [$userId, "%\"{$tag}\"%"];
        
        $sql .= " ORDER BY created_at DESC LIMIT ?";
        $params[] = $limit;

        return $this->db->fetchAll($sql, $params);
    }

    /**
     * Get project knowledge
     */
    public function getProjectKnowledge($userId, $projectId, $limit = 50) {
        $sql = "SELECT * FROM knowledge WHERE user_id = ? AND project_id = ?";
        $params = [$userId, $projectId];
        
        $sql .= " ORDER BY created_at DESC LIMIT ?";
        $params[] = $limit;

        return $this->db->fetchAll($sql, $params);
    }

    /**
     * Get knowledge by ID
     */
    public function getById($id, $userId) {
        return $this->db->fetch(
            "SELECT * FROM knowledge WHERE id = ? AND user_id = ?",
            [$id, $userId]
        );
    }

    /**
     * Update knowledge
     */
    public function update($id, $userId, $data) {
        return $this->db->update('knowledge', $data, 'id = ? AND user_id = ?', [$id, $userId]);
    }

    /**
     * Delete knowledge
     */
    public function delete($id, $userId) {
        return $this->db->delete('knowledge', 'id = ? AND user_id = ?', [$id, $userId]);
    }

    /**
     * Get all knowledge
     */
    public function getAll($userId, $limit = 100) {
        $sql = "SELECT * FROM knowledge WHERE user_id = ?";
        $params = [$userId];
        
        $sql .= " ORDER BY created_at DESC LIMIT ?";
        $params[] = $limit;

        return $this->db->fetchAll($sql, $params);
    }

    /**
     * Simple TF-IDF-like search (for now, basic text matching)
     * In production, use vector embeddings for semantic search
     */
    public function semanticSearch($userId, $query, $limit = 5) {
        // Split query into words
        $words = explode(' ', strtolower($query));
        
        // Build scoring query
        $scoredResults = [];
        
        foreach ($words as $word) {
            $results = $this->search($userId, $word, 20);
            
            foreach ($results as $result) {
                $id = $result['id'];
                if (!isset($scoredResults[$id])) {
                    $scoredResults[$id] = [
                        'entry' => $result,
                        'score' => 0
                    ];
                }
                
                // Simple scoring: title match = 3, content match = 1
                if (stripos($result['title'], $word) !== false) {
                    $scoredResults[$id]['score'] += 3;
                }
                if (stripos($result['content'], $word) !== false) {
                    $scoredResults[$id]['score'] += 1;
                }
            }
        }

        // Sort by score
        usort($scoredResults, function($a, $b) {
            return $b['score'] - $a['score'];
        });

        // Return top results
        return array_slice(
            array_column($scoredResults, 'entry'),
            0,
            $limit
        );
    }

    /**
     * Get context for AI response
     */
    public function getRelevantContext($userId, $query, $projectId = null) {
        $context = [];
        
        // Search general knowledge
        $knowledge = $this->semanticSearch($userId, $query, 3);
        if ($knowledge) {
            $context['knowledge'] = $knowledge;
        }
        
        // Search project-specific knowledge
        if ($projectId) {
            $projectKnowledge = $this->getProjectKnowledge($userId, $projectId, 3);
            if ($projectKnowledge) {
                $context['project_knowledge'] = $projectKnowledge;
            }
        }

        return $context;
    }
}
