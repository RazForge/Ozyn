const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
    platform: process.platform,
    
    getStore: (key) => ipcRenderer.invoke('get-store', key),
    setStore: (key, value) => ipcRenderer.invoke('set-store', key, value),
    
    onNewChat: (callback) => ipcRenderer.on('new-chat', callback),
    onExportData: (callback) => ipcRenderer.on('export-data', callback),
    onOpenSettings: (callback) => ipcRenderer.on('open-settings', callback),
    onNavigate: (callback) => ipcRenderer.on('navigate', (event, path) => callback(path)),
    onCommand: (callback) => ipcRenderer.on('command', (event, cmd) => callback(cmd)),
    onShowAbout: (callback) => ipcRenderer.on('show-about', callback),
    
    captureScreen: () => ipcRenderer.invoke('capture-screen'),
    getScreenSources: () => ipcRenderer.invoke('get-screen-sources'),
    
    getMLStatus: () => ipcRenderer.invoke('get-ml-status'),
    getNotificationPermission: () => ipcRenderer.invoke('get-notification-permission'),
    
    sendNotification: (title, body) => ipcRenderer.send('send-notification', { title, body }),
    
    minimize: () => ipcRenderer.send('window-minimize'),
    maximize: () => ipcRenderer.send('window-maximize'),
    close: () => ipcRenderer.send('window-close'),
    
    getVersion: () => ipcRenderer.invoke('get-version')
});
