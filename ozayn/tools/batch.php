<?php
/**
 * Ozayn Batch Operations
 * Execute multiple operations in single request
 */

class BatchOperations {
    
    private $db;
    private $maxBatchSize = 50;

    public function __construct() {
        $this->db = \Database::getInstance();
    }

    public function execute($operations, $userId) {
        $results = [];
        $startTime = microtime(true);

        if (count($operations) > $this->maxBatchSize) {
            return ['error' => "Batch size exceeds limit of {$this->maxBatchSize}"];
        }

        foreach ($operations as $index => $op) {
            $opStart = microtime(true);
            try {
                $result = $this->executeOperation($op, $userId);
                $results[] = [
                    'index' => $index,
                    'success' => true,
                    'result' => $result,
                    'duration_ms' => round((microtime(true) - $opStart) * 1000, 2)
                ];
            } catch (Exception $e) {
                $results[] = [
                    'index' => $index,
                    'success' => false,
                    'error' => $e->getMessage(),
                    'duration_ms' => round((microtime(true) - $opStart) * 1000, 2)
                ];
            }
        }

        $totalDuration = round((microtime(true) - $startTime) * 1000, 2);
        $successCount = count(array_filter($results, fn($r) => $r['success']));

        return [
            'total' => count($operations),
            'success' => $successCount,
            'failed' => count($operations) - $successCount,
            'duration_ms' => $totalDuration,
            'results' => $results
        ];
    }

    private function executeOperation($op, $userId) {
        $type = $op['type'] ?? '';
        $data = $op['data'] ?? [];

        switch ($type) {
            case 'create_task':
                return $this->db->insert('tasks', [
                    'user_id' => $userId,
                    'title' => $data['title'] ?? '',
                    'description' => $data['description'] ?? '',
                    'priority' => $data['priority'] ?? 'medium',
                    'status' => 'pending'
                ]);

            case 'create_knowledge':
                return $this->db->insert('knowledge', [
                    'user_id' => $userId,
                    'title' => $data['title'] ?? '',
                    'content' => $data['content'] ?? '',
                    'tags' => $data['tags'] ?? '',
                    'category' => $data['category'] ?? 'general'
                ]);

            case 'store_memory':
                return $this->db->insert('memory', [
                    'user_id' => $userId,
                    'key' => $data['key'] ?? '',
                    'value' => $data['value'] ?? '',
                    'type' => $data['type'] ?? 'short_term',
                    'importance' => $data['importance'] ?? 0.5
                ]);

            case 'update_task':
                $taskId = $data['id'] ?? 0;
                $updates = array_diff_key($data, array_flip(['id']));
                $this->db->update('tasks', $updates, 'id = ? AND user_id = ?', [$taskId, $userId]);
                return $taskId;

            case 'delete_task':
                $taskId = $data['id'] ?? 0;
                $this->db->delete('tasks', 'id = ? AND user_id = ?', [$taskId, $userId]);
                return $taskId;

            case 'create_decision':
                return $this->db->insert('decisions', [
                    'user_id' => $userId,
                    'title' => $data['title'] ?? '',
                    'context' => $data['context'] ?? '',
                    'options' => json_encode($data['options'] ?? []),
                    'status' => 'pending'
                ]);

            case 'log_audit':
                return $this->db->insert('audit_log', [
                    'user_id' => $userId,
                    'action' => $data['action'] ?? '',
                    'details' => $data['details'] ?? '',
                    'result' => $data['result'] ?? 'info'
                ]);

            case 'arwe_config_save':
                require_once __DIR__ . '/arwe_config.php';
                $config = new ARWEConfig();
                return $config->save($data['system'] ?? '', $data['config'] ?? []);

            default:
                throw new Exception("Unknown operation type: {$type}");
        }
    }

    public function getSupportedTypes() {
        return [
            'create_task',
            'create_knowledge',
            'store_memory',
            'update_task',
            'delete_task',
            'create_decision',
            'log_audit',
            'arwe_config_save'
        ];
    }

    public function getExample() {
        return [
            'operations' => [
                ['type' => 'create_task', 'data' => ['title' => 'Task 1', 'priority' => 'high']],
                ['type' => 'create_task', 'data' => ['title' => 'Task 2', 'priority' => 'medium']],
                ['type' => 'store_memory', 'data' => ['key' => 'project_deadline', 'value' => '2026-12-31']]
            ]
        ];
    }
}
