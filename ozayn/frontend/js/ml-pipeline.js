/**
 * Ozayn ML Pipeline
 * Orchestrates between local TensorFlow.js models and remote Python ML server
 */

class MLPipeline {
    constructor() {
        this.localModels = window.tfModelManager;
        this.remoteML = window.ozaynML;
        this.fallbackMode = 'local';
        this.processing = false;
        this.frameQueue = [];
        this.results = {};
        this.stats = {
            localInferences: 0,
            remoteInferences: 0,
            errors: 0,
            avgLatency: 0,
            totalLatency: 0,
            inferenceCount: 0
        };
        this.mode = 'auto';
        this.remoteAvailable = false;
        this.localAvailable = false;
    }

    async init() {
        try {
            this.localAvailable = await this.localModels.init();
            console.log(`Local ML: ${this.localAvailable ? 'available' : 'unavailable'}`);
        } catch (e) {
            console.warn('Local ML init failed:', e);
            this.localAvailable = false;
        }

        try {
            if (this.remoteML && !this.remoteML.connected) {
                await this.remoteML.connect();
                this.remoteAvailable = true;
                console.log('Remote ML: connected');
            }
        } catch (e) {
            console.warn('Remote ML init failed:', e);
            this.remoteAvailable = false;
        }

        return this.localAvailable || this.remoteAvailable;
    }

    setMode(mode) {
        if (['local', 'remote', 'auto'].includes(mode)) {
            this.mode = mode;
            console.log(`ML Pipeline mode: ${mode}`);
        }
    }

    _selectBackend(task) {
        if (this.mode === 'local') return 'local';
        if (this.mode === 'remote') return 'remote';

        if (!this.remoteAvailable && !this.localAvailable) return null;

        const remotePreferred = ['detect_faces', 'detect_objects', 'analyze_screen'];
        if (remotePreferred.includes(task) && this.remoteAvailable) {
            return 'remote';
        }

        if (this.localAvailable) return 'local';
        if (this.remoteAvailable) return 'remote';
        return null;
    }

    async processFrame(imageSource, task, options = {}) {
        if (this.processing && !options.allowConcurrent) {
            return { queued: true };
        }

        this.processing = true;
        const startTime = performance.now();

        try {
            const backend = this._selectBackend(task);
            if (!backend) {
                throw new Error('No ML backend available');
            }

            let result;
            if (backend === 'local') {
                result = await this._processLocal(imageSource, task, options);
                this.stats.localInferences++;
            } else {
                result = await this._processRemote(imageSource, task, options);
                this.stats.remoteInferences++;
            }

            const latency = performance.now() - startTime;
            this.stats.totalLatency += latency;
            this.stats.inferenceCount++;
            this.stats.avgLatency = this.stats.totalLatency / this.stats.inferenceCount;

            result._meta = {
                backend,
                latency: Math.round(latency),
                timestamp: Date.now()
            };

            this.results[task] = result;
            return result;
        } catch (e) {
            this.stats.errors++;
            console.error(`ML Pipeline error (${task}):`, e);

            if (this.mode === 'auto') {
                return this._handleFallback(imageSource, task, options, e);
            }
            throw e;
        } finally {
            this.processing = false;
        }
    }

    async _processLocal(imageSource, task, options) {
        const tensor = this._imageToTensor(imageSource);

        try {
            switch (task) {
                case 'detect_hands':
                    return await this._localHandDetection(tensor);
                case 'detect_pose':
                    return await this._localPoseDetection(tensor, options.model);
                case 'detect_objects':
                    return await this._localObjectDetection(tensor);
                case 'classify':
                    return await this._localClassification(tensor, options.topK);
                case 'gesture':
                    return await this._localGestureRecognition(tensor);
                default:
                    throw new Error(`Unknown task: ${task}`);
            }
        } finally {
            if (tensor && !tensor.isDisposed) {
                tensor.dispose();
            }
        }
    }

