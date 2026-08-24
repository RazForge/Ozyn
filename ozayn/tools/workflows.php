<?php
/**
 * Ozayn Workflow Automation Tools
 * Create and manage automated workflows
 */

class WorkflowTools {
    
    private $db;

    public function __construct() {
        $this->db = \Database::getInstance();
    }

    /**
     * Create a workflow
     */
    public function createWorkflow($userId, $name, $description, $steps, $trigger = null) {
        return $this->db->insert('workflows', [
            'user_id' => $userId,
            'name' => $name,
            'description' => $description,
            'steps' => json_encode($steps),
            'trigger' => $trigger ? json_encode($trigger) : null,
            'status' => 'active',
            'run_count' => 0
        ]);
    }

    /**
     * Get user workflows
     */
    public function getUserWorkflows($userId) {
        return $this->db->fetchAll(
            "SELECT * FROM workflows WHERE user_id = ? ORDER BY created_at DESC",
            [$userId]
        );
    }

    /**
     * Get workflow by ID
     */
    public function getWorkflow($id, $userId) {
        return $this->db->fetch(
            "SELECT * FROM workflows WHERE id = ? AND user_id = ?",
            [$id, $userId]
        );
    }

    /**
     * Update workflow
     */
    public function updateWorkflow($id, $userId, $data) {
        return $this->db->update('workflows', $data, 'id = ? AND user_id = ?', [$id, $userId]);
    }

    /**
     * Delete workflow
     */
    public function deleteWorkflow($id, $userId) {
        return $this->db->delete('workflows', 'id = ? AND user_id = ?', [$id, $userId]);
    }

    /**
     * Run a workflow
     */
    public function runWorkflow($id, $userId) {
        $workflow = $this->getWorkflow($id, $userId);
        if (!$workflow) return ['error' => 'Workflow not found'];
        
        $steps = json_decode($workflow['steps'], true);
        $results = [];
        
        foreach ($steps as $index => $step) {
            $result = $this->executeStep($step);
            $results[] = [
                'step' => $index + 1,
                'name' => $step['name'] ?? "Step " . ($index + 1),
                'result' => $result
            ];
            
            // Stop on failure if not configured to continue
            if ($result['status'] === 'error' && empty($step['continue_on_error'])) {
                break;
            }
        }
        
        // Update run count
        $this->db->query(
            "UPDATE workflows SET run_count = run_count + 1, last_run = ? WHERE id = ?",
            [date('Y-m-d H:i:s'), $id]
        );
        
        return [
            'workflow' => $workflow['name'],
            'results' => $results,
            'completed' => true
        ];
    }

    /**
     * Execute a single workflow step
     */
    private function executeStep($step) {
        $type = $step['type'] ?? 'unknown';
        
        switch ($type) {
            case 'command':
                return $this->executeCommand($step);
            case 'notification':
                return $this->executeNotification($step);
            case 'delay':
                return ['status' => 'success', 'message' => "Delayed {$step['seconds']} seconds"];
            case 'condition':
                return $this->evaluateCondition($step);
            default:
                return ['status' => 'error', 'message' => "Unknown step type: {$type}"];
        }
    }

    /**
     * Execute command step
     */
    private function executeCommand($step) {
        $command = $step['command'] ?? '';
        // Placeholder for command execution
        return ['status' => 'success', 'message' => "Command executed: {$command}"];
    }

    /**
     * Execute notification step
     */
    private function executeNotification($step) {
        $title = $step['title'] ?? 'Workflow Notification';
        $message = $step['message'] ?? '';
        // Would use NotificationSystem here
        return ['status' => 'success', 'message' => "Notification sent: {$title}"];
    }

    /**
     * Evaluate condition step
     */
    private function evaluateCondition($step) {
        $condition = $step['condition'] ?? 'true';
        // Placeholder for condition evaluation
        return ['status' => 'success', 'message' => "Condition evaluated: {$condition}"];
    }

    /**
     * Get predefined workflow templates
     */
    public function getTemplates() {
        return [
            'templates' => [
                [
                    'name' => 'System Health Check',
                    'description' => 'Check all ARWE systems and notify if any are offline',
                    'steps' => [
                        ['type' => 'command', 'name' => 'Check ARWE Status', 'command' => 'arwe_status'],
                        ['type' => 'condition', 'name' => 'Check if any offline', 'condition' => 'has_offline'],
                        ['type' => 'notification', 'name' => 'Send Alert', 'title' => 'ARWE Alert', 'message' => 'System status check complete']
                    ]
                ],
                [
                    'name' => 'Daily Briefing',
                    'description' => 'Generate daily ARWE briefing report',
                    'steps' => [
                        ['type' => 'command', 'name' => 'Get Briefing', 'command' => 'arwe_briefing'],
                        ['type' => 'notification', 'name' => 'Send Briefing', 'title' => 'Daily Briefing', 'message' => 'Your daily briefing is ready']
                    ]
                ],
                [
                    'name' => 'Backup Workflow',
                    'description' => 'Create backup of important files',
                    'steps' => [
                        ['type' => 'command', 'name' => 'List Files', 'command' => 'list_files'],
                        ['type' => 'command', 'name' => 'Create Backup', 'command' => 'create_backup'],
                        ['type' => 'notification', 'name' => 'Complete', 'title' => 'Backup Complete', 'message' => 'Backup completed successfully']
                    ]
                ]
            ]
        ];
    }

    /**
     * Format workflow for display
     */
    public function formatWorkflow($workflow) {
        $steps = json_decode($workflow['steps'], true);
        
        $output = "**{$workflow['name']}**\n";
        $output .= "{$workflow['description']}\n\n";
        $output .= "Status: " . ucfirst($workflow['status']) . "\n";
        $output .= "Runs: {$workflow['run_count']}\n";
        if ($workflow['last_run']) {
            $output .= "Last Run: {$workflow['last_run']}\n";
        }
        $output .= "\n**Steps:**\n";
        
        foreach ($steps as $i => $step) {
            $stepName = $step['name'] ?? $step['type'];
            $output .= ($i + 1) . ". {$stepName}\n";
        }
        
        return $output;
    }
}
