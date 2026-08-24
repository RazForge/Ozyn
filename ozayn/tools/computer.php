<?php
/**
 * Ozayn Computer Control Tools
 * File operations, application launching, system commands
 */

class ComputerTools {
    
    /**
     * List files in directory
     */
    public function listFiles($path = '.', $recursive = false) {
        $path = $this->sanitizePath($path);
        
        if (!is_dir($path)) {
            return ['error' => "Directory not found: {$path}"];
        }

        $items = [];
        
        if ($recursive) {
            $iterator = new RecursiveIteratorIterator(
                new RecursiveDirectoryIterator($path, RecursiveDirectoryIterator::SKIP_DOTS)
            );
            foreach ($iterator as $item) {
                $items[] = $this->getFileInfo($item);
            }
        } else {
            foreach (new DirectoryIterator($path) as $item) {
                if (!$item->isDot()) {
                    $items[] = $this->getFileInfo($item);
                }
            }
        }

        return [
            'path' => $path,
            'items' => $items,
            'count' => count($items)
        ];
    }

    /**
     * Read file contents
     */
    public function readFile($path, $maxSize = 1048576) {
        $path = $this->sanitizePath($path);
        
        if (!file_exists($path)) {
            return ['error' => "File not found: {$path}"];
        }

        if (!is_readable($path)) {
            return ['error' => "File not readable: {$path}"];
        }

        $size = filesize($path);
        
        if ($size > $maxSize) {
            // Read in chunks for large files
            $handle = fopen($path, 'r');
            $content = fread($handle, $maxSize);
            fclose($handle);
            return [
                'path' => $path,
                'content' => $content,
                'truncated' => true,
                'size' => $size,
                'max_size' => $maxSize
            ];
        }

        return [
            'path' => $path,
            'content' => file_get_contents($path),
            'truncated' => false,
            'size' => $size
        ];
    }

    /**
     * Write file contents
     */
    public function writeFile($path, $content, $append = false) {
        $path = $this->sanitizePath($path);
        
        // Create directory if not exists
        $dir = dirname($path);
        if (!is_dir($dir)) {
            mkdir($dir, 0755, true);
        }

        $mode = $append ? 'a' : 'w';
        $handle = fopen($path, $mode);
        
        if (!$handle) {
            return ['error' => "Cannot write to file: {$path}"];
        }

        fwrite($handle, $content);
        fclose($handle);

        return [
            'success' => true,
            'path' => $path,
            'bytes_written' => strlen($content),
            'mode' => $append ? 'append' : 'write'
        ];
    }

    /**
     * Create directory
     */
    public function createDirectory($path) {
        $path = $this->sanitizePath($path);
        
        if (is_dir($path)) {
            return ['error' => "Directory already exists: {$path}"];
        }

        if (mkdir($path, 0755, true)) {
            return ['success' => true, 'path' => $path];
        }

        return ['error' => "Failed to create directory: {$path}"];
    }

    /**
     * Delete file or directory
     */
    public function delete($path) {
        $path = $this->sanitizePath($path);
        
        if (!file_exists($path)) {
            return ['error' => "Path not found: {$path}"];
        }

        // Safety check - prevent deleting critical directories
        $protected = ['/', '/home', '/etc', '/var', '/usr', '/bin', '/sbin'];
        $realPath = realpath($path);
        
        foreach ($protected as $p) {
            if ($realPath === $p || strpos($realPath, $p . '/') === 0) {
                return ['error' => "Cannot delete protected path: {$path}"];
            }
        }

        if (is_dir($path)) {
            $this->deleteDirectoryRecursive($path);
        } else {
            unlink($path);
        }

        return ['success' => true, 'path' => $path];
    }

    /**
     * Copy file
     */
    public function copyFile($source, $destination) {
        $source = $this->sanitizePath($source);
        $destination = $this->sanitizePath($destination);
        
        if (!file_exists($source)) {
            return ['error' => "Source not found: {$source}"];
        }

        $destDir = dirname($destination);
        if (!is_dir($destDir)) {
            mkdir($destDir, 0755, true);
        }

        if (copy($source, $destination)) {
            return [
                'success' => true,
                'source' => $source,
                'destination' => $destination
            ];
        }

        return ['error' => "Failed to copy file"];
    }

