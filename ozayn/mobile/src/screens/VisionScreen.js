import React, { useState, useEffect, useRef } from 'react';
import {
    View, Text, TouchableOpacity, StyleSheet, Alert, Dimensions
} from 'react-native';
import { Camera } from 'expo-camera';

const API_BASE = 'http://localhost:8765';

export default function VisionScreen() {
    const [hasPermission, setHasPermission] = useState(null);
    const [cameraType, setCameraType] = useState(Camera.Constants.Type.front);
    const [detecting, setDetecting] = useState(false);
    const [lastResult, setLastResult] = useState(null);
    const cameraRef = useRef(null);
    const detectInterval = useRef(null);

    useEffect(() => {
        (async () => {
            const { status } = await Camera.requestCameraPermissionsAsync();
            setHasPermission(status === 'granted');
        })();
        return () => {
            if (detectInterval.current) clearInterval(detectInterval.current);
        };
    }, []);

    const startDetection = async () => {
        if (!cameraRef.current) return;
        setDetecting(true);

        detectInterval.current = setInterval(async () => {
            if (!cameraRef.current) return;
            try {
                const photo = await cameraRef.current.takePictureAsync({
                    base64: true,
                    quality: 0.5,
                    skipProcessing: true
                });

                const ws = new WebSocket(`ws://localhost:8765`);
                ws.onopen = () => {
                    ws.send(JSON.stringify({
                        action: 'detect_gesture',
                        frame: photo.base64
                    }));
                };
                ws.onmessage = (event) => {
                    const result = JSON.parse(event.data);
                    setLastResult(result);
                    ws.close();
                };
                ws.onerror = () => {
                    ws.close();
                };
            } catch (e) {
                console.error('Detection error:', e);
            }
        }, 1000);
    };

    const stopDetection = () => {
        setDetecting(false);
        if (detectInterval.current) {
            clearInterval(detectInterval.current);
            detectInterval.current = null;
        }
    };

    const toggleCamera = () => {
        setCameraType(
            cameraType === Camera.Constants.Type.front
                ? Camera.Constants.Type.back
                : Camera.Constants.Type.front
        );
    };

    if (hasPermission === null) {
        return <View style={styles.container}><Text style={styles.loading}>Requesting camera permission...</Text></View>;
    }
    if (hasPermission === false) {
        return <View style={styles.container}><Text style={styles.error}>No camera access</Text></View>;
    }

    return (
        <View style={styles.container}>
            <Camera ref={cameraRef} style={styles.camera} type={cameraType}>
                <View style={styles.overlay}>
                    {lastResult && lastResult.gesture && lastResult.gesture !== 'none' && (
                        <View style={styles.gestureBadge}>
                            <Text style={styles.gestureText}>{lastResult.gesture}</Text>
                            {lastResult.confidence && (
                                <Text style={styles.confidence}>{Math.round(lastResult.confidence * 100)}%</Text>
                            )}
                        </View>
                    )}
                </View>
            </Camera>

            <View style={styles.controls}>
                <TouchableOpacity style={styles.controlBtn} onPress={toggleCamera}>
                    <Text style={styles.controlBtnText}>Flip</Text>
                </TouchableOpacity>

                <TouchableOpacity
                    style={[styles.controlBtn, detecting && styles.activeBtn]}
                    onPress={detecting ? stopDetection : startDetection}
                >
                    <Text style={styles.controlBtnText}>
                        {detecting ? 'Stop' : 'Detect'}
                    </Text>
                </TouchableOpacity>
            </View>

            {lastResult && (
                <View style={styles.resultPanel}>
                    <Text style={styles.resultTitle}>Detection Result</Text>
                    <Text style={styles.resultText}>Gesture: {lastResult.gesture || 'none'}</Text>
                    <Text style={styles.resultText}>Hands: {lastResult.hands || 0}</Text>
                    {lastResult.details && lastResult.details.map((d, i) => (
                        <Text key={i} style={styles.resultText}>
                            Hand {i + 1}: {d.gesture} ({d.fingers ? d.fingers.filter(Boolean).length : 0} fingers)
                        </Text>
                    ))}
                </View>
            )}
        </View>
    );
}

const styles = StyleSheet.create({
    container: { flex: 1, backgroundColor: '#0a0a0f' },
    loading: { color: '#fff', textAlign: 'center', marginTop: 100, fontSize: 16 },
    error: { color: '#ef4444', textAlign: 'center', marginTop: 100, fontSize: 16 },
    camera: { flex: 1 },
    overlay: { flex: 1, justifyContent: 'flex-start', alignItems: 'center', paddingTop: 50 },
    gestureBadge: {
        backgroundColor: 'rgba(168, 85, 247, 0.9)',
        paddingHorizontal: 20, paddingVertical: 10,
        borderRadius: 20, flexDirection: 'row', alignItems: 'center'
    },
    gestureText: { color: '#fff', fontSize: 20, fontWeight: 'bold' },
    confidence: { color: '#fff', fontSize: 14, marginLeft: 10, opacity: 0.8 },
    controls: {
        flexDirection: 'row', justifyContent: 'center', padding: 16,
        backgroundColor: '#1a1a2e', borderTopWidth: 1, borderTopColor: '#333'
    },
    controlBtn: {
        backgroundColor: '#333', paddingHorizontal: 30, paddingVertical: 12,
        borderRadius: 8, marginHorizontal: 8
    },
    activeBtn: { backgroundColor: '#a855f7' },
    controlBtnText: { color: '#fff', fontSize: 16, fontWeight: '600' },
    resultPanel: {
        backgroundColor: '#1a1a2e', padding: 16,
        borderTopWidth: 1, borderTopColor: '#333'
    },
    resultTitle: { color: '#a855f7', fontSize: 14, fontWeight: '600', marginBottom: 8 },
    resultText: { color: '#fff', fontSize: 13, marginBottom: 4 }
});
