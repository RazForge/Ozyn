<?php
/**
 * Ozayn Database Initialization
 */

require_once __DIR__ . '/backend/database.php';

// Read schema file
$schema = file_get_contents(__DIR__ . '/database/schema.sql');

// Execute schema
$db = Database::getInstance();
$statements = array_filter(array_map('trim', explode(';', $schema)));

foreach ($statements as $statement) {
    if (!empty($statement)) {
        try {
            $db->getConnection()->exec($statement);
        } catch (PDOException $e) {
            echo "Error executing: " . substr($statement, 0, 50) . "...\n";
            echo "Error: " . $e->getMessage() . "\n";
        }
    }
}

echo "Database initialized successfully!\n";
echo "Database location: " . __DIR__ . "/database/ozayn.db\n";
