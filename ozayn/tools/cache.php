<?php
/**
 * Ozayn Cache System
 * Simple file-based caching for performance
 */

class CacheSystem {
    
    private $cacheDir;
    private $defaultTTL;

    public function __construct($cacheDir = null, $defaultTTL = 3600) {
        $this->cacheDir = $cacheDir ?? __DIR__ . '/../cache/';
        $this->defaultTTL = $defaultTTL;
        
        if (!is_dir($this->cacheDir)) {
            mkdir($this->cacheDir, 0755, true);
        }
    }

    /**
     * Get cached value
     */
    public function get($key) {
        $file = $this->getCacheFile($key);
        
        if (!file_exists($file)) {
            return null;
        }
        
        $data = unserialize(file_get_contents($file));
        
        // Check if expired
        if (time() > $data['expires']) {
            unlink($file);
            return null;
        }
        
        return $data['value'];
    }

    /**
     * Set cache value
     */
    public function set($key, $value, $ttl = null) {
        $ttl = $ttl ?? $this->defaultTTL;
        $file = $this->getCacheFile($key);
        
        $data = [
            'key' => $key,
            'value' => $value,
            'created' => time(),
            'expires' => time() + $ttl
        ];
        
        return file_put_contents($file, serialize($data)) !== false;
    }

    /**
     * Delete cached value
     */
    public function delete($key) {
        $file = $this->getCacheFile($key);
        
        if (file_exists($file)) {
            return unlink($file);
        }
        
        return true;
    }

    /**
     * Check if key exists and is valid
     */
    public function has($key) {
        return $this->get($key) !== null;
    }

    /**
     * Clear all cache
     */
    public function clear() {
        $files = glob($this->cacheDir . '*.cache');
        
        foreach ($files as $file) {
            unlink($file);
        }
        
        return true;
    }

    /**
     * Get cache stats
     */
    public function getStats() {
        $files = glob($this->cacheDir . '*.cache');
        $totalSize = 0;
        $valid = 0;
        $expired = 0;
        
        foreach ($files as $file) {
            $totalSize += filesize($file);
            $data = unserialize(file_get_contents($file));
            
            if (time() > $data['expires']) {
                $expired++;
            } else {
                $valid++;
            }
        }
        
        return [
            'total_files' => count($files),
            'valid' => $valid,
            'expired' => $expired,
            'total_size' => $totalSize,
            'total_size_formatted' => $this->formatSize($totalSize)
        ];
    }

    /**
     * Cleanup expired cache
     */
    public function cleanup() {
        $files = glob($this->cacheDir . '*.cache');
        $removed = 0;
        
        foreach ($files as $file) {
            $data = unserialize(file_get_contents($file));
            
            if (time() > $data['expires']) {
                unlink($file);
                $removed++;
            }
        }
        
        return $removed;
    }

    /**
     * Get cache file path
     */
    private function getCacheFile($key) {
        $safeKey = preg_replace('/[^a-zA-Z0-9_]/', '_', $key);
        return $this->cacheDir . $safeKey . '.cache';
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

    /**
     * Remember - get or set
     */
    public function remember($key, $ttl, $callback) {
        $value = $this->get($key);
        
        if ($value === null) {
            $value = $callback();
            $this->set($key, $value, $ttl);
        }
        
        return $value;
    }

    /**
     * Cache ARWE status
     */
    public function cacheARWEStatus($system, $status, $ttl = 300) {
        return $this->set("arwe_{$system}", $status, $ttl);
    }

    /**
     * Get cached ARWE status
     */
    public function getARWEStatus($system) {
        return $this->get("arwe_{$system}");
    }
}
