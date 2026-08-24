<?php
/**
 * Ozayn Analytics Tool
 * Track and display usage statistics
 */

class AnalyticsTool {
    
    private $db;

    public function __construct() {
        $this->db = \Database::getInstance();
    }

    /**
     * Get user statistics
     */
    public function getUserStats($userId) {
        $stats = [];
        
        // Total messages
        $result = $this->db->fetch(
            "SELECT COUNT(*) as count FROM messages m 
             JOIN conversations c ON m.conversation_id = c.id 
             WHERE c.user_id = ?",
            [$userId]
        );
        $stats['total_messages'] = $result['count'] ?? 0;
        
        // Total conversations
        $result = $this->db->fetch(
            "SELECT COUNT(*) as count FROM conversations WHERE user_id = ?",
            [$userId]
        );
        $stats['total_conversations'] = $result['count'] ?? 0;
        
        // Total projects
        $result = $this->db->fetch(
            "SELECT COUNT(*) as count FROM projects WHERE user_id = ?",
            [$userId]
        );
        $stats['total_projects'] = $result['count'] ?? 0;
        
        // Total tasks
        $result = $this->db->fetch(
            "SELECT COUNT(*) as count FROM tasks WHERE user_id = ?",
            [$userId]
        );
        $stats['total_tasks'] = $result['count'] ?? 0;
        
        // Completed tasks
        $result = $this->db->fetch(
            "SELECT COUNT(*) as count FROM tasks WHERE user_id = ? AND status = 'completed'",
            [$userId]
        );
        $stats['completed_tasks'] = $result['count'] ?? 0;
        
        // Total knowledge entries
        $result = $this->db->fetch(
            "SELECT COUNT(*) as count FROM knowledge WHERE user_id = ?",
            [$userId]
        );
        $stats['total_knowledge'] = $result['count'] ?? 0;
        
        // Total decisions
        $result = $this->db->fetch(
            "SELECT COUNT(*) as count FROM decisions WHERE user_id = ?",
            [$userId]
        );
        $stats['total_decisions'] = $result['count'] ?? 0;
        
        // Account age
        $result = $this->db->fetch(
            "SELECT created_at FROM users WHERE id = ?",
            [$userId]
        );
        if ($result) {
            $created = new DateTime($result['created_at']);
            $now = new DateTime();
            $stats['account_age_days'] = $created->diff($now)->days;
        }
        
        // Messages per day (last 7 days)
        $stats['messages_this_week'] = $this->getMessagesThisWeek($userId);
        
        // Task completion rate
        $stats['task_completion_rate'] = $stats['total_tasks'] > 0 
            ? round(($stats['completed_tasks'] / $stats['total_tasks']) * 100, 1)
            : 0;
        
        return $stats;
    }

    /**
     * Get messages this week
     */
    private function getMessagesThisWeek($userId) {
        $result = $this->db->fetch(
            "SELECT COUNT(*) as count FROM messages m 
             JOIN conversations c ON m.conversation_id = c.id 
             WHERE c.user_id = ? AND m.created_at >= datetime('now', '-7 days')",
            [$userId]
        );
        return $result['count'] ?? 0;
    }

    /**
     * Get activity timeline
     */
    public function getActivityTimeline($userId, $limit = 10) {
        $activities = [];
        
        // Recent messages
        $messages = $this->db->fetchAll(
            "SELECT m.content, m.created_at, c.title as conv_title 
             FROM messages m 
             JOIN conversations c ON m.conversation_id = c.id 
             WHERE c.user_id = ? AND m.role = 'user'
             ORDER BY m.created_at DESC LIMIT ?",
            [$userId, $limit]
        );
        
        foreach ($messages as $msg) {
            $activities[] = [
                'type' => 'message',
                'content' => substr($msg['content'], 0, 100),
                'timestamp' => $msg['created_at'],
                'icon' => '💬'
            ];
        }
        
        // Sort by timestamp
        usort($activities, function($a, $b) {
            return strtotime($b['timestamp']) - strtotime($a['timestamp']);
        });
        
        return array_slice($activities, 0, $limit);
    }

    /**
     * Get usage trends
     */
    public function getUsageTrends($userId, $days = 30) {
        $trends = [];
        
        // Messages per day
        $results = $this->db->fetchAll(
            "SELECT DATE(m.created_at) as date, COUNT(*) as count 
             FROM messages m 
             JOIN conversations c ON m.conversation_id = c.id 
             WHERE c.user_id = ? AND m.created_at >= datetime('now', '-' || ? || ' days')
             GROUP BY DATE(m.created_at)
             ORDER BY date",
            [$userId, $days]
        );
        
        foreach ($results as $row) {
            $trends['messages'][] = [
                'date' => $row['date'],
                'count' => $row['count']
            ];
        }
        
        return $trends;
    }

    /**
     * Format stats for display
     */
    public function formatStats($stats) {
        $output = "**Your Statistics**\n\n";
        
        $output .= "**Activity**\n";
        $output .= "- Messages: {$stats['total_messages']}\n";
        $output .= "- Conversations: {$stats['total_conversations']}\n";
        $output .= "- This Week: {$stats['messages_this_week']} messages\n\n";
        
        $output .= "**Projects**\n";
        $output .= "- Total: {$stats['total_projects']}\n\n";
        
        $output .= "**Tasks**\n";
        $output .= "- Total: {$stats['total_tasks']}\n";
        $output .= "- Completed: {$stats['completed_tasks']}\n";
        $output .= "- Completion Rate: {$stats['task_completion_rate']}%\n\n";
        
        $output .= "**Knowledge**\n";
        $output .= "- Entries: {$stats['total_knowledge']}\n\n";
        
        $output .= "**Decisions**\n";
        $output .= "- Total: {$stats['total_decisions']}\n\n";
        
        $output .= "**Account**\n";
        $output .= "- Age: {$stats['account_age_days']} days\n";
        
        return $output;
    }
}
