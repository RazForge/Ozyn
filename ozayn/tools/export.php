<?php
/**
 * Ozayn Export Tools
 * Export data to JSON, CSV, Markdown formats
 */

class ExportTools {
    
    private $db;

    public function __construct() {
        $this->db = \Database::getInstance();
    }

    public function export($type, $format = 'json', $options = []) {
        $data = $this->getData($type, $options);
        switch ($format) {
            case 'json': return $this->toJson($data, $options);
            case 'csv': return $this->toCsv($data, $options);
            case 'markdown': return $this->toMarkdown($data, $options);
            case 'xml': return $this->toXml($data, $options);
            default: return ['success' => false, 'error' => 'Unsupported format'];
        }
    }

    private function getData($type, $options) {
        $limit = $options['limit'] ?? 1000;
        $where = $options['where'] ?? '1=1';
        $params = $options['params'] ?? [];
        switch ($type) {
            case 'conversations':
                return $this->db->fetchAll(
                    "SELECT c.*, u.username FROM conversations c LEFT JOIN users u ON c.user_id = u.id WHERE {$where} ORDER BY c.created_at DESC LIMIT {$limit}",
                    $params
                );
            case 'messages':
                return $this->db->fetchAll(
                    "SELECT * FROM messages WHERE {$where} ORDER BY created_at DESC LIMIT {$limit}",
                    $params
                );
            case 'tasks':
                return $this->db->fetchAll(
                    "SELECT * FROM tasks WHERE {$where} ORDER BY created_at DESC LIMIT {$limit}",
                    $params
                );
            case 'decisions':
                return $this->db->fetchAll(
                    "SELECT * FROM decisions WHERE {$where} ORDER BY created_at DESC LIMIT {$limit}",
                    $params
                );
            case 'projects':
                return $this->db->fetchAll(
                    "SELECT * FROM projects WHERE {$where} ORDER BY created_at DESC LIMIT {$limit}",
                    $params
                );
            case 'audit':
                return $this->db->fetchAll(
                    "SELECT * FROM audit_log WHERE {$where} ORDER BY created_at DESC LIMIT {$limit}",
                    $params
                );
            case 'knowledge':
                return $this->db->fetchAll(
                    "SELECT * FROM knowledge WHERE {$where} ORDER BY created_at DESC LIMIT {$limit}",
                    $params
                );
            default:
                return [];
        }
    }

    private function toJson($data, $options) {
        $pretty = $options['pretty'] ?? true;
        $json = $pretty ? json_encode($data, JSON_PRETTY_PRINT) : json_encode($data);
        return ['success' => true, 'format' => 'json', 'content' => $json, 'count' => count($data)];
    }

    private function toCsv($data, $options) {
        if (empty($data)) {
            return ['success' => true, 'format' => 'csv', 'content' => '', 'count' => 0];
        }
        $delimiter = $options['delimiter'] ?? ',';
        $headers = array_keys($data[0]);
        $output = fopen('php://temp', 'r+');
        fputcsv($output, $headers, $delimiter);
        foreach ($data as $row) {
            fputcsv($output, $row, $delimiter);
        }
        rewind($output);
        $csv = stream_get_contents($output);
        fclose($output);
        return ['success' => true, 'format' => 'csv', 'content' => $csv, 'count' => count($data)];
    }

    private function toMarkdown($data, $options) {
        if (empty($data)) {
            return ['success' => true, 'format' => 'markdown', 'content' => 'No data', 'count' => 0];
        }
        $headers = array_keys($data[0]);
        $md = "| " . implode(" | ", $headers) . " |\n";
        $md .= "| " . implode(" | ", array_fill(0, count($headers), "---")) . " |\n";
        foreach ($data as $row) {
            $md .= "| " . implode(" | ", array_map(function($v) {
                return is_null($v) ? '' : str_replace('|', '\\|', (string)$v);
            }, $row)) . " |\n";
        }
        return ['success' => true, 'format' => 'markdown', 'content' => $md, 'count' => count($data)];
    }

    private function toXml($data, $options) {
        $root = $options['root'] ?? 'data';
        $xml = new SimpleXMLElement("<?xml version=\"1.0\" encoding=\"UTF-8\"?><{$root}/>");
        foreach ($data as $row) {
            $item = $xml->addChild('item');
            foreach ($row as $key => $value) {
                $item->addChild($key, htmlspecialchars((string)$value));
            }
        }
        return ['success' => true, 'format' => 'xml', 'content' => $xml->asXML(), 'count' => count($data)];
    }

    public function saveToFile($content, $filename, $format) {
        $ext = $format === 'markdown' ? 'md' : $format;
        $path = __DIR__ . '/../exports/' . $filename . '.' . $ext;
        $dir = dirname($path);
        if (!is_dir($dir)) {
            mkdir($dir, 0755, true);
        }
        $bytes = file_put_contents($path, $content);
        return ['success' => $bytes !== false, 'path' => $path, 'bytes' => $bytes];
    }

    public function getSupportedTypes() {
        return ['conversations', 'messages', 'tasks', 'decisions', 'projects', 'audit', 'knowledge'];
    }

    public function getSupportedFormats() {
        return ['json', 'csv', 'markdown', 'xml'];
    }
}
