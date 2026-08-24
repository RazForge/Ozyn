<?php
/**
 * Ozayn System Health Monitor
 * Comprehensive system health checks and alerts
 */

require_once __DIR__ . '/../backend/database.php';

class HealthMonitor {
    
    private $db;
    private $checks = [];

    public function __construct() {
        $this->db = \Database::getInstance();
        $this->initChecks();
    }

    private function initChecks() {
        $this->checks = [
            'database' => [$this, 'checkDatabase'],
            'memory' => [$this, 'checkMemory'],
            'disk' => [$this, 'checkDisk'],
            'cpu' => [$this, 'checkCPU'],
            'api' => [$this, 'checkAPI'],
            'ml_server' => [$this, 'checkMLServer'],
            'arwe_systems' => [$this, 'checkARWESystems'],
            'sessions' => [$this, 'checkSessions'],
            'cache' => [$this, 'checkCache'],
            'logs' => [$this, 'checkLogs']
        ];
    }

    public function runAllChecks() {
        $results = [];
        foreach ($this->checks as $name => $check) {
            $results[$name] = $check();
        }
        return [
            'timestamp' => date('Y-m-d H:i:s'),
            'overall' => $this->getOverallStatus($results),
            'checks' => $results
        ];
    }

    public function runCheck($name) {
        if (!isset($this->checks[$name])) {
            return ['status' => 'unknown', 'error' => 'Check not found'];
        }
        return $this->checks[$name]();
    }

    private function getOverallStatus($results) {
        $statuses = array_column($results, 'status');
        if (in_array('critical', $statuses)) return 'critical';
        if (in_array('warning', $statuses)) return 'warning';
        return 'healthy';
    }

    private function checkDatabase() {
        try {
            $start = microtime(true);
            $this->db->fetch("SELECT COUNT(*) as count FROM users");
            $duration = round((microtime(true) - $start) * 1000, 2);
            
            $dbSize = filesize(__DIR__ . '/../database/ozayn.db');
            
            return [
                'status' => $duration < 100 ? 'healthy' : 'warning',
                'message' => 'Database responding',
                'metrics' => [
                    'response_time_ms' => $duration,
                    'size_mb' => round($dbSize / 1048576, 2)
                ]
            ];
        } catch (Exception $e) {
            return ['status' => 'critical', 'message' => $e->getMessage()];
        }
    }

    private function checkMemory() {
        $usage = memory_get_usage(true);
        $limit = ini_get('memory_limit');
        $limitBytes = $this->parseBytes($limit);
        
        // Handle unlimited memory (-1)
        if ($limitBytes <= 0) {
            return [
                'status' => 'healthy',
                'message' => 'Unlimited memory',
                'metrics' => [
                    'used_mb' => round($usage / 1048576, 2),
                    'limit_mb' => -1,
                    'percent' => 0
                ]
            ];
        }
        
        $percent = ($usage / $limitBytes) * 100;
        
        return [
            'status' => $percent > 80 ? 'critical' : ($percent > 60 ? 'warning' : 'healthy'),
            'message' => round($percent, 1) . '% used',
            'metrics' => [
                'used_mb' => round($usage / 1048576, 2),
                'limit_mb' => round($limitBytes / 1048576, 2),
                'percent' => round($percent, 1)
            ]
        ];
    }

    private function checkDisk() {
        $free = disk_free_space('/');
        $total = disk_total_space('/');
        $used = $total - $free;
        $percent = ($used / $total) * 100;
        
        return [
            'status' => $percent > 90 ? 'critical' : ($percent > 80 ? 'warning' : 'healthy'),
            'message' => round($percent, 1) . '% used',
            'metrics' => [
                'free_gb' => round($free / 1073741824, 2),
                'total_gb' => round($total / 1073741824, 2),
                'percent' => round($percent, 1)
            ]
        ];
    }

