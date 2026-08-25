/**
 * Ozayn TensorFlow.js Model Manager
 * Handles loading, inference, and management of client-side ML models
 */

class TFModelManager {
    constructor() {
        this.models = {};
        this.loadedModels = {};
        this.isReady = false;
        this.tfReady = false;
        this.loadingQueue = [];
        this.modelCache = {};
    }

    async init() {
        try {
            if (typeof tf === 'undefined') {
                console.warn('TensorFlow.js not loaded');
                return false;
            }
            await tf.ready();
            this.tfReady = true;
            console.log(`TF.js backend: ${tf.getBackend()}`);
            this.isReady = true;
            return true;
        } catch (e) {
            console.error('TF.js init failed:', e);
            return false;
        }
    }

    async loadModel(name, config = {}) {
        if (this.loadedModels[name]) {
            return this.loadedModels[name];
        }

        const modelConfigs = {
            handpose: {
                url: 'https://tfhub.dev/mediapipe/tfjs-model/handpose/1',
                type: 'graph_model',
                inputSize: [1, 192, 192, 3],
                description: 'Hand landmark detection (21 points per hand)'
            },
            movenet: {
                url: 'https://tfhub.dev/google/tfjs-model/movenet/singlepose/lightning/4',
                type: 'graph_model',
                inputSize: [1, 192, 192, 3],
                description: 'Full body pose estimation (17 keypoints)'
            },
            movenet_multi: {
                url: 'https://tfhub.dev/google/tfjs-model/movenet/multipose/lightning/1',
                type: 'graph_model',
                inputSize: [1, 256, 256, 3],
                description: 'Multi-person pose estimation'
            },
            coco_ssd: {
                url: 'https://tfhub.dev/tensorflow/coco-ssd/1',
                type: 'graph_model',
                description: 'Object detection (80 classes from COCO)'
            },
            mobilenet: {
                url: 'https://storage.googleapis.com/tfjs-models/tfjs/mobilenet_v1_0.25_224/model.json',
                type: 'layers_model',
                inputSize: [1, 224, 224, 3],
                description: 'Image classification (1000 classes)'
            },
            efficientnet: {
                url: 'https://tfhub.dev/google/imagenet/efficientnet_v2_imagenet1k_b0/feature_vector/2',
                type: 'saved_model',
                inputSize: [1, 224, 224, 3],
                description: 'Feature extraction for custom classification'
            }
        };

        const cfg = { ...modelConfigs[name], ...config };
        if (!cfg.url) {
            throw new Error(`Unknown model: ${name}`);
        }

        try {
            console.log(`Loading model: ${name}...`);
            let model;

            if (cfg.type === 'graph_model') {
                model = await tf.loadGraphModel(cfg.url, { numFrames: 1 });
            } else if (cfg.type === 'layers_model') {
                model = await tf.loadLayersModel(cfg.url);
            } else {
                model = await tf.loadGraphModel(cfg.url);
            }

            this.loadedModels[name] = {
                model,
                config: cfg,
                loadedAt: Date.now(),
                inferenceCount: 0
            };

            console.log(`Model loaded: ${name}`);
            return this.loadedModels[name];
        } catch (e) {
            console.error(`Failed to load model ${name}:`, e);
            throw e;
        }
    }

    async predict(modelName, inputData, options = {}) {
        const entry = this.loadedModels[modelName];
        if (!entry) {
            throw new Error(`Model not loaded: ${modelName}`);
        }

        const startTime = performance.now();
        let inputTensor;

        try {
            if (inputData instanceof tf.Tensor) {
                inputTensor = inputData;
            } else if (inputData instanceof HTMLImageElement ||
                       inputData instanceof HTMLCanvasElement ||
                       inputData instanceof HTMLVideoElement) {
                inputTensor = tf.browser.fromPixels(inputData);
            } else if (inputData instanceof ImageData) {
                inputTensor = tf.browser.fromPixels(inputData);
            } else {
                throw new Error('Unsupported input type');
            }

            const config = entry.config;
            if (config.inputSize) {
                const [, h, w] = config.inputSize;
                if (inputTensor.shape[1] !== h || inputTensor.shape[2] !== w) {
                    const resized = tf.image.resizeBilinear(inputTensor, [h, w]);
                    inputTensor.dispose();
                    inputTensor = resized;
                }
            }

            if (inputTensor.shape.length === 3) {
                const batched = inputTensor.expandDims(0);
                inputTensor.dispose();
                inputTensor = batched;
            }

            let result;
            if (modelName === 'coco_ssd') {
                result = await entry.model.executeAsync(inputTensor);
            } else {
                result = entry.model.predict(inputTensor);
            }

            const inferenceTime = performance.now() - startTime;
            entry.inferenceCount++;

            return {
                result,
                inferenceTime,
                modelName,
                timestamp: Date.now()
            };
        } catch (e) {
            console.error(`Inference failed for ${modelName}:`, e);
            throw e;
        } finally {
            if (inputTensor && !inputTensor.isDisposed) {
                inputTensor.dispose();
            }
        }
    }

    async detectPose(imageElement, modelName = 'movenet') {
        const output = await this.predict(modelName, imageElement);
        const predictions = await output.result.array();
        output.result.dispose();

        if (modelName === 'movenet') {
            return this._parseMoveNet(predictions[0]);
        }
        return predictions;
    }

    async detectHands(imageElement) {
        if (!this.loadedModels.handpose) {
            await this.loadModel('handpose');
        }
        const output = await this.predict('handpose', imageElement);
        const landmarks = await output.result.array();
        output.result.dispose();

        return this._parseHandLandmarks(landmarks);
    }

