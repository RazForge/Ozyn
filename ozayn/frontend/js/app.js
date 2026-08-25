/**
 * Ozayn Frontend Application
 * Version: 0.2.0
 */

class OzaynApp {
    constructor() {
        this.sessionId = localStorage.getItem('ozayn_session');
        this.user = JSON.parse(localStorage.getItem('ozayn_user') || 'null');
        this.currentConversation = null;
        this.currentProject = null;
        this.apiBase = '/ozayn/backend/api';
        this.settings = this.loadSettings();
        
        // Voice
        this.recognition = null;
        this.synthesis = window.speechSynthesis;
        this.isRecording = false;

        // Command history
        this.commandHistory = JSON.parse(localStorage.getItem('ozayn_command_history') || '[]');
        this.historyIndex = -1;

        this.init();
    }

    init() {
        this.bindEvents();
        this.checkAuth();
        this.initVoice();
        this.loadVoices();
        this.initKeyboardShortcuts();
    }

    // ==================== Settings ====================
    
    loadSettings() {
        const settings = JSON.parse(localStorage.getItem('ozayn_settings') || '{}');
        this.currentAccentColor = settings.accentColor || '#0a84ff';
        if (settings.theme) this.applyTheme(settings.theme);
        if (settings.accentColor) this.applyAccentColor(settings.accentColor);
        return settings;
    }

    saveSettings() {
        this.settings = {
            aiProvider: document.getElementById('ai-provider')?.value || 'demo',
            aiApiKey: document.getElementById('ai-apikey')?.value || '',
            aiModel: document.getElementById('ai-model')?.value || '',
            voiceLanguage: document.getElementById('voice-language')?.value || 'en-US',
            autoSpeak: document.getElementById('auto-speak')?.checked || false,
            ttsVoice: document.getElementById('tts-voice')?.value || '',
            theme: document.getElementById('theme-select')?.value || 'dark',
            accentColor: this.currentAccentColor || '#0a84ff'
        };
        localStorage.setItem('ozayn_settings', JSON.stringify(this.settings));
        this.applyTheme(this.settings.theme);
        this.applyAccentColor(this.settings.accentColor);
        if (this.recognition) {
            this.recognition.lang = this.settings.voiceLanguage;
        }
    }

    applyTheme(theme) {
        const root = document.documentElement;
        const themes = {
            dark: { bg: '#08081a', surface: '#161632', glass: 'rgba(28, 28, 56, 0.55)' },
            midnight: { bg: '#0a1628', surface: '#0f2035', glass: 'rgba(15, 32, 53, 0.6)' },
            purple: { bg: '#0d0818', surface: '#1a0f2e', glass: 'rgba(26, 15, 46, 0.55)' },
            green: { bg: '#081a0f', surface: '#0f2e18', glass: 'rgba(15, 46, 24, 0.55)' }
        };

        const t = themes[theme] || themes.dark;
        root.style.setProperty('--bg-base', t.bg);
        root.style.setProperty('--glass-bg', t.glass);
    }

    applyAccentColor(color) {
        document.documentElement.style.setProperty('--accent', color);
        document.documentElement.style.setProperty('--accent-hover', color + 'cc');
        document.documentElement.style.setProperty('--accent-glow', color + '40');
    }

    // ==================== API Methods ====================

    async api(endpoint, method = 'GET', data = null) {
        const options = {
            method,
            headers: {
                'Content-Type': 'application/json',
                'X-Session-ID': this.sessionId || ''
            }
        };

        if (data) {
            options.body = JSON.stringify(data);
        }

        try {
            const response = await fetch(`${this.apiBase}${endpoint}`, options);
            const result = await response.json();
            
            if (!response.ok) {
                throw new Error(result.error || 'Request failed');
            }
            
            return result;
        } catch (error) {
            console.error('API Error:', error);
            throw error;
        }
    }

    // ==================== Auth Methods ====================

    checkAuth() {
        if (this.sessionId && this.user) {
            this.showScreen('main');
            this.loadProjects();
            this.loadConversations();
            this.updateUserInfo();
        } else {
            this.showScreen('auth');
        }
    }

