<?php
/**
 * Ozayn Git Integration
 * Git operations for version control
 */

class GitIntegration {
    
    private $repoPath;

    public function __construct($repoPath = null) {
        $this->repoPath = $repoPath ?? getcwd();
    }

    public function isGitRepo() {
        return is_dir($this->repoPath . '/.git');
    }

    public function getStatus() {
        if (!$this->isGitRepo()) {
            return ['error' => 'Not a git repository'];
        }
        $output = $this->exec('git status --porcelain');
        $lines = array_filter(explode("\n", $output));
        $changes = ['modified' => [], 'added' => [], 'deleted' => [], 'untracked' => []];
        foreach ($lines as $line) {
            $status = substr($line, 0, 2);
            $file = trim(substr($line, 3));
            if (strpos($status, 'M') !== false) $changes['modified'][] = $file;
            elseif (strpos($status, 'A') !== false) $changes['added'][] = $file;
            elseif (strpos($status, 'D') !== false) $changes['deleted'][] = $file;
            elseif (strpos($status, '?') !== false) $changes['untracked'][] = $file;
        }
        return $changes;
    }

    public function getLog($limit = 20) {
        $output = $this->exec("git log --oneline -{$limit} --pretty=format:'%H|%an|%ae|%ai|%s'");
        $commits = [];
        foreach (explode("\n", $output) as $line) {
            $line = trim($line, "'");
            if (empty($line)) continue;
            $parts = explode('|', $line, 5);
            if (count($parts) >= 5) {
                $commits[] = [
                    'hash' => $parts[0],
                    'author' => $parts[1],
                    'email' => $parts[2],
                    'date' => $parts[3],
                    'message' => $parts[4]
                ];
            }
        }
        return $commits;
    }

    public function getBranches() {
        $output = $this->exec('git branch -a');
        $branches = ['current' => '', 'local' => [], 'remote' => []];
        foreach (explode("\n", $output) as $line) {
            $line = trim($line);
            if (empty($line)) continue;
            $branch = preg_replace('/^[* ]+/', '', $line);
            if (strpos($line, '*') !== false) {
                $branches['current'] = $branch;
                $branches['local'][] = $branch;
            } elseif (strpos($branch, 'remotes/') !== false) {
                $branches['remote'][] = str_replace('remotes/origin/', '', $branch);
            } else {
                $branches['local'][] = $branch;
            }
        }
        return $branches;
    }

    public function diff($file = null) {
        $cmd = $file ? "git diff -- {$file}" : 'git diff --stat';
        return $this->exec($cmd);
    }

    public function diffStaged($file = null) {
        $cmd = $file ? "git diff --cached -- {$file}" : 'git diff --cached --stat';
        return $this->exec($cmd);
    }

    public function add($files = '.') {
        $this->exec("git add {$files}");
        return ['success' => true];
    }

    public function commit($message, $files = null) {
        if ($files) {
            $this->exec("git add {$files}");
        }
        $output = $this->exec("git commit -m " . escapeshellarg($message));
        return ['success' => true, 'output' => $output];
    }

    public function push($remote = 'origin', $branch = null) {
        if (!$branch) {
            $branch = $this->getCurrentBranch();
        }
        $output = $this->exec("git push {$remote} {$branch} 2>&1");
        return ['success' => true, 'output' => $output];
    }

    public function pull($remote = 'origin', $branch = null) {
        if (!$branch) {
            $branch = $this->getCurrentBranch();
        }
        $output = $this->exec("git pull {$remote} {$branch} 2>&1");
        return ['success' => true, 'output' => $output];
    }

    public function stash($message = null) {
        $cmd = $message ? "git stash push -m " . escapeshellarg($message) : 'git stash';
        $output = $this->exec($cmd);
        return ['success' => true, 'output' => $output];
    }

    public function stashPop() {
        $output = $this->exec('git stash pop 2>&1');
        return ['success' => true, 'output' => $output];
    }

    public function createBranch($name) {
        $output = $this->exec("git checkout -b {$name}");
        return ['success' => true, 'output' => $output];
    }

    public function switchBranch($name) {
        $output = $this->exec("git checkout {$name} 2>&1");
        return ['success' => strpos($output, 'error') === false, 'output' => $output];
    }

    public function getCurrentBranch() {
        return trim($this->exec('git branch --show-current'));
    }

    public function getFileHistory($file, $limit = 10) {
        $output = $this->exec("git log --oneline -{$limit} --format=%H|%ai|%s -- {$file}");
        $history = [];
        foreach (explode("\n", $output) as $line) {
            if (empty($line)) continue;
            $parts = explode('|', $line, 3);
            if (count($parts) >= 3) {
                $history[] = ['hash' => $parts[0], 'date' => $parts[1], 'message' => $parts[2]];
            }
        }
        return $history;
    }

    public function getRemoteUrl($remote = 'origin') {
        return trim($this->exec("git remote get-url {$remote}"));
    }

    private function exec($command) {
        $output = [];
        $returnCode = 0;
        exec("cd {$this->repoPath} && {$command} 2>&1", $output, $returnCode);
        return implode("\n", $output);
    }
}
