<?php
/**
 * Ozayn Project Context System
 */

require_once __DIR__ . '/database.php';

class Projects {
    private $db;

    public function __construct() {
        $this->db = Database::getInstance();
    }

    /**
     * Create project
     */
    public function create($userId, $name, $description = null) {
        return $this->db->insert('projects', [
            'user_id' => $userId,
            'name' => $name,
            'description' => $description,
            'status' => 'active',
            'progress' => 0
        ]);
    }

    /**
     * Get user projects
     */
    public function getUserProjects($userId, $status = null) {
        $sql = "SELECT * FROM projects WHERE user_id = ?";
        $params = [$userId];

        if ($status) {
            $sql .= " AND status = ?";
            $params[] = $status;
        }

        $sql .= " ORDER BY updated_at DESC";

        return $this->db->fetchAll($sql, $params);
    }

    /**
     * Get project by ID
     */
    public function getById($id, $userId) {
        return $this->db->fetch(
            "SELECT * FROM projects WHERE id = ? AND user_id = ?",
            [$id, $userId]
        );
    }

    /**
     * Update project
     */
    public function update($id, $userId, $data) {
        $data['updated_at'] = date('Y-m-d H:i:s');
        return $this->db->update('projects', $data, 'id = ? AND user_id = ?', [$id, $userId]);
    }

    /**
     * Delete project
     */
    public function delete($id, $userId) {
        return $this->db->delete('projects', 'id = ? AND user_id = ?', [$id, $userId]);
    }

    /**
     * Get project with all context
     */
    public function getWithContext($id, $userId) {
        $project = $this->getById($id, $userId);
        if (!$project) {
            return null;
        }

        // Get related data
        $tasks = $this->db->fetchAll(
            "SELECT * FROM tasks WHERE project_id = ? ORDER BY priority DESC, created_at DESC",
            [$id]
        );

        $decisions = $this->db->fetchAll(
            "SELECT * FROM decisions WHERE project_id = ? ORDER BY created_at DESC LIMIT 10",
            [$id]
        );

        $conversations = $this->db->fetchAll(
            "SELECT * FROM conversations WHERE project_id = ? ORDER BY updated_at DESC LIMIT 5",
            [$id]
        );

        return [
            'project' => $project,
            'tasks' => $tasks,
            'decisions' => $decisions,
            'conversations' => $conversations
        ];
    }

    /**
     * Update project progress
     */
    public function updateProgress($id, $userId) {
        $tasks = $this->db->fetchAll(
            "SELECT status FROM tasks WHERE project_id = ?",
            [$id]
        );

        if (empty($tasks)) {
            return $this->update($id, $userId, ['progress' => 0]);
        }

        $completed = count(array_filter($tasks, fn($t) => $t['status'] === 'completed'));
        $progress = round(($completed / count($tasks)) * 100);

        return $this->update($id, $userId, ['progress' => $progress]);
    }

    /**
     * Get active project for user
     */
    public function getActiveProject($userId) {
        return $this->db->fetch(
            "SELECT * FROM projects WHERE user_id = ? AND status = 'active' ORDER BY updated_at DESC LIMIT 1",
            [$userId]
        );
    }

    /**
     * Create conversation
     */
    public function createConversation($userId, $projectId = null, $title = null) {
        return $this->db->insert('conversations', [
            'user_id' => $userId,
            'project_id' => $projectId,
            'title' => $title
        ]);
    }

    /**
     * Get conversation
     */
    public function getConversation($id, $userId) {
        return $this->db->fetch(
            "SELECT * FROM conversations WHERE id = ? AND user_id = ?",
            [$id, $userId]
        );
    }

    /**
     * Get user conversations
     */
    public function getUserConversations($userId, $limit = 20) {
        return $this->db->fetchAll(
            "SELECT * FROM conversations WHERE user_id = ? ORDER BY updated_at DESC LIMIT ?",
            [$userId, $limit]
        );
    }

    /**
     * Add message to conversation
     */
    public function addMessage($conversationId, $role, $content) {
        $messageId = $this->db->insert('messages', [
            'conversation_id' => $conversationId,
            'role' => $role,
            'content' => $content
        ]);

        // Update conversation timestamp
        $this->db->update('conversations', 
            ['updated_at' => date('Y-m-d H:i:s')],
            'id = ?',
            [$conversationId]
        );

        return $messageId;
    }

    /**
     * Get conversation messages
     */
    public function getMessages($conversationId, $limit = 50) {
        return $this->db->fetchAll(
            "SELECT * FROM messages WHERE conversation_id = ? ORDER BY created_at ASC LIMIT ?",
            [$conversationId, $limit]
        );
    }

    /**
     * Create task
     */
    public function createTask($userId, $title, $description = null, $projectId = null, $priority = 'medium', $dueDate = null) {
        return $this->db->insert('tasks', [
            'user_id' => $userId,
            'project_id' => $projectId,
            'title' => $title,
            'description' => $description,
            'priority' => $priority,
            'due_date' => $dueDate
        ]);
    }

    /**
     * Get user tasks
     */
    public function getUserTasks($userId, $status = null, $projectId = null) {
        $sql = "SELECT * FROM tasks WHERE user_id = ?";
        $params = [$userId];

        if ($status) {
            $sql .= " AND status = ?";
            $params[] = $status;
        }

        if ($projectId) {
            $sql .= " AND project_id = ?";
            $params[] = $projectId;
        }

        $sql .= " ORDER BY 
            CASE priority 
                WHEN 'urgent' THEN 1 
                WHEN 'high' THEN 2 
                WHEN 'medium' THEN 3 
                WHEN 'low' THEN 4 
            END,
            due_date ASC";

        return $this->db->fetchAll($sql, $params);
    }

    /**
     * Update task
     */
    public function updateTask($id, $userId, $data) {
        if (isset($data['status']) && $data['status'] === 'completed') {
            $data['completed_at'] = date('Y-m-d H:i:s');
        }
        
        return $this->db->update('tasks', $data, 'id = ? AND user_id = ?', [$id, $userId]);
    }

    /**
     * Save decision
     */
    public function saveDecision($userId, $context, $options, $projectId = null) {
        return $this->db->insert('decisions', [
            'user_id' => $userId,
            'project_id' => $projectId,
            'context' => $context,
            'options' => json_encode($options)
        ]);
    }

    /**
     * Get project summary
     */
    public function getSummary($userId, $projectId) {
        $project = $this->getById($projectId, $userId);
        if (!$project) {
            return null;
        }

        $tasks = $this->db->fetchAll(
            "SELECT status, COUNT(*) as count FROM tasks WHERE project_id = ? GROUP BY status",
            [$projectId]
        );

        $taskSummary = [];
        foreach ($tasks as $task) {
            $taskSummary[$task['status']] = $task['count'];
        }

        return [
            'name' => $project['name'],
            'status' => $project['status'],
            'progress' => $project['progress'],
            'tasks' => $taskSummary
        ];
    }
}
