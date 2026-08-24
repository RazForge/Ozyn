<?php
/**
 * Ozayn Email Notification Tool
 * Send email notifications for alerts and events
 */

class EmailNotificationTool {
    
    private $db;
    private $config;

    public function __construct() {
        $this->db = \Database::getInstance();
        $this->loadConfig();
    }

    private function loadConfig() {
        $configPath = __DIR__ . '/../backend/config/email.json';
        if (file_exists($configPath)) {
            $this->config = json_decode(file_get_contents($configPath), true);
        } else {
            $this->config = [
                'enabled' => false,
                'smtp_host' => '',
                'smtp_port' => 587,
                'smtp_user' => '',
                'smtp_pass' => '',
                'from_email' => '',
                'from_name' => 'Ozayn'
            ];
        }
    }

    /**
     * Send email notification
     */
    public function send($to, $subject, $body, $isHtml = true) {
        if (!$this->config['enabled']) {
            return ['error' => 'Email notifications are not configured'];
        }

        $headers = [
            'From: ' . $this->config['from_name'] . ' <' . $this->config['from_email'] . '>',
            'Reply-To: ' . $this->config['from_email'],
            'X-Mailer: Ozayn/1.0'
        ];

        if ($isHtml) {
            $headers[] = 'MIME-Version: 1.0';
            $headers[] = 'Content-Type: text/html; charset=UTF-8';
        }

        // Use PHP mail() as fallback
        $result = @mail($to, $subject, $body, implode("\r\n", $headers));

        // Log the email
        $this->logEmail($to, $subject, $result);

        return [
            'success' => $result,
            'to' => $to,
            'subject' => $subject
        ];
    }

    /**
     * Send ARWE alert email
     */
    public function sendARWEAlert($to, $system, $status, $details) {
        $subject = "ARWE Alert: {$system} status changed to {$status}";
        
        $body = "<h2>ARWE System Alert</h2>";
        $body .= "<p><strong>System:</strong> " . ucfirst($system) . "</p>";
        $body .= "<p><strong>Status:</strong> " . ucfirst($status) . "</p>";
        $body .= "<p><strong>Details:</strong> {$details}</p>";
        $body .= "<p><strong>Time:</strong> " . date('Y-m-d H:i:s') . "</p>";
        $body .= "<hr><p><small>Sent by Ozayn - Personal AI Digital Twin</small></p>";

        return $this->send($to, $subject, $body);
    }

    /**
     * Send task reminder email
     */
    public function sendTaskReminder($to, $taskTitle, $dueDate = null) {
        $subject = "Task Reminder: {$taskTitle}";
        
        $body = "<h2>Task Reminder</h2>";
        $body .= "<p><strong>Task:</strong> {$taskTitle}</p>";
        if ($dueDate) {
            $body .= "<p><strong>Due:</strong> {$dueDate}</p>";
        }
        $body .= "<hr><p><small>Sent by Ozayn - Personal AI Digital Twin</small></p>";

        return $this->send($to, $subject, $body);
    }

    /**
     * Send daily summary email
     */
    public function sendDailySummary($to, $summary) {
        $subject = "Daily Summary - " . date('Y-m-d');
        
        $body = "<h2>Daily Summary</h2>";
        $body .= "<p>{$summary}</p>";
        $body .= "<hr><p><small>Sent by Ozayn - Personal AI Digital Twin</small></p>";

        return $this->send($to, $subject, $body);
    }

    /**
     * Log email
     */
    private function logEmail($to, $subject, $success) {
        $this->db->insert('audit_log', [
            'action' => 'email_sent',
            'details' => json_encode([
                'to' => $to,
                'subject' => $subject,
                'success' => $success
            ]),
            'result' => $success ? 'success' : 'failure'
        ]);
    }

    /**
     * Get email settings
     */
    public function getSettings() {
        return [
            'enabled' => $this->config['enabled'],
            'smtp_host' => $this->config['smtp_host'],
            'smtp_port' => $this->config['smtp_port'],
            'from_email' => $this->config['from_email'],
            'from_name' => $this->config['from_name']
        ];
    }

    /**
     * Update email settings
     */
    public function updateSettings($settings) {
        $this->config = array_merge($this->config, $settings);
        $configPath = __DIR__ . '/../backend/config/email.json';
        file_put_contents($configPath, json_encode($this->config, JSON_PRETTY_PRINT));
        return true;
    }
}
