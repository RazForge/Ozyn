/**
 * Ozayn Screen Analyzer
 * Captures screen content and provides structured understanding via ML
 */

class ScreenAnalyzer {
    constructor() {
        this.pipeline = window.mlPipeline;
        this.remoteML = window.ozaynML;
        this.isCapturing = false;
        this.captureInterval = null;
        this.lastAnalysis = null;
        this.captureCanvas = null;
        this.captureCtx = null;
        this.onAnalysis = null;
        this.screenshotHistory = [];
        this.maxHistory = 20;
        this.captureFPS = 0.5;
        this.stats = {
            capturesCount: 0,
            analysesCount: 0,
            avgLatency: 0
        };
    }

    init() {
        this.captureCanvas = document.createElement('canvas');
        this.captureCtx = this.captureCanvas.getContext('2d');
        return true;
    }

    async captureScreen() {
        try {
            if (navigator.mediaDevices && navigator.mediaDevices.getDisplayMedia) {
                const stream = await navigator.mediaDevices.getDisplayMedia({
                    video: { cursor: 'always' },
                    audio: false
                });

                const video = document.createElement('video');
                video.srcObject = stream;
                await video.play();

                await new Promise(r => setTimeout(r, 200));

                this.captureCanvas.width = video.videoWidth;
                this.captureCanvas.height = video.videoHeight;
                this.captureCtx.drawImage(video, 0, 0);

                stream.getTracks().forEach(t => t.stop());
                video.remove();

                this.stats.capturesCount++;
                return this.captureCanvas;
            }
        } catch (e) {
            console.warn('Screen capture not available:', e);
        }
        return null;
    }

    captureVideoFrame(videoElement) {
        if (!videoElement || !videoElement.videoWidth) return null;

        this.captureCanvas.width = videoElement.videoWidth;
        this.captureCanvas.height = videoElement.videoHeight;
        this.captureCtx.drawImage(videoElement, 0, 0);

        return this.captureCanvas;
    }

    captureCanvasElement(canvasElement) {
        if (!canvasElement) return null;

        this.captureCanvas.width = canvasElement.width;
        this.captureCanvas.height = canvasElement.height;
        this.captureCtx.drawImage(canvasElement, 0, 0);

        return this.captureCanvas;
    }

    async analyzeFrame(imageSource, mode = 'full') {
        const startTime = performance.now();

        try {
            let result;

            if (mode === 'quick') {
                result = await this._quickAnalyze(imageSource);
            } else if (mode === 'objects') {
                result = await this._analyzeObjects(imageSource);
            } else if (mode === 'text') {
                result = await this._analyzeTextRegions(imageSource);
            } else {
                result = await this._fullAnalyze(imageSource);
            }

            const latency = performance.now() - startTime;
            this.stats.analysesCount++;
            this.stats.avgLatency = (this.stats.avgLatency + latency) / 2;

            result._meta = {
                mode,
                latency: Math.round(latency),
                timestamp: Date.now()
            };

            this.lastAnalysis = result;

            if (this.onAnalysis) {
                this.onAnalysis(result);
            }

            return result;
        } catch (e) {
            console.error('Screen analysis error:', e);
            return { error: e.message };
        }
    }

    async _quickAnalyze(imageSource) {
        const canvas = this._getSourceCanvas(imageSource);
        if (!canvas) return { error: 'No image source' };

        const ctx = canvas.getContext('2d');
        const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
        const data = imageData.data;

        let totalR = 0, totalG = 0, totalB = 0;
        let brightness = 0;
        const pixelCount = data.length / 4;

        for (let i = 0; i < data.length; i += 4) {
            totalR += data[i];
            totalG += data[i + 1];
            totalB += data[i + 2];
            brightness += (data[i] + data[i + 1] + data[i + 2]) / 3;
        }

        const avgBrightness = brightness / pixelCount;
        const avgR = totalR / pixelCount;
        const avgG = totalG / pixelCount;
        const avgB = totalB / pixelCount;

        let dominantColor = 'unknown';
        if (avgR > avgG && avgR > avgB) dominantColor = 'red';
        else if (avgG > avgR && avgG > avgB) dominantColor = 'green';
        else if (avgB > avgR && avgB > avgG) dominantColor = 'blue';

        let theme = 'light';
        if (avgBrightness < 80) theme = 'dark';
        else if (avgBrightness < 130) theme = 'mixed';

        return {
            type: 'quick',
            resolution: `${canvas.width}x${canvas.height}`,
            brightness: Math.round(avgBrightness),
            theme,
            dominantColor,
            avgRGB: { r: Math.round(avgR), g: Math.round(avgG), b: Math.round(avgB) },
            isDark: avgBrightness < 80
        };
    }