    /**
     * Move/rename file
     */
    public function moveFile($source, $destination) {
        $source = $this->sanitizePath($source);
        $destination = $this->sanitizePath($destination);
        
        if (!file_exists($source)) {
            return ['error' => "Source not found: {$source}"];
        }

        if (rename($source, $destination)) {
            return [
                'success' => true,
                'source' => $source,
                'destination' => $destination
            ];
        }

        return ['error' => "Failed to move file"];
    }

    /**
     * Search files by name pattern
     */
    public function searchFiles($path, $pattern, $maxResults = 50) {
        $path = $this->sanitizePath($path);
        
        if (!is_dir($path)) {
            return ['error' => "Directory not found: {$path}"];
        }

        $results = [];
        $iterator = new RecursiveIteratorIterator(
            new RecursiveDirectoryIterator($path, RecursiveDirectoryIterator::SKIP_DOTS)
        );

        foreach ($iterator as $item) {
            if (count($results) >= $maxResults) break;
            
            if (fnmatch($pattern, $item->getFilename(), FNM_CASEFOLD)) {
                $results[] = [
                    'path' => $item->getPathname(),
                    'name' => $item->getFilename(),
                    'size' => $item->getSize(),
                    'modified' => date('Y-m-d H:i:s', $item->getMTime())
                ];
            }
        }

        return [
            'path' => $path,
            'pattern' => $pattern,
            'results' => $results,
            'count' => count($results)
        ];
    }

    /**
     * Search file contents
     */
    public function grep($path, $pattern, $maxResults = 50) {
        $path = $this->sanitizePath($path);
        
        if (!file_exists($path)) {
            return ['error' => "Path not found: {$path}"];
        }

        $results = [];
        
        if (is_file($path)) {
            $lines = file($path);
            foreach ($lines as $lineNum => $line) {
                if (preg_match($pattern, $line)) {
                    $results[] = [
                        'file' => $path,
                        'line' => $lineNum + 1,
                        'content' => trim($line)
                    ];
                    if (count($results) >= $maxResults) break;
                }
            }
        } else {
            $iterator = new RecursiveIteratorIterator(
                new RecursiveDirectoryIterator($path, RecursiveDirectoryIterator::SKIP_DOTS)
            );
            
            foreach ($iterator as $item) {
                if ($item->isFile() && $item->isReadable()) {
                    $lines = file($item->getPathname());
                    foreach ($lines as $lineNum => $line) {
                        if (preg_match($pattern, $line)) {
                            $results[] = [
                                'file' => $item->getPathname(),
                                'line' => $lineNum + 1,
                                'content' => trim($line)
                            ];
                            if (count($results) >= $maxResults) break 2;
                        }
                    }
                }
            }
        }

        return [
            'pattern' => $pattern,
            'results' => $results,
            'count' => count($results)
        ];
    }

    /**
     * Get file information
     */
    public function getFileInfo($item) {
        return [
            'name' => $item->getFilename(),
            'path' => $item->getPathname(),
            'type' => $item->isDir() ? 'directory' : 'file',
            'size' => $item->getSize(),
            'modified' => date('Y-m-d H:i:s', $item->getMTime()),
            'permissions' => substr(sprintf('%o', $item->getPerms()), -4)
        ];
    }

