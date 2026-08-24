<?php
/**
 * Ozayn Tutorial System
 * In-app onboarding and tutorials
 */

class TutorialSystem {
    
    private $db;

    public function __construct() {
        $this->db = \Database::getInstance();
    }

    /**
     * Get tutorials
     */
    public function getTutorials() {
        return [
            'tutorials' => [
                [
                    'id' => 'getting_started',
                    'title' => 'Getting Started',
                    'description' => 'Learn the basics of Ozayn',
                    'steps' => 5,
                    'duration' => '5 min'
                ],
                [
                    'id' => 'chat_commands',
                    'title' => 'Chat Commands',
                    'description' => 'Master all available commands',
                    'steps' => 8,
                    'duration' => '10 min'
                ],
                [
                    'id' => 'arwe_systems',
                    'title' => 'ARWE Integration',
                    'description' => 'Connect with ARWE systems',
                    'steps' => 6,
                    'duration' => '8 min'
                ],
                [
                    'id' => 'voice_interface',
                    'title' => 'Voice Interface',
                    'description' => 'Use voice commands',
                    'steps' => 4,
                    'duration' => '5 min'
                ],
                [
                    'id' => 'decision_support',
                    'title' => 'Decision Support',
                    'description' => 'Make better decisions with AI',
                    'steps' => 5,
                    'duration' => '7 min'
                ],
                [
                    'id' => 'project_management',
                    'title' => 'Project Management',
                    'description' => 'Organize your projects',
                    'steps' => 4,
                    'duration' => '5 min'
                ]
            ]
        ];
    }

    /**
     * Get tutorial content
     */
    public function getTutorialContent($tutorialId) {
        $tutorials = [
            'getting_started' => [
                'title' => 'Getting Started with Ozayn',
                'steps' => [
                    [
                        'title' => 'Welcome to Ozayn',
                        'content' => 'Ozayn is your personal AI digital twin for the ARWE ecosystem. It helps you with tasks, decisions, and system management.',
                        'tip' => 'Type "help" anytime to see all available commands.'
                    ],
                    [
                        'title' => 'Chat Interface',
                        'content' => 'Use the chat to communicate with Ozayn. You can type messages or use voice input.',
                        'tip' => 'Press Ctrl+K to focus the chat input quickly.'
                    ],
                    [
                        'title' => 'Navigation',
                        'content' => 'Use the bottom navigation to switch between Chat, Projects, Tasks, ARWE, and Settings.',
                        'tip' => 'The ARWE tab shows real-time system status.'
                    ],
                    [
                        'title' => 'Voice Commands',
                        'content' => 'Click the microphone button to use voice commands. Supports English, Amharic, and Afaan Oromo.',
                        'tip' => 'Speak clearly for best recognition results.'
                    ],
                    [
                        'title' => 'Settings',
                        'content' => 'Customize your experience in Settings. Change themes, accent colors, and AI provider.',
                        'tip' => 'Enable auto-speak to hear responses aloud.'
                    ]
                ]
            ],
            'chat_commands' => [
                'title' => 'Mastering Chat Commands',
                'steps' => [
                    [
                        'title' => 'System Commands',
                        'content' => 'Type "system", "cpu", "memory", or "disk" to get system information.',
                        'tip' => 'These commands show real-time system stats.'
                    ],
                    [
                        'title' => 'ARWE Commands',
                        'content' => 'Type "arwe" for all systems, or "edunex", "govyx", "kidane" for specific systems.',
                        'tip' => 'Use "summary" for a quick overview.'
                    ],
                    [
                        'title' => 'File Operations',
                        'content' => 'Use "ls", "read", "mkdir" to manage files. Example: "ls /home"',
                        'tip' => 'Be careful with file operations.'
                    ],
                    [
                        'title' => 'Web Search',
                        'content' => 'Type "search [query]" to search the web. Example: "search ARWE ecosystem"',
                        'tip' => 'Results are displayed in a clean format.'
                    ],
                    [
                        'title' => 'Task Management',
                        'content' => 'Use "add task [title]" to create tasks. Check them in the Tasks tab.',
                        'tip' => 'Set priorities for better organization.'
                    ],
                    [
                        'title' => 'Decision Support',
                        'content' => 'Type "decide [context]" to create a decision for analysis.',
                        'tip' => 'Add options to compare different choices.'
                    ],
                    [
                        'title' => 'Notifications',
                        'content' => 'Type "notifications" to view alerts. Use "mark read" to clear them.',
                        'tip' => 'Important events trigger notifications automatically.'
                    ],
                    [
                        'title' => 'Help',
                        'content' => 'Type "help" anytime to see all available commands.',
                        'tip' => 'Press Ctrl+/ to show help quickly.'
                    ]
                ]
            ],
            'arwe_systems' => [
                'title' => 'ARWE Integration',
                'steps' => [
                    [
                        'title' => 'ARWE Overview',
                        'content' => 'ARWE is the ecosystem Ozayn connects to. It includes Education, Government, Identity, Transparency, Security, and Robotics systems.',
                        'tip' => 'Each system has its own status and capabilities.'
                    ],
                    [
                        'title' => 'Edunex (Education)',
                        'content' => 'Edunex manages students, teachers, courses, and learning sessions.',
                        'tip' => 'Type "edunex" to see current stats.'
                    ],
                    [
                        'title' => 'Govyx (Government)',
                        'content' => 'Govyx handles government tasks, approvals, and department management.',
                        'tip' => 'Use "govyx" to check pending tasks.'
                    ],
                    [
                        'title' => 'Kidane & Canivox (Robotics)',
                        'content' => 'Kidane manages aerial drones, Canivox manages ground robots.',
                        'tip' => 'Check fleet status before missions.'
                    ],
                    [
                        'title' => 'Bilen (Security)',
                        'content' => 'Bilen monitors security alerts and intelligence sources.',
                        'tip' => 'Critical alerts are highlighted.'
                    ],
                    [
                        'title' => 'ARWE Dashboard',
                        'content' => 'Visit the ARWE tab for a visual dashboard of all systems.',
                        'tip' => 'The dashboard updates in real-time.'
                    ]
                ]
            ],
            'voice_interface' => [
                'title' => 'Using Voice Interface',
                'steps' => [
                    [
                        'title' => 'Voice Input',
                        'content' => 'Click the microphone button to start voice input. Speak your command clearly.',
                        'tip' => 'Allow microphone access when prompted.'
                    ],
                    [
                        'title' => 'Language Selection',
                        'content' => 'Ozayn supports English, Amharic, and Afaan Oromo. Change in Settings > Voice.',
                        'tip' => 'Select your preferred language for best results.'
                    ],
                    [
                        'title' => 'Text-to-Speech',
                        'content' => 'Click the speaker button to hear the last response. Enable auto-speak in Settings.',
                        'tip' => 'Adjust voice speed in settings.'
                    ],
                    [
                        'title' => 'Voice Commands',
                        'content' => 'Speak naturally: "Show me ARWE status" or "Create a new task"',
                        'tip' => 'Short commands work best.'
                    ]
                ]
            ],
            'decision_support' => [
                'title' => 'Decision Support System',
                'steps' => [
                    [
                        'title' => 'Creating Decisions',
                        'content' => 'Type "decide [context]" to create a decision. Example: "decide choosing a hosting provider"',
                        'tip' => 'Be specific about the context.'
                    ],
                    [
                        'title' => 'Adding Options',
                        'content' => 'After creating a decision, add options to compare. Each option can have pros and cons.',
                        'tip' => 'Consider multiple factors for each option.'
                    ],
                    [
                        'title' => 'Analysis',
                        'content' => 'Ozayn analyzes options based on pros, cons, risk level, and impact.',
                        'tip' => 'The more details you provide, the better the analysis.'
                    ],
                    [
                        'title' => 'Making Decisions',
                        'content' => 'Review the analysis and choose an option. Record your reasoning for future reference.',
                        'tip' => 'Your decision history builds a knowledge base.'
                    ],
                    [
                        'title' => 'Decision History',
                        'content' => 'Type "decisions" to view all your past decisions and their outcomes.',
                        'tip' => 'Learn from past decisions to improve future ones.'
                    ]
                ]
            ],
            'project_management' => [
                'title' => 'Project Management',
                'steps' => [
                    [
                        'title' => 'Creating Projects',
                        'content' => 'Type "create project [name]" or use the Projects tab to create new projects.',
                        'tip' => 'Add descriptions for better organization.'
                    ],
                    [
                        'title' => 'Managing Tasks',
                        'content' => 'Create tasks with "add task [title]" and assign them to projects.',
                        'tip' => 'Use priorities: low, medium, high, urgent.'
                    ],
                    [
                        'title' => 'Progress Tracking',
                        'content' => 'View project progress in the Projects tab. Track completed vs pending tasks.',
                        'tip' => 'Regular reviews help maintain momentum.'
                    ],
                    [
                        'title' => 'Knowledge Base',
                        'content' => 'Store project-related knowledge in the Knowledge tab. Add notes and documents.',
                        'tip' => 'Use tags for easy searching.'
                    ]
                ]
            ]
        ];

        return $tutorials[$tutorialId] ?? null;
    }