    async login(username, password) {
        try {
            const result = await this.api('/auth/login', 'POST', { username, password });
            if (result.success) {
                this.sessionId = result.session_id;
                this.user = result.user;
                localStorage.setItem('ozayn_session', this.sessionId);
                localStorage.setItem('ozayn_user', JSON.stringify(this.user));
                this.showScreen('main');
                this.loadProjects();
                this.loadConversations();
                this.updateUserInfo();
            }
        } catch (error) {
            this.showError('auth-error', error.message);
        }
    }

    async register(username, password, email, fullName) {
        try {
            const result = await this.api('/auth/register', 'POST', {
                username, password, email, full_name: fullName
            });
            if (result.success) {
                await this.login(username, password);
            }
        } catch (error) {
            this.showError('auth-error', error.message);
        }
    }

    logout() {
        this.api('/auth/logout', 'POST').catch(() => {});
        this.sessionId = null;
        this.user = null;
        localStorage.removeItem('ozayn_session');
        localStorage.removeItem('ozayn_user');
        this.showScreen('auth');
    }

    // ==================== Chat Methods ====================

    async sendMessage(message) {
        if (!message.trim()) return;

        this.addToHistory(message);
        this.addMessage('user', message);
        this.clearInput();
        this.showTypingIndicator();

        try {
            const result = await this.api('/chat/send', 'POST', {
                message,
                conversation_id: this.currentConversation,
                project_id: this.currentProject
            });

            this.hideTypingIndicator();

            if (result.success) {
                this.currentConversation = result.conversation_id;
                this.addMessage('assistant', result.response);
                this.loadConversations();
                
                // Auto-speak if enabled
                if (this.settings.autoSpeak) {
                    this.speak(result.response);
                }
            }
        } catch (error) {
            this.hideTypingIndicator();
            this.addMessage('system', 'Error: ' + error.message);
        }
    }

    addMessage(role, content) {
        const container = document.getElementById('chat-messages');
        const message = document.createElement('div');
        message.className = `message ${role}`;
        
        // Simple markdown rendering
        let html = content
            .replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>')
            .replace(/\*(.*?)\*/g, '<em>$1</em>')
            .replace(/`(.*?)`/g, '<code>$1</code>')
            .replace(/\n/g, '<br>');
        
        message.innerHTML = html;
        container.appendChild(message);
        container.scrollTop = container.scrollHeight;
    }

    showTypingIndicator() {
        const container = document.getElementById('chat-messages');
        const indicator = document.createElement('div');
        indicator.className = 'message assistant typing';
        indicator.id = 'typing-indicator';
        indicator.innerHTML = '<span class="dot"></span><span class="dot"></span><span class="dot"></span>';
        container.appendChild(indicator);
        container.scrollTop = container.scrollHeight;
    }

    hideTypingIndicator() {
        const indicator = document.getElementById('typing-indicator');
        if (indicator) indicator.remove();
    }

    clearInput() {
        const input = document.getElementById('chat-input');
        input.value = '';
        input.style.height = 'auto';
    }

    async loadConversations() {
        try {
            const result = await this.api('/chat/list');
            const list = document.getElementById('conversations-list');
            list.innerHTML = '';

            result.conversations.forEach(conv => {
                const li = document.createElement('li');
                li.textContent = conv.title || `Conversation ${conv.id}`;
                li.dataset.id = conv.id;
                if (conv.id == this.currentConversation) {
                    li.classList.add('active');
                }
                li.onclick = () => this.loadConversation(conv.id);
                list.appendChild(li);
            });
        } catch (error) {
            console.error('Failed to load conversations:', error);
        }
    }

    async loadConversation(id) {
        this.currentConversation = id;
        document.getElementById('chat-messages').innerHTML = '';

        try {
            const result = await this.api(`/chat/history/${id}`);
            result.messages.forEach(msg => {
                this.addMessage(msg.role, msg.content);
            });
            this.loadConversations();
            this.showView('chat');
        } catch (error) {
            console.error('Failed to load conversation:', error);
        }
    }