    async _fullAnalyze(imageSource) {
        const canvas = this._getSourceCanvas(imageSource);
        if (!canvas) return { error: 'No image source' };

        const quickResult = await this._quickAnalyze(canvas);

        const edgeResult = this._detectEdges(canvas);
        const layoutResult = this._analyzeLayout(canvas);
        const regionResult = this._detectRegions(canvas);

        let objectResult = null;
        try {
            objectResult = await this.pipeline.processFrame(canvas, 'detect_objects', {
                allowConcurrent: true
            });
        } catch (e) {
            console.warn('Object detection failed:', e);
        }

        return {
            type: 'full',
            basic: quickResult,
            edges: edgeResult,
            layout: layoutResult,
            regions: regionResult,
            objects: objectResult,
            screenType: this._classifyScreenType(quickResult, edgeResult, regionResult)
        };
    }

    async _analyzeObjects(imageSource) {
        const canvas = this._getSourceCanvas(imageSource);
        if (!canvas) return { error: 'No image source' };

        const result = await this.pipeline.processFrame(canvas, 'detect_objects', {
            allowConcurrent: true
        });

        return {
            type: 'objects',
            objects: result
        };
    }

    async _analyzeTextRegions(imageSource) {
        const canvas = this._getSourceCanvas(imageSource);
        if (!canvas) return { error: 'No image source' };

        const regions = this._detectRegions(canvas);
        const textRegions = regions.filter(r => r.type === 'text');

        return {
            type: 'text',
            textRegions,
            totalRegions: regions.length,
            textDensity: textRegions.length / regions.length || 0
        };
    }

    _getSourceCanvas(imageSource) {
        if (imageSource instanceof HTMLCanvasElement) {
            return imageSource;
        }
        if (imageSource instanceof HTMLVideoElement) {
            const canvas = document.createElement('canvas');
            canvas.width = imageSource.videoWidth;
            canvas.height = imageSource.videoHeight;
            canvas.getContext('2d').drawImage(imageSource, 0, 0);
            return canvas;
        }
        if (imageSource instanceof HTMLImageElement) {
            const canvas = document.createElement('canvas');
            canvas.width = imageSource.naturalWidth;
            canvas.height = imageSource.naturalHeight;
            canvas.getContext('2d').drawImage(imageSource, 0, 0);
            return canvas;
        }
        return null;
    }

    _detectEdges(canvas) {
        const ctx = canvas.getContext('2d');
        const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
        const gray = this._toGrayscale(imageData);

        const width = canvas.width;
        const height = canvas.height;
        const edges = new Uint8ClampedArray(gray.length);

        for (let y = 1; y < height - 1; y++) {
            for (let x = 1; x < width - 1; x++) {
                const idx = y * width + x;
                const gx = -gray[(y - 1) * width + (x - 1)] + gray[(y - 1) * width + (x + 1)]
                    - 2 * gray[y * width + (x - 1)] + 2 * gray[y * width + (x + 1)]
                    - gray[(y + 1) * width + (x - 1)] + gray[(y + 1) * width + (x + 1)];
                const gy = -gray[(y - 1) * width + (x - 1)] - 2 * gray[(y - 1) * width + x] - gray[(y - 1) * width + (x + 1)]
                    + gray[(y + 1) * width + (x - 1)] + 2 * gray[(y + 1) * width + x] + gray[(y + 1) * width + (x + 1)];
                edges[idx] = Math.min(255, Math.sqrt(gx * gx + gy * gy));
            }
        }

        let edgeCount = 0;
        for (let i = 0; i < edges.length; i++) {
            if (edges[i] > 50) edgeCount++;
        }

        return {
            edgeDensity: edgeCount / edges.length,
            hasStrongEdges: edgeCount / edges.length > 0.05
        };
    }

    _toGrayscale(imageData) {
        const data = imageData.data;
        const gray = new Uint8ClampedArray(data.length / 4);
        for (let i = 0; i < data.length; i += 4) {
            gray[i / 4] = 0.299 * data[i] + 0.587 * data[i + 1] + 0.114 * data[i + 2];
        }
        return gray;
    }

    _analyzeLayout(canvas) {
        const ctx = canvas.getContext('2d');
        const w = canvas.width;
        const h = canvas.height;
        const blockSize = 32;
        const blocksX = Math.ceil(w / blockSize);
        const blocksY = Math.ceil(h / blockSize);

        const grid = [];
        for (let by = 0; by < blocksY; by++) {
            const row = [];
            for (let bx = 0; bx < blocksX; bx++) {
                const imageData = ctx.getImageData(
                    bx * blockSize, by * blockSize,
                    Math.min(blockSize, w - bx * blockSize),
                    Math.min(blockSize, h - by * blockSize)
                );
                const avg = this._blockAverage(imageData);
                row.push(avg);
            }
            grid.push(row);
        }

        let horizontalDividers = 0;
        let verticalDividers = 0;

        for (let by = 0; by < blocksY; by++) {
            let changes = 0;
            for (let bx = 1; bx < blocksX; bx++) {
                if (Math.abs(grid[by][bx] - grid[by][bx - 1]) > 60) changes++;
            }
            if (changes >= 3) horizontalDividers++;
        }

        for (let bx = 0; bx < blocksX; bx++) {
            let changes = 0;
            for (let by = 1; by < blocksY; by++) {
                if (Math.abs(grid[by][bx] - grid[by - 1][bx]) > 60) changes++;
            }
            if (changes >= 3) verticalDividers++;
        }

        return {
            hasHorizontalLayout: horizontalDividers > 2,
            hasVerticalLayout: verticalDividers > 2,
            gridCols: blocksX,
            gridRows: blocksY,
            complexity: (horizontalDividers + verticalDividers) / (blocksX + blocksY)
        };
    }

