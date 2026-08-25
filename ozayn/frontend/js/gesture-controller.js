/**
 * Ozayn Gesture Controller
 * Real-time webcam gesture detection and action mapping
 */

class GestureController {
    constructor() {
        this.video = null;
        this.canvas = null;
        this.ctx = null;
        this.stream = null;
        this.isRunning = false;
        this.detectInterval = null;
        this.pipeline = window.mlPipeline;
        this.gestureHistory = [];
        this.maxHistory = 10;
        this.lastGesture = null;
        this.lastGestureTime = 0;
        this.gestureThreshold = 500;
        this.confidenceThreshold = 0.6;
        this.onGesture = null;
        this.onHandDetected = null;
        this.onHandLost = null;
        this.handPresent = false;
        this.detectionFPS = 15;
        this.showOverlay = true;
        this.overlayElement = null;
        this.videoContainer = null;
        this.stats = {
            framesProcessed: 0,
            handsDetected: 0,
            gesturesRecognized: 0
        };

        this.gestureActions = {
            open_hand: { action: 'pause', label: 'Pause', icon: '\u23F8' },
            fist: { action: 'cancel', label: 'Cancel', icon: '\u2716' },
            peace: { action: 'scroll_down', label: 'Scroll Down', icon: '\u2193' },
            thumbs_up: { action: 'confirm', label: 'Confirm', icon: '\u2714' },
            pointing: { action: 'select', label: 'Select', icon: '\u261D' },
            swipe_left: { action: 'previous', label: 'Previous', icon: '\u2190' },
            swipe_right: { action: 'next', label: 'Next', icon: '\u2192' },
            swipe_up: { action: 'scroll_up', label: 'Scroll Up', icon: '\u2191' },
            swipe_down: { action: 'scroll_down', label: 'Scroll Down', icon: '\u2193' },
            two_fingers: { action: 'zoom_in', label: 'Zoom In', icon: '\u{1F50D}' },
            three_fingers: { action: 'zoom_out', label: 'Zoom Out', icon: '\u{1F50E}' }
        };
    }

    async init(containerId = 'gesture-panel') {
        this.videoContainer = document.getElementById(containerId);
        if (!this.videoContainer) {
            this.videoContainer = this._createContainer(containerId);
        }

        this._createVideoElement();
        this._createCanvas();
        this._createOverlay();

        return true;
    }

    _createContainer(id) {
        const container = document.createElement('div');
        container.id = id;
        container.className = 'gesture-container';
        container.style.cssText = `
            position: fixed; bottom: 20px; left: 20px;
            width: 320px; z-index: 9999;
            background: rgba(10, 10, 15, 0.95);
            border: 1px solid rgba(168, 85, 247, 0.3);
            border-radius: 12px; overflow: hidden;
            font-family: system-ui; color: white;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.5);
        `;
        document.body.appendChild(container);
        return container;
    }

    _createVideoElement() {
        this.video = document.createElement('video');
        this.video.setAttribute('playsinline', '');
        this.video.setAttribute('autoplay', '');
        this.video.muted = true;
        this.video.style.cssText = `
            width: 100%; height: auto; display: block;
            transform: scaleX(-1);
        `;
        this.videoContainer.appendChild(this.video);
    }

    _createCanvas() {
        this.canvas = document.createElement('canvas');
        this.canvas.style.cssText = `
            position: absolute; top: 0; left: 0;
            width: 100%; height: 100%;
            pointer-events: none;
        `;
        this.videoContainer.appendChild(this.canvas);
        this.ctx = this.canvas.getContext('2d');
    }

    _createOverlay() {
        this.overlayElement = document.createElement('div');
        this.overlayElement.className = 'gesture-overlay';
        this.overlayElement.style.cssText = `
            position: absolute; bottom: 0; left: 0; right: 0;
            padding: 8px 12px; background: rgba(0, 0, 0, 0.7);
            display: flex; align-items: center; justify-content: space-between;
            font-size: 13px;
        `;
        this.overlayElement.innerHTML = `
            <span class="gesture-status">Gesture: <strong id="gesture-name">None</strong></span>
            <span class="gesture-fps" id="gesture-fps">0 FPS</span>
        `;
        this.videoContainer.appendChild(this.overlayElement);
    }