    private function checkCPU() {
        $load = sys_getloadavg();
        $percent = $load[0];
        
        return [
            'status' => $percent > 90 ? 'critical' : ($percent > 70 ? 'warning' : 'healthy'),
            'message' => round($percent, 1) . '% load',
            'metrics' => [
                'load_1m' => round($load[0], 2),
                'load_5m' => round($load[1], 2),
                'load_15m' => round($load[2], 2)
            ]
        ];
    }

    private function checkAPI() {
        try {
            $start = microtime(true);
            $ch = curl_init('http://localhost:9090/ozayn/backend/api/auth/login');
            curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
            curl_setopt($ch, CURLOPT_TIMEOUT, 5);
            curl_exec($ch);
            $duration = round((microtime(true) - $start) * 1000, 2);
            $code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
            curl_close($ch);
            
            return [
                'status' => $code === 200 ? 'healthy' : 'warning',
                'message' => "HTTP {$code}",
                'metrics' => ['response_time_ms' => $duration, 'http_code' => $code]
            ];
        } catch (Exception $e) {
            return ['status' => 'critical', 'message' => $e->getMessage()];
        }
    }

    private function checkMLServer() {
        try {
            $ch = curl_init('ws://localhost:8765');
            curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
            curl_setopt($ch, CURLOPT_TIMEOUT, 2);
            curl_exec($ch);
            $error = curl_error($ch);
            curl_close($ch);
            
            $connected = empty($error);
            return [
                'status' => $connected ? 'healthy' : 'warning',
                'message' => $connected ? 'ML server running' : 'ML server offline',
                'metrics' => ['connected' => $connected]
            ];
        } catch (Exception $e) {
            return ['status' => 'warning', 'message' => 'ML server not available'];
        }
    }

    private function checkARWESystems() {
        $configFile = __DIR__ . '/../config/arwe_apis.json';
        $configured = file_exists($configFile);
        
        return [
            'status' => 'healthy',
            'message' => $configured ? 'Configuration exists' : 'Using mock data',
            'metrics' => ['configured' => $configured]
        ];
    }

    private function checkSessions() {
        try {
            $count = $this->db->fetch("SELECT COUNT(*) as count FROM sessions WHERE expires_at > datetime('now')")['count'];
            return [
                'status' => $count > 100 ? 'warning' : 'healthy',
                'message' => "{$count} active sessions",
                'metrics' => ['active_sessions' => $count]
            ];
        } catch (Exception $e) {
            return ['status' => 'warning', 'message' => 'Sessions check failed'];
        }
    }

    private function checkCache() {
        $cacheDir = __DIR__ . '/../cache';
        $exists = is_dir($cacheDir);
        $files = $exists ? count(glob($cacheDir . '/*')) : 0;
        
        return [
            'status' => 'healthy',
            'message' => "{$files} cached files",
            'metrics' => ['files' => $files]
        ];
    }

    private function checkLogs() {
        $logDir = __DIR__ . '/../logs';
        $exists = is_dir($logDir);
        $files = $exists ? count(glob($logDir . '/*.log')) : 0;
        
        return [
            'status' => 'healthy',
            'message' => "{$files} log files",
            'metrics' => ['files' => $files]
        ];
    }

    private function parseBytes($value) {
        $unit = strtolower(substr($value, -1));
        $bytes = (int)$value;
        switch ($unit) {
            case 'g': $bytes *= 1073741824; break;
            case 'm': $bytes *= 1048576; break;
            case 'k': $bytes *= 1024; break;
        }
        return $bytes;
    }

    public function getAlerts() {
        $results = $this->runAllChecks();
        $alerts = [];
        foreach ($results['checks'] as $name => $check) {
            if ($check['status'] === 'critical' || $check['status'] === 'warning') {
                $alerts[] = [
                    'check' => $name,
                    'status' => $check['status'],
                    'message' => $check['message']
                ];
            }
        }
        return $alerts;
    }
}