    async _processRemote(imageSource, task, options) {
        const imageData = this._imageToBase64(imageSource);

        const taskMap = {
            'detect_hands': 'detect_gesture',
            'detect_faces': 'detect_faces',
            'detect_objects': 'detect_objects',
            'analyze_screen': 'analyze_screen',
            'gesture': 'detect_gesture',
            'screen_understand': 'analyze_screen'
        };

        const remoteAction = taskMap[task] || task;
        const data = remoteAction === 'detect_gesture' ? { frame: imageData } : { image: imageData };

        const result = await this.remoteML.send(remoteAction, data);
        return result;
    }

    async _handleFallback(imageSource, task, options, originalError) {
        console.log(`Fallback triggered for ${task}`);

        if (this._selectBackend(task) === 'remote' && this.localAvailable) {
            try {
                return await this._processLocal(imageSource, task, options);
            } catch (e) {
                console.error('Local fallback also failed:', e);
            }
        }

        return {
            error: originalError.message,
            fallback: true,
            task
        };
    }

    _imageToTensor(imageSource) {
        if (imageSource instanceof tf.Tensor) return imageSource;

        let pixels;
        if (imageSource instanceof HTMLCanvasElement) {
            pixels = tf.browser.fromPixels(imageSource);
        } else if (imageSource instanceof HTMLVideoElement) {
            if (!imageSource.videoWidth) {
                throw new Error('Video not ready');
            }
            pixels = tf.browser.fromPixels(imageSource);
        } else if (imageSource instanceof HTMLImageElement) {
            pixels = tf.browser.fromPixels(imageSource);
        } else if (imageSource instanceof ImageData) {
            pixels = tf.browser.fromPixels(imageSource);
        } else {
            throw new Error('Unsupported image source');
        }

        return pixels;
    }

    _imageToBase64(imageSource) {
        const canvas = document.createElement('canvas');
        const ctx = canvas.getContext('2d');

        if (imageSource instanceof HTMLVideoElement) {
            canvas.width = imageSource.videoWidth || 640;
            canvas.height = imageSource.videoHeight || 480;
            ctx.drawImage(imageSource, 0, 0, canvas.width, canvas.height);
        } else if (imageSource instanceof HTMLCanvasElement) {
            canvas.width = imageSource.width;
            canvas.height = imageSource.height;
            ctx.drawImage(imageSource, 0, 0);
        } else if (imageSource instanceof HTMLImageElement) {
            canvas.width = imageSource.naturalWidth || imageSource.width;
            canvas.height = imageSource.naturalHeight || imageSource.height;
            ctx.drawImage(imageSource, 0, 0, canvas.width, canvas.height);
        } else {
            throw new Error('Unsupported image source for base64');
        }

        return canvas.toDataURL('image/jpeg', 0.8);
    }

    async _localHandDetection(tensor) {
        const resized = tf.image.resizeBilinear(tensor, [192, 192]);
        const batched = resized.expandDims(0).div(127.5).sub(1);

        try {
            if (!this.localModels.loadedModels.handpose) {
                await this.localModels.loadModel('handpose');
            }
            const output = await this.localModels.predict('handpose', batched);
            const landmarks = await output.result.array();
            output.result.dispose();

            return {
                hands: this.localModels._parseHandLandmarks(landmarks),
                count: landmarks.length || 0
            };
        } finally {
            resized.dispose();
            batched.dispose();
        }
    }

    async _localPoseDetection(tensor, modelName = 'movenet') {
        const modelKey = modelName === 'movenet_multi' ? 'movenet_multi' : 'movenet';
        const size = modelKey === 'movenet_multi' ? 256 : 192;
        const resized = tf.image.resizeBilinear(tensor, [size, size]);
        const batched = resized.expandDims(0).div(127.5).sub(1);

        try {
            if (!this.localModels.loadedModels[modelKey]) {
                await this.localModels.loadModel(modelKey);
            }
            const output = await this.localModels.predict(modelKey, batched);
            const keypoints = await output.result.array();
            output.result.dispose();

            return {
                poses: this.localModels._parseMoveNet(keypoints[0]),
                model: modelKey
            };
        } finally {
            resized.dispose();
            batched.dispose();
        }
    }