    async startCamera() {
        try {
            this.stream = await navigator.mediaDevices.getUserMedia({
                video: {
                    width: { ideal: 640 },
                    height: { ideal: 480 },
                    facingMode: 'user'
                }
            });
            this.video.srcObject = this.stream;
            await this.video.play();

            this.canvas.width = this.video.videoWidth || 640;
            this.canvas.height = this.video.videoHeight || 480;

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
        this.video.srcObject = null;
    }

    start(fps = 15) {
        if (this.isRunning) return;

        this.isRunning = true;
        this.detectionFPS = fps;
        const interval = 1000 / fps;

        let lastFrameTime = 0;
        const detect = async (timestamp) => {
            if (!this.isRunning) return;

            if (timestamp - lastFrameTime >= interval) {
                lastFrameTime = timestamp;
                await this._detectFrame();
            }

            requestAnimationFrame(detect);
        };

        requestAnimationFrame(detect);
    }

    stop() {
        this.isRunning = false;
        this.stopCamera();
        this._clearOverlay();
    }

    async _detectFrame() {
        if (!this.video || !this.video.videoWidth || !this.pipeline) return;

        this.stats.framesProcessed++;

        try {
            const result = await this.pipeline.processFrame(this.video, 'gesture', {
                allowConcurrent: true
            });

            if (result.error) return;

            const hasHand = result.count > 0 || (result.hands && result.hands.length > 0);

            if (hasHand && !this.handPresent) {
                this.handPresent = true;
                if (this.onHandDetected) this.onHandDetected(result);
            } else if (!hasHand && this.handPresent) {
                this.handPresent = false;
                if (this.onHandLost) this.onHandLost();
                this._updateGestureDisplay('None', '');
            }

            if (hasHand) {
                this.stats.handsDetected++;
                const gesture = result.gesture || this._extractGesture(result);

                if (gesture && gesture !== 'none') {
                    this._processGesture(gesture, result);
                    this._drawHandOverlay(result);
                }
            }
        } catch (e) {
            console.error('Gesture detection error:', e);
        }
    }

    _extractGesture(result) {
        if (result.gesture) return result.gesture;
        if (result.hands && result.hands.length > 0) {
            return this._classifyFromLandmarks(result.hands[0]);
        }
        return 'none';
    }

    _classifyFromLandmarks(hand) {
        if (!hand.landmarks) return 'none';

        const f = hand.fingersUp || [];
        const total = hand.fingerCount || f.filter(Boolean).length;

        if (total === 5) return 'open_hand';
        if (total === 0) return 'fist';
        if (f[1] && f[2] && !f[3]) return 'peace';
        if (f[0] && total === 1) return 'thumbs_up';
        if (f[1] && !f[2] && !f[3] && !f[4]) return 'pointing';
        if (total === 2) return 'two_fingers';
        if (total === 3) return 'three_fingers';

        return `fingers_${total}`;
    }

    _processGesture(gesture, result) {
        const now = Date.now();

        if (gesture === this.lastGesture && (now - this.lastGestureTime) < this.gestureThreshold) {
            return;
        }

        if (gesture !== this.lastGesture) {
            this.gestureHistory.push({ gesture, time: now });
            if (this.gestureHistory.length > this.maxHistory) {
                this.gestureHistory.shift();
            }
        }

        this.lastGesture = gesture;
        this.lastGestureTime = now;
        this.stats.gesturesRecognized++;

        const action = this.gestureActions[gesture];
        if (action) {
            this._updateGestureDisplay(action.label, action.icon);
            this._dispatchGestureEvent(gesture, action, result);

            if (this.onGesture) {
                this.onGesture({
                    gesture,
                    action: action.action,
                    label: action.label,
                    confidence: result.confidence || 0.8,
                    timestamp: now
                });
            }
        }
    }

    _dispatchGestureEvent(gesture, action, result) {
        const event = new CustomEvent('ozayn-gesture', {
            detail: {
                gesture,
                action: action.action,
                label: action.label,
                icon: action.icon,
                confidence: result.confidence || 0.8,
                fingers: result.fingersUp || 0,
                timestamp: Date.now()
            }
        });
        window.dispatchEvent(event);
    }

    _updateGestureDisplay(name, icon) {
        const nameEl = document.getElementById('gesture-name');
        const fpsEl = document.getElementById('gesture-fps');
        if (nameEl) nameEl.textContent = `${icon} ${name}`;
        if (fpsEl) fpsEl.textContent = `${this.detectionFPS} FPS`;
    }

    _drawHandOverlay(result) {
        if (!this.ctx || !this.showOverlay) return;

        this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

        if (result.landmarks) {
            this._drawLandmarks(result.landmarks);
        } else if (result.hands) {
            result.hands.forEach(hand => {
                if (hand.landmarks) {
                    this._drawHandLandmarks(hand);
                }
            });
        }
    }

    _drawHandLandmarks(hand) {
        const ctx = this.ctx;
        const w = this.canvas.width;
        const h = this.canvas.height;

        ctx.fillStyle = '#a855f7';
        ctx.strokeStyle = 'rgba(168, 85, 247, 0.6)';
        ctx.lineWidth = 2;

        const points = hand.landmarks.map(kp => ({
            x: (1 - kp.x) * w,
            y: kp.y * h
        }));

        if (hand.connections) {
            hand.connections.forEach(([a, b]) => {
                if (points[a] && points[b]) {
                    ctx.beginPath();
                    ctx.moveTo(points[a].x, points[a].y);
                    ctx.lineTo(points[b].x, points[b].y);
                    ctx.stroke();
                }
            });
        }

        points.forEach(p => {
            ctx.beginPath();
            ctx.arc(p.x, p.y, 4, 0, 2 * Math.PI);
            ctx.fill();
        });
    }

    _drawLandmarks(landmarks) {
        const ctx = this.ctx;
        const w = this.canvas.width;
        const h = this.canvas.height;

        ctx.fillStyle = '#00ff88';
        ctx.strokeStyle = 'rgba(0, 255, 136, 0.5)';
        ctx.lineWidth = 2;

        landmarks.forEach(p => {
            const x = (typeof p.x === 'number') ? (1 - p.x) * w : 0;
            const y = (typeof p.y === 'number') ? p.y * h : 0;
            ctx.beginPath();
            ctx.arc(x, y, 3, 0, 2 * Math.PI);
            ctx.fill();
        });
    }

    _clearOverlay() {
        if (this.ctx) {
            this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
        }
    }

    setGestureAction(gesture, action) {
        this.gestureActions[gesture] = action;
    }

    setConfidenceThreshold(threshold) {
        this.confidenceThreshold = Math.max(0, Math.min(1, threshold));
    }

    setDetectionFPS(fps) {
        this.detectionFPS = Math.max(1, Math.min(30, fps));
    }

    toggleOverlay() {
        this.showOverlay = !this.showOverlay;
        this.canvas.style.display = this.showOverlay ? 'block' : 'none';
    }

    getGestureHistory() {
        return [...this.gestureHistory];
    }

    getStats() {
        return { ...this.stats };
    }

    show() {
        if (this.videoContainer) {
            this.videoContainer.style.display = 'block';
        }
    }

    hide() {
        if (this.videoContainer) {
            this.videoContainer.style.display = 'none';
        }
    }

    toggle() {
        if (this.videoContainer) {
            const visible = this.videoContainer.style.display !== 'none';
            if (visible) this.hide();
            else this.show();
        }
    }
}

window.gestureController = new GestureController();
