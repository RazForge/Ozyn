<?php
/**
 * Ozayn Export/Import Tools
 * Export and import user data
 */

class ExportImportTools {
    
    private $db;

    public function __construct() {
        $this->db = \Database::getInstance();
    }

    /**
     * Export user data
     */
    public function exportData($userId, $type = 'all') {
        $data = [
            'version' => '1.0',
            'exported_at' => date('Y-m-d H:i:s'),
            'user_id' => $userId
        ];

        switch ($type) {
            case 'all':
                $data['profile'] = $this->exportProfile($userId);
                $data['projects'] = $this->exportProjects($userId);
                $data['tasks'] = $this->exportTasks($userId);
                $data['knowledge'] = $this->exportKnowledge($userId);
                $data['decisions'] = $this->exportDecisions($userId);
                $data['conversations'] = $this->exportConversations($userId);
                break;
            case 'profile':
                $data['profile'] = $this->exportProfile($userId);
                break;
            case 'projects':
                $data['projects'] = $this->exportProjects($userId);
                break;
            case 'tasks':
                $data['tasks'] = $this->exportTasks($userId);
                break;
            case 'knowledge':
                $data['knowledge'] = $this->exportKnowledge($userId);
                break;
            case 'decisions':
                $data['decisions'] = $this->exportDecisions($userId);
                break;
            case 'conversations':
                $data['conversations'] = $this->exportConversations($userId);
                break;
            default:
                return ['error' => 'Unknown export type'];
        }

        return $data;
    }

    /**
     * Export profile
     */
    private function exportProfile($userId) {
        return $this->db->fetch(
            "SELECT username, email, full_name, created_at FROM users WHERE id = ?",
            [$userId]
        );
    }

    /**
     * Export projects
     */
    private function exportProjects($userId) {
        return $this->db->fetchAll(
            "SELECT name, description, status, progress, created_at FROM projects WHERE user_id = ?",
            [$userId]
        );
    }

    /**
     * Export tasks
     */
    private function exportTasks($userId) {
        return $this->db->fetchAll(
            "SELECT title, description, status, priority, due_date, created_at, completed_at FROM tasks WHERE user_id = ?",
            [$userId]
        );
    }

    /**
     * Export knowledge
     */
    private function exportKnowledge($userId) {
        return $this->db->fetchAll(
            "SELECT title, content, source, tags, created_at FROM knowledge WHERE user_id = ?",
            [$userId]
        );
    }

    /**
     * Export decisions
     */
    private function exportDecisions($userId) {
        return $this->db->fetchAll(
            "SELECT context, options, chosen_option, reasoning, outcome, status, created_at FROM decisions WHERE user_id = ?",
            [$userId]
        );
    }

    /**
     * Export conversations
     */
    private function exportConversations($userId) {
        $conversations = $this->db->fetchAll(
            "SELECT id, title, created_at FROM conversations WHERE user_id = ?",
            [$userId]
        );

        foreach ($conversations as &$conv) {
            $conv['messages'] = $this->db->fetchAll(
                "SELECT role, content, created_at FROM messages WHERE conversation_id = ?",
                [$conv['id']]
            );
            unset($conv['id']); // Remove internal ID from export
        }

        return $conversations;
    }

    /**
     * Import data
     */
    public function importData($userId, $data) {
        if (!isset($data['version'])) {
            return ['error' => 'Invalid import file'];
        }

        $imported = [];

        // Import profile (skip username, just update name/email)
        if (isset($data['profile'])) {
            $this->db->update('users', [
                'email' => $data['profile']['email'] ?? null,
                'full_name' => $data['profile']['full_name'] ?? null
            ], 'id = ?', [$userId]);
            $imported[] = 'profile';
        }

        // Import projects
        if (isset($data['projects'])) {
            foreach ($data['projects'] as $project) {
                $this->db->insert('projects', [
                    'user_id' => $userId,
                    'name' => $project['name'],
                    'description' => $project['description'] ?? null,
                    'status' => $project['status'] ?? 'active'
                ]);
            }
            $imported[] = 'projects (' . count($data['projects']) . ')';
        }

        // Import tasks
        if (isset($data['tasks'])) {
            foreach ($data['tasks'] as $task) {
                $this->db->insert('tasks', [
                    'user_id' => $userId,
                    'title' => $task['title'],
                    'description' => $task['description'] ?? null,
                    'status' => $task['status'] ?? 'pending',
                    'priority' => $task['priority'] ?? 'medium'
                ]);
            }
            $imported[] = 'tasks (' . count($data['tasks']) . ')';
        }

        // Import knowledge
        if (isset($data['knowledge'])) {
            foreach ($data['knowledge'] as $entry) {
                $this->db->insert('knowledge', [
                    'user_id' => $userId,
                    'title' => $entry['title'],
                    'content' => $entry['content'],
                    'source' => $entry['source'] ?? null,
                    'tags' => $entry['tags'] ?? null
                ]);
            }
            $imported[] = 'knowledge (' . count($data['knowledge']) . ')';
        }

        // Import decisions
        if (isset($data['decisions'])) {
            foreach ($data['decisions'] as $decision) {
                $this->db->insert('decisions', [
                    'user_id' => $userId,
                    'context' => $decision['context'],
                    'options' => $decision['options'] ?? '[]',
                    'chosen_option' => $decision['chosen_option'] ?? null,
                    'reasoning' => $decision['reasoning'] ?? null,
                    'outcome' => $decision['outcome'] ?? null,
                    'status' => $decision['status'] ?? 'pending'
                ]);
            }
            $imported[] = 'decisions (' . count($data['decisions']) . ')';
        }

        return ['success' => true, 'imported' => $imported];
    }

    /**
     * Format export for download
     */
    public function formatForDownload($data, $format = 'json') {
        switch ($format) {
            case 'json':
                return json_encode($data, JSON_PRETTY_PRINT);
            case 'csv':
                return $this->convertToCSV($data);
            default:
                return json_encode($data, JSON_PRETTY_PRINT);
        }
    }

    /**
     * Convert to CSV (simplified)
     */
    private function convertToCSV($data) {
        // For simple flat data only
        if (!is_array($data) || empty($data)) {
            return '';
        }

        $output = '';
        $headers = array_keys(first($data));
        $output .= implode(',', $headers) . "\n";

        foreach ($data as $row) {
            $values = array_map(function($v) {
                return '"' . str_replace('"', '""', (string)$v) . '"';
            }, $row);
            $output .= implode(',', $values) . "\n";
        }

        return $output;
    }
}
