<?php
/**
 * Ozayn Notification System
 * Alert and notification management for ARWE events
 */

class NotificationSystem {
    
    private $db;

    public function __construct() {
        $this->db = \Database::getInstance();
    }

    /**
     * Create a notification
     */
    public function create($userId, $title, $message, $type = 'info', $source = null, $metadata = []) {
        return $this->db->insert('notifications', [
            'user_id' => $userId,
            'title' => $title,
            'message' => $message,
            'type' => $type,
            'source' => $source,
            'metadata' => json_encode($metadata),
            'read' => 0
        ]);
    }

    /**
     * Get user notifications
     */
    public function getUserNotifications($userId, $limit = 20, $unreadOnly = false) {
        $sql = "SELECT * FROM notifications WHERE user_id = ?";
        $params = [$userId];
        
        if ($unreadOnly) {
            $sql .= " AND read = 0";
        }
        
        $sql .= " ORDER BY created_at DESC LIMIT ?";
        $params[] = $limit;
        
        return $this->db->fetchAll($sql, $params);
    }

    /**
     * Mark notification as read
     */
    public function markRead($id, $userId) {
        return $this->db->update('notifications', [
            'read' => 1
        ], 'id = ? AND user_id = ?', [$id, $userId]);
    }

    /**
     * Mark all as read
     */
    public function markAllRead($userId) {
        return $this->db->update('notifications', [
            'read' => 1
        ], 'user_id = ? AND read = 0', [$userId]);
    }

    /**
     * Get unread count
     */
    public function getUnreadCount($userId) {
        $result = $this->db->fetch(
            "SELECT COUNT(*) as count FROM notifications WHERE user_id = ? AND read = 0",
            [$userId]
        );
        return $result['count'] ?? 0;
    }

    /**
     * Delete notification
     */
    public function delete($id, $userId) {
        return $this->db->delete('notifications', 'id = ? AND user_id = ?', [$id, $userId]);
    }

    /**
     * Create ARWE system alert
     */
    public function createARWEAlert($userId, $system, $status, $details = '') {
        $type = $status === 'offline' ? 'error' : ($status === 'warning' ? 'warning' : 'info');
        
        return $this->create(
            $userId,
            ucfirst($system) . " " . ucfirst($status),
            "{$system} status changed to {$status}. {$details}",
            $type,
            'arwe',
            ['system' => $system, 'status' => $status]
        );
    }

    /**
     * Create task reminder
     */
    public function createTaskReminder($userId, $taskTitle, $dueDate = null) {
        $message = "Reminder: {$taskTitle}";
        if ($dueDate) {
            $message .= " is due on {$dueDate}";
        }
        
        return $this->create(
            $userId,
            "Task Reminder",
            $message,
            'reminder',
            'tasks'
        );
    }

    /**
     * Create security alert
     */
    public function createSecurityAlert($userId, $alertType, $details) {
        return $this->create(
            $userId,
            "Security Alert: {$alertType}",
            $details,
            'critical',
            'bilen'
        );
    }

    /**
     * Get notification types
     */
    public function getTypes() {
        return [
            'info' => ['label' => 'Information', 'color' => '#0a84ff'],
            'success' => ['label' => 'Success', 'color' => '#30d158'],
            'warning' => ['label' => 'Warning', 'color' => '#ff9f0a'],
            'error' => ['label' => 'Error', 'color' => '#ff453a'],
            'critical' => ['label' => 'Critical', 'color' => '#ff453a'],
            'reminder' => ['label' => 'Reminder', 'color' => '#bf5af2']
        ];
    }

    /**
     * Format notification for display
     */
    public function formatNotification($notification) {
        $types = $this->getTypes();
        $typeInfo = $types[$notification['type']] ?? $types['info'];
        
        $output = "**{$notification['title']}**\n";
        $output .= "Type: {$typeInfo['label']}\n";
        $output .= "Time: {$notification['created_at']}\n";
        if (!empty($notification['source'])) {
            $output .= "Source: {$notification['source']}\n";
        }
        $output .= "\n{$notification['message']}\n";
        
        return $output;
    }
}
