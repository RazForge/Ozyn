<?php
/**
 * Ozayn ARWE Integration Tools
 * Connects to Edunex, Govyx, Locify, TerraChain, Bilen, Kidane, Canivox
 */

class ARWETools {
    
    private $db;
    private $baseUrl;
    private $apiKey;

    public function __construct() {
        $this->db = \Database::getInstance();
        $this->loadConfig();
    }

    private function loadConfig() {
        $configPath = __DIR__ . '/../backend/config/arwe.json';
        if (file_exists($configPath)) {
            $config = json_decode(file_get_contents($configPath), true);
            $this->baseUrl = $config['base_url'] ?? '';
            $this->apiKey = $config['api_key'] ?? '';
        }
    }

    /**
     * Get status of all ARWE systems
     */
    public function getAllStatus() {
        $systems = [
            'edunex' => $this->getStatus('edunex'),
            'govyx' => $this->getStatus('govyx'),
            'locify' => $this->getStatus('locify'),
            'terrachain' => $this->getStatus('terrachain'),
            'bilen' => $this->getStatus('bilen'),
            'kidane' => $this->getStatus('kidane'),
            'canivox' => $this->getStatus('canivox')
        ];

        return $systems;
    }

    /**
     * Get status of a specific system
     */
    public function getStatus($system) {
        // Check if we have a cached status
        $cached = $this->db->fetch(
            "SELECT * FROM arwe_status WHERE system_name = ?",
            [$system]
        );

        if ($cached && strtotime($cached['last_checked']) > time() - 300) {
            return [
                'system' => $system,
                'status' => $cached['status'],
                'last_checked' => $cached['last_checked'],
                'details' => json_decode($cached['details'], true) ?? []
            ];
        }

        // Try to fetch from API
        $status = $this->fetchStatus($system);
        
        // Cache the result
        $this->db->query(
            "INSERT OR REPLACE INTO arwe_status (system_name, status, last_checked, details) VALUES (?, ?, ?, ?)",
            [$system, $status['status'], date('Y-m-d H:i:s'), json_encode($status['details'] ?? [])]
        );

        return $status;
    }

    /**
     * Fetch status from ARWE API
     */
    private function fetchStatus($system) {
        if (!$this->baseUrl) {
            return $this->getMockStatus($system);
        }

        $ch = curl_init("{$this->baseUrl}/api/v1/{$system}/status");
        curl_setopt_array($ch, [
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_HTTPHEADER => [
                'Authorization: Bearer ' . $this->apiKey
            ],
            CURLOPT_TIMEOUT => 10
        ]);

        $response = curl_exec($ch);
        $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
        curl_close($ch);

        if ($httpCode === 200) {
            $data = json_decode($response, true);
            return [
                'system' => $system,
                'status' => $data['status'] ?? 'unknown',
                'last_checked' => date('Y-m-d H:i:s'),
                'details' => $data['details'] ?? []
            ];
        }

        return $this->getMockStatus($system);
    }

    /**
     * Get mock status for demo/testing
     */
    private function getMockStatus($system) {
        $mockStatuses = [
            'edunex' => [
                'status' => 'online',
                'details' => [
                    'students' => 1250,
                    'teachers' => 85,
                    'courses' => 42,
                    'active_sessions' => 156
                ]
            ],
            'govyx' => [
                'status' => 'online',
                'details' => [
                    'departments' => 12,
                    'pending_tasks' => 34,
                    'completed_today' => 18,
                    'pending_approvals' => 7
                ]
            ],
            'locify' => [
                'status' => 'online',
                'details' => [
                    'citizens' => 15680,
                    'applications_today' => 45,
                    'pending' => 23,
                    'completed' => 22
                ]
            ],
            'terrachain' => [
                'status' => 'online',
                'details' => [
                    'records' => 8920,
                    'transactions_today' => 12,
                    'pending_verification' => 5
                ]
            ],
            'bilen' => [
                'status' => 'online',
                'details' => [
                    'active_cases' => 8,
                    'alerts_today' => 15,
                    'critical' => 2,
                    'sources_monitored' => 1250
                ]
            ],
            'kidane' => [
                'status' => 'online',
                'details' => [
                    'drones' => 12,
                    'active' => 8,
                    'charging' => 2,
                    'maintenance' => 1,
                    'offline' => 1
                ]
            ],
            'canivox' => [
                'status' => 'online',
                'details' => [
                    'robots' => 6,
                    'operational' => 4,
                    'standby' => 1,
                    'maintenance' => 1
                ]
            ]
        ];

        $status = $mockStatuses[$system] ?? ['status' => 'unknown', 'details' => []];
        $status['system'] = $system;
        $status['last_checked'] = date('Y-m-d H:i:s');
        
        return $status;
    }

