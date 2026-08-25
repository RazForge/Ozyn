/**
 * Ozayn Collaboration Client
 * Real-time multi-user collaboration with cursor sync, shared editing
 */

class CollaborationClient {
    constructor() {
        this.ws = null;
        this.url = `ws://${window.location.hostname}:8082`;
        this.connected = false;
        this.sessionId = null;
        this.userId = null;
        this.username = null;
        this.role = 'viewer';
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 10;
        this.listeners = {};
        this.messageQueue = [];
        this.users = new Map();
        this.cursors = new Map();
        this.sharedDocuments = [];
        this.isSyncing = false;
        this.lastCursorUpdate = 0;
        this.cursorThrottle = 50;
        this.pendingEvents = [];
    }

    async connect() {
        return new Promise((resolve, reject) => {
            try {
                this.ws = new WebSocket(this.url);

                this.ws.onopen = () => {
                    this.connected = true;
                    this.reconnectAttempts = 0;
                    console.log('Collaboration WebSocket connected');
                    this.emit('connected');
                    this.flushQueue();
                    resolve();
                };

                this.ws.onmessage = (event) => {
                    try {
                        const data = JSON.parse(event.data);
                        this._handleMessage(data);
                    } catch (e) {
                        console.error('Collaboration WS parse error:', e);
                    }
                };

                this.ws.onclose = () => {
                    this.connected = false;
                    console.log('Collaboration WebSocket disconnected');
                    this.emit('disconnected');
                    this.attemptReconnect();
                };

                this.ws.onerror = (error) => {
                    console.error('Collaboration WebSocket error:', error);
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
            console.log(`Collaboration reconnecting in ${delay}ms (attempt ${this.reconnectAttempts})`);
            setTimeout(() => this.connect(), delay);
        }
    }

    _handleMessage(data) {
        switch (data.type) {
            case 'session_joined':
                this.sessionId = data.session_id;
                this.users = new Map(data.users.map(u => [u.id, u]));
                this.emit('session_joined', data);
                break;

            case 'user_joined':
                this.users.set(data.user.id, data.user);
                this.emit('user_joined', data.user);
                break;

            case 'user_left':
                this.users.delete(data.user_id);
                this.cursors.delete(data.user_id);
                this.emit('user_left', { user_id: data.user_id });
                break;

            case 'cursor_move':
                this.cursors.set(data.user_id, {
                    x: data.x,
                    y: data.y,
                    username: data.username,
                    timestamp: Date.now()
                });
                this.emit('cursor_move', data);
                break;

            case 'document_update':
                this.emit('document_update', data);
                break;

            case 'document_created':
                this.sharedDocuments.push(data.document);
                this.emit('document_created', data.document);
                break;

            case 'chat_message':
                this.emit('chat_message', data);
                break;

            case 'event':
                this.emit('collaboration_event', data);
                break;

            case 'error':
                console.error('Collaboration error:', data.message);
                this.emit('error', data);
                break;

            default:
                this.emit(data.type, data);
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

    async joinSession(sessionId, userId, username, role = 'viewer') {
        this.userId = userId;
        this.username = username;
        this.role = role;

        this.send({
            type: 'join_session',
            session_id: sessionId,
            user_id: userId,
            username: username,
            role: role
        });
    }

    leaveSession() {
        if (this.sessionId) {
            this.send({
                type: 'leave_session',
                session_id: this.sessionId,
                user_id: this.userId
            });
        }
        this.sessionId = null;
        this.users.clear();
        this.cursors.clear();
    }

    updateCursor(x, y) {
        const now = Date.now();
        if (now - this.lastCursorUpdate < this.cursorThrottle) return;
        this.lastCursorUpdate = now;

        this.send({
            type: 'cursor_update',
            session_id: this.sessionId,
            user_id: this.userId,
            username: this.username,
            x,
            y
        });
    }

    updateDocument(docId, content, position = null) {
        this.send({
            type: 'document_edit',
            session_id: this.sessionId,
            user_id: this.userId,
            document_id: docId,
            content,
            position,
            timestamp: Date.now()
        });
    }

    createDocument(title, content = '') {
        this.send({
            type: 'create_document',
            session_id: this.sessionId,
            user_id: this.userId,
            title,
            content
        });
    }

    sendChat(message) {
        this.send({
            type: 'chat',
            session_id: this.sessionId,
            user_id: this.userId,
            username: this.username,
            message,
            timestamp: Date.now()
        });
    }

    broadcastEvent(eventType, data) {
        this.send({
            type: 'broadcast',
            session_id: this.sessionId,
            user_id: this.userId,
            event_type: eventType,
            data
        });
    }

    getUsers() {
        return Array.from(this.users.values());
    }

    getCursors() {
        return Object.fromEntries(this.cursors);
    }

    getSessionId() {
        return this.sessionId;
    }

    isConnected() {
        return this.connected;
    }

    disconnect() {
        this.leaveSession();
        if (this.ws) {
            this.ws.close();
        }
    }
}

window.collaborationClient = new CollaborationClient();
