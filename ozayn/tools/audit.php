<?php
/**
 * Ozayn Audit System
 * Logs all important actions for accountability
 */

class AuditSystem {
    
    private $db;

    public function __construct() {
        $this->db = \Database::getInstance();
    }

    /**
     * Log an action
     */
    public function log($userId, $action, $target = null, $details = null, $result = 'success') {
        return $this->db->insert('audit_log', [
            'user_id' => $userId,
            'action' => $action,
            'target' => $target,
            'details' => is_array($details) ? json_encode($details) : $details,
            'result' => $result,
            'ip_address' => $_SERVER['REMOTE_ADDR'] ?? null,
            'user_agent' => $_SERVER['HTTP_USER_AGENT'] ?? null,
            'created_at' => date('Y-m-d H:i:s')
        ]);
    }

    /**
     * Log AI action
     */
    public function logAIAction($userId, $action, $input, $output, $tools = []) {
        return $this->log($userId, 'ai_' . $action, null, [
            'input' => substr($input, 0, 1000),
            'output' => substr($output, 0, 1000),
            'tools_used' => $tools,
            'timestamp' => date('Y-m-d H:i:s')
        ]);
    }

    /**
     * Log system access
     */
    public function logSystemAccess($userId, $system, $action, $result = 'success') {
        return $this->log($userId, 'system_access', $system, [
            'action' => $action,
            'system' => $system
        ], $result);
    }

    /**
     * Log file operation
     */
    public function logFileOperation($userId, $operation, $path, $result = 'success') {
        return $this->log($userId, 'file_' . $operation, $path, [
            'operation' => $operation,
            'path' => $path
        ], $result);
    }

    /**
     * Log security event
     */
    public function logSecurityEvent($userId, $event, $details = null) {
        return $this->log($userId, 'security_' . $event, null, [
            'event' => $event,
            'details' => $details
        ], 'warning');
    }

    /**
     * Get user audit log
     */
    public function getUserLog($userId, $limit = 100, $action = null) {
        $sql = "SELECT * FROM audit_log WHERE user_id = ?";
        $params = [$userId];

        if ($action) {
            $sql .= " AND action LIKE ?";
            $params[] = "%{$action}%";
        }

        $sql .= " ORDER BY created_at DESC LIMIT ?";
        $params[] = $limit;

        return $this->db->fetchAll($sql, $params);
    }

    /**
     * Get recent actions
     */
    public function getRecentActions($limit = 50) {
        return $this->db->fetchAll(
            "SELECT al.*, u.username FROM audit_log al 
             LEFT JOIN users u ON al.user_id = u.id 
             ORDER BY al.created_at DESC LIMIT ?",
            [$limit]
        );
    }

    /**
     * Get action statistics
     */
    public function getStats($userId, $days = 30) {
        $startDate = date('Y-m-d', strtotime("-{$days} days"));
        
        $stats = $this->db->fetch(
            "SELECT action, COUNT(*) as count FROM audit_log 
             WHERE user_id = ? AND created_at >= ?
             GROUP BY action ORDER BY count DESC",
            [$userId, $startDate]
        );

        return $stats;
    }

    /**
     * Format audit log for display
     */
    public function formatLogEntry($entry) {
        $timestamp = $entry['created_at'];
        $action = $entry['action'];
        $target = $entry['target'] ?? '';
        $result = $entry['result'] ?? 'success';
        
        $resultIcon = $result === 'success' ? '✓' : ($result === 'warning' ? '⚠' : '✗');
        
        $output = "{$resultIcon} [{$timestamp}] {$action}";
        if ($target) {
            $output .= " → {$target}";
        }
        
        return $output;
    }

    /**
     * Format full audit log
     */
    public function formatAuditLog($entries) {
        if (empty($entries)) {
            return "No audit entries found.";
        }

        $output = "**Audit Log**\n\n";
        
        foreach ($entries as $entry) {
            $output .= $this->formatLogEntry($entry) . "\n";
        }
        
        return $output;
    }

    /**
     * Search audit log
     */
    public function search($query, $userId = null, $limit = 50) {
        $sql = "SELECT * FROM audit_log WHERE (action LIKE ? OR target LIKE ? OR details LIKE ?)";
        $params = ["%{$query}%", "%{$query}%", "%{$query}%"];
        
        if ($userId) {
            $sql .= " AND user_id = ?";
            $params[] = $userId;
        }
        
        $sql .= " ORDER BY created_at DESC LIMIT ?";
        $params[] = $limit;
        
        return $this->db->fetchAll($sql, $params);
    }

    /**
     * Get security events
     */
    public function getSecurityEvents($days = 7) {
        $startDate = date('Y-m-d', strtotime("-{$days} days"));
        
        return $this->db->fetchAll(
            "SELECT * FROM audit_log 
             WHERE action LIKE 'security_%' AND created_at >= ?
             ORDER BY created_at DESC",
            [$startDate]
        );
    }

    /**
     * Cleanup old logs
     */
    public function cleanup($daysToKeep = 90) {
        $cutoff = date('Y-m-d', strtotime("-{$daysToKeep} days"));
        
        return $this->db->delete(
            'audit_log',
            'created_at < ?',
            [$cutoff]
        );
    }
}