    newConversation() {
        this.currentConversation = null;
        document.getElementById('chat-messages').innerHTML = 
            '<div class="message system">New conversation started. How can I help?</div>';
        document.getElementById('chat-title').textContent = 'New Conversation';
        this.loadConversations();
        this.showView('chat');
    }

    // ==================== Voice Methods ====================

    initVoice() {
        if ('webkitSpeechRecognition' in window || 'SpeechRecognition' in window) {
            const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
            this.recognition = new SpeechRecognition();
            this.recognition.continuous = false;
            this.recognition.interimResults = true;
            this.recognition.lang = this.settings.voiceLanguage || 'en-US';

            this.recognition.onresult = (event) => {
                const transcript = Array.from(event.results)
                    .map(result => result[0].transcript)
                    .join('');
                
                document.getElementById('chat-input').value = transcript;
            };

            this.recognition.onend = () => {
                this.stopRecording();
            };

            this.recognition.onerror = (event) => {
                console.error('Speech recognition error:', event.error);
                this.stopRecording();
            };
        }
    }

    loadVoices() {
        const voiceSelect = document.getElementById('tts-voice');
        if (!voiceSelect) return;

        const loadVoiceList = () => {
            const voices = this.synthesis.getVoices();
            const language = this.settings.voiceLanguage || 'en-US';
            voiceSelect.innerHTML = '';
            
            // Filter voices by language, then show all
            const langPrefix = language.split('-')[0];
            const filteredVoices = voices.filter(v => v.lang.startsWith(langPrefix));
            const otherVoices = voices.filter(v => !v.lang.startsWith(langPrefix));
            
            const allVoices = [...filteredVoices, ...otherVoices];
            allVoices.forEach((voice, i) => {
                const option = document.createElement('option');
                option.value = voices.indexOf(voice);
                option.textContent = `${voice.name} (${voice.lang})`;
                if (voice.lang.startsWith(langPrefix)) {
                    option.textContent = `★ ${voice.name} (${voice.lang})`;
                }
                voiceSelect.appendChild(option);
            });
        };

        loadVoiceList();
        this.synthesis.onvoiceschanged = loadVoiceList;
    }

    toggleRecording() {
        if (this.isRecording) {
            this.recognition?.stop();
        } else {
            this.recognition?.start();
            this.startRecording();
        }
    }

    startRecording() {
        this.isRecording = true;
        document.getElementById('voice-btn').classList.add('recording');
        document.getElementById('voice-indicator').classList.remove('hidden');
    }

    stopRecording() {
        this.isRecording = false;
        document.getElementById('voice-btn').classList.remove('recording');
        document.getElementById('voice-indicator').classList.add('hidden');
    }

    speak(text) {
        if (!this.synthesis) return;
        
        this.synthesis.cancel();
        
        const utterance = new SpeechSynthesisUtterance(text);
        utterance.rate = 1;
        utterance.pitch = 1;
        
        const voices = this.synthesis.getVoices();
        const voiceIndex = this.settings.ttsVoice;
        if (voices[voiceIndex]) {
            utterance.voice = voices[voiceIndex];
        }
        
        this.synthesis.speak(utterance);
    }

    stopSpeaking() {
        this.synthesis?.cancel();
    }

    // ==================== Project Methods ====================

    async loadProjects() {
        try {
            const result = await this.api('/projects/list');
            const list = document.getElementById('projects-list');
            const grid = document.getElementById('projects-grid');
            
            list.innerHTML = '';
            grid.innerHTML = '';

            result.projects.forEach(project => {
                // Sidebar list
                const li = document.createElement('li');
                li.textContent = project.name;
                li.dataset.id = project.id;
                if (project.id == this.currentProject) {
                    li.classList.add('active');
                }
                li.onclick = () => this.selectProject(project.id);
                list.appendChild(li);

                // Grid card
                const card = document.createElement('div');
                card.className = 'project-card';
                card.innerHTML = `
                    <h4>${this.escapeHtml(project.name)}</h4>
                    <p>${this.escapeHtml(project.description || 'No description')}</p>
                    <div class="progress-bar">
                        <div class="progress-fill" style="width: ${project.progress}%"></div>
                    </div>
                `;
                card.onclick = () => this.selectProject(project.id);
                grid.appendChild(card);
            });
        } catch (error) {
            console.error('Failed to load projects:', error);
        }
    }

