<?php
/**
 * Ozayn Web Search Tools
 * Internet search and URL fetching capabilities
 */

class WebSearchTools {
    
    private $db;

    public function __construct() {
        $this->db = \Database::getInstance();
    }

    /**
     * Search the web
     */
    public function search($query, $limit = 10) {
        $results = [];
        
        // Use DuckDuckGo HTML lite as a simple search
        $encodedQuery = urlencode($query);
        $url = "https://html.duckduckgo.com/html/?q={$encodedQuery}";
        
        $ch = curl_init($url);
        curl_setopt_array($ch, [
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_FOLLOWLOCATION => true,
            CURLOPT_TIMEOUT => 15,
            CURLOPT_USERAGENT => 'Mozilla/5.0 (compatible; Ozayn/1.0)'
        ]);
        
        $html = curl_exec($ch);
        $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
        curl_close($ch);
        
        if ($httpCode !== 200 || !$html) {
            return ['error' => 'Search failed', 'results' => []];
        }
        
        // Parse results from DuckDuckGo HTML
        preg_match_all('/<a[^>]+class="result__a"[^>]*href="([^"]*)"[^>]*>(.*?)<\/a>/si', $html, $links);
        preg_match_all('/<a[^>]+class="result__snippet"[^>]*>(.*?)<\/a>/si', $html, $snippets);
        
        $count = min($limit, count($links[1]));
        
        for ($i = 0; $i < $count; $i++) {
            $url = html_entity_decode(trim($links[1][$i]));
            // Extract actual URL from DuckDuckGo redirect
            if (preg_match('/uddg=([^&]+)/', $url, $m)) {
                $url = urldecode($m[1]);
            }
            
            $title = strip_tags($links[2][$i]);
            $snippet = isset($snippets[0][$i]) ? strip_tags($snippets[0][$i]) : '';
            
            $results[] = [
                'title' => trim($title),
                'url' => trim($url),
                'snippet' => trim($snippet)
            ];
        }
        
        // Log the search
        $this->logSearch($query, count($results));
        
        return ['results' => $results, 'query' => $query];
    }

    /**
     * Fetch URL content
     */
    public function fetchURL($url, $maxLength = 5000) {
        $ch = curl_init($url);
        curl_setopt_array($ch, [
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_FOLLOWLOCATION => true,
            CURLOPT_TIMEOUT => 15,
            CURLOPT_USERAGENT => 'Mozilla/5.0 (compatible; Ozayn/1.0)',
            CURLOPT_ENCODING => ''
        ]);
        
        $content = curl_exec($ch);
        $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
        $contentType = curl_getinfo($ch, CURLINFO_CONTENT_TYPE);
        curl_close($ch);
        
        if ($httpCode !== 200) {
            return ['error' => "HTTP {$httpCode}", 'url' => $url];
        }
        
        // Strip HTML tags if it's HTML content
        if (strpos($contentType, 'text/html') !== false) {
            $content = strip_tags($content);
            $content = preg_replace('/\s+/', ' ', $content);
        }
        
        $truncated = strlen($content) > $maxLength;
        if ($truncated) {
            $content = substr($content, 0, $maxLength) . '...';
        }
        
        return [
            'url' => $url,
            'content' => $content,
            'truncated' => $truncated,
            'content_type' => $contentType,
            'length' => strlen($content)
        ];
    }

    /**
     * Log search for audit
     */
    private function logSearch($query, $resultCount) {
        $this->db->insert('audit_log', [
            'action' => 'web_search',
            'details' => json_encode(['query' => $query, 'results' => $resultCount]),
            'result' => 'success'
        ]);
    }

    /**
     * Format search results for display
     */
    public function formatResults($results) {
        if (empty($results['results'])) {
            return "No results found for: {$results['query']}";
        }
        
        $output = "**Search Results for: {$results['query']}**\n\n";
        
        foreach ($results['results'] as $i => $result) {
            $output .= ($i + 1) . ". **{$result['title']}**\n";
            $output .= "   {$result['url']}\n";
            if (!empty($result['snippet'])) {
                $output .= "   {$result['snippet']}\n";
            }
            $output .= "\n";
        }
        
        return $output;
    }
}