    async _localObjectDetection(tensor) {
        if (!this.localModels.loadedModels.coco_ssd) {
            await this.localModels.loadModel('coco_ssd');
        }

        const canvas = document.createElement('canvas');
        canvas.width = tensor.shape[2];
        canvas.height = tensor.shape[1];
        await tf.browser.toPixels(tensor, canvas);
        const output = await this.localModels.predict('coco_ssd', canvas);
        const result = await this.localModels._parseCOCOSSD(output.result);
        output.result.dispose();
        canvas.remove();

        return { objects: result };
    }

    async _localClassification(tensor, topK = 5) {
        const resized = tf.image.resizeBilinear(tensor, [224, 224]);
        const batched = resized.expandDims(0).div(127.5).sub(1);

        try {
            if (!this.localModels.loadedModels.mobilenet) {
                await this.localModels.loadModel('mobilenet');
            }
            const output = await this.localModels.predict('mobilenet', batched);
            const softmax = tf.softmax(output.result);
            const values = await softmax.data();
            output.result.dispose();
            softmax.dispose();

            const indexed = Array.from(values)
                .map((v, i) => ({ probability: v, classIndex: i }))
                .sort((a, b) => b.probability - a.probability)
                .slice(0, topK);

            return { classifications: indexed };
        } finally {
            resized.dispose();
            batched.dispose();
        }
    }

    async _localGestureRecognition(tensor) {
        const handResult = await this._localHandDetection(tensor);

        if (!handResult.hands || handResult.hands.length === 0) {
            return { gesture: 'none', hands: 0 };
        }

        const hand = handResult.hands[0];
        const gesture = this._classifyGesture(hand);

        return {
            gesture: gesture.name,
            confidence: gesture.confidence,
            fingersUp: hand.fingerCount,
            landmarks: hand.landmarks.map(kp => ({ x: kp.x, y: kp.y }))
        };
    }

    _classifyGesture(hand) {
        const f = hand.fingersUp;
        const total = hand.fingerCount;

        const gestures = [
            { name: 'open_hand', check: () => total === 5, confidence: 0.95 },
            { name: 'fist', check: () => total === 0, confidence: 0.9 },
            { name: 'peace', check: () => f[1] && f[2] && !f[3] && !f[4], confidence: 0.85 },
            { name: 'thumbs_up', check: () => f[0] && total === 1, confidence: 0.8 },
            { name: 'pointing', check: () => f[1] && !f[2] && !f[3] && !f[4], confidence: 0.8 },
            { name: 'three_fingers', check: () => total === 3, confidence: 0.75 },
            { name: 'two_fingers', check: () => total === 2, confidence: 0.7 },
            { name: 'phone', check: () => f[0] && f[1] && !f[2] && !f[3] && f[4], confidence: 0.7 },
            { name: 'rock', check: () => f[0] && f[1] && !f[2] && !f[3] && !f[4], confidence: 0.7 },
            { name: 'ok', check: () => f[0] && f[1] && !f[2] && !f[3] && !f[4], confidence: 0.65 }
        ];

        for (const g of gestures) {
            if (g.check()) {
                return { name: g.name, confidence: g.confidence };
            }
        }

        return { name: `fingers_${total}`, confidence: 0.5 };
    }

    getStats() {
        return {
            ...this.stats,
            mode: this.mode,
            localAvailable: this.localAvailable,
            remoteAvailable: this.remoteAvailable
        };
    }

    async checkHealth() {
        const health = {
            local: { available: false, models: [] },
            remote: { available: false, capabilities: {} }
        };

        if (this.localAvailable) {
            health.local.available = true;
            health.local.models = Object.keys(this.localModels.loadedModels);
        }

        if (this.remoteAvailable) {
            try {
                const ping = await this.remoteML.ping();
                health.remote.available = true;
                health.remote.capabilities = ping.capabilities || {};
            } catch (e) {
                health.remote.available = false;
            }
        }

        return health;
    }
}

window.mlPipeline = new MLPipeline();
