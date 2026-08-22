<?php
/**
 * Ozayn Configuration
 */

// Database
define('DB_PATH', __DIR__ . '/../../database/ozayn.db');

// Security
define('SESSION_LIFETIME', 3600 * 24); // 24 hours
define('CSRF_TOKEN_NAME', 'ozayn_csrf');

// API
define('API_VERSION', 'v1');
define('API_BASE_URL', '/ozayn/backend/api');

// AI Settings
define('AI_MODEL', 'local'); // 'local', 'openai', 'anthropic'
define('MAX_TOKENS', 2048);
define('TEMPERATURE', 0.7);

// Logging
define('LOG_PATH', __DIR__ . '/../../logs/');
define('LOG_LEVEL', 'info'); // 'debug', 'info', 'warning', 'error'

// ARWE Systems
define('ARWE_SYSTEMS', [
    'edunex' => ['name' => 'Edunex', 'type' => 'education'],
    'govyx' => ['name' => 'Govyx', 'type' => 'government'],
    'locify' => ['name' => 'Locify', 'type' => 'identity'],
    'terrachain' => ['name' => 'TerraChain', 'type' => 'transparency'],
    'bilen' => ['name' => 'Bilen', 'type' => 'intelligence'],
    'kidane' => ['name' => 'Kidane', 'type' => 'aerial_robotics'],
    'canivox' => ['name' => 'Canivox', 'type' => 'ground_robotics'],
]);
