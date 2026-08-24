<?php
/**
 * Ozayn Decision Support System
 * Helps users make decisions by presenting analysis and options
 */

class DecisionSupport {
    
    private $db;

    public function __construct() {
        $this->db = \Database::getInstance();
    }

    /**
     * Create a new decision
     */
    public function createDecision($userId, $context, $options, $projectId = null) {
        $id = $this->db->insert('decisions', [
            'user_id' => $userId,
            'project_id' => $projectId,
            'context' => $context,
            'options' => json_encode($options),
            'status' => 'pending'
        ]);

        return $id;
    }

    /**
     * Get decision by ID
     */
    public function getDecision($id, $userId) {
        return $this->db->fetch(
            "SELECT * FROM decisions WHERE id = ? AND user_id = ?",
            [$id, $userId]
        );
    }

    /**
     * Get user decisions
     */
    public function getUserDecisions($userId, $status = null, $limit = 20) {
        $sql = "SELECT * FROM decisions WHERE user_id = ?";
        $params = [$userId];

        if ($status) {
            $sql .= " AND status = ?";
            $params[] = $status;
        }

        $sql .= " ORDER BY created_at DESC LIMIT ?";
        $params[] = $limit;

        return $this->db->fetchAll($sql, $params);
    }

    /**
     * Update decision with choice
     */
    public function makeDecision($id, $userId, $chosenOption, $reasoning = null) {
        return $this->db->update('decisions', [
            'chosen_option' => $chosenOption,
            'reasoning' => $reasoning,
            'status' => 'decided',
            'decided_at' => date('Y-m-d H:i:s')
        ], 'id = ? AND user_id = ?', [$id, $userId]);
    }

    /**
     * Record outcome
     */
    public function recordOutcome($id, $userId, $outcome) {
        return $this->db->update('decisions', [
            'outcome' => $outcome,
            'status' => 'completed'
        ], 'id = ? AND user_id = ?', [$id, $userId]);
    }

    /**
     * Analyze decision context
     */
    public function analyzeDecision($context, $options) {
        $analysis = [
            'context_summary' => $this->summarizeContext($context),
            'option_analysis' => [],
            'recommendations' => [],
            'risks' => [],
            'considerations' => []
        ];

        foreach ($options as $index => $option) {
            $analysis['option_analysis'][] = [
                'index' => $index + 1,
                'name' => $option['name'] ?? "Option " . ($index + 1),
                'pros' => $option['pros'] ?? [],
                'cons' => $option['cons'] ?? [],
                'risk_level' => $option['risk_level'] ?? 'medium',
                'effort' => $option['effort'] ?? 'unknown',
                'impact' => $option['impact'] ?? 'unknown'
            ];
        }

        // Generate recommendations based on analysis
        $analysis['recommendations'] = $this->generateRecommendations($options);
        $analysis['risks'] = $this->identifyRisks($options);
        $analysis['considerations'] = $this->getConsiderations($context);

        return $analysis;
    }

    /**
     * Summarize context
     */
    private function summarizeContext($context) {
        $words = explode(' ', $context);
        $keyWords = array_filter($words, function($word) {
            return strlen($word) > 4;
        });
        
        return [
            'word_count' => count($words),
            'key_topics' => array_slice(array_unique($keyWords), 0, 5),
            'complexity' => count($words) > 50 ? 'high' : (count($words) > 20 ? 'medium' : 'low')
        ];
    }

    /**
     * Generate recommendations
     */
    private function generateRecommendations($options) {
        $recommendations = [];
        
        // Simple analysis based on pros/cons
        foreach ($options as $index => $option) {
            $pros = count($option['pros'] ?? []);
            $cons = count($option['cons'] ?? []);
            $score = $pros - $cons;
            
            $recommendations[] = [
                'option' => $option['name'] ?? "Option " . ($index + 1),
                'score' => $score,
                'recommendation' => $score > 2 ? 'strongly_recommended' : 
                                   ($score > 0 ? 'recommended' : 
                                   ($score == 0 ? 'neutral' : 'not_recommended'))
            ];
        }
        
        // Sort by score
        usort($recommendations, function($a, $b) {
            return $b['score'] - $a['score'];
        });
        
        return $recommendations;
    }

    /**
     * Identify risks
     */
    private function identifyRisks($options) {
        $risks = [];
        
        foreach ($options as $index => $option) {
            $riskLevel = $option['risk_level'] ?? 'medium';
            
            if ($riskLevel === 'high') {
                $risks[] = [
                    'option' => $option['name'] ?? "Option " . ($index + 1),
                    'risk' => 'High risk option - requires careful consideration',
                    'mitigation' => $option['mitigation'] ?? 'Additional review recommended'
                ];
            }
        }
        
        return $risks;
    }

