<?php
/**
 * Ozayn Logging System
 * Detailed logging for debugging and monitoring
 */

class LogSystem {
    
    private $logDir;
    private $logLevel;
    private $levels = [
        'debug' => 0,
        'info' => 1,
        'warning' => 2,
        'error' => 3,
        'critical' => 4
    ];

    public function __construct($logDir = null, $logLevel = 'info') {
        $this->logDir = $logDir ?? __DIR__ . '/../logs/';
        $this->logLevel = $logLevel;
        
        if (!is_dir($this->logDir)) {
            mkdir($this->logDir, 0755, true);
        }
    }

    /**
     * Log a message
     */
    public function log($level, $message, $context = [], $component = 'app') {
        if (!$this->shouldLog($level)) {
            return false;
        }

        $entry = [
            'timestamp' => date('Y-m-d H:i:s.u'),
            'level' => $level,
            'component' => $component,
            'message' => $message,
            'context' => $context,
            'pid' => getmypid(),
            'memory' => memory_get_usage(true)
        ];

        // Add stack trace for errors
        if (in_array($level, ['error', 'critical'])) {
            $entry['trace'] = $this->getStackTrace();
        }

        $logFile = $this->getLogFile($level);
        $line = json_encode($entry) . "\n";

        return file_put_contents($logFile, $line, FILE_APPEND | LOCK_EX) !== false;
    }

    /**
     * Convenience methods
     */
    public function debug($message, $context = [], $component = 'app') {
        return $this->log('debug', $message, $context, $component);
    }

    public function info($message, $context = [], $component = 'app') {
        return $this->log('info', $message, $context, $component);
    }

    public function warning($message, $context = [], $component = 'app') {
        return $this->log('warning', $message, $context, $component);
    }

    public function error($message, $context = [], $component = 'app') {
        return $this->log('error', $message, $context, $component);
    }

    public function critical($message, $context = [], $component = 'app') {
        return $this->log('critical', $message, $context, $component);
    }

    /**
     * Log API request
     */
    public function logRequest($method, $path, $userId = null, $statusCode = 200, $duration = 0) {
        return $this->info('API Request', [
            'method' => $method,
            'path' => $path,
            'user_id' => $userId,
            'status' => $statusCode,
            'duration_ms' => $duration
        ], 'api');
    }

    /**
     * Log chat message
     */
    public function logChat($userId, $message, $response, $duration = 0) {
        return $this->info('Chat Message', [
            'user_id' => $userId,
            'message' => substr($message, 0, 100),
            'response_length' => strlen($response),
            'duration_ms' => $duration
        ], 'chat');
    }

    /**
     * Log system event
     */
    public function logSystem($event, $details = []) {
        return $this->info('System Event', array_merge([
            'event' => $event
        ], $details), 'system');
    }

    /**
     * Log security event
     */
    public function logSecurity($event, $details = []) {
        return $this->warning('Security Event', array_merge([
            'event' => $event
        ], $details), 'security');
    }

    /**
     * Check if should log at this level
     */
    private function shouldLog($level) {
        return ($this->levels[$level] ?? 0) >= ($this->levels[$this->logLevel] ?? 0);
    }

    /**
     * Get log file for level
     */
    private function getLogFile($level) {
        $date = date('Y-m-d');
        $filename = "{$level}_{$date}.log";
        return $this->logDir . $filename;
    }

    /**
     * Get stack trace
     */
    private function getStackTrace() {
        $trace = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS, 10);
        $stack = [];

        foreach ($trace as $i => $frame) {
            $stack[] = [
                'function' => $frame['function'] ?? 'unknown',
                'file' => $frame['file'] ?? 'unknown',
                'line' => $frame['line'] ?? 0
            ];
        }

        return $stack;
    }

    /**
     * Get recent logs
     */
    public function getRecent($level = null, $limit = 100) {
        $logs = [];
        $files = glob($this->logDir . '*.log');

        // Sort by modification time (newest first)
        usort($files, function($a, $b) {
            return filemtime($b) - filemtime($a);
        });

        foreach ($files as $file) {
            if ($level && strpos(basename($file), $level) === false) {
                continue;
            }

            $lines = file($file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
            foreach (array_reverse($lines) as $line) {
                $entry = json_decode($line, true);
                if ($entry) {
                    $logs[] = $entry;
                    if (count($logs) >= $limit) {
                        return $logs;
                    }
                }
            }
        }

        return $logs;
    }

    /**
     * Get log stats
     */
    public function getStats() {
        $files = glob($this->logDir . '*.log');
        $stats = [
            'total_files' => count($files),
            'total_size' => 0,
            'by_level' => []
        ];

        foreach ($files as $file) {
            $stats['total_size'] += filesize($file);
            $basename = basename($file);
            $level = explode('_', $basename)[0];
            $stats['by_level'][$level] = ($stats['by_level'][$level] ?? 0) + 1;
        }

        $stats['total_size_formatted'] = $this->formatSize($stats['total_size']);

        return $stats;
    }

    /**
     * Clear old logs
     */
    public function clearOld($days = 30) {
        $files = glob($this->logDir . '*.log');
        $cutoff = time() - ($days * 86400);
        $removed = 0;

        foreach ($files as $file) {
            if (filemtime($file) < $cutoff) {
                unlink($file);
                $removed++;
            }
        }

        return $removed;
    }

    /**
     * Format file size
     */
    private function formatSize($bytes) {
        $units = ['B', 'KB', 'MB', 'GB'];
        $i = 0;

        while ($bytes >= 1024 && $i < count($units) - 1) {
            $bytes /= 1024;
            $i++;
        }

        return round($bytes, 2) . ' ' . $units[$i];
    }
}