    async selectProject(id) {
        this.currentProject = id;
        const project = await this.api(`/projects/${id}`).catch(() => null);
        document.getElementById('chat-project').textContent = project ? `Project: ${project.project.name}` : '';
        this.loadProjects();
        this.showView('chat');
    }

    async createProject(name, description) {
        try {
            await this.api('/projects/create', 'POST', { name, description });
            this.loadProjects();
            this.closeModal();
        } catch (error) {
            console.error('Failed to create project:', error);
        }
    }

    // ==================== Task Methods ====================

    async loadTasks() {
        try {
            const result = await this.api('/tasks/list');
            const list = document.getElementById('tasks-list');
            list.innerHTML = '';

            if (result.tasks.length === 0) {
                list.innerHTML = '<div class="message system">No tasks yet. Create one to get started.</div>';
                return;
            }

            result.tasks.forEach(task => {
                const item = document.createElement('div');
                item.className = 'task-item';
                item.innerHTML = `
                    <div class="task-checkbox ${task.status === 'completed' ? 'checked' : ''}" 
                         data-id="${task.id}" data-status="${task.status}">
                        ${task.status === 'completed' ? '✓' : ''}
                    </div>
                    <div class="task-content">
                        <h4>${this.escapeHtml(task.title)}</h4>
                        <p>${this.escapeHtml(task.description || '')}</p>
                    </div>
                    <span class="task-priority ${task.priority}">${task.priority}</span>
                `;
                
                const checkbox = item.querySelector('.task-checkbox');
                checkbox.onclick = () => this.toggleTask(task.id, task.status);
                
                list.appendChild(item);
            });
        } catch (error) {
            console.error('Failed to load tasks:', error);
        }
    }

    async toggleTask(id, currentStatus) {
        const newStatus = currentStatus === 'completed' ? 'pending' : 'completed';
        try {
            await this.api(`/tasks/${id}`, 'PUT', { status: newStatus });
            this.loadTasks();
        } catch (error) {
            console.error('Failed to update task:', error);
        }
    }

    async createTask(title, description, priority) {
        try {
            await this.api('/tasks/create', 'POST', {
                title,
                description,
                priority,
                project_id: this.currentProject
            });
            this.loadTasks();
            this.closeModal();
        } catch (error) {
            console.error('Failed to create task:', error);
        }
    }

    // ==================== Knowledge Methods ====================

    async loadKnowledge() {
        try {
            const result = await this.api('/knowledge/list');
            const list = document.getElementById('knowledge-list');
            list.innerHTML = '';

            if (result.results.length === 0) {
                list.innerHTML = '<div class="message system">Knowledge base is empty. Add your first entry.</div>';
                return;
            }

            result.results.forEach(entry => {
                const item = document.createElement('div');
                item.className = 'knowledge-item';
                
                const tags = JSON.parse(entry.tags || '[]');
                const tagsHtml = tags.map(t => `<span class="tag">${this.escapeHtml(t)}</span>`).join('');
                
                item.innerHTML = `
                    <h4>${this.escapeHtml(entry.title)}</h4>
                    <p>${this.escapeHtml(entry.content.substring(0, 250))}${entry.content.length > 250 ? '...' : ''}</p>
                    <div class="knowledge-tags">${tagsHtml}</div>
                `;
                list.appendChild(item);
            });
        } catch (error) {
            console.error('Failed to load knowledge:', error);
        }
    }

    async addKnowledge(title, content, tags) {
        try {
            await this.api('/knowledge/add', 'POST', {
                title,
                content,
                tags: tags.split(',').map(t => t.trim()).filter(t => t),
                project_id: this.currentProject
            });
            this.loadKnowledge();
            this.closeModal();
        } catch (error) {
            console.error('Failed to add knowledge:', error);
        }
    }

    // ==================== ARWE Methods ====================

