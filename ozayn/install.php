<?php
/**
 * Ozayn Database Initialization
 *
 * SECURITY: This script creates the schema and must never be reachable over
 * the web. It is blocked by the web server config (see apache.conf / deploy.sh).
 * It also requires a one-time setup token to run and refuses to re-run if the
 * database already exists, so it cannot be used to (re)create accounts.
 */

require_once __DIR__ . '/backend/database.php';

// Refuse to run if the database is already initialized.
$dbPath = __DIR__ . '/database/ozayn.db';
if (file_exists($dbPath)) {
    die("Database already initialized. Remove install.php or the database to start over.\n");
}

// Require a setup token via env (OZAYN_SETUP_TOKEN) or a local token file.
// This prevents unauthorized execution even if the file is somehow served.
$setupToken = getenv('OZAYN_SETUP_TOKEN');
$tokenFile = __DIR__ . '/.setup_token';
if ($setupToken === false && file_exists($tokenFile)) {
    $setupToken = trim(file_get_contents($tokenFile));
}

if ($setupToken === false || $setupToken === '') {
    die("Setup token required. Set OZAYN_SETUP_TOKEN or create .setup_token before running install.\n");
}

$provided = $_SERVER['argv'][1] ?? ($_GET['token'] ?? null);
if (!is_string($provided) || !hash_equals($setupToken, $provided)) {
    die("Invalid or missing setup token.\n");
}

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
echo "No default accounts were created. Register the first user via the API.\n";

// Best-effort: remove the token file and this installer so they can't be reused.
@unlink($tokenFile);
@unlink(__FILE__);