    /**
     * Query Edunex
     */
    public function queryEdunex($query, $params = []) {
        return $this->querySystem('edunex', $query, $params);
    }

    /**
     * Query Govyx
     */
    public function queryGovyx($query, $params = []) {
        return $this->querySystem('govyx', $query, $params);
    }

    /**
     * Query Locify
     */
    public function queryLocify($query, $params = []) {
        return $this->querySystem('locify', $query, $params);
    }

    /**
     * Query TerraChain
     */
    public function queryTerraChain($query, $params = []) {
        return $this->querySystem('terrachain', $query, $params);
    }

    /**
     * Query Bilen
     */
    public function queryBilen($query, $params = []) {
        return $this->querySystem('bilen', $query, $params);
    }

    /**
     * Get Kidane fleet status
     */
    public function getKidaneStatus() {
        $status = $this->getStatus('kidane');
        return $this->formatRoboticsStatus('Kidane', $status);
    }

    /**
     * Get Canivox fleet status
     */
    public function getCanivoxStatus() {
        $status = $this->getStatus('canivox');
        return $this->formatRoboticsStatus('Canivox', $status);
    }

    /**
     * Format robotics status
     */
    private function formatRoboticsStatus($name, $status) {
        $details = $status['details'] ?? [];
        
        $output = "**{$name} Fleet Status**\n\n";
        $output .= "- Status: " . ucfirst($status['status']) . "\n";
        
        if ($name === 'Kidane') {
            $output .= "- Total Drones: " . ($details['drones'] ?? 0) . "\n";
            $output .= "- Active: " . ($details['active'] ?? 0) . "\n";
            $output .= "- Charging: " . ($details['charging'] ?? 0) . "\n";
            $output .= "- Maintenance: " . ($details['maintenance'] ?? 0) . "\n";
            $output .= "- Offline: " . ($details['offline'] ?? 0) . "\n";
        } else {
            $output .= "- Total Robots: " . ($details['robots'] ?? 0) . "\n";
            $output .= "- Operational: " . ($details['operational'] ?? 0) . "\n";
            $output .= "- Standby: " . ($details['standby'] ?? 0) . "\n";
            $output .= "- Maintenance: " . ($details['maintenance'] ?? 0) . "\n";
        }
        
        return $output;
    }

    /**
     * Generic system query
     */
    private function querySystem($system, $query, $params) {
        if (!$this->baseUrl) {
            return [
                'system' => $system,
                'query' => $query,
                'results' => [],
                'message' => 'ARWE API not configured'
            ];
        }

        $ch = curl_init("{$this->baseUrl}/api/v1/{$system}/query");
        curl_setopt_array($ch, [
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_POST => true,
            CURLOPT_POSTFIELDS => json_encode(['query' => $query, 'params' => $params]),
            CURLOPT_HTTPHEADER => [
                'Content-Type: application/json',
                'Authorization: Bearer ' . $this->apiKey
            ],
            CURLOPT_TIMEOUT => 30
        ]);

        $response = curl_exec($ch);
        $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
        curl_close($ch);

        if ($httpCode === 200) {
            return json_decode($response, true);
        }

        return [
            'system' => $system,
            'query' => $query,
            'error' => 'Query failed',
            'http_code' => $httpCode
        ];
    }

    /**
     * Format system status for chat
     */
    public function formatStatus($system, $status) {
        $details = $status['details'] ?? [];
        $output = "**" . ucfirst($system) . " Status**: " . ucfirst($status['status']) . "\n\n";
        
        foreach ($details as $key => $value) {
            $label = ucwords(str_replace('_', ' ', $key));
            $output .= "- {$label}: {$value}\n";
        }
        
        return $output;
    }

    /**
     * Get ARWE summary for daily briefing
     */
    public function getDailyBriefing() {
        $allStatus = $this->getAllStatus();
        
        $output = "**ARWE Daily Briefing**\n";
        $output .= date('Y-m-d H:i:s') . "\n\n";
        
        foreach ($allStatus as $system => $status) {
            $statusIcon = $status['status'] === 'online' ? '🟢' : '🔴';
            $output .= "{$statusIcon} **" . ucfirst($system) . "**: " . ucfirst($status['status']) . "\n";
        }
        
        return $output;
    }
}
