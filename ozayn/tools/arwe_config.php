<?php
/**
 * Ozayn ARWE API Configuration
 * Store real API endpoints and credentials for ARWE systems
 */

require_once __DIR__ . '/../backend/database.php';

class ARWEConfig {
    
    private $db;
    private $configFile;

    public function __construct() {
        $this->db = \Database::getInstance();
        $this->configFile = __DIR__ . '/../config/arwe_apis.json';
        $this->ensureTable();
    }

    private function ensureTable() {
        $this->db->query("CREATE TABLE IF NOT EXISTS arwe_api_configs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            system_name TEXT NOT NULL UNIQUE,
            api_url TEXT,
            api_key TEXT,
            api_secret TEXT,
            username TEXT,
            password TEXT,
            auth_type TEXT DEFAULT 'none',
            timeout INTEGER DEFAULT 30,
            is_active INTEGER DEFAULT 1,
            last_sync DATETIME,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )");
    }

    public function getAll() {
        return $this->db->fetchAll("SELECT * FROM arwe_api_configs ORDER BY system_name");
    }

    public function get($systemName) {
        return $this->db->fetch("SELECT * FROM arwe_api_configs WHERE system_name = ?", [$systemName]);
    }

    public function save($systemName, $config) {
        $existing = $this->get($systemName);
        $data = [
            'system_name' => $systemName,
            'api_url' => $config['api_url'] ?? '',
            'api_key' => $config['api_key'] ?? '',
            'api_secret' => $config['api_secret'] ?? '',
            'username' => $config['username'] ?? '',
            'password' => $config['password'] ?? '',
            'auth_type' => $config['auth_type'] ?? 'none',
            'timeout' => $config['timeout'] ?? 30,
            'is_active' => $config['is_active'] ?? 1,
            'updated_at' => date('Y-m-d H:i:s')
        ];

        if ($existing) {
            $this->db->update('arwe_api_configs', $data, 'system_name = ?', [$systemName]);
        } else {
            $this->db->insert('arwe_api_configs', $data);
        }

        return true;
    }

    public function delete($systemName) {
        $this->db->delete('arwe_api_configs', 'system_name = ?', [$systemName]);
        return true;
    }

    public function testConnection($systemName) {
        $config = $this->get($systemName);
        if (!$config) {
            return ['success' => false, 'error' => 'System not configured'];
        }

        if (empty($config['api_url'])) {
            return ['success' => false, 'error' => 'No API URL configured'];
        }

        $startTime = microtime(true);
        $ch = curl_init();
        curl_setopt($ch, CURLOPT_URL, $config['api_url']);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($ch, CURLOPT_TIMEOUT, $config['timeout']);
        curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 10);
        curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false);

        // Add auth headers
        if ($config['auth_type'] === 'bearer' && !empty($config['api_key'])) {
            curl_setopt($ch, CURLOPT_HTTPHEADER, ['Authorization: Bearer ' . $config['api_key']]);
        } elseif ($config['auth_type'] === 'basic' && !empty($config['username'])) {
            curl_setopt($ch, CURLOPT_USERPWD, $config['username'] . ':' . $config['password']);
        } elseif ($config['auth_type'] === 'api_key' && !empty($config['api_key'])) {
            curl_setopt($ch, CURLOPT_HTTPHEADER, ['X-API-Key: ' . $config['api_key']]);
        }

        $response = curl_exec($ch);
        $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
        $error = curl_error($ch);
        $duration = round((microtime(true) - $startTime) * 1000, 2);
        curl_close($ch);

        if ($error) {
            return [
                'success' => false,
                'error' => $error,
                'duration_ms' => $duration
            ];
        }

        $this->db->update('arwe_api_configs', [
            'last_sync' => date('Y-m-d H:i:s')
        ], 'system_name = ?', [$systemName]);

        return [
            'success' => $httpCode >= 200 && $httpCode < 400,
            'http_code' => $httpCode,
            'duration_ms' => $duration,
            'response_size' => strlen($response ?? '')
        ];
    }

    public function getSystems() {
        return [
            'edunex' => ['name' => 'Edunex', 'description' => 'Education Platform', 'icon' => '📚'],
            'govyx' => ['name' => 'Govyx', 'description' => 'Government Services', 'icon' => '🏛️'],
            'locify' => ['name' => 'Locify', 'description' => 'Digital Identity', 'icon' => '📍'],
            'terrachain' => ['name' => 'TerraChain', 'description' => 'Land Transparency', 'icon' => '🌍'],
            'bilen' => ['name' => 'Bilen', 'description' => 'Security Intelligence', 'icon' => '🛡️'],
            'kidane' => ['name' => 'Kidane', 'description' => 'Aerial Robotics Fleet', 'icon' => '🚁'],
            'canivox' => ['name' => 'Canivox', 'description' => 'Ground Robotics Fleet', 'icon' => '🤖'],
            'ozayn' => ['name' => 'Ozayn', 'description' => 'AI Engine', 'icon' => '🧠']
        ];
    }

    public function exportConfig() {
        $configs = $this->getAll();
        $export = [];
        foreach ($configs as $config) {
            $export[$config['system_name']] = [
                'api_url' => $config['api_url'],
                'auth_type' => $config['auth_type'],
                'is_active' => $config['is_active'],
                'last_sync' => $config['last_sync']
            ];
            // Don't export secrets
        }
        return $export;
    }

    public function importConfig($data) {
        foreach ($data as $systemName => $config) {
            $this->save($systemName, $config);
        }
        return true;
    }
}
