<?php
/**
 * Ozayn File Version Control Tool
 * Track file changes and maintain version history
 */

class FileVersionTool {
    
    private $db;
    private $versionsDir;

    public function __construct() {
        $this->db = \Database::getInstance();
        $this->versionsDir = __DIR__ . '/../versions/';
        
        if (!is_dir($this->versionsDir)) {
            mkdir($this->versionsDir, 0755, true);
        }
    }

    /**
     * Save file version
     */
    public function saveVersion($filePath, $userId, $message = null) {
        if (!file_exists($filePath)) {
            return ['error' => 'File not found'];
        }

        $content = file_get_contents($filePath);
        $filename = basename($filePath);
        $hash = md5($content);
        
        // Check if version already exists
        $existing = $this->db->fetch(
            "SELECT * FROM file_versions WHERE file_path = ? AND hash = ?",
            [$filePath, $hash]
        );
        
        if ($existing) {
            return ['message' => 'No changes detected', 'version' => $existing['version']];
        }

        // Get next version number
        $lastVersion = $this->db->fetch(
            "SELECT MAX(version) as max_version FROM file_versions WHERE file_path = ?",
            [$filePath]
        );
        $version = ($lastVersion['max_version'] ?? 0) + 1;

        // Save version file
        $versionDir = $this->versionsDir . md5($filePath) . '/';
        if (!is_dir($versionDir)) {
            mkdir($versionDir, 0755, true);
        }
        
        $versionFile = $versionDir . "v{$version}_{$hash}";
        copy($filePath, $versionFile);

        // Save to database
        $id = $this->db->insert('file_versions', [
            'user_id' => $userId,
            'file_path' => $filePath,
            'filename' => $filename,
            'version' => $version,
            'hash' => $hash,
            'size' => filesize($filePath),
            'message' => $message,
            'version_file' => $versionFile
        ]);

        return [
            'success' => true,
            'version' => $version,
            'hash' => $hash
        ];
    }

    /**
     * Get file versions
     */
    public function getFileVersions($filePath, $limit = 20) {
        return $this->db->fetchAll(
            "SELECT * FROM file_versions WHERE file_path = ? ORDER BY version DESC LIMIT ?",
            [$filePath, $limit]
        );
    }

    /**
     * Get version by ID
     */
    public function getVersion($id) {
        return $this->db->fetch(
            "SELECT * FROM file_versions WHERE id = ?",
            [$id]
        );
    }

    /**
     * Restore file to specific version
     */
    public function restoreVersion($id, $userId) {
        $version = $this->getVersion($id);
        if (!$version) {
            return ['error' => 'Version not found'];
        }

        if (!file_exists($version['version_file'])) {
            return ['error' => 'Version file not found'];
        }

        // Backup current file first
        if (file_exists($version['file_path'])) {
            $this->saveVersion($version['file_path'], $userId, 'Auto-backup before restore');
        }

        // Restore the file
        copy($version['version_file'], $version['file_path']);

        return [
            'success' => true,
            'restored_version' => $version['version'],
            'file' => $version['file_path']
        ];
    }

    /**
     * Compare two versions
     */
    public function compareVersions($id1, $id2) {
        $v1 = $this->getVersion($id1);
        $v2 = $this->getVersion($id2);

        if (!$v1 || !$v2) {
            return ['error' => 'One or both versions not found'];
        }

        if (!file_exists($v1['version_file']) || !file_exists($v2['version_file'])) {
            return ['error' => 'Version files not found'];
        }

        $content1 = file_get_contents($v1['version_file']);
        $content2 = file_get_contents($v2['version_file']);

        // Simple diff (line by line)
        $lines1 = explode("\n", $content1);
        $lines2 = explode("\n", $content2);

        $diff = [];
        $maxLines = max(count($lines1), count($lines2));

        for ($i = 0; $i < $maxLines; $i++) {
            $line1 = $lines1[$i] ?? null;
            $line2 = $lines2[$i] ?? null;

            if ($line1 !== $line2) {
                $diff[] = [
                    'line' => $i + 1,
                    'old' => $line1,
                    'new' => $line2
                ];
            }
        }

        return [
            'version1' => $v1['version'],
            'version2' => $v2['version'],
            'changes' => $diff,
            'total_changes' => count($diff)
        ];
    }

    /**
     * Delete old versions (keep last N)
     */
    public function cleanupVersions($filePath, $keep = 10) {
        $versions = $this->getFileVersions($filePath, 1000);
        
        if (count($versions) <= $keep) {
            return ['deleted' => 0];
        }

        $toDelete = array_slice($versions, $keep);
        $deleted = 0;

        foreach ($toDelete as $version) {
            if (file_exists($version['version_file'])) {
                unlink($version['version_file']);
            }
            $this->db->delete('file_versions', 'id = ?', [$version['id']]);
            $deleted++;
        }

        return ['deleted' => $deleted];
    }

    /**
     * Get version history summary
     */
    public function getSummary($userId) {
        $result = $this->db->fetch(
            "SELECT 
                COUNT(DISTINCT file_path) as total_files,
                COUNT(*) as total_versions,
                SUM(size) as total_size
            FROM file_versions WHERE user_id = ?",
            [$userId]
        );

        return $result;
    }
}
