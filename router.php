<?php
/**
 * Ozayn Router
 * Simple router to serve frontend and API
 */

$requestUri = $_SERVER['REQUEST_URI'];
$path = parse_url($requestUri, PHP_URL_PATH);

// Remove trailing slash
$path = rtrim($path, '/');

// Route API requests
if (strpos($path, '/ozayn/backend/api') === 0) {
    require __DIR__ . '/ozayn/backend/api/index.php';
    exit();
}

// Route frontend requests
if ($path === '/ozayn' || $path === '/ozayn/') {
    // Serve main page
    require __DIR__ . '/ozayn/frontend/index.html';
    exit();
}

// Serve static files
if (strpos($path, '/ozayn/') === 0) {
    $filePath = __DIR__ . $path;
    
    // Security: prevent directory traversal
    $realPath = realpath($filePath);
    $baseDir = realpath(__DIR__ . '/ozayn/frontend');
    
    if ($realPath && strpos($realPath, $baseDir) === 0 && file_exists($realPath)) {
        // Set content type
        $ext = pathinfo($realPath, PATHINFO_EXTENSION);
        $mimeTypes = [
            'html' => 'text/html',
            'css' => 'text/css',
            'js' => 'application/javascript',
            'json' => 'application/json',
            'png' => 'image/png',
            'jpg' => 'image/jpeg',
            'gif' => 'image/gif',
            'svg' => 'image/svg+xml',
            'ico' => 'image/x-icon'
        ];
        
        if (isset($mimeTypes[$ext])) {
            header('Content-Type: ' . $mimeTypes[$ext]);
        }
        
        readfile($realPath);
        exit();
    }
}

// 404 for everything else
http_response_code(404);
echo json_encode(['error' => 'Not found']);
