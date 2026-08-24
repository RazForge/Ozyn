<?php
/**
 * Ozayn Backup/Restore Tool
 * Backup and restore database and user data
 */

class BackupRestoreTool {
    
    private $db;
    private $backupDir;

    public function __construct() {
        $this->db = \Database::getInstance();
        $this->backupDir = __DIR__ . '/../backups/';
        
        if (!is_dir($this->backupDir)) {
            mkdir($this->backupDir, 0755, true);
        }
    }

    /**
     * Create full backup
     */
    public function createBackup($userId = null) {
        $timestamp = date('Y-m-d_H-i-s');
        $filename = "ozayn_backup_{$timestamp}.sql";
        $filepath = $this->backupDir . $filename;
        
        $tables = $this->getTables();
        $sql = "-- Ozayn Backup\n";
        $sql .= "-- Date: " . date('Y-m-d H:i:s') . "\n\n";
        
        foreach ($tables as $table) {
            $sql .= $this->exportTable($table, $userId);
        }
        
        file_put_contents($filepath, $sql);
        
        return [
            'filename' => $filename,
            'filepath' => $filepath,
            'size' => filesize($filepath),
            'tables' => count($tables)
        ];
    }

    /**
     * Get all tables
     */
    private function getTables() {
        $result = $this->db->fetchAll("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name");
        return array_column($result, 'name');
    }

    /**
     * Export a single table
     */
    private function exportTable($table, $userId = null) {
        $sql = "-- Table: {$table}\n";
        $sql .= "DROP TABLE IF EXISTS {$table};\n";
        
        // Get table structure
        $result = $this->db->fetch("SELECT sql FROM sqlite_master WHERE type='table' AND name=?", [$table]);
        if ($result) {
            $sql .= $result['sql'] . ";\n\n";
        }
        
        // Get data
        $where = $userId ? " WHERE user_id = {$userId}" : '';
        $rows = $this->db->fetchAll("SELECT * FROM {$table}{$where}");
        
        if (!empty($rows)) {
            $columns = array_keys($rows[0]);
            $sql .= "INSERT INTO {$table} (" . implode(', ', $columns) . ") VALUES\n";
            
            $values = [];
            foreach ($rows as $row) {
                $rowValues = array_map(function($v) {
                    if ($v === null) return 'NULL';
                    return "'" . addslashes($v) . "'";
                }, $row);
                $values[] = '(' . implode(', ', $rowValues) . ')';
            }
            
            $sql .= implode(",\n", $values) . ";\n\n";
        }
        
        return $sql;
    }

    /**
     * Restore from backup
     */
    public function restoreBackup($filename) {
        $filepath = $this->backupDir . $filename;
        
        if (!file_exists($filepath)) {
            return ['error' => 'Backup file not found'];
        }
        
        $sql = file_get_contents($filepath);
        
        // Split by semicolons and execute
        $statements = array_filter(array_map('trim', explode(';', $sql)));
        
        $executed = 0;
        $errors = [];
        
        foreach ($statements as $statement) {
            if (empty($statement) || substr($statement, 0, 2) === '--') {
                continue;
            }
            
            try {
                $this->db->query($statement);
                $executed++;
            } catch (Exception $e) {
                $errors[] = $e->getMessage();
            }
        }
        
        return [
            'success' => empty($errors),
            'statements_executed' => $executed,
            'errors' => $errors
        ];
    }

    /**
     * List available backups
     */
    public function listBackups() {
        $files = glob($this->backupDir . 'ozayn_backup_*.sql');
        
        $backups = [];
        foreach ($files as $file) {
            $backups[] = [
                'filename' => basename($file),
                'size' => filesize($file),
                'created' => date('Y-m-d H:i:s', filemtime($file))
            ];
        }
        
        usort($backups, function($a, $b) {
            return strtotime($b['created']) - strtotime($a['created']);
        });
        
        return $backups;
    }

    /**
     * Delete backup
     */
    public function deleteBackup($filename) {
        $filepath = $this->backupDir . $filename;
        
        if (file_exists($filepath)) {
            unlink($filepath);
            return true;
        }
        
        return false;
    }

    /**
     * Get backup statistics
     */
    public function getStats() {
        $backups = $this->listBackups();
        
        $totalSize = array_sum(array_column($backups, 'size'));
        
        return [
            'total_backups' => count($backups),
            'total_size' => $totalSize,
            'latest_backup' => !empty($backups) ? $backups[0]['created'] : null
        ];
    }

    /**
     * Auto-backup (for scheduled tasks)
     */
    public function autoBackup() {
        // Keep only last 7 backups
        $backups = $this->listBackups();
        
        if (count($backups) > 7) {
            $toDelete = array_slice($backups, 7);
            foreach ($toDelete as $backup) {
                $this->deleteBackup($backup['filename']);
            }
        }
        
        return $this->createBackup();
    }
}
