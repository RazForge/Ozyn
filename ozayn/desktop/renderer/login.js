const api = window.electronAPI;
let sessionId = null;

// ==================== Tab Switching ====================

document.getElementById('tab-login').addEventListener('click', () => {
    document.getElementById('tab-login').classList.add('active');
    document.getElementById('tab-register').classList.remove('active');
    document.getElementById('login-form').classList.add('active');
    document.getElementById('register-form').classList.remove('active');
    document.getElementById('auth-error').textContent = '';
});

document.getElementById('tab-register').addEventListener('click', () => {
    document.getElementById('tab-register').classList.add('active');
    document.getElementById('tab-login').classList.remove('active');
    document.getElementById('register-form').classList.add('active');
    document.getElementById('login-form').classList.remove('active');
    document.getElementById('auth-error').textContent = '';
});

// ==================== Window Controls ====================

document.querySelectorAll('.titlebar-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        const action = btn.dataset.action;
        if (action === 'minimize') api.minimize();
        else if (action === 'maximize') api.maximize();
        else if (action === 'close') api.close();
    });
});

// ==================== Login ====================

document.getElementById('login-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const username = document.getElementById('login-username').value.trim();
    const password = document.getElementById('login-password').value;
    const btn = document.getElementById('login-btn');
    const error = document.getElementById('auth-error');

    if (!username || !password) {
        error.textContent = 'Please fill in all fields';
        return;
    }

    btn.disabled = true;
    btn.textContent = 'Logging in...';
    error.textContent = '';

    try {
        const result = await api.api('/auth/login', 'POST', { username, password });
        if (result.ok && result.data.success) {
            sessionId = result.data.session_id;
            await api.setStore('sessionId', sessionId);
            await api.setStore('user', JSON.stringify(result.data.user));
            loadApp(result.data.user);
        } else {
            error.textContent = result.data.error || 'Login failed';
        }
    } catch (err) {
        error.textContent = err.error || 'Cannot connect to server';
    } finally {
        btn.disabled = false;
        btn.textContent = 'Login';
    }
});

// ==================== Register ====================

document.getElementById('register-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const username = document.getElementById('reg-username').value.trim();
    const password = document.getElementById('reg-password').value;
    const email = document.getElementById('reg-email').value.trim() || null;
    const fullName = document.getElementById('reg-fullname').value.trim() || null;
    const btn = document.getElementById('reg-btn');
    const error = document.getElementById('auth-error');

    if (!username || !password) {
        error.textContent = 'Please fill in username and password';
        return;
    }

    btn.disabled = true;
    btn.textContent = 'Registering...';
    error.textContent = '';

    try {
        const result = await api.api('/auth/register', 'POST', { username, password, email, full_name: fullName });
        if (result.ok && result.data.success) {
            // Auto-login after register
            const loginResult = await api.api('/auth/login', 'POST', { username, password });
            if (loginResult.ok && loginResult.data.success) {
                sessionId = loginResult.data.session_id;
                await api.setStore('sessionId', sessionId);
                await api.setStore('user', JSON.stringify(loginResult.data.user));
                loadApp(loginResult.data.user);
            }
        } else {
            error.textContent = result.data.error || 'Registration failed';
        }
    } catch (err) {
        error.textContent = err.error || 'Cannot connect to server';
    } finally {
        btn.disabled = false;
        btn.textContent = 'Register';
    }
});

// ==================== Load App ====================

function loadApp(user) {
    document.body.classList.add('switching');
    setTimeout(() => {
        window.location.href = 'app.html';
    }, 300);
}

// ==================== Check Existing Session ====================

async function checkExistingSession() {
    const statusEl = document.getElementById('server-status');
    statusEl.textContent = 'Connecting to server...';

    const serverOk = await api.getServerStatus();
    if (!serverOk.php) {
        statusEl.textContent = 'Starting server, please wait...';
        let retries = 0;
        while (retries < 15) {
            await new Promise(r => setTimeout(r, 1000));
            const check = await api.getServerStatus();
            if (check.php) break;
            retries++;
        }
    }

    statusEl.textContent = '';

    const savedSession = await api.getStore('sessionId');
    const savedUser = await api.getStore('user');

    if (savedSession && savedUser) {
        // Validate session
        try {
            const result = await api.api('/chat/list', 'GET', null, savedSession);
            if (result.ok) {
                sessionId = savedSession;
                loadApp(JSON.parse(savedUser));
                return;
            }
        } catch (e) {}
        // Invalid session, clear it
        await api.setStore('sessionId', null);
        await api.setStore('user', null);
    }
}

checkExistingSession();
