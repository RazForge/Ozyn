/**
 * Ozayn ML Client
 * Connects to Python ML backend for vision/gesture
 */

class MLClient {
    constructor(url = 'ws://localhost:8765') {
        this.url = url;
        this.ws = null;
        this.connected = false;
        this.callbacks = {};
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
    }

    connect() {
        return new Promise((resolve, reject) => {
            try {
                this.ws = new WebSocket(this.url);
                
                this.ws.onopen = () => {
                    this.connected = true;
                    this.reconnectAttempts = 0;
                    console.log('ML Server connected');
                    resolve();
                };

                this.ws.onmessage = (event) => {
                    try {
                        const data = JSON.parse(event.data);
                        const id = data.id || 'default';
                        if (this.callbacks[id]) {
                            this.callbacks[id](data);
                            delete this.callbacks[id];
                        }
                    } catch (e) {
                        console.error('ML response parse error:', e);
                    }
                };

                this.ws.onclose = () => {
                    this.connected = false;
                    console.log('ML Server disconnected');
                    this.attemptReconnect();
                };

                this.ws.onerror = (error) => {
                    console.error('ML Server error:', error);
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
            setTimeout(() => this.connect(), 2000 * this.reconnectAttempts);
        }
    }

    send(action, data = {}) {
        return new Promise((resolve, reject) => {
            if (!this.connected) {
                reject(new Error('Not connected to ML server'));
                return;
            }

            const id = Math.random().toString(36).substr(2, 9);
            this.callbacks[id] = resolve;

            this.ws.send(JSON.stringify({ id, action, ...data }));

            setTimeout(() => {
                if (this.callbacks[id]) {
                    delete this.callbacks[id];
                    reject(new Error('ML request timeout'));
                }
            }, 10000);
        });
    }

    async detectFaces(imageData) {
        return this.send('detect_faces', { image: imageData });
    }

    async detectObjects(imageData) {
        return this.send('detect_objects', { image: imageData });
    }

    async analyzeScreen(imageData) {
        return this.send('analyze_screen', { image: imageData });
    }

    async detectGesture(frameData) {
        return this.send('detect_gesture', { frame: frameData });
    }

    async ping() {
        return this.send('ping');
    }

    async getCapabilities() {
        const result = await this.ping();
        return result.capabilities || {};
    }

    disconnect() {
        if (this.ws) {
            this.ws.close();
        }
    }
}

// Global instance
window.ozaynML = new MLClient();
