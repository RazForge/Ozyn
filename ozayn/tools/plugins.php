<?php
/**
 * Ozayn Plugin System
 * Load and manage custom plugins
 */

class PluginSystem {
    
    private $db;
    private $pluginsDir;
    private $loadedPlugins = [];

    public function __construct() {
        $this->db = \Database::getInstance();
        $this->pluginsDir = __DIR__ . '/../plugins/';
        
        if (!is_dir($this->pluginsDir)) {
            mkdir($this->pluginsDir, 0755, true);
        }
    }

    /**
     * Get all available plugins
     */
    public function getAvailablePlugins() {
        $plugins = [];
        
        // Get from database
        $dbPlugins = $this->db->fetchAll("SELECT * FROM plugins ORDER BY name");
        
        foreach ($dbPlugins as $plugin) {
            $plugins[$plugin['name']] = [
                'name' => $plugin['name'],
                'description' => $plugin['description'],
                'version' => $plugin['version'],
                'enabled' => $plugin['enabled'],
                'author' => $plugin['author'] ?? 'Unknown'
            ];
        }
        
        return $plugins;
    }

    /**
     * Install plugin
     */
    public function install($name, $description, $version, $author = null, $code = null) {
        // Check if already exists
        $existing = $this->db->fetch(
            "SELECT id FROM plugins WHERE name = ?",
            [$name]
        );
        
        if ($existing) {
            return ['error' => 'Plugin already installed'];
        }
        
        // Create plugin directory
        $pluginDir = $this->pluginsDir . $name . '/';
        if (!is_dir($pluginDir)) {
            mkdir($pluginDir, 0755, true);
        }
        
        // Save plugin code if provided
        if ($code) {
            file_put_contents($pluginDir . 'plugin.php', $code);
        }
        
        // Save to database
        $id = $this->db->insert('plugins', [
            'name' => $name,
            'description' => $description,
            'version' => $version,
            'author' => $author,
            'enabled' => 1,
            'installed_at' => date('Y-m-d H:i:s')
        ]);
        
        return [
            'success' => true,
            'id' => $id,
            'name' => $name
        ];
    }

    /**
     * Enable/disable plugin
     */
    public function toggle($name) {
        $plugin = $this->db->fetch(
            "SELECT * FROM plugins WHERE name = ?",
            [$name]
        );
        
        if (!$plugin) {
            return ['error' => 'Plugin not found'];
        }
        
        $newState = $plugin['enabled'] ? 0 : 1;
        
        $this->db->update('plugins', [
            'enabled' => $newState
        ], 'name = ?', [$name]);
        
        return [
            'success' => true,
            'enabled' => $newState
        ];
    }

    /**
     * Uninstall plugin
     */
    public function uninstall($name) {
        $plugin = $this->db->fetch(
            "SELECT * FROM plugins WHERE name = ?",
            [$name]
        );
        
        if (!$plugin) {
            return ['error' => 'Plugin not found'];
        }
        
        // Remove plugin directory
        $pluginDir = $this->pluginsDir . $name . '/';
        if (is_dir($pluginDir)) {
            // Delete files
            $files = glob($pluginDir . '*');
            foreach ($files as $file) {
                if (is_file($file)) {
                    unlink($file);
                }
            }
            rmdir($pluginDir);
        }
        
        // Remove from database
        $this->db->delete('plugins', 'name = ?', [$name]);
        
        return ['success' => true];
    }

    /**
     * Load enabled plugins
     */
    public function loadPlugins() {
        $plugins = $this->db->fetchAll(
            "SELECT * FROM plugins WHERE enabled = 1"
        );
        
        foreach ($plugins as $plugin) {
            $pluginFile = $this->pluginsDir . $plugin['name'] . '/plugin.php';
            if (file_exists($pluginFile)) {
                require_once $pluginFile;
                $this->loadedPlugins[$plugin['name']] = true;
            }
        }
        
        return count($this->loadedPlugins);
    }

    /**
     * Execute plugin hook
     */
    public function executeHook($hookName, $data = []) {
        $results = [];
        
        foreach ($this->loadedPlugins as $pluginName => $loaded) {
            $functionName = 'plugin_' . $pluginName . '_' . $hookName;
            if (function_exists($functionName)) {
                $results[$pluginName] = $functionName($data);
            }
        }
        
        return $results;
    }

    /**
     * Get plugin info
     */
    public function getPluginInfo($name) {
        $plugin = $this->db->fetch(
            "SELECT * FROM plugins WHERE name = ?",
            [$name]
        );
        
        if (!$plugin) {
            return null;
        }
        
        // Check for README
        $readmeFile = $this->pluginsDir . $name . '/README.md';
        $readme = null;
        if (file_exists($readmeFile)) {
            $readme = file_get_contents($readmeFile);
        }
        
        return [
            'name' => $plugin['name'],
            'description' => $plugin['description'],
            'version' => $plugin['version'],
            'author' => $plugin['author'],
            'enabled' => $plugin['enabled'],
            'installed_at' => $plugin['installed_at'],
            'readme' => $readme
        ];
    }

    /**
     * Get installed plugins count
     */
    public function getStats() {
        $result = $this->db->fetch(
            "SELECT 
                COUNT(*) as total,
                SUM(CASE WHEN enabled = 1 THEN 1 ELSE 0 END) as enabled
            FROM plugins"
        );
        
        return $result;
    }
}
