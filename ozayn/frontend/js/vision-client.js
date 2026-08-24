/**
 * Ozayn Advanced Vision Client
 * Real-time vision processing from browser camera
 */

class VisionClient {
    constructor() {
        this.video = null;
        this.canvas = null;
        this.ctx = null;
        this.stream = null;
        this.mlClient = null;
        this.isCapturing = false;
        this.captureInterval = null;
        this.lastAnalysis = null;
    }

    async init() {
        try {
            this.video = document.createElement('video');
            this.video.setAttribute('playsinline', '');
            this.video.style.display = 'none';
            document.body.appendChild(this.video);

            this.canvas = document.createElement('canvas');
            this.ctx = this.canvas.getContext('2d');

            this.mlClient = window.ozaynML;
            if (this.mlClient) {
                await this.mlClient.connect();
            }

            return true;
        } catch (e) {
            console.error('Vision init failed:', e);
            return false;
        }
    }

    async startCamera() {
        try {
            this.stream = await navigator.mediaDevices.getUserMedia({
                video: { width: 640, height: 480, facingMode: 'user' }
            });
            this.video.srcObject = this.stream;
            await this.video.play();
            return true;
        } catch (e) {
            console.error('Camera access denied:', e);
            return false;
        }
    }

    stopCamera() {
        if (this.stream) {
            this.stream.getTracks().forEach(t => t.stop());
            this.stream = null;
        }
        if (this.captureInterval) {
            clearInterval(this.captureInterval);
            this.captureInterval = null;
        }
        this.isCapturing = false;
    }

    captureFrame() {
        if (!this.video || !this.video.videoWidth) return null;
        this.canvas.width = this.video.videoWidth;
        this.canvas.height = this.video.videoHeight;
        this.ctx.drawImage(this.video, 0, 0);
        return this.canvas.toDataURL('image/jpeg', 0.8);
    }

    async detectFaces() {
        const frame = this.captureFrame();
        if (!frame) return { error: 'No frame available' };
        if (!this.mlClient || !this.mlClient.connected) {
            return { error: 'ML server not connected' };
        }
        return await this.mlClient.detectFaces(frame);
    }

    async detectGesture() {
        const frame = this.captureFrame();
        if (!frame) return { error: 'No frame available' };
        if (!this.mlClient || !this.mlClient.connected) {
            return { error: 'ML server not connected' };
        }
        return await this.mlClient.detectGesture(frame);
    }

    async analyzeScreen() {
        const frame = this.captureFrame();
        if (!frame) return { error: 'No frame available' };
        if (!this.mlClient || !this.mlClient.connected) {
            return { error: 'ML server not connected' };
        }
        return await this.mlClient.analyzeScreen(frame);
    }

    startAutoCapture(callback, intervalMs = 1000) {
        this.isCapturing = true;
        this.captureInterval = setInterval(async () => {
            if (!this.isCapturing) return;
            const result = await this.detectFaces();
            this.lastAnalysis = result;
            if (callback) callback(result);
        }, intervalMs);
    }

    stopAutoCapture() {
        this.isCapturing = false;
        if (this.captureInterval) {
            clearInterval(this.captureInterval);
            this.captureInterval = null;
        }
    }

    getCameraElement() {
        return this.video;
    }
}

window.visionClient = new VisionClient();