    /**
     * Get considerations
     */
    private function getConsiderations($context) {
        $considerations = [
            'Time constraints',
            'Resource availability',
            'Stakeholder impact',
            'Long-term implications',
            'Reversibility'
        ];
        
        return $considerations;
    }

    /**
     * Format decision for display
     */
    public function formatDecision($decision) {
        $options = json_decode($decision['options'], true);
        
        $output = "**Decision: {$decision['context']}**\n\n";
        $output .= "**Status**: " . ucfirst($decision['status']) . "\n";
        $output .= "**Created**: {$decision['created_at']}\n\n";
        
        $output .= "**Options:**\n";
        foreach ($options as $index => $option) {
            $name = $option['name'] ?? "Option " . ($index + 1);
            $output .= "\n**" . ($index + 1) . ". {$name}**\n";
            
            if (!empty($option['pros'])) {
                $output .= "  Pros:\n";
                foreach ($option['pros'] as $pro) {
                    $output .= "    ✓ {$pro}\n";
                }
            }
            
            if (!empty($option['cons'])) {
                $output .= "  Cons:\n";
                foreach ($option['cons'] as $con) {
                    $output .= "    ✗ {$con}\n";
                }
            }
            
            $output .= "  Risk: " . ucfirst($option['risk_level'] ?? 'medium') . "\n";
        }
        
        if ($decision['chosen_option']) {
            $output .= "\n**Chosen**: {$decision['chosen_option']}\n";
        }
        
        if ($decision['reasoning']) {
            $output .= "**Reasoning**: {$decision['reasoning']}\n";
        }
        
        if ($decision['outcome']) {
            $output .= "**Outcome**: {$decision['outcome']}\n";
        }
        
        return $output;
    }

    /**
     * Format analysis for display
     */
    public function formatAnalysis($analysis) {
        $output = "**Decision Analysis**\n\n";
        
        $output .= "**Context Summary:**\n";
        $output .= "- Complexity: " . ucfirst($analysis['context_summary']['complexity']) . "\n";
        $output .= "- Key topics: " . implode(', ', $analysis['context_summary']['key_topics']) . "\n\n";
        
        $output .= "**Option Analysis:**\n";
        foreach ($analysis['option_analysis'] as $option) {
            $output .= "\n**{$option['index']}. {$option['name']}**\n";
            $output .= "- Risk Level: " . ucfirst($option['risk_level']) . "\n";
            $output .= "- Effort: " . ucfirst($option['effort']) . "\n";
            $output .= "- Impact: " . ucfirst($option['impact']) . "\n";
        }
        
        if (!empty($analysis['recommendations'])) {
            $output .= "\n**Recommendations:**\n";
            foreach ($analysis['recommendations'] as $rec) {
                $output .= "- {$rec['option']}: " . ucfirst(str_replace('_', ' ', $rec['recommendation'])) . "\n";
            }
        }
        
        if (!empty($analysis['risks'])) {
            $output .= "\n**Risks:**\n";
            foreach ($analysis['risks'] as $risk) {
                $output .= "- {$risk['option']}: {$risk['risk']}\n";
            }
        }
        
        return $output;
    }

    /**
     * Add options to an existing decision
     */
    public function addOptions($decisionId, $userId, $newOptions) {
        $decision = $this->getDecision($decisionId, $userId);
        if (!$decision) return false;

        $existingOptions = json_decode($decision['options'], true) ?? [];
        $mergedOptions = array_merge($existingOptions, $newOptions);

        return $this->db->update('decisions', [
            'options' => json_encode($mergedOptions)
        ], 'id = ? AND user_id = ?', [$decisionId, $userId]);
    }

    /**
     * Get decision statistics
     */
    public function getDecisionStats($userId) {
        $stats = $this->db->fetch(
            "SELECT 
                COUNT(*) as total,
                SUM(CASE WHEN status = 'decided' THEN 1 ELSE 0 END) as decided,
                SUM(CASE WHEN status = 'pending' THEN 1 ELSE 0 END) as pending,
                SUM(CASE WHEN status = 'completed' THEN 1 ELSE 0 END) as completed
            FROM decisions WHERE user_id = ?",
            [$userId]
        );

        return $stats;
    }

    /**
     * Compare multiple decisions
     */
    public function compareDecisions($decisionIds, $userId) {
        $decisions = [];
        foreach ($decisionIds as $id) {
            $decision = $this->getDecision($id, $userId);
            if ($decision) {
                $decisions[] = $decision;
            }
        }

        if (empty($decisions)) return null;

        $output = "**Decision Comparison**\n\n";
        
        foreach ($decisions as $d) {
            $output .= $this->formatDecision($d) . "\n---\n\n";
        }

        return $output;
    }
}
