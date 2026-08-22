<?php
/**
 * Ozayn System Monitoring Tools
 * CPU, memory, disk, processes, network
 */

class SystemMonitor {

    /**
     * Get system overview
     */
    public function getOverview() {
        return [
            'hostname' => gethostname(),
            'os' => php_uname('s') . ' ' . php_uname('r'),
            'architecture' => php_uname('m'),
            'php_version' => phpversion(),
            'uptime' => $this->getUptime(),
            'load_average' => $this->getLoadAverage(),
            'cpu' => $this->getCpuInfo(),
            'memory' => $this->getMemoryInfo(),
            'disk' => $this->getDiskInfo(),
            'timestamp' => date('Y-m-d H:i:s')
        ];
    }

    /**
     * Get CPU information
     */
    public function getCpuInfo() {
        $info = [
            'model' => 'Unknown',
            'cores' => 0,
            'usage' => 0
        ];

        // Get CPU model
        if (is_readable('/proc/cpuinfo')) {
            $cpuinfo = file_get_contents('/proc/cpuinfo');
            if (preg_match('/model name\s*:\s*(.+)/i', $cpuinfo, $matches)) {
                $info['model'] = trim($matches[1]);
            }
            if (preg_match_all('/^processor\s*:/m', $cpuinfo, $matches)) {
                $info['cores'] = count($matches[0]);
            }
        }

        // Get CPU usage
        $info['usage'] = $this->getCpuUsage();
        $info['load_average'] = $this->getLoadAverage();

        return $info;
    }

    /**
     * Get CPU usage percentage
     */
    public function getCpuUsage() {
        $stat1 = file_get_contents('/proc/stat');
        usleep(100000); // 100ms
        $stat2 = file_get_contents('/proc/stat');

        if (!$stat1 || !$stat2) return 0;

        preg_match('/cpu\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)/', $stat1, $m1);
        preg_match('/cpu\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)/', $stat2, $m2);

        if (empty($m1) || empty($m2)) return 0;

        $total1 = array_sum(array_slice($m1, 1));
        $total2 = array_sum(array_slice($m2, 1));
        $idle1 = $m1[4];
        $idle2 = $m2[4];

        $totalDiff = $total2 - $total1;
        $idleDiff = $idle2 - $idle1;

        if ($totalDiff == 0) return 0;

        return round((1 - $idleDiff / $totalDiff) * 100, 1);
    }

    /**
     * Get memory information
     */
    public function getMemoryInfo() {
        $info = [
            'total' => 0,
            'used' => 0,
            'free' => 0,
            'available' => 0,
            'percent_used' => 0
        ];

        if (is_readable('/proc/meminfo')) {
            $meminfo = file_get_contents('/proc/meminfo');
            
            preg_match('/MemTotal:\s+(\d+)/', $meminfo, $total);
            preg_match('/MemFree:\s+(\d+)/', $meminfo, $free);
            preg_match('/MemAvailable:\s+(\d+)/', $meminfo, $available);
            preg_match('/Buffers:\s+(\d+)/', $meminfo, $buffers);
            preg_match('/Cached:\s+(\d+)/', $meminfo, $cached);

            $info['total'] = isset($total[1]) ? $total[1] * 1024 : 0;
            $info['free'] = isset($free[1]) ? $free[1] * 1024 : 0;
            $info['available'] = isset($available[1]) ? $available[1] * 1024 : $info['free'];
            $info['buffers'] = isset($buffers[1]) ? $buffers[1] * 1024 : 0;
            $info['cached'] = isset($cached[1]) ? $cached[1] * 1024 : 0;
            $info['used'] = $info['total'] - $info['available'];
            $info['percent_used'] = $info['total'] > 0 
                ? round($info['used'] / $info['total'] * 100, 1) 
                : 0;
        } else {
            // Fallback using PHP
            $info['total'] = ini_get('memory_limit');
            $info['used'] = memory_get_usage(true);
            $info['free'] = $info['total'] - $info['used'];
        }

        return $info;
    }

