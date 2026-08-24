<?php
/**
 * Ozayn Performance Profiler
 * Track execution time and memory usage
 */

class Profiler {
    
    private static $marks = [];
    private static $counters = [];

    public static function start($label) {
        self::$marks[$label] = [
            'start' => microtime(true),
            'memory_start' => memory_get_usage(true)
        ];
    }

    public static function stop($label) {
        if (!isset(self::$marks[$label])) {
            return null;
        }
        $end = microtime(true);
        $memoryEnd = memory_get_usage(true);
        $start = self::$marks[$label]['start'];
        $memoryStart = self::$marks[$label]['memory_start'];

        $result = [
            'label' => $label,
            'duration_ms' => round(($end - $start) * 1000, 2),
            'memory_bytes' => $memoryEnd - $memoryStart,
            'memory_formatted' => self::formatBytes($memoryEnd - $memoryStart),
            'peak_memory' => self::formatBytes(memory_get_peak_usage(true))
        ];

        unset(self::$marks[$label]);
        return $result;
    }

    public static function elapsed($label) {
        if (!isset(self::$marks[$label])) {
            return null;
        }
        return round((microtime(true) - self::$marks[$label]['start']) * 1000, 2);
    }

    public static function increment($counter, $value = 1) {
        self::$counters[$counter] = (self::$counters[$counter] ?? 0) + $value;
    }

    public static function getCounter($counter) {
        return self::$counters[$counter] ?? 0;
    }

    public static function getAllCounters() {
        return self::$counters;
    }

    public static function reset() {
        self::$marks = [];
        self::$counters = [];
    }

    public static function getMemoryUsage() {
        return [
            'current' => self::formatBytes(memory_get_usage(true)),
            'peak' => self::formatBytes(memory_get_peak_usage(true)),
            'real' => self::formatBytes(memory_get_usage()),
            'real_peak' => self::formatBytes(memory_get_peak_usage())
        ];
    }

    public static function getServerInfo() {
        return [
            'php_version' => PHP_VERSION,
            'server_software' => $_SERVER['SERVER_SOFTWARE'] ?? 'CLI',
            'document_root' => $_SERVER['DOCUMENT_ROOT'] ?? getcwd(),
            'max_execution_time' => ini_get('max_execution_time') . 's',
            'memory_limit' => ini_get('memory_limit'),
            'upload_max_filesize' => ini_get('upload_max_filesize'),
            'post_max_size' => ini_get('post_max_size'),
            'extensions' => get_loaded_extensions()
        ];
    }

    public static function profile($callback, $label = 'profile') {
        self::start($label);
        $result = $callback();
        $stats = self::stop($label);
        return ['result' => $result, 'stats' => $stats];
    }

    public static function benchmark($callback, $iterations = 100) {
        $times = [];
        for ($i = 0; $i < $iterations; $i++) {
            $start = microtime(true);
            $callback();
            $times[] = (microtime(true) - $start) * 1000;
        }
        sort($times);
        return [
            'iterations' => $iterations,
            'avg_ms' => round(array_sum($times) / count($times), 2),
            'min_ms' => round(min($times), 2),
            'max_ms' => round(max($times), 2),
            'median_ms' => round($times[(int)($iterations / 2)], 2),
            'p95_ms' => round($times[(int)($iterations * 0.95)], 2),
            'p99_ms' => round($times[(int)($iterations * 0.99)], 2)
        ];
    }

    private static function formatBytes($bytes) {
        $units = ['B', 'KB', 'MB', 'GB'];
        $i = 0;
        $bytes = max($bytes, 0);
        while ($bytes >= 1024 && $i < count($units) - 1) {
            $bytes /= 1024;
            $i++;
        }
        return round($bytes, 2) . ' ' . $units[$i];
    }
}
