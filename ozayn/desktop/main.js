const { app, BrowserWindow, Menu, ipcMain, shell, Tray, nativeImage, desktopCapturer, Notification } = require('electron');
const path = require('path');
const http = require('http');
const { spawn } = require('child_process');
const Store = require('electron-store');

const store = new Store();
let mainWindow;
let tray;
let phpProcess;
let collabProcess;
let mlProcess;

const ROOT_DIR = path.join(__dirname, '..', '..');
const API_BASE = 'http://127.0.0.1:8000';

// ==================== Server Management ====================

function checkServer(url) {
    return new Promise((resolve) => {
        const req = http.get(url, (res) => {
            res.resume();
            resolve(true);
        });
        req.on('error', () => resolve(false));
        req.setTimeout(1000, () => { req.destroy(); resolve(false); });
    });
}

function startPHPServer() {
    return new Promise((resolve) => {
        checkServer(`${API_BASE}/ozayn/`).then((running) => {
            if (running) {
                console.log('PHP server already running');
                resolve(true);
                return;
            }
            console.log('Starting PHP server...');
            phpProcess = spawn('php', ['-S', '127.0.0.1:8000', 'router.php'], {
                cwd: ROOT_DIR,
                stdio: 'ignore',
                detached: true
            });
            phpProcess.unref();
            let attempts = 0;
            const check = setInterval(() => {
                attempts++;
                checkServer(`${API_BASE}/ozayn/`).then((ok) => {
                    if (ok || attempts > 10) {
                        clearInterval(check);
                        resolve(ok);
                    }
                });
            }, 500);
        });
    });
}

function startMLServer() {
    const mlDir = path.join(ROOT_DIR, 'ozayn', 'ml');
    const venvPython = path.join(mlDir, 'venv', 'bin', 'python3');
    const pythonCmd = require('fs').existsSync(venvPython) ? venvPython : 'python3';

    checkServer('ws://127.0.0.1:8765').then((running) => {
        if (!running) {
            console.log('Starting ML server...');
            mlProcess = spawn(pythonCmd, ['server.py'], {
                cwd: mlDir,
                stdio: 'ignore',
                detached: true,
                env: { ...process.env, ML_HOST: '127.0.0.1', ML_PORT: '8765' }
            });
            mlProcess.unref();
        }
    });
}

// ==================== Window ====================

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 1200,
        height: 800,
        minWidth: 800,
        minHeight: 600,
        title: 'Ozayn',
        frame: false,
        backgroundColor: '#08081a',
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            nodeIntegration: false,
            contextIsolation: true,
            webSecurity: false
        },
        show: false
    });

    mainWindow.loadFile(path.join(__dirname, 'renderer', 'login.html'));

    mainWindow.once('ready-to-show', () => {
        mainWindow.show();
    });

    mainWindow.on('closed', () => {
        mainWindow = null;
    });

    mainWindow.webContents.setWindowOpenHandler(({ url }) => {
        shell.openExternal(url);
        return { action: 'deny' };
    });

    createMenu();
    createTray();
}

function createMenu() {
    const template = [
        {
            label: 'File',
            submenu: [
                { label: 'New Chat', accelerator: 'CmdOrCtrl+N', click: () => mainWindow?.webContents.send('new-chat') },
                { label: 'Export Data', accelerator: 'CmdOrCtrl+E', click: () => mainWindow?.webContents.send('export-data') },
                { type: 'separator' },
                { label: 'Settings', accelerator: 'CmdOrCtrl+,', click: () => mainWindow?.webContents.send('open-settings') },
                { type: 'separator' },
                { role: 'quit' }
            ]
        },
        {
            label: 'View',
            submenu: [
                { role: 'reload' },
                { role: 'toggledevtools' },
                { type: 'separator' },
                { role: 'resetzoom' },
                { role: 'zoomin' },
                { role: 'zoomout' },
                { type: 'separator' },
                { role: 'togglefullscreen' }
            ]
        },
        {
            label: 'Help',
            submenu: [
                { label: 'Documentation', click: () => shell.openExternal('https://github.com/henokakriso/Ozyn') },
                { label: 'Report Issue', click: () => shell.openExternal('https://github.com/henokakriso/Ozyn/issues') },
                { type: 'separator' },
                { label: 'About Ozayn', click: () => mainWindow?.webContents.send('show-about') }
            ]
        }
    ];
    Menu.setApplicationMenu(Menu.buildFromTemplate(template));
}

function createTray() {
    const icon = nativeImage.createEmpty();
    tray = new Tray(icon);
    tray.setToolTip('Ozayn');
    tray.on('click', () => mainWindow?.show());
}

// ==================== IPC Handlers ====================

// Window controls
ipcMain.on('window-minimize', () => mainWindow?.minimize());
ipcMain.on('window-maximize', () => {
    if (mainWindow?.isMaximized()) mainWindow.unmaximize();
    else mainWindow?.maximize();
});
ipcMain.on('window-close', () => mainWindow?.close());

// API proxy — main process makes HTTP calls, no CORS issues
ipcMain.handle('api-call', async (event, { endpoint, method, data, sessionId }) => {
    return new Promise((resolve, reject) => {
        const url = new URL(`${API_BASE}/ozayn/backend/api${endpoint}`);
        const options = {
            hostname: url.hostname,
            port: url.port,
            path: url.pathname + url.search,
            method: method || 'GET',
            headers: {
                'Content-Type': 'application/json'
            }
        };
        if (sessionId) {
            options.headers['X-Session-ID'] = sessionId;
        }

        const req = http.request(options, (res) => {
            let body = '';
            res.on('data', (chunk) => body += chunk);
            res.on('end', () => {
                try {
                    const json = JSON.parse(body);
                    resolve({ ok: res.statusCode >= 200 && res.statusCode < 300, status: res.statusCode, data: json });
                } catch (e) {
                    resolve({ ok: false, status: res.statusCode, data: { error: 'Invalid response' } });
                }
            });
        });

        req.on('error', (err) => {
            reject({ ok: false, error: err.message });
        });

        req.setTimeout(15000, () => {
            req.destroy();
            reject({ ok: false, error: 'Request timeout' });
        });

        if (data) {
            req.write(JSON.stringify(data));
        }
        req.end();
    });
});

// Server status
ipcMain.handle('get-server-status', async () => {
    const phpRunning = await checkServer(`${API_BASE}/ozayn/`);
    return { php: phpRunning, port: 8000 };
});

// Local storage
ipcMain.handle('get-store', (event, key) => store.get(key));
ipcMain.handle('set-store', (event, key, value) => store.set(key, value));

// Screen capture
ipcMain.handle('capture-screen', async () => {
    try {
        const sources = await desktopCapturer.getSources({ types: ['screen'], thumbnailSize: { width: 1920, height: 1080 } });
        return sources.length > 0 ? sources[0].thumbnail.toDataURL() : null;
    } catch (e) {
        return null;
    }
});

// Version
ipcMain.handle('get-version', () => app.getVersion());

// Notifications
ipcMain.on('send-notification', (event, { title, body }) => {
    if (Notification.isSupported()) {
        new Notification({ title: title || 'Ozayn', body: body || '', silent: false }).show();
    }
});

// ==================== App Lifecycle ====================

app.whenReady().then(async () => {
    await startPHPServer();
    startMLServer();
    createWindow();
});

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') {
        if (phpProcess) try { phpProcess.kill(); } catch(e) {}
        if (mlProcess) try { mlProcess.kill(); } catch(e) {}
        app.quit();
    }
});

app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
});
