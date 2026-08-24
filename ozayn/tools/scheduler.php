<?php
/**
 * Ozayn Scheduled Tasks Tool
 * Manage scheduled tasks and cron-like jobs
 */

class ScheduledTasksTool {
    
    private $db;

    public function __construct() {
        $this->db = \Database::getInstance();
    }

    /**
     * Create a scheduled task
     */
    public function create($userId, $name, $command, $schedule, $enabled = true) {
        return $this->db->insert('scheduled_tasks', [
            'user_id' => $userId,
            'name' => $name,
            'command' => $command,
            'schedule' => $schedule, // 'daily', 'hourly', 'weekly', or cron expression
            'enabled' => $enabled ? 1 : 0,
            'last_run' => null,
            'next_run' => $this->calculateNextRun($schedule)
        ]);
    }

    /**
     * Get user's scheduled tasks
     */
    public function getUserTasks($userId) {
        return $this->db->fetchAll(
            "SELECT * FROM scheduled_tasks WHERE user_id = ? ORDER BY name",
            [$userId]
        );
    }

    /**
     * Get task by ID
     */
    public function getTask($id, $userId) {
        return $this->db->fetch(
            "SELECT * FROM scheduled_tasks WHERE id = ? AND user_id = ?",
            [$id, $userId]
        );
    }

    /**
     * Update task
     */
    public function updateTask($id, $userId, $data) {
        return $this->db->update('scheduled_tasks', $data, 'id = ? AND user_id = ?', [$id, $userId]);
    }

    /**
     * Delete task
     */
    public function deleteTask($id, $userId) {
        return $this->db->delete('scheduled_tasks', 'id = ? AND user_id = ?', [$id, $userId]);
    }

    /**
     * Enable/disable task
     */
    public function toggleTask($id, $userId) {
        $task = $this->getTask($id, $userId);
        if (!$task) return false;
        
        return $this->updateTask($id, $userId, [
            'enabled' => $task['enabled'] ? 0 : 1
        ]);
    }

    /**
     * Get tasks that need to run
     */
    public function getDueTasks() {
        return $this->db->fetchAll(
            "SELECT * FROM scheduled_tasks WHERE enabled = 1 AND next_run <= ?",
            [date('Y-m-d H:i:s')]
        );
    }

    /**
     * Mark task as run
     */
    public function markRun($id) {
        $task = $this->db->fetch("SELECT schedule FROM scheduled_tasks WHERE id = ?", [$id]);
        if (!$task) return false;
        
        return $this->db->update('scheduled_tasks', [
            'last_run' => date('Y-m-d H:i:s'),
            'next_run' => $this->calculateNextRun($task['schedule'])
        ], 'id = ?', [$id]);
    }

    /**
     * Calculate next run time
     */
    private function calculateNextRun($schedule) {
        $now = new DateTime();
        
        switch ($schedule) {
            case 'hourly':
                $now->modify('+1 hour');
                break;
            case 'daily':
                $now->modify('+1 day');
                $now->setTime(0, 0, 0);
                break;
            case 'weekly':
                $now->modify('+1 week');
                $now->setTime(0, 0, 0);
                break;
            case 'monthly':
                $now->modify('+1 month');
                $now->setTime(0, 0, 0);
                break;
            default:
                // Try to parse cron expression (simplified)
                $now->modify('+1 hour');
                break;
        }
        
        return $now->format('Y-m-d H:i:s');
    }

    /**
     * Get predefined schedules
     */
    public function getSchedules() {
        return [
            'hourly' => 'Every hour',
            'daily' => 'Every day at midnight',
            'weekly' => 'Every week',
            'monthly' => 'Every month'
        ];
    }

    /**
     * Get predefined commands
     */
    public function getCommands() {
        return [
            ['name' => 'ARWE Status Check', 'command' => 'arwe_status'],
            ['name' => 'System Health Check', 'command' => 'system_overview'],
            ['name' => 'Daily Briefing', 'command' => 'arwe_briefing'],
            ['name' => 'Memory Cleanup', 'command' => 'memory_cleanup'],
            ['name' => 'Audit Log Backup', 'command' => 'audit_backup']
        ];
    }

    /**
     * Format task for display
     */
    public function formatTask($task) {
        $schedules = $this->getSchedules();
        
        $output = "**{$task['name']}**\n";
        $output .= "Command: `{$task['command']}`\n";
        $output .= "Schedule: " . ($schedules[$task['schedule']] ?? $task['schedule']) . "\n";
        $output .= "Status: " . ($task['enabled'] ? '✓ Enabled' : '✗ Disabled') . "\n";
        
        if ($task['last_run']) {
            $output .= "Last Run: {$task['last_run']}\n";
        }
        
        $output .= "Next Run: {$task['next_run']}\n";
        
        return $output;
    }
}