    async loadARWEStatus() {
        try {
            const result = await this.api('/arwe/status');
            this.renderARWEDashboard(result);
        } catch (error) {
            console.error('Failed to load ARWE status:', error);
            document.getElementById('arwe-dashboard').innerHTML = '<div class="error">Failed to load ARWE status</div>';
        }
    }

    renderARWEDashboard(data) {
        const dashboard = document.getElementById('arwe-dashboard');
        if (!data || !data.systems) {
            dashboard.innerHTML = '<div class="empty">No ARWE data available</div>';
            return;
        }

        let html = '<div class="arwe-grid">';
        for (const [name, info] of Object.entries(data.systems)) {
            const statusClass = info.status === 'online' ? 'online' : (info.status === 'offline' ? 'offline' : 'unknown');
            const details = info.details || {};
            let detailsHtml = '';
            
            // Add specific details based on system type
            if (name === 'edunex') {
                detailsHtml = `
                    <div class="arwe-details">
                        <span>Students: ${details.students || 0}</span>
                        <span>Teachers: ${details.teachers || 0}</span>
                        <span>Courses: ${details.courses || 0}</span>
                    </div>
                `;
            } else if (name === 'govyx') {
                detailsHtml = `
                    <div class="arwe-details">
                        <span>Departments: ${details.departments || 0}</span>
                        <span>Pending: ${details.pending_tasks || 0}</span>
                        <span>Completed: ${details.completed_today || 0}</span>
                    </div>
                `;
            } else if (name === 'kidane') {
                detailsHtml = `
                    <div class="arwe-details">
                        <span>Drones: ${details.drones || 0}</span>
                        <span>Active: ${details.active || 0}</span>
                        <span>Charging: ${details.charging || 0}</span>
                    </div>
                `;
            } else if (name === 'canivox') {
                detailsHtml = `
                    <div class="arwe-details">
                        <span>Robots: ${details.robots || 0}</span>
                        <span>Operational: ${details.operational || 0}</span>
                        <span>Standby: ${details.standby || 0}</span>
                    </div>
                `;
            }

            html += `
                <div class="arwe-card ${statusClass}">
                    <h4>${name.charAt(0).toUpperCase() + name.slice(1)}</h4>
                    <span class="status-badge">${info.status || 'unknown'}</span>
                    ${detailsHtml}
                </div>
            `;
        }
        html += '</div>';
        dashboard.innerHTML = html;

        // Load briefing
        this.loadARWEBriefing();
    }

    async loadARWEBriefing() {
        try {
            const result = await this.api('/arwe/briefing');
            document.getElementById('arwe-briefing').innerHTML = `
                <div class="briefing-content">
                    <h4>Daily Briefing</h4>
                    <pre>${result.briefing || 'No briefing available'}</pre>
                </div>
            `;
        } catch (error) {
            console.error('Failed to load briefing:', error);
        }
    }

    // ==================== Decisions Methods ====================

    async loadDecisions() {
        try {
            const result = await this.api('/decisions/list');
            this.renderDecisions(result.decisions || []);
        } catch (error) {
            console.error('Failed to load decisions:', error);
            document.getElementById('decisions-list').innerHTML = '<div class="error">Failed to load decisions</div>';
        }
    }

    renderDecisions(decisions) {
        const list = document.getElementById('decisions-list');
        if (!decisions.length) {
            list.innerHTML = '<div class="empty">No decisions yet. Start by creating one in chat.</div>';
            return;
        }

        let html = '';
        decisions.forEach(d => {
            html += `
                <div class="decision-card" data-id="${d.id}">
                    <div class="decision-header">
                        <span class="decision-status ${d.status}">${d.status}</span>
                        <span class="decision-date">${new Date(d.created_at).toLocaleDateString()}</span>
                    </div>
                    <p>${this.escapeHtml(d.context)}</p>
                    ${d.chosen_option ? `<div class="chosen-option"><strong>Decision:</strong> ${this.escapeHtml(d.chosen_option)}</div>` : ''}
                </div>
            `;
        });
        list.innerHTML = html;
    }