    /**
     * Get disk information
     */
    public function getDiskInfo() {
        $disks = [];
        
        // Get disk usage using df
        $output = [];
        exec('df -h 2>/dev/null', $output);
        
        foreach ($output as $line) {
            if (preg_match('/^(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(.+)$/', $line, $matches)) {
                $disks[] = [
                    'filesystem' => $matches[1],
                    'size' => $matches[2],
                    'used' => $matches[3],
                    'available' => $matches[4],
                    'percent_used' => $matches[5],
                    'mount_point' => $matches[6]
                ];
            }
        }

        return $disks;
    }

    /**
     * Get load average
     */
    public function getLoadAverage() {
        if (function_exists('sys_getloadavg')) {
            $load = sys_getloadavg();
            return [
                '1min' => round($load[0], 2),
                '5min' => round($load[1], 2),
                '15min' => round($load[2], 2)
            ];
        }
        
        if (is_readable('/proc/loadavg')) {
            $loadavg = file_get_contents('/proc/loadavg');
            $parts = explode(' ', $loadavg);
            return [
                '1min' => round(floatval($parts[0]), 2),
                '5min' => round(floatval($parts[1]), 2),
                '15min' => round(floatval($parts[2]), 2)
            ];
        }

        return ['1min' => 0, '5min' => 0, '15min' => 0];
    }

    /**
     * Get uptime
     */
    public function getUptime() {
        if (is_readable('/proc/uptime')) {
            $uptime = file_get_contents('/proc/uptime');
            $seconds = floatval(explode(' ', $uptime)[0]);
            
            $days = floor($seconds / 86400);
            $hours = floor(($seconds % 86400) / 3600);
            $minutes = floor(($seconds % 3600) / 60);
            
            return [
                'seconds' => $seconds,
                'formatted' => "{$days}d {$hours}h {$minutes}m"
            ];
        }

        return ['seconds' => 0, 'formatted' => 'Unknown'];
    }

    /**
     * Get running processes
     */
    public function getProcesses($sortBy = 'cpu', $limit = 20) {
        $processes = [];
        
        $output = [];
        exec('ps aux --sort=-' . $sortBy . ' 2>/dev/null | head -' . ($limit + 1), $output);
        
        // Skip header
        for ($i = 1; $i < count($output); $i++) {
            $parts = preg_split('/\s+/', $output[$i], 11);
            if (count($parts) >= 11) {
                $processes[] = [
                    'user' => $parts[0],
                    'pid' => intval($parts[1]),
                    'cpu' => floatval($parts[2]),
                    'mem' => floatval($parts[3]),
                    'vsz' => intval($parts[4]),
                    'rss' => intval($parts[5]),
                    'command' => $parts[10]
                ];
            }
        }

        return [
            'processes' => $processes,
            'count' => count($processes),
            'sort_by' => $sortBy
        ];
    }

    /**
     * Get process count
     */
    public function getProcessCount() {
        $output = [];
        exec('ps aux 2>/dev/null | wc -l', $output);
        return ['count' => max(0, intval($output[0] ?? 0) - 1)];
    }

    /**
     * Get network interfaces
     */
    public function getNetworkInfo() {
        $interfaces = [];
        
        // Get IP addresses
        $output = [];
        exec('ip -4 addr show 2>/dev/null', $output);
        
        $current = null;
        foreach ($output as $line) {
            if (preg_match('/^\d+:\s+(\w+):/', $line, $matches)) {
                $current = $matches[1];
                $interfaces[$current] = ['name' => $current, 'ips' => []];
            } elseif ($current && preg_match('/inet\s+(\d+\.\d+\.\d+\.\d+)/', $line, $matches)) {
                $interfaces[$current]['ips'][] = $matches[1];
            }
        }

        // Get network stats
        if (is_readable('/proc/net/dev')) {
            $netdev = file_get_contents('/proc/net/dev');
            preg_match_all('/(\w+):\s*(\d+)\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+(\d+)/', $netdev, $matches);
            
            for ($i = 0; $i < count($matches[1]); $i++) {
                $name = $matches[1][$i];
                if (!isset($interfaces[$name])) {
                    $interfaces[$name] = ['name' => $name, 'ips' => []];
                }
                $interfaces[$name]['rx_bytes'] = intval($matches[2][$i]);
                $interfaces[$name]['tx_bytes'] = intval($matches[3][$i]);
                $interfaces[$name]['rx_mb'] = round($matches[2][$i] / 1048576, 2);
                $interfaces[$name]['tx_mb'] = round($matches[3][$i] / 1048576, 2);
            }
        }

        return array_values($interfaces);
    }