    /**
     * Check if user completed tutorial
     */
    public function isCompleted($userId, $tutorialId) {
        $result = $this->db->fetch(
            "SELECT completed FROM user_tutorials WHERE user_id = ? AND tutorial_id = ?",
            [$userId, $tutorialId]
        );
        
        return $result && $result['completed'];
    }

    /**
     * Mark tutorial as completed
     */
    public function markCompleted($userId, $tutorialId) {
        $existing = $this->db->fetch(
            "SELECT id FROM user_tutorials WHERE user_id = ? AND tutorial_id = ?",
            [$userId, $tutorialId]
        );
        
        if ($existing) {
            return $this->db->update('user_tutorials', [
                'completed' => 1,
                'completed_at' => date('Y-m-d H:i:s')
            ], 'user_id = ? AND tutorial_id = ?', [$userId, $tutorialId]);
        }
        
        return $this->db->insert('user_tutorials', [
            'user_id' => $userId,
            'tutorial_id' => $tutorialId,
            'completed' => 1,
            'completed_at' => date('Y-m-d H:i:s')
        ]);
    }

    /**
     * Get user progress
     */
    public function getProgress($userId) {
        $tutorials = $this->getTutorials()['tutorials'];
        $completed = $this->db->fetchAll(
            "SELECT tutorial_id FROM user_tutorials WHERE user_id = ? AND completed = 1",
            [$userId]
        );
        
        $completedIds = array_column($completed, 'tutorial_id');
        
        $total = count($tutorials);
        $done = count(array_intersect($completedIds, array_column($tutorials, 'id')));
        
        return [
            'total' => $total,
            'completed' => $done,
            'percentage' => $total > 0 ? round(($done / $total) * 100) : 0,
            'remaining' => $total - $done
        ];
    }
}
