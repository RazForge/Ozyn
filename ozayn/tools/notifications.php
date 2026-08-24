<?php
/**
 * Ozayn Notification Preferences
 * User-configurable notification settings
 */

class NotificationPrefs {
    
    private $db;

    public function __construct() {
        $this->db = \Database::getInstance();
    }

    public function get($userId) {
        $prefs = $this->db->fetch(
            "SELECT * FROM user_preferences WHERE user_id = ? AND `key` LIKE 'notif_%'",
            [$userId]
        );

        $defaults = [
            'notif_email_enabled' => 0,
            'notif_email_address' => '',
            'notif_task_reminders' => 1,
            'notif_deadline_alerts' => 1,
            'notif_security_alerts' => 1,
            'notif_arwe_alerts' => 1,
            'notif_system_alerts' => 1,
            'notif_chat_replies' => 1,
            'notif_digest_frequency' => 'daily',
            'notif_quiet_hours_start' => '22:00',
            'notif_quiet_hours_end' => '07:00',
            'notif_sound_enabled' => 1,
            'notif_desktop_enabled' => 0
        ];

        $result = [];
        foreach ($defaults as $key => $default) {
            $found = false;
            foreach ($prefs as $p) {
                if ($p['key'] === $key) {
                    $result[$key] = $p['value'];
                    $found = true;
                    break;
                }
            }
            if (!$found) {
                $result[$key] = $default;
            }
        }

        return $result;
    }

    public function set($userId, $key, $value) {
        $existing = $this->db->fetch(
            "SELECT id FROM user_preferences WHERE user_id = ? AND `key` = ?",
            [$userId, $key]
        );

        if ($existing) {
            $this->db->update('user_preferences', ['value' => $value], 'id = ?', [$existing['id']]);
        } else {
            $this->db->insert('user_preferences', [
                'user_id' => $userId,
                'key' => $key,
                'value' => $value
            ]);
        }

        return true;
    }

    public function setMultiple($userId, $prefs) {
        foreach ($prefs as $key => $value) {
            $this->set($userId, $key, $value);
        }
        return true;
    }

    public function isQuietHours($userId) {
        $prefs = $this->get($userId);
        $start = $prefs['notif_quiet_hours_start'];
        $end = $prefs['notif_quiet_hours_end'];
        $now = date('H:i');

        if ($start <= $end) {
            return ($now >= $start && $now <= $end);
        } else {
            return ($now >= $start || $now <= $end);
        }
    }

    public function shouldNotify($userId, $type) {
        $prefs = $this->get($userId);

        if ($this->isQuietHours($userId) && $type !== 'security_alerts') {
            return false;
        }

        $key = "notif_{$type}";
        return isset($prefs[$key]) && $prefs[$key];
    }

    public function getChannels($userId) {
        $prefs = $this->get($userId);
        $channels = ['in_app'];

        if (!empty($prefs['notif_email_enabled']) && !empty($prefs['notif_email_address'])) {
            $channels[] = 'email';
        }
        if (!empty($prefs['notif_desktop_enabled'])) {
            $channels[] = 'desktop';
        }

        return $channels;
    }

    public function reset($userId) {
        $this->db->delete('user_preferences', "user_id = ? AND `key` LIKE 'notif_%'", [$userId]);
        return true;
    }

    public function getTypes() {
        return [
            'task_reminders' => 'Task Reminders',
            'deadline_alerts' => 'Deadline Alerts',
            'security_alerts' => 'Security Alerts',
            'arwe_alerts' => 'ARWE System Alerts',
            'system_alerts' => 'System Alerts',
            'chat_replies' => 'Chat Replies'
        ];
    }
}