    showNewDecisionModal() {
        this.showModal('New Decision', `
            <textarea id="decision-context" placeholder="Describe the decision context..." required></textarea>
            <button onclick="app.createDecision(document.getElementById('decision-context').value)">Create Decision</button>
        `);
    }

    async createDecision(context) {
        if (!context.trim()) return;
        try {
            await this.api('/decisions/create', 'POST', { context });
            this.closeModal();
            this.loadDecisions();
        } catch (error) {
            console.error('Failed to create decision:', error);
        }
    }

    // ==================== Audit Methods ====================

    async loadAuditLog() {
        try {
            const result = await this.api('/audit/log');
            this.renderAuditLog(result.log || []);
        } catch (error) {
            console.error('Failed to load audit log:', error);
            document.getElementById('audit-log').innerHTML = '<div class="error">Failed to load audit log</div>';
        }
    }

    renderAuditLog(log) {
        const container = document.getElementById('audit-log');
        if (!log.length) {
            container.innerHTML = '<div class="empty">No audit entries yet.</div>';
            return;
        }

        let html = '<div class="audit-entries">';
        log.forEach(entry => {
            html += `
                <div class="audit-entry">
                    <span class="audit-time">${new Date(entry.created_at).toLocaleString()}</span>
                    <span class="audit-action">${this.escapeHtml(entry.action)}</span>
                    <span class="audit-result ${entry.result}">${entry.result}</span>
                </div>
            `;
        });
        html += '</div>';
        container.innerHTML = html;
    }

    // ==================== Keyboard Shortcuts ====================

    initKeyboardShortcuts() {
        document.addEventListener('keydown', (e) => {
            // Ctrl/Cmd + K: Focus chat input
            if ((e.ctrlKey || e.metaKey) && e.key === 'k') {
                e.preventDefault();
                document.getElementById('chat-input')?.focus();
            }

            // Ctrl/Cmd + N: New chat
            if ((e.ctrlKey || e.metaKey) && e.key === 'n') {
                e.preventDefault();
                this.newConversation();
            }

            // Ctrl/Cmd + /: Show help
            if ((e.ctrlKey || e.metaKey) && e.key === '/') {
                e.preventDefault();
                this.sendMessage('help');
            }

            // Escape: Close modal
            if (e.key === 'Escape') {
                this.closeModal();
            }

            // Arrow Up/Down: Command history (only when chat input is focused)
            if (e.target.id === 'chat-input') {
                if (e.key === 'ArrowUp') {
                    e.preventDefault();
                    this.navigateHistory(-1);
                } else if (e.key === 'ArrowDown') {
                    e.preventDefault();
                    this.navigateHistory(1);
                }
            }
        });
    }

    navigateHistory(direction) {
        if (this.commandHistory.length === 0) return;

        const input = document.getElementById('chat-input');
        
        if (direction === -1) {
            // Up arrow - go back in history
            if (this.historyIndex < this.commandHistory.length - 1) {
                this.historyIndex++;
                input.value = this.commandHistory[this.historyIndex];
            }
        } else {
            // Down arrow - go forward in history
            if (this.historyIndex > 0) {
                this.historyIndex--;
                input.value = this.commandHistory[this.historyIndex];
            } else {
                this.historyIndex = -1;
                input.value = '';
            }
        }
    }

    addToHistory(command) {
        if (!command.trim()) return;
        
        // Avoid duplicates at the end
        if (this.commandHistory[this.commandHistory.length - 1] !== command) {
            this.commandHistory.push(command);
            
            // Keep last 50 commands
            if (this.commandHistory.length > 50) {
                this.commandHistory = this.commandHistory.slice(-50);
            }
            
            localStorage.setItem('ozayn_command_history', JSON.stringify(this.commandHistory));
        }
        
        this.historyIndex = -1;
    }

    // ==================== File Upload ====================

