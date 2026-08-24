<?php
/**
 * Ozayn Application Launcher
 * Launch and manage applications
 */

class AppLauncher {
    
    private $db;
    private $allowedApps;

    public function __construct() {
        $this->db = \Database::getInstance();
        $this->loadAllowedApps();
    }

    private function loadAllowedApps() {
        $configPath = __DIR__ . '/../backend/config/apps.json';
        if (file_exists($configPath)) {
            $this->allowedApps = json_decode(file_get_contents($configPath), true);
        } else {
            $this->allowedApps = $this->getDefaultApps();
        }
    }

    private function getDefaultApps() {
        return [
            'browser' => ['name' => 'Browser', 'command' => 'xdg-open', 'icon' => '🌐'],
            'terminal' => ['name' => 'Terminal', 'command' => 'xterm', 'icon' => '💻'],
            'editor' => ['name' => 'Text Editor', 'command' => 'xdg-open', 'icon' => '📝'],
            'files' => ['name' => 'File Manager', 'command' => 'xdg-open', 'icon' => '📁'],
            'calculator' => ['name' => 'Calculator', 'command' => 'gnome-calculator', 'icon' => '🔢'],
            'settings' => ['name' => 'Settings', 'command' => 'gnome-control-center', 'icon' => '⚙️'],
            'phpstorm' => ['name' => 'PhpStorm', 'command' => 'phpstorm', 'icon' => '🔧'],
            'vscode' => ['name' => 'VS Code', 'command' => 'code', 'icon' => '📝'],
            'chrome' => ['name' => 'Chrome', 'command' => 'google-chrome', 'icon' => '🌐'],
            'firefox' => ['name' => 'Firefox', 'command' => 'firefox', 'icon' => '🦊']
        ];
    }

    /**
     * List available applications
     */
    public function listApps() {
        return $this->allowedApps;
    }

    /**
     * Launch application
     */
    public function launch($appName, $args = []) {
        $lower = strtolower($appName);
        
        // Check if app is allowed
        if (!isset($this->allowedApps[$lower])) {
            return ['error' => "Application not allowed: {$appName}"];
        }

        $app = $this->allowedApps[$lower];
        $command = $app['command'];
        
        // Add arguments
        if (!empty($args)) {
            $command .= ' ' . implode(' ', array_map('escapeshellarg', $args));
        }

        // Execute in background
        $command .= ' > /dev/null 2>&1 &';
        exec($command, $output, $returnCode);
        
        return [
            'success' => $returnCode === 0,
            'app' => $app['name'],
            'command' => $command
        ];
    }

    /**
     * Open file with default application
     */
    public function openFile($path) {
        if (!file_exists($path)) {
            return ['error' => "File not found: {$path}"];
        }

        $command = "xdg-open " . escapeshellarg($path) . " > /dev/null 2>&1 &";
        exec($command, $output, $returnCode);
        
        return [
            'success' => $returnCode === 0,
            'file' => $path
        ];
    }

    /**
     * Open URL in browser
     */
    public function openURL($url) {
        $command = "xdg-open " . escapeshellarg($url) . " > /dev/null 2>&1 &";
        exec($command, $output, $returnCode);
        
        return [
            'success' => $returnCode === 0,
            'url' => $url
        ];
    }

    /**
     * Format app list
     */
    public function formatAppList() {
        $output = "**Available Applications**\n\n";
        
        foreach ($this->allowedApps as $key => $app) {
            $output .= "{$app['icon']} **{$app['name']}** ({$key})\n";
        }
        
        return $output;
    }
}