    /**
     * Get top processes by CPU
     */
    public function getTopCpu($limit = 10) {
        return $this->getProcesses('cpu', $limit);
    }

    /**
     * Get top processes by memory
     */
    public function getTopMemory($limit = 10) {
        return $this->getProcesses('mem', $limit);
    }

    /**
     * Kill process (requires confirmation)
     */
    public function killProcess($pid, $signal = 15) {
        // Validate PID
        if (!is_numeric($pid) || $pid <= 0) {
            return ['error' => 'Invalid PID'];
        }

        // Check if process exists
        if (!file_exists("/proc/{$pid}")) {
            return ['error' => "Process {$pid} not found"];
        }

        // Safety: prevent killing init/systemd
        if ($pid == 1) {
            return ['error' => 'Cannot kill PID 1'];
        }

        posix_kill(intval($pid), intval($signal));
        
        return [
            'success' => true,
            'pid' => $pid,
            'signal' => $signal
        ];
    }

    /**
     * Get disk usage for specific path
     */
    public function getDirectorySize($path) {
        if (!is_dir($path)) {
            return ['error' => "Directory not found: {$path}"];
        }

        $size = 0;
        $files = 0;
        $directories = 0;

        $iterator = new RecursiveIteratorIterator(
            new RecursiveDirectoryIterator($path, RecursiveDirectoryIterator::SKIP_DOTS)
        );

        foreach ($iterator as $item) {
            if ($item->isFile()) {
                $size += $item->getSize();
                $files++;
            } else {
                $directories++;
            }
        }

        return [
            'path' => $path,
            'size_bytes' => $size,
            'size_formatted' => $this->formatBytes($size),
            'files' => $files,
            'directories' => $directories
        ];
    }

    /**
     * Get system temperature (if available)
     */
    public function getTemperature() {
        $temps = [];
        
        // Try thermal zones
        for ($i = 0; $i < 10; $i++) {
            $path = "/sys/class/thermal/thermal_zone{$i}/temp";
            if (is_readable($path)) {
                $temp = intval(file_get_contents($path)) / 1000;
                $type = @file_get_contents("/sys/class/thermal/thermal_zone{$i}/type");
                $temps[] = [
                    'zone' => $i,
                    'type' => trim($type ?: "Zone {$i}"),
                    'temperature' => round($temp, 1),
                    'celsius' => round($temp, 1),
                    'fahrenheit' => round($temp * 9/5 + 32, 1)
                ];
            }
        }

        return $temps;
    }

    /**
     * Get logged in users
     */
    public function getLoggedUsers() {
        $users = [];
        $output = [];
        exec('who 2>/dev/null', $output);
        
        foreach ($output as $line) {
            if (preg_match('/^(\w+)\s+(\S+)\s+(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2})/', $line, $matches)) {
                $users[] = [
                    'user' => $matches[1],
                    'terminal' => $matches[2],
                    'login_time' => $matches[3]
                ];
            }
        }

        return $users;
    }

    /**
     * Format bytes to human readable
     */
    private function formatBytes($bytes, $precision = 2) {
        $units = ['B', 'KB', 'MB', 'GB', 'TB'];
        
        $bytes = max($bytes, 0);
        $pow = floor(($bytes ? log($bytes) : 0) / log(1024));
        $pow = min($pow, count($units) - 1);
        
        $bytes /= pow(1024, $pow);
        
        return round($bytes, $precision) . ' ' . $units[$pow];
    }
}
