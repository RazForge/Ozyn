const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
    platform: process.platform,

    // API proxy — goes through main process, no CORS
    api: (endpoint, method, data, sessionId) =>
        ipcRenderer.invoke('api-call', { endpoint, method, data, sessionId }),

    // Server
    getServerStatus: () => ipcRenderer.invoke('get-server-status'),

    // Storage
    getStore: (key) => ipcRenderer.invoke('get-store', key),
    setStore: (key, value) => ipcRenderer.invoke('set-store', key, value),

    // Window controls
    minimize: () => ipcRenderer.send('window-minimize'),
    maximize: () => ipcRenderer.send('window-maximize'),
    close: () => ipcRenderer.send('window-close'),

    // Events
    onNewChat: (cb) => ipcRenderer.on('new-chat', cb),
    onNavigate: (cb) => ipcRenderer.on('navigate', (e, path) => cb(path)),
    onCommand: (cb) => ipcRenderer.on('command', (e, cmd) => cb(cmd)),
    onShowAbout: (cb) => ipcRenderer.on('show-about', cb),

    // Screen
    captureScreen: () => ipcRenderer.invoke('capture-screen'),

    // Notifications
    sendNotification: (title, body) => ipcRenderer.send('send-notification', { title, body }),

    // Version
    getVersion: () => ipcRenderer.invoke('get-version')
});
