<?php
/**
 * Ozayn REST API Client
 * Make HTTP requests to external services
 */

class RestClient {
    
    private $defaultHeaders = [];
    private $timeout = 30;

    public function __construct($timeout = 30) {
        $this->timeout = $timeout;
    }

    public function get($url, $headers = []) {
        return $this->request('GET', $url, null, $headers);
    }

    public function post($url, $data, $headers = []) {
        return $this->request('POST', $url, $data, $headers);
    }

    public function put($url, $data, $headers = []) {
        return $this->request('PUT', $url, $data, $headers);
    }

    public function delete($url, $headers = []) {
        return $this->request('DELETE', $url, null, $headers);
    }

    public function patch($url, $data, $headers = []) {
        return $this->request('PATCH', $url, $data, $headers);
    }

    private function request($method, $url, $data = null, $headers = []) {
        $startTime = microtime(true);
        $ch = curl_init();

        curl_setopt($ch, CURLOPT_URL, $url);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($ch, CURLOPT_TIMEOUT, $this->timeout);
        curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 10);
        curl_setopt($ch, CURLOPT_CUSTOMREQUEST, $method);
        curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false);

        $allHeaders = array_merge($this->defaultHeaders, $headers);
        if (!empty($allHeaders)) {
            $headerArr = [];
            foreach ($allHeaders as $key => $value) {
                $headerArr[] = "{$key}: {$value}";
            }
            curl_setopt($ch, CURLOPT_HTTPHEADER, $headerArr);
        }

        if ($data !== null) {
            $jsonData = is_string($data) ? $data : json_encode($data);
            curl_setopt($ch, CURLOPT_POSTFIELDS, $jsonData);
            if (!isset($allHeaders['Content-Type'])) {
                curl_setopt($ch, CURLOPT_HTTPHEADER, array_merge($headerArr ?? [], ['Content-Type: application/json']));
            }
        }

        $response = curl_exec($ch);
        $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
        $error = curl_error($ch);
        $duration = round((microtime(true) - $startTime) * 1000, 2);
        curl_close($ch);

        if ($error) {
            return [
                'success' => false,
                'error' => $error,
                'duration_ms' => $duration
            ];
        }

        $decoded = json_decode($response, true);

        return [
            'success' => $httpCode >= 200 && $httpCode < 400,
            'http_code' => $httpCode,
            'data' => $decoded !== null ? $decoded : $response,
            'duration_ms' => $duration
        ];
    }

    public function setDefaultHeaders($headers) {
        $this->defaultHeaders = array_merge($this->defaultHeaders, $headers);
    }

    public function setTimeout($timeout) {
        $this->timeout = $timeout;
    }
}
