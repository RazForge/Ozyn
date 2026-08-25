const api = window.electronAPI;
let sessionId = null;
let user = null;
let currentConversation = null;
let currentProject = null;

// ==================== Init ====================

async function init() {
    sessionId = await api.getStore('sessionId');
    const userStr = await api.getStore('user');
    user = userStr ? JSON.parse(userStr) : null;

    if (!sessionId || !user) {
        window.location.href = 'login.html';
        return;
    }

    document.getElementById('user-info').textContent = user.username;
    bindEvents();
    loadConversations();
    loadProjects();
}

// ==================== API Helper ====================

async function callApi(endpoint, method = 'GET', data = null) {
    const result = await api.api(endpoint, method, data, sessionId);
    if (!result.ok) {
        throw new Error(result.data?.error || 'Request failed');
    }
    return result.data;
}

// ==================== Events ====================

function bindEvents() {
    // Title bar
    document.querySelectorAll('.titlebar-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            const a = btn.dataset.action;
            if (a === 'minimize') api.minimize();
            else if (a === 'maximize') api.maximize();
            else if (a === 'close') api.close();
        });
    });

    // Nav
    document.querySelectorAll('.nav-btn').forEach(btn => {
        btn.addEventListener('click', () => showView(btn.dataset.view));
    });

    // New chat
    document.getElementById('new-chat-btn').addEventListener('click', newConversation);

    // Send
    document.getElementById('send-btn').addEventListener('click', () => {
        sendMessage(document.getElementById('chat-input').value);
    });

    document.getElementById('chat-input').addEventListener('keydown', (e) => {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            sendMessage(e.target.value);
        }
    });

    document.getElementById('chat-input').addEventListener('input', function () {
        this.style.height = 'auto';
        this.style.height = Math.min(this.scrollHeight, 120) + 'px';
    });

    // Add project buttons
    document.getElementById('add-project-btn')?.addEventListener('click', showNewProjectModal);
    document.getElementById('add-project-btn-2')?.addEventListener('click', showNewProjectModal);

    // Add task button
    document.getElementById('add-task-btn')?.addEventListener('click', showNewTaskModal);

    // Logout
    document.getElementById('logout-btn').addEventListener('click', logout);

    // Modal
    document.getElementById('modal-cancel').addEventListener('click', closeModal);
    document.getElementById('modal-overlay').addEventListener('click', (e) => {
        if (e.target === e.currentTarget) closeModal();
    });

    // Settings
    document.getElementById('save-settings-btn')?.addEventListener('click', saveSettings);

    // Keyboard
    document.addEventListener('keydown', (e) => {
        if ((e.ctrlKey || e.metaKey) && e.key === 'n') {
            e.preventDefault();
            newConversation();
        }
        if (e.key === 'Escape') closeModal();
    });
}

// ==================== Chat ====================

async function sendMessage(message) {
    if (!message.trim()) return;

    const input = document.getElementById('chat-input');
    input.value = '';
    input.style.height = 'auto';
    addMessage('user', message);
    showTyping();

    try {
        const result = await callApi('/chat/send', 'POST', {
            message,
            conversation_id: currentConversation,
            project_id: currentProject
        });
        hideTyping();
        if (result.success) {
            currentConversation = result.conversation_id;
            addMessage('assistant', result.response);
            loadConversations();
        }
    } catch (err) {
        hideTyping();
        addMessage('system', 'Error: ' + err.message);
    }
}

function addMessage(role, content) {
    const container = document.getElementById('chat-messages');
    const msg = document.createElement('div');
    msg.className = `message ${role}`;
    let html = content
        .replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>')
        .replace(/\*(.*?)\*/g, '<em>$1</em>')
        .replace(/`(.*?)`/g, '<code>$1</code>')
        .replace(/\n/g, '<br>');
    msg.innerHTML = html;
    container.appendChild(msg);
    container.scrollTop = container.scrollHeight;
}

function showTyping() {
    const container = document.getElementById('chat-messages');
    const el = document.createElement('div');
    el.className = 'message typing';
    el.id = 'typing-indicator';
    el.innerHTML = '<span class="dot"></span><span class="dot"></span><span class="dot"></span>';
    container.appendChild(el);
    container.scrollTop = container.scrollHeight;
}

function hideTyping() {
    document.getElementById('typing-indicator')?.remove();
}

function newConversation() {
    currentConversation = null;
    document.getElementById('chat-messages').innerHTML = '<div class="message system">New conversation started. How can I help?</div>';
    document.getElementById('chat-title').textContent = 'New Conversation';
    loadConversations();
    showView('chat');
}

// ==================== Conversations ====================

async function loadConversations() {
    try {
        const result = await callApi('/chat/list');
        const list = document.getElementById('conversations-list');
        list.innerHTML = '';
        (result.conversations || []).forEach(conv => {
            const li = document.createElement('li');
            li.textContent = conv.title || `Chat ${conv.id}`;
            if (conv.id == currentConversation) li.classList.add('active');
            li.addEventListener('click', () => loadConversation(conv.id));
            list.appendChild(li);
        });
    } catch (e) {}
}

