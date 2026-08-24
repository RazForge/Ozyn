<?php
/**
 * Ozayn Rate Limiter
 * Protect API from abuse
 */

class RateLimiter {
    
    private $db;
    private $cache;
    private $limits = [
        'default' => ['requests' => 60, 'window' => 60], // 60 requests per minute
        'chat' => ['requests' => 30, 'window' => 60], // 30 chat messages per minute
        'auth' => ['requests' => 5, 'window' => 300], // 5 login attempts per 5 minutes
        'export' => ['requests' => 3, 'window' => 3600], // 3 exports per hour
    ];

    public function __construct() {
        $this->db = \Database::getInstance();
        $this->cache = new CacheSystem();
    }

    /**
     * Check if request is allowed
     */
    public function isAllowed($identifier, $endpoint = 'default') {
        $limit = $this->limits[$endpoint] ?? $this->limits['default'];
        $key = "rate_{$endpoint}_{$identifier}";
        
        $requests = $this->cache->get($key) ?? [];
        $window = $limit['window'];
        $now = time();
        
        // Remove old requests outside window
        $requests = array_filter($requests, function($timestamp) use ($now, $window) {
            return ($now - $timestamp) < $window;
        });
        
        // Check if limit exceeded
        if (count($requests) >= $limit['requests']) {
            $oldest = min($requests);
            $retryAfter = $window - ($now - $oldest);
            
            return [
                'allowed' => false,
                'retry_after' => $retryAfter,
                'limit' => $limit['requests'],
                'remaining' => 0,
                'reset_at' => $oldest + $window
            ];
        }
        
        return [
            'allowed' => true,
            'limit' => $limit['requests'],
            'remaining' => $limit['requests'] - count($requests) - 1,
            'reset_at' => $now + $window
        ];
    }

    /**
     * Record a request
     */
    public function record($identifier, $endpoint = 'default') {
        $key = "rate_{$endpoint}_{$identifier}";
        $requests = $this->cache->get($key) ?? [];
        
        $requests[] = time();
        
        // Keep only requests within window
        $limit = $this->limits[$endpoint] ?? $this->limits['default'];
        $window = $limit['window'];
        $now = time();
        
        $requests = array_filter($requests, function($timestamp) use ($now, $window) {
            return ($now - $timestamp) < $window;
        });
        
        $this->cache->set($key, array_values($requests), $window);
        
        return true;
    }

    /**
     * Get rate limit headers
     */
    public function getHeaders($identifier, $endpoint = 'default') {
        $status = $this->isAllowed($identifier, $endpoint);
        
        return [
            'X-RateLimit-Limit' => $status['limit'],
            'X-RateLimit-Remaining' => $status['remaining'],
            'X-RateLimit-Reset' => $status['reset_at'],
            'Retry-After' => $status['retry_after'] ?? 0
        ];
    }

    /**
     * Check IP rate limit
     */
    public function checkIP($ip, $endpoint = 'default') {
        return $this->isAllowed($ip, $endpoint);
    }

    /**
     * Check user rate limit
     */
    public function checkUser($userId, $endpoint = 'default') {
        return $this->isAllowed("user_{$userId}", $endpoint);
    }

    /**
     * Record IP request
     */
    public function recordIP($ip, $endpoint = 'default') {
        return $this->record($ip, $endpoint);
    }

    /**
     * Record user request
     */
    public function recordUser($userId, $endpoint = 'default') {
        return $this->record("user_{$userId}", $endpoint);
    }

    /**
     * Get stats
     */
    public function getStats() {
        $stats = [];
        
        foreach ($this->limits as $endpoint => $limit) {
            $stats[$endpoint] = [
                'requests' => $limit['requests'],
                'window' => $limit['window']
            ];
        }
        
        return $stats;
    }

    /**
     * Update limits
     */
    public function updateLimits($endpoint, $requests, $window) {
        $this->limits[$endpoint] = [
            'requests' => $requests,
            'window' => $window
        ];
        
        return true;
    }
}
