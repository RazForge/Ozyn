<?php
/**
 * Ozayn File Diff Tool
 * Compare files and text with highlighting
 */

class FileDiff {
    
    public function diffFiles($file1, $file2) {
        if (!file_exists($file1)) return ['error' => "File not found: {$file1}"];
        if (!file_exists($file2)) return ['error' => "File not found: {$file2}"];
        
        $content1 = file_get_contents($file1);
        $content2 = file_get_contents($file2);
        
        return $this->diffText($content1, $content2, basename($file1), basename($file2));
    }

    public function diffText($text1, $text2, $name1 = 'text1', $name2 = 'text2') {
        $lines1 = explode("\n", $text1);
        $lines2 = explode("\n", $text2);
        
        $diff = [];
        $maxLines = max(count($lines1), count($lines2));
        
        for ($i = 0; $i < $maxLines; $i++) {
            $line1 = $lines1[$i] ?? null;
            $line2 = $lines2[$i] ?? null;
            
            if ($line1 === $line2) {
                $diff[] = ['type' => 'same', 'line' => $i + 1, 'content' => $line1];
            } elseif ($line1 === null) {
                $diff[] = ['type' => 'added', 'line' => $i + 1, 'content' => $line2];
            } elseif ($line2 === null) {
                $diff[] = ['type' => 'removed', 'line' => $i + 1, 'content' => $line1];
            } else {
                $diff[] = ['type' => 'changed', 'line' => $i + 1, 'old' => $line1, 'new' => $line2];
            }
        }
        
        $stats = [
            'same' => count(array_filter($diff, fn($d) => $d['type'] === 'same')),
            'added' => count(array_filter($diff, fn($d) => $d['type'] === 'added')),
            'removed' => count(array_filter($diff, fn($d) => $d['type'] === 'removed')),
            'changed' => count(array_filter($diff, fn($d) => $d['type'] === 'changed'))
        ];
        
        return [
            'file1' => $name1,
            'file2' => $name2,
            'diff' => $diff,
            'stats' => $stats,
            'html' => $this->generateHtml($diff, $name1, $name2)
        ];
    }

    public function sideBySide($text1, $text2, $width = 80) {
        $lines1 = explode("\n", $text1);
        $lines2 = explode("\n", $text2);
        $maxLines = max(count($lines1), count($lines2));
        
        $result = [];
        for ($i = 0; $i < $maxLines; $i++) {
            $left = isset($lines1[$i]) ? str_pad(substr($lines1[$i], 0, $width), $width) : str_repeat(' ', $width);
            $right = isset($lines2[$i]) ? substr($lines2[$i], 0, $width) : '';
            $changed = ($lines1[$i] ?? '') !== ($lines2[$i] ?? '');
            $result[] = [
                'line' => $i + 1,
                'left' => $left,
                'right' => $right,
                'changed' => $changed
            ];
        }
        return $result;
    }

    public function mergeText($text1, $text2, $strategy = 'last') {
        $lines1 = explode("\n", $text1);
        $lines2 = explode("\n", $text2);
        $maxLines = max(count($lines1), count($lines2));
        
        $merged = [];
        for ($i = 0; $i < $maxLines; $i++) {
            $line1 = $lines1[$i] ?? null;
            $line2 = $lines2[$i] ?? null;
            
            if ($line1 === $line2 || $line1 === null) {
                $merged[] = $line2;
            } elseif ($line2 === null) {
                $merged[] = $line1;
            } else {
                $merged[] = $strategy === 'first' ? $line1 : $line2;
            }
        }
        return implode("\n", $merged);
    }

    public function findSimilar($text1, $text2) {
        $words1 = str_word_count(strtolower($text1), 1);
        $words2 = str_word_count(strtolower($text2), 1);
        $common = array_intersect($words1, $words2);
        $total = array_unique(array_merge($words1, $words2));
        
        return [
            'common_words' => count($common),
            'total_words' => count($total),
            'similarity' => count($total) > 0 ? round((count($common) / count($total)) * 100, 1) : 0,
            'common_words_list' => array_slice(array_values($common), 0, 20)
        ];
    }

    private function generateHtml($diff, $name1, $name2) {
        $html = "<div class='diff-container'>";
        $html .= "<div class='diff-header'><span class='file1'>{$name1}</span> vs <span class='file2'>{$name2}</span></div>";
        $html .= "<div class='diff-content'>";
        
        foreach ($diff as $d) {
            $lineNum = str_pad($d['line'], 4, ' ', STR_PAD_LEFT);
            switch ($d['type']) {
                case 'same':
                    $html .= "<div class='diff-line same'><span class='line-num'>{$lineNum}</span> {$d['content']}</div>";
                    break;
                case 'added':
                    $html .= "<div class='diff-line added'><span class='line-num'>{$lineNum}</span> + {$d['content']}</div>";
                    break;
                case 'removed':
                    $html .= "<div class='diff-line removed'><span class='line-num'>{$lineNum}</span> - {$d['content']}</div>";
                    break;
                case 'changed':
                    $html .= "<div class='diff-line removed'><span class='line-num'>{$lineNum}</span> - {$d['old']}</div>";
                    $html .= "<div class='diff-line added'><span class='line-num'>{$lineNum}</span> + {$d['new']}</div>";
                    break;
            }
        }
        
        $html .= "</div></div>";
        return $html;
    }

    public function formatDiff($diff) {
        $output = "";
        foreach ($diff['diff'] as $d) {
            $lineNum = str_pad($d['line'], 4, ' ', STR_PAD_LEFT);
            switch ($d['type']) {
                case 'same':
                    $output .= "  {$lineNum} | {$d['content']}\n";
                    break;
                case 'added':
                    $output .= "+ {$lineNum} | {$d['content']}\n";
                    break;
                case 'removed':
                    $output .= "- {$lineNum} | {$d['content']}\n";
                    break;
                case 'changed':
                    $output .= "- {$lineNum} | {$d['old']}\n";
                    $output .= "+ {$lineNum} | {$d['new']}\n";
                    break;
            }
        }
        return $output;
    }
}
