const { app, BrowserWindow, Menu, ipcMain, shell, Tray, nativeImage, desktopCapturer, Notification } = require('electron');
const path = require('path');
const Store = require('electron-store');

const store = new Store();
let mainWindow;
let tray;

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 1200,
        height: 800,
        minWidth: 800,
        minHeight: 600,
        title: 'Ozayn',
        icon: path.join(__dirname, 'icon.png'),
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            nodeIntegration: false,
            contextIsolation: true,
            webSecurity: true
        },
        backgroundColor: '#0a0a0f',
        titleBarStyle: 'hiddenInset',
        show: false
    });

    mainWindow.loadURL('http://localhost:9090/ozayn');

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

    const template = [
        {
            label: 'File',
            submenu: [
                { label: 'New Chat', accelerator: 'CmdOrCtrl+N', click: () => mainWindow.webContents.send('new-chat') },
                { label: 'Export Data', accelerator: 'CmdOrCtrl+E', click: () => mainWindow.webContents.send('export-data') },
                { type: 'separator' },
                { label: 'Settings', accelerator: 'CmdOrCtrl+,', click: () => mainWindow.webContents.send('open-settings') },
                { type: 'separator' },
                { role: 'quit' }
            ]
        },
        {
            label: 'Edit',
            submenu: [
                { role: 'undo' },
                { role: 'redo' },
                { type: 'separator' },
                { role: 'cut' },
                { role: 'copy' },
                { role: 'paste' },
                { role: 'selectall' }
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
            label: 'Ozayn',
            submenu: [
                { label: 'Dashboard', click: () => mainWindow.webContents.send('navigate', '/ozayn/dashboard') },
                { label: 'Gesture Control', click: () => mainWindow.webContents.send('navigate', '/ozayn/gesture') },
                { label: 'Screen Vision', click: () => mainWindow.webContents.send('navigate', '/ozayn/vision') },
                { label: 'Collaboration', click: () => mainWindow.webContents.send('navigate', '/ozayn/collaborate') },
                { type: 'separator' },
                { label: 'ML Server Status', click: () => mainWindow.webContents.send('command', 'ml-status') },
                { label: 'Health Check', click: () => mainWindow.webContents.send('command', 'health') }
            ]
        },
        {
            label: 'Help',
            submenu: [
                { label: 'Documentation', click: () => shell.openExternal('https://github.com/henokakriso/Ozyn') },
                { label: 'Report Issue', click: () => shell.openExternal('https://github.com/henokakriso/Ozyn/issues') },
                { type: 'separator' },
                { label: 'About Ozayn', click: () => mainWindow.webContents.send('show-about') }
            ]
        }
    ];

    const menu = Menu.buildFromTemplate(template);
    Menu.setApplicationMenu(menu);

    createTray();
}

function createTray() {
    const icon = nativeImage.createEmpty();
    tray = new Tray(icon);
    tray.setToolTip('Ozayn');
    tray.on('click', () => {
        if (mainWindow) {
            mainWindow.show();
        }
    });
}

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') {
        app.quit();
    }
});

app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
        createWindow();
    }
});

ipcMain.handle('get-store', (event, key) => {
    return store.get(key);
});

ipcMain.handle('set-store', (event, key, value) => {
    store.set(key, value);
});

ipcMain.handle('capture-screen', async () => {
    try {
        const sources = await desktopCapturer.getSources({
            types: ['screen'],
            thumbnailSize: { width: 1920, height: 1080 }
        });
        if (sources.length > 0) {
            return sources[0].thumbnail.toDataURL();
        }
        return null;
    } catch (e) {
        console.error('Screen capture failed:', e);
        return null;
    }
});

ipcMain.handle('get-screen-sources', async () => {
    try {
        const sources = await desktopCapturer.getSources({
            types: ['screen', 'window'],
            thumbnailSize: { width: 320, height: 180 }
        });
        return sources.map(s => ({
            id: s.id,
            name: s.name,
            thumbnail: s.thumbnail.toDataURL()
        }));
    } catch (e) {
        console.error('Get sources failed:', e);
        return [];
    }
});

ipcMain.handle('get-ml-status', async () => {
    return { status: 'checking', backend: 'electron' };
});

ipcMain.handle('get-notification-permission', () => {
    return Notification.isSupported() ? 'granted' : 'denied';
});

ipcMain.handle('get-version', () => {
    return app.getVersion();
});

ipcMain.on('send-notification', (event, { title, body }) => {
    if (Notification.isSupported()) {
        const notification = new Notification({
            title: title || 'Ozayn',
            body: body || '',
            silent: false
        });
        notification.show();
    }
});

ipcMain.on('window-minimize', () => {
    if (mainWindow) mainWindow.minimize();
});

ipcMain.on('window-maximize', () => {
    if (mainWindow) {
        if (mainWindow.isMaximized()) {
            mainWindow.unmaximize();
        } else {
            mainWindow.maximize();
        }
    }
});

ipcMain.on('window-close', () => {
    if (mainWindow) mainWindow.close();
});