    _blockAverage(imageData) {
        const data = imageData.data;
        let total = 0;
        const pixels = data.length / 4;
        for (let i = 0; i < data.length; i += 4) {
            total += (data[i] + data[i + 1] + data[i + 2]) / 3;
        }
        return total / pixels;
    }

    _detectRegions(canvas) {
        const ctx = canvas.getContext('2d');
        const w = canvas.width;
        const h = canvas.height;
        const sampleSize = 16;
        const regions = [];

        const samplesX = Math.ceil(w / sampleSize);
        const samplesY = Math.ceil(h / sampleSize);
        const brightnessGrid = [];

        for (let sy = 0; sy < samplesY; sy++) {
            const row = [];
            for (let sx = 0; sx < samplesX; sx++) {
                const imageData = ctx.getImageData(
                    sx * sampleSize, sy * sampleSize,
                    Math.min(sampleSize, w - sx * sampleSize),
                    Math.min(sampleSize, h - sy * sampleSize)
                );
                row.push(this._blockAverage(imageData));
            }
            brightnessGrid.push(row);
        }

        const visited = Array.from({ length: samplesY }, () => new Array(samplesX).fill(false));

        for (let sy = 0; sy < samplesY; sy++) {
            for (let sx = 0; sx < samplesX; sx++) {
                if (visited[sy][sx]) continue;

                const brightness = brightnessGrid[sy][sx];
                const region = this._floodFill(brightnessGrid, visited, sx, sy, brightness, 30);

                if (region.area > 4) {
                    const type = brightness > 200 ? 'light_bg' :
                                 brightness < 50 ? 'dark_bg' : 'mixed';

                    regions.push({
                        x: region.minX * sampleSize,
                        y: region.minY * sampleSize,
                        width: (region.maxX - region.minX + 1) * sampleSize,
                        height: (region.maxY - region.minY + 1) * sampleSize,
                        area: region.area,
                        avgBrightness: Math.round(brightness),
                        type
                    });
                }
            }
        }

        return regions.sort((a, b) => b.area - a.area);
    }

    _floodFill(grid, visited, startX, startY, threshold, tolerance) {
        const rows = grid.length;
        const cols = grid[0].length;
        const stack = [[startX, startY]];
        let minX = startX, maxX = startX, minY = startY, maxY = startY;
        let area = 0;

        while (stack.length > 0) {
            const [x, y] = stack.pop();
            if (x < 0 || x >= cols || y < 0 || y >= rows) continue;
            if (visited[y][x]) continue;
            if (Math.abs(grid[y][x] - threshold) > tolerance) continue;

            visited[y][x] = true;
            area++;
            minX = Math.min(minX, x);
            maxX = Math.max(maxX, x);
            minY = Math.min(minY, y);
            maxY = Math.max(maxY, y);

            stack.push([x + 1, y], [x - 1, y], [x, y + 1], [x, y - 1]);
        }

        return { minX, maxX, minY, maxY, area };
    }

    _classifyScreenType(basic, edges, regions) {
        const hasText = regions.filter(r => r.type === 'mixed').length > 3;
        const hasStrongUI = edges.hasStrongEdges && basic.brightness < 150;
        const isVideoCall = basic.dominantColor === 'blue' && basic.brightness < 100;
        const isDocument = hasText && basic.theme === 'light';
        const isCodeEditor = hasStrongUI && basic.theme === 'dark';
        const isTerminal = basic.theme === 'dark' && basic.brightness < 50;

        if (isVideoCall) return 'video_call';
        if (isTerminal) return 'terminal';
        if (isCodeEditor) return 'code_editor';
        if (isDocument) return 'document';
        if (hasStrongUI) return 'application';
        return 'desktop';
    }

    startAutoCapture(callback, fps = 0.5) {
        this.isCapturing = true;
        this.captureFPS = fps;
        this.captureInterval = setInterval(async () => {
            if (!this.isCapturing) return;
            const canvas = await this.captureScreen();
            if (canvas) {
                const result = await this.analyzeFrame(canvas, 'full');
                if (callback) callback(result);
            }
        }, 1000 / fps);
    }

    stopAutoCapture() {
        this.isCapturing = false;
        if (this.captureInterval) {
            clearInterval(this.captureInterval);
            this.captureInterval = null;
        }
    }

    addToHistory(analysis) {
        this.screenshotHistory.unshift(analysis);
        if (this.screenshotHistory.length > this.maxHistory) {
            this.screenshotHistory.pop();
        }
    }

    getHistory() {
        return [...this.screenshotHistory];
    }

    getStats() {
        return { ...this.stats };
    }

    getLastAnalysis() {
        return this.lastAnalysis;
    }
}

window.screenAnalyzer = new ScreenAnalyzer();
