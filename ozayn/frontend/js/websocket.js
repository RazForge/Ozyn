/**
 * Ozayn WebSocket Client
 * Real-time notifications and updates
 */

class OzaynWS {
    constructor() {
        this.ws = null;
        this.url = `ws://${window.location.hostname}:8081`;
        this.connected = false;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 10;
        this.listeners = {};
        this.messageQueue = [];
    }

    connect() {
        return new Promise((resolve, reject) => {
            try {
                this.ws = new WebSocket(this.url);

                this.ws.onopen = () => {
                    this.connected = true;
                    this.reconnectAttempts = 0;
                    console.log('WebSocket connected');
                    this.emit('connected');
                    this.flushQueue();
                    resolve();
                };

                this.ws.onmessage = (event) => {
                    try {
                        const data = JSON.parse(event.data);
                        this.emit(data.type || 'message', data);
                    } catch (e) {
                        console.error('WS parse error:', e);
                    }
                };

                this.ws.onclose = () => {
                    this.connected = false;
                    console.log('WebSocket disconnected');
                    this.emit('disconnected');
                    this.attemptReconnect();
                };

                this.ws.onerror = (error) => {
                    console.error('WebSocket error:', error);
                    reject(error);
                };
            } catch (e) {
                reject(e);
            }
        });
    }

    attemptReconnect() {
        if (this.reconnectAttempts < this.maxReconnectAttempts) {
            this.reconnectAttempts++;
            const delay = Math.min(1000 * Math.pow(2, this.reconnectAttempts), 30000);
            console.log(`Reconnecting in ${delay}ms (attempt ${this.reconnectAttempts})`);
            setTimeout(() => this.connect(), delay);
        }
    }

    send(data) {
        const message = typeof data === 'string' ? data : JSON.stringify(data);
        if (this.connected) {
            this.ws.send(message);
        } else {
            this.messageQueue.push(message);
        }
    }

    flushQueue() {
        while (this.messageQueue.length > 0) {
            const msg = this.messageQueue.shift();
            this.ws.send(msg);
        }
    }

    on(event, callback) {
        if (!this.listeners[event]) {
            this.listeners[event] = [];
        }
        this.listeners[event].push(callback);
        return () => this.off(event, callback);
    }

    off(event, callback) {
        if (this.listeners[event]) {
            this.listeners[event] = this.listeners[event].filter(cb => cb !== callback);
        }
    }

    emit(event, data) {
        if (this.listeners[event]) {
            this.listeners[event].forEach(cb => cb(data));
        }
    }

    subscribe(channel) {
        this.send({ type: 'subscribe', channel });
    }

    unsubscribe(channel) {
        this.send({ type: 'unsubscribe', channel });
    }

    disconnect() {
        if (this.ws) {
            this.ws.close();
        }
    }
}

class NotificationManager {
    constructor(ws) {
        this.ws = ws;
        this.notifications = [];
        this.maxNotifications = 100;
        this.setupListeners();
    }

    setupListeners() {
        this.ws.on('notification', (data) => this.addNotification(data));
        this.ws.on('alert', (data) => this.addNotification({ ...data, priority: 'high' }));
        this.ws.on('system', (data) => this.addNotification({ ...data, type: 'system' }));
    }

    addNotification(data) {
        const notification = {
            id: Date.now() + Math.random(),
            timestamp: new Date().toISOString(),
            read: false,
            ...data
        };

        this.notifications.unshift(notification);

        if (this.notifications.length > this.maxNotifications) {
            this.notifications.pop();
        }

        this.renderNotification(notification);
        this.updateBadge();
    }

    renderNotification(data) {
        if (Notification.permission === 'granted' && data.priority === 'high') {
            new Notification(data.title || 'Ozayn', {
                body: data.message || data.text,
                icon: '/ozayn/frontend/images/icon.png'
            });
        }

        this.showToast(data);
    }

    showToast(data) {
        const toast = document.createElement('div');
        toast.className = `ozayn-toast ${data.priority || 'info'}`;
        toast.innerHTML = `
            <div class="toast-title">${data.title || 'Notification'}</div>
            <div class="toast-message">${data.message || data.text || ''}</div>
        `;
        toast.style.cssText = `
            position: fixed; bottom: 20px; right: 20px;
            background: rgba(10,10,15,0.95); border: 1px solid rgba(168,85,247,0.3);
            border-radius: 8px; padding: 12px 16px; color: white; z-index: 10000;
            max-width: 350px; animation: slideIn 0.3s ease;
        `;

        document.body.appendChild(toast);
        setTimeout(() => toast.remove(), 5000);
    }

    getUnread() {
        return this.notifications.filter(n => !n.read);
    }

    markAllRead() {
        this.notifications.forEach(n => n.read = true);
        this.updateBadge();
    }

    updateBadge() {
        const badge = document.querySelector('.notification-badge');
        if (badge) {
            const count = this.getUnread().length;
            badge.textContent = count > 0 ? (count > 99 ? '99+' : count) : '';
            badge.style.display = count > 0 ? 'block' : 'none';
        }
    }

    getAll() {
        return this.notifications;
    }

    clear() {
        this.notifications = [];
        this.updateBadge();
    }
}

window.ozaynWS = new OzaynWS();
window.ozaynNotifications = new NotificationManager(window.ozaynWS);
