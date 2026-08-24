<?php
/**
 * Ozayn WebSocket Server
 * Real-time notifications and updates
 * 
 * Usage: php websocket_server.php
 * Requires: Ratchet library (composer require cboden/ratchet)
 */

require_once __DIR__ . '/vendor/autoload.php';

use Ratchet\MessageComponentInterface;
use Ratchet\ConnectionInterface;
use Ratchet\Server\IoServer;
use Ratchet\Http\HttpServer;
use Ratchet\WebSocket\WsServer;

class OzaynWebSocket implements MessageComponentInterface {
    protected $clients;
    protected $users; // Map connection to user ID

    public function __construct() {
        $this->clients = new \SplObjectStorage;
        $this->users = [];
    }

    public function onOpen(ConnectionInterface $conn) {
        $this->clients->attach($conn);
        echo "New connection! ({$conn->resourceId})\n";
    }

    public function onMessage(ConnectionInterface $from, $msg) {
        $data = json_decode($msg, true);
        
        if (!$data) {
            return;
        }

        switch ($data['type'] ?? '') {
            case 'auth':
                // Associate connection with user
                $userId = $data['user_id'] ?? null;
                if ($userId) {
                    $this->users[$from->resourceId] = $userId;
                    $from->send(json_encode([
                        'type' => 'auth_success',
                        'message' => 'Authenticated'
                    ]));
                }
                break;

            case 'subscribe':
                // Subscribe to channel (e.g., 'arwe', 'notifications')
                $channel = $data['channel'] ?? 'general';
                $from->send(json_encode([
                    'type' => 'subscribed',
                    'channel' => $channel
                ]));
                break;

            case 'ping':
                $from->send(json_encode(['type' => 'pong']));
                break;
        }
    }

    public function onClose(ConnectionInterface $conn) {
        $this->clients->detach($conn);
        unset($this->users[$conn->resourceId]);
        echo "Connection closed! ({$conn->resourceId})\n";
    }

    public function onError(ConnectionInterface $conn, \Exception $e) {
        echo "Error: {$e->getMessage()}\n";
        $conn->close();
    }

    /**
     * Send notification to a specific user
     */
    public function sendToUser($userId, $data) {
        foreach ($this->clients as $client) {
            if (isset($this->users[$client->resourceId]) && 
                $this->users[$client->resourceId] == $userId) {
                $client->send(json_encode($data));
            }
        }
    }

    /**
     * Broadcast to all connected clients
     */
    public function broadcast($data) {
        foreach ($this->clients as $client) {
            $client->send(json_encode($data));
        }
    }
}

// Start server
echo "Starting Ozayn WebSocket Server on ws://localhost:9000\n";

$server = IoServer::factory(
    new HttpServer(
        new WsServer(
            new OzaynWebSocket()
        )
    ),
    9000
);

$server->run();