async function loadConversation(id) {
    currentConversation = id;
    document.getElementById('chat-messages').innerHTML = '';
    try {
        const result = await callApi(`/chat/history/${id}`);
        (result.messages || []).forEach(msg => addMessage(msg.role, msg.content));
        loadConversations();
        showView('chat');
    } catch (e) {}
}

// ==================== Projects ====================

async function loadProjects() {
    try {
        const result = await callApi('/projects/list');
        const sidebarList = document.getElementById('projects-list');
        const grid = document.getElementById('projects-grid');
        sidebarList.innerHTML = '';
        grid.innerHTML = '';
        (result.projects || []).forEach(project => {
            const li = document.createElement('li');
            li.textContent = project.name;
            if (project.id == currentProject) li.classList.add('active');
            li.addEventListener('click', () => selectProject(project.id));
            sidebarList.appendChild(li);

            const card = document.createElement('div');
            card.className = 'project-card';
            card.innerHTML = `<h4>${esc(project.name)}</h4><p>${esc(project.description || 'No description')}</p>`;
            card.addEventListener('click', () => selectProject(project.id));
            grid.appendChild(card);
        });
    } catch (e) {}
}

async function selectProject(id) {
    currentProject = id;
    loadProjects();
    showView('chat');
}

function showNewProjectModal() {
    showModal('New Project', `
        <input type="text" id="project-name" placeholder="Project name">
        <textarea id="project-desc" placeholder="Description (optional)"></textarea>
    `, async () => {
        const name = document.getElementById('project-name').value.trim();
        if (!name) return;
        await callApi('/projects/create', 'POST', { name, description: document.getElementById('project-desc').value });
        closeModal();
        loadProjects();
    });
}

// ==================== Tasks ====================

async function loadTasks() {
    try {
        const result = await callApi('/tasks/list');
        const list = document.getElementById('tasks-list');
        list.innerHTML = '';
        if (!result.tasks?.length) {
            list.innerHTML = '<div class="empty">No tasks yet.</div>';
            return;
        }
        result.tasks.forEach(task => {
            const item = document.createElement('div');
            item.className = 'task-item';
            item.innerHTML = `
                <div class="task-checkbox ${task.status === 'completed' ? 'checked' : ''}">${task.status === 'completed' ? '&#10003;' : ''}</div>
                <div class="task-content"><h4>${esc(task.title)}</h4><p>${esc(task.description || '')}</p></div>
                <span class="task-priority ${task.priority}">${task.priority}</span>
            `;
            item.querySelector('.task-checkbox').addEventListener('click', async () => {
                const newStatus = task.status === 'completed' ? 'pending' : 'completed';
                await callApi(`/tasks/${task.id}`, 'PUT', { status: newStatus });
                loadTasks();
            });
            list.appendChild(item);
        });
    } catch (e) {}
}

function showNewTaskModal() {
    showModal('New Task', `
        <input type="text" id="task-title" placeholder="Task title">
        <textarea id="task-desc" placeholder="Description (optional)"></textarea>
        <select id="task-priority" style="width:100%;padding:10px;background:var(--glass-lighter);border:1px solid var(--border);border-radius:var(--radius-xs);color:var(--text);font-size:14px;font-family:inherit;">
            <option value="low">Low</option>
            <option value="medium" selected>Medium</option>
            <option value="high">High</option>
            <option value="urgent">Urgent</option>
        </select>
    `, async () => {
        const title = document.getElementById('task-title').value.trim();
        if (!title) return;
        await callApi('/tasks/create', 'POST', {
            title,
            description: document.getElementById('task-desc').value,
            priority: document.getElementById('task-priority').value,
            project_id: currentProject
        });
        closeModal();
        loadTasks();
    });
}

// ==================== Settings ====================

async function saveSettings() {
    const settings = {
        aiProvider: document.getElementById('ai-provider').value,
        aiApiKey: document.getElementById('ai-apikey').value,
        aiModel: document.getElementById('ai-model').value,
        voiceLanguage: document.getElementById('voice-language').value,
    };
    await api.setStore('settings', JSON.stringify(settings));
    alert('Settings saved!');
}

// ==================== UI Helpers ====================

function showView(view) {
    document.querySelectorAll('.view').forEach(v => v.classList.remove('active'));
    document.getElementById(`${view}-view`)?.classList.add('active');
    document.querySelectorAll('.nav-btn').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.view === view);
    });
    if (view === 'tasks') loadTasks();
    if (view === 'projects') loadProjects();
}

function showModal(title, content, onConfirm) {
    document.getElementById('modal-title').textContent = title;
    document.getElementById('modal-content').innerHTML = content;
    document.getElementById('modal-overlay').classList.add('active');
    document.getElementById('modal-confirm').onclick = onConfirm;
}

function closeModal() {
    document.getElementById('modal-overlay').classList.remove('active');
}

function esc(text) {
    const d = document.createElement('div');
    d.textContent = text;
    return d.innerHTML;
}

function logout() {
    callApi('/auth/logout', 'POST').catch(() => {});
    api.setStore('sessionId', null);
    api.setStore('user', null);
    window.location.href = 'login.html';
}

// ==================== IPC Events ====================

api.onNewChat(() => newConversation());
api.onShowAbout(() => alert('Ozayn - Personal AI Digital Twin'));

// ==================== Start ====================

init();