    async detectObjects(imageElement) {
        if (!this.loadedModels.coco_ssd) {
            await this.loadModel('coco_ssd');
        }
        const output = await this.predict('coco_ssd', imageElement);
        const predictions = await output.result;
        output.result.dispose();

        return this._parseCOCOSSD(predictions);
    }

    async classifyImage(imageElement, topK = 5) {
        if (!this.loadedModels.mobilenet) {
            await this.loadModel('mobilenet');
        }
        const output = await this.predict('mobilenet', imageElement);
        const predictions = output.result;
        output.result.dispose();

        const softmax = tf.softmax(predictions);
        const values = await softmax.data();
        softmax.dispose();

        const indexed = Array.from(values)
            .map((v, i) => ({ probability: v, classIndex: i }))
            .sort((a, b) => b.probability - a.probability)
            .slice(0, topK);

        return indexed;
    }

    _parseMoveNet(keypoints) {
        const BODY_KEYPOINTS = [
            'nose', 'left_eye', 'right_eye', 'left_ear', 'right_ear',
            'left_shoulder', 'right_shoulder', 'left_elbow', 'right_elbow',
            'left_wrist', 'right_wrist', 'left_hip', 'right_hip',
            'left_knee', 'right_knee', 'left_ankle', 'right_ankle'
        ];

        return keypoints.map((kp, i) => ({
            name: BODY_KEYPOINTS[i] || `keypoint_${i}`,
            x: kp[1],
            y: kp[0],
            score: kp[2],
            visible: kp[2] > 0.2
        }));
    }

    _parseHandLandmarks(landmarks) {
        if (!landmarks || landmarks.length === 0) return [];

        const HAND_CONNECTIONS = [
            [0, 1], [1, 2], [2, 3], [3, 4],
            [0, 5], [5, 6], [6, 7], [7, 8],
            [0, 9], [9, 10], [10, 11], [11, 12],
            [0, 13], [13, 14], [14, 15], [15, 16],
            [0, 17], [17, 18], [18, 19], [19, 20],
            [5, 9], [9, 13], [13, 17]
        ];

        const FINGER_TIPS = [4, 8, 12, 16, 20];
        const FINGER_PIPS = [3, 6, 10, 14, 18];

        return landmarks.map(hand => {
            const keypoints = hand.map((kp, i) => ({
                index: i,
                x: kp[0],
                y: kp[1],
                z: kp[2] || 0
            }));

            const fingersUp = FINGER_TIPS.map((tip, i) => {
                if (i === 0) {
                    return keypoints[tip].x < keypoints[FINGER_PIPS[i]].x;
                }
                return keypoints[tip].y < keypoints[FINGER_PIPS[i]].y;
            });

            return {
                landmarks: keypoints,
                connections: HAND_CONNECTIONS,
                fingersUp,
                fingerCount: fingersUp.filter(Boolean).length
            };
        });
    }

    _parseCOCOSSD(predictions) {
        if (!predictions) return [];

        const result = {
            boxes: [],
            classes: [],
            scores: [],
            count: 0
        };

        if (predictions.boxes) {
            const boxes = predictions.boxes;
            result.boxes = Array.from(boxes.data).reduce((acc, val, i) => {
                const idx = Math.floor(i / 4);
                if (!acc[idx]) acc[idx] = [];
                acc[idx].push(val);
                return acc;
            }, []);

            result.classes = Array.from(predictions.classes.data);
            result.scores = Array.from(predictions.score.data);
            result.count = predictions.count;
        }

        const COCO_CLASSES = [
            'person', 'bicycle', 'car', 'motorcycle', 'airplane', 'bus',
            'train', 'truck', 'boat', 'traffic light', 'fire hydrant',
            'stop sign', 'parking meter', 'bench', 'bird', 'cat', 'dog',
            'horse', 'sheep', 'cow', 'elephant', 'bear', 'zebra', 'giraffe',
            'backpack', 'umbrella', 'handbag', 'tie', 'suitcase', 'frisbee',
            'skis', 'snowboard', 'sports ball', 'kite', 'baseball bat',
            'baseball glove', 'skateboard', 'surfboard', 'tennis racket',
            'bottle', 'wine glass', 'cup', 'fork', 'knife', 'spoon', 'bowl',
            'banana', 'apple', 'sandwich', 'orange', 'broccoli', 'carrot',
            'hot dog', 'pizza', 'donut', 'cake', 'chair', 'couch',
            'potted plant', 'bed', 'dining table', 'toilet', 'tv', 'laptop',
            'mouse', 'remote', 'keyboard', 'cell phone', 'microwave', 'oven',
            'toaster', 'sink', 'refrigerator', 'book', 'clock', 'vase',
            'scissors', 'teddy bear', 'hair drier', 'toothbrush'
        ];

        return result.scores.map((score, i) => ({
            class: COCO_CLASSES[result.classes[i]] || `class_${result.classes[i]}`,
            classIndex: result.classes[i],
            confidence: score,
            bbox: result.boxes[i] || []
        }));
    }

    getStatus() {
        return {
            tfReady: this.tfReady,
            backend: typeof tf !== 'undefined' ? tf.getBackend() : 'unavailable',
            loadedModels: Object.keys(this.loadedModels).map(name => ({
                name,
                inferenceCount: this.loadedModels[name].inferenceCount,
                loadedAt: this.loadedModels[name].loadedAt
            }))
        };
    }

    disposeModel(name) {
        if (this.loadedModels[name]) {
            this.loadedModels[name].model.dispose();
            delete this.loadedModels[name];
            console.log(`Model disposed: ${name}`);
        }
    }

    disposeAll() {
        Object.keys(this.loadedModels).forEach(name => this.disposeModel(name));
    }
}

window.tfModelManager = new TFModelManager();