    /**
     * Run shell command (restricted)
     */
    public function runCommand($command, $timeout = 30) {
        // Whitelist allowed commands
        $allowed = ['ls', 'pwd', 'whoami', 'date', 'uptime', 'df', 'du', 'free', 
                    'cat', 'head', 'tail', 'wc', 'grep', 'find', 'which', 'echo',
                    'php', 'python', 'node', 'git'];
        
        // Block dangerous patterns
        $dangerous = ['rm -rf', 'mkfs', 'dd if=', ':(){:', 'fork', 'wget', 'curl', 
                      'chmod 777', 'chown', 'sudo', 'su -', '/etc/passwd', '/etc/shadow',
                      ';', '|', '&', '`', '$(', '${', '>', '>>', '<'];
        
        $commandLower = strtolower($command);
        foreach ($dangerous as $pattern) {
            if (strpos($commandLower, $pattern) !== false) {
                return ['error' => "Dangerous pattern detected: {$pattern}"];
            }
        }
        
        $cmdParts = preg_split('/\s+/', trim($command));
        if (empty($cmdParts)) {
            return ['error' => 'Empty command'];
        }
        
        $baseCmd = basename($cmdParts[0]);
        
        if (!in_array($baseCmd, $allowed)) {
            return ['error' => "Command not allowed: {$baseCmd}"];
        }

        // Escape all arguments except the first (command)
        $escaped = [$baseCmd];
        for ($i = 1; $i < count($cmdParts); $i++) {
            $escaped[] = escapeshellarg($cmdParts[$i]);
        }
        $safeCommand = implode(' ', $escaped);

        $output = [];
        $returnCode = 0;
        
        exec($safeCommand . " 2>&1", $output, $returnCode);
        
        return [
            'command' => $safeCommand,
            'output' => implode("\n", $output),
            'return_code' => $returnCode,
            'success' => $returnCode === 0
        ];
    }

    /**
     * Get current directory
     */
    public function getCurrentDirectory() {
        return ['path' => getcwd()];
    }

    /**
     * Change directory (returns new path, doesn't actually change)
     */
    public function resolvePath($path) {
        $path = $this->sanitizePath($path);
        
        if (is_dir($path)) {
            return ['path' => realpath($path)];
        }

        return ['error' => "Directory not found: {$path}"];
    }

    /**
     * Get file stats
     */
    public function stat($path) {
        $path = $this->sanitizePath($path);
        
        if (!file_exists($path)) {
            return ['error' => "File not found: {$path}"];
        }

        $stat = stat($path);
        
        return [
            'path' => $path,
            'size' => $stat['size'],
            'accessed' => date('Y-m-d H:i:s', $stat['atime']),
            'modified' => date('Y-m-d H:i:s', $stat['mtime']),
            'changed' => date('Y-m-d H:i:s', $stat['ctime']),
            'permissions' => substr(sprintf('%o', $stat['mode']), -4),
            'is_file' => is_file($path),
            'is_dir' => is_dir($path),
            'readable' => is_readable($path),
            'writable' => is_writable($path)
        ];
    }

    /**
     * Watch file for changes
     */
    public function watchFile($path, $callback = null) {
        $path = $this->sanitizePath($path);
        
        if (!file_exists($path)) {
            return ['error' => "File not found: {$path}"];
        }

        $lastModified = filemtime($path);
        
        return [
            'path' => $path,
            'last_modified' => date('Y-m-d H:i:s', $lastModified),
            'message' => 'Use polling to check for changes'
        ];
    }

    /**
     * Sanitize file path
     */
    private function sanitizePath($path) {
        // Remove null bytes
        $path = str_replace("\0", '', $path);
        
        // Expand ~ to home directory
        if (strpos($path, '~') === 0) {
            $path = $_SERVER['HOME'] . substr($path, 1);
        }
        
        // Make relative paths absolute
        if (strpos($path, '/') !== 0) {
            $path = getcwd() . '/' . $path;
        }
        
        // Resolve .. and .
        $path = realpath($path) ?: $path;
        
        return $path;
    }

    /**
     * Recursively delete directory
     */
    private function deleteDirectoryRecursive($path) {
        $items = new RecursiveIteratorIterator(
            new RecursiveDirectoryIterator($path, RecursiveDirectoryIterator::SKIP_DOTS),
            RecursiveIteratorIterator::CHILD_FIRST
        );
        
        foreach ($items as $item) {
            if ($item->isDir()) {
                rmdir($item->getRealPath());
            } else {
                unlink($item->getRealPath());
            }
        }
        
        rmdir($path);
    }
}