    async handleFileUpload(event) {
        const file = event.target.files[0];
        if (!file) return;

        const maxSize = 1024 * 1024; // 1MB limit
        if (file.size > maxSize) {
            this.addMessage('system', 'File too large. Maximum size is 1MB.');
            return;
        }

        const allowedTypes = ['.txt', '.md', '.json', '.csv', '.php', '.js', '.html', '.css', '.py', '.sql'];
        const ext = '.' + file.name.split('.').pop().toLowerCase();
        
        if (!allowedTypes.includes(ext)) {
            this.addMessage('system', 'File type not allowed. Supported: ' + allowedTypes.join(', '));
            return;
        }

        try {
            const content = await this.readFileContent(file);
            const message = `File uploaded: ${file.name}\n\n\`\`\`${ext.slice(1)}\n${content}\n\`\`\``;
            this.sendMessage(message);
        } catch (error) {
            this.addMessage('system', 'Error reading file: ' + error.message);
        }

        // Reset file input
        event.target.value = '';
    }

    readFileContent(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onload = () => resolve(reader.result);
            reader.onerror = () => reject(new Error('Failed to read file'));
            reader.readAsText(file);
        });
    }

    // ==================== UI Methods ====================

    showScreen(screen) {
        document.querySelectorAll('.screen').forEach(s => s.classList.remove('active'));
        document.getElementById(`${screen}-screen`).classList.add('active');
    }

    showView(view) {
        document.querySelectorAll('.view').forEach(v => v.classList.remove('active'));
        document.getElementById(`${view}-view`).classList.add('active');
        
        document.querySelectorAll('.nav-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.view === view);
        });

        if (view === 'tasks') this.loadTasks();
        if (view === 'knowledge') this.loadKnowledge();
        if (view === 'arwe') this.loadARWEStatus();
        if (view === 'decisions') this.loadDecisions();
        if (view === 'audit') this.loadAuditLog();
    }

    updateUserInfo() {
        document.getElementById('user-info').textContent = this.user?.username || '';
    }

    showError(elementId, message) {
        const el = document.getElementById(elementId);
        if (el) {
            el.textContent = message;
            setTimeout(() => el.textContent = '', 5000);
        }
    }

    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    showModal(title, content) {
        document.getElementById('modal-title').textContent = title;
        document.getElementById('modal-content').innerHTML = content;
        document.getElementById('modal-overlay').classList.remove('hidden');
    }

    closeModal() {
        document.getElementById('modal-overlay').classList.add('hidden');
    }

    showNewProjectModal() {
        this.showModal('New Project', `
            <input type="text" id="project-name" placeholder="Project name" required>
            <textarea id="project-desc" placeholder="Description (optional)"></textarea>
            <button onclick="app.createProject(
                document.getElementById('project-name').value,
                document.getElementById('project-desc').value
            )">Create Project</button>
        `);
    }

    showNewTaskModal() {
        this.showModal('New Task', `
            <input type="text" id="task-title" placeholder="Task title" required>
            <textarea id="task-desc" placeholder="Description (optional)"></textarea>
            <select id="task-priority">
                <option value="low">Low Priority</option>
                <option value="medium" selected>Medium Priority</option>
                <option value="high">High Priority</option>
                <option value="urgent">Urgent</option>
            </select>
            <button onclick="app.createTask(
                document.getElementById('task-title').value,
                document.getElementById('task-desc').value,
                document.getElementById('task-priority').value
            )">Create Task</button>
        `);
    }

    showAddKnowledgeModal() {
        this.showModal('Add Knowledge', `
            <input type="text" id="knowledge-title" placeholder="Title" required>
            <textarea id="knowledge-content" placeholder="Content" required></textarea>
            <input type="text" id="knowledge-tags" placeholder="Tags (comma separated)">
            <button onclick="app.addKnowledge(
                document.getElementById('knowledge-title').value,
                document.getElementById('knowledge-content').value,
                document.getElementById('knowledge-tags').value
            )">Add Entry</button>
        `);
    }

    // ==================== Event Binding ====================

    bindEvents() {
        // Auth tabs
        document.querySelectorAll('.tab-btn').forEach(btn => {
            btn.onclick = () => {
                document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
                document.querySelectorAll('.auth-form').forEach(f => f.classList.remove('active'));
                btn.classList.add('active');
                document.getElementById(`${btn.dataset.tab}-form`).classList.add('active');
            };
        });

        // Login form
        document.getElementById('login-form').onsubmit = (e) => {
            e.preventDefault();
            this.login(
                document.getElementById('login-username').value,
                document.getElementById('login-password').value
            );
        };

        // Register form
        document.getElementById('register-form').onsubmit = (e) => {
            e.preventDefault();
            this.register(
                document.getElementById('reg-username').value,
                document.getElementById('reg-password').value,
                document.getElementById('reg-email').value,
                document.getElementById('reg-fullname').value
            );
        };

        // Logout
        document.getElementById('logout-btn').onclick = () => this.logout();

        // Navigation
        document.querySelectorAll('.nav-btn').forEach(btn => {
            btn.onclick = () => this.showView(btn.dataset.view);
        });

        // Chat input
        const chatInput = document.getElementById('chat-input');
        chatInput.onkeydown = (e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                this.sendMessage(e.target.value);
            }
        };

        chatInput.oninput = function() {
            this.style.height = 'auto';
            this.style.height = Math.min(this.scrollHeight, 120) + 'px';
        };

        document.getElementById('send-btn').onclick = () => {
            this.sendMessage(document.getElementById('chat-input').value);
        };

        // Voice button
        document.getElementById('voice-btn').onclick = () => this.toggleRecording();

        // TTS button
        document.getElementById('tts-btn').onclick = () => {
            const lastAssistant = document.querySelector('.message.assistant:last-child');
            if (lastAssistant) {
                this.speak(lastAssistant.textContent);
            }
        };

        // New chat
        document.getElementById('new-chat-btn').onclick = () => this.newConversation();

        // Add buttons (sidebar + view header)
        const addProjBtn = document.getElementById('add-project-btn');
        if (addProjBtn) addProjBtn.onclick = () => this.showNewProjectModal();
        const addProjBtn2 = document.getElementById('add-project-btn-2');
        if (addProjBtn2) addProjBtn2.onclick = () => this.showNewProjectModal();
        const addTaskBtn2 = document.getElementById('add-task-btn-2');
        if (addTaskBtn2) addTaskBtn2.onclick = () => this.showNewTaskModal();
        const addKnowBtn2 = document.getElementById('add-knowledge-btn-2');
        if (addKnowBtn2) addKnowBtn2.onclick = () => this.showAddKnowledgeModal();

        // ARWE refresh
        const refreshArweBtn = document.getElementById('refresh-arwe-btn');
        if (refreshArweBtn) refreshArweBtn.onclick = () => this.loadARWEStatus();

        // Decisions
        const newDecBtn = document.getElementById('new-decision-btn');
        if (newDecBtn) newDecBtn.onclick = () => this.showNewDecisionModal();

        // Audit refresh
        const refreshAuditBtn = document.getElementById('refresh-audit-btn');
        if (refreshAuditBtn) refreshAuditBtn.onclick = () => this.loadAuditLog();

        // File upload
        const fileInput = document.getElementById('file-input');
        if (fileInput) {
            fileInput.onchange = (e) => this.handleFileUpload(e);
        }

        // Mobile menu toggle
        document.getElementById('mobile-menu-btn').onclick = () => {
            document.getElementById('sidebar').classList.toggle('open');
        };

        // Modal close
        document.getElementById('modal-close').onclick = () => this.closeModal();
        document.getElementById('modal-overlay').onclick = (e) => {
            if (e.target === e.currentTarget) this.closeModal();
        };

        // Settings
        const saveSettingsBtn = document.getElementById('save-settings-btn');
        if (saveSettingsBtn) {
            saveSettingsBtn.onclick = () => {
                this.saveSettings();
                alert('Settings saved!');
            };
        }

        // Theme color buttons
        document.querySelectorAll('.color-btn').forEach(btn => {
            btn.onclick = () => {
                this.currentAccentColor = btn.dataset.color;
                this.applyAccentColor(btn.dataset.color);
            };
        });

        // Export data button
        const exportBtn = document.getElementById('export-data-btn');
        if (exportBtn) {
            exportBtn.onclick = () => {
                this.sendMessage('export all');
            };
        }
    }
}

// Initialize app
const app = new OzaynApp();
