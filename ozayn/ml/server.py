#!/usr/bin/env python3
"""
Ozayn ML Backend Server - Advanced Version
Full computer vision, gesture recognition, and screen understanding
"""

import asyncio
import json
import base64
import io
import sys
import os
import time
from pathlib import Path

try:
    import websockets
except ImportError:
    os.system(f"{sys.executable} -m pip install websockets")
    import websockets

import numpy as np
import cv2
from PIL import Image

try:
    from vision_model import VisionModel
except ImportError:
    VisionModel = None

HAS_MEDIAPIPE = False
try:
    import mediapipe as mp
    HAS_MEDIAPIPE = hasattr(mp, 'solutions')
except Exception:
    pass


class HandGestureRecognizer:
    """Hand gesture recognition with MediaPipe or fallback"""
    
    def __init__(self):
        self.mp_hands = None
        self.hands = None
        self.mp_draw = None
        self.prev_gray = None
        
        if HAS_MEDIAPIPE:
            try:
                self.mp_hands = mp.solutions.hands
                self.hands = self.mp_hands.Hands(
                    static_image_mode=False,
                    max_num_hands=2,
                    min_detection_confidence=0.7,
                    min_tracking_confidence=0.5
                )
                self.mp_draw = mp.solutions.drawing_utils
            except Exception:
                self.hands = None
    
    def detect(self, frame):
        if self.hands:
            return self._mediapipe_gesture(frame)
        return self._motion_gesture(frame)
    
    def _mediapipe_gesture(self, frame):
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        results = self.hands.process(rgb)
        
        if not results.multi_hand_landmarks:
            return {"gesture": "none", "hands": 0, "confidence": 0}
        
        gestures = []
        for hand_landmarks in results.multi_hand_landmarks:
            gesture = self._classify_landmarks(hand_landmarks)
            gestures.append(gesture)
        
        return {
            "gesture": gestures[0]["gesture"] if gestures else "none",
            "hands": len(gestures),
            "details": gestures,
            "confidence": gestures[0]["confidence"] if gestures else 0
        }
    
    def _classify_landmarks(self, landmarks):
        tips = [4, 8, 12, 16, 20]
        pips = [3, 6, 10, 14, 18]
        
        fingers_up = []
        for i in range(5):
            if i == 0:
                fingers_up.append(landmarks.landmark[tips[i]].x < landmarks.landmark[pips[i]].x)
            else:
                fingers_up.append(landmarks.landmark[tips[i]].y < landmarks.landmark[pips[i]].y)
        
        total = sum(fingers_up)
        
        gesture_map = {
            0: "fist",
            5: "open_hand",
        }
        
        if fingers_up[1] and fingers_up[2] and not fingers_up[3]:
            gesture = "peace"
        elif fingers_up[0] and not fingers_up[1]:
            gesture = "thumbs_up"
        elif fingers_up[1] and not fingers_up[2]:
            gesture = "pointing"
        else:
            gesture = gesture_map.get(total, f"fingers_{total}")
        
        wrist = landmarks.landmark[0]
        return {
            "gesture": gesture,
            "fingers": [int(f) for f in fingers_up],
            "confidence": 0.9,
            "position": {"x": wrist.x, "y": wrist.y}
        }
    
    def _motion_gesture(self, frame):
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        gray = cv2.GaussianBlur(gray, (21, 21), 0)
        
        if self.prev_gray is None:
            self.prev_gray = gray
            return {"gesture": "none", "hands": 0, "confidence": 0}
        
        diff = cv2.absdiff(self.prev_gray, gray)
        thresh = cv2.threshold(diff, 25, 255, cv2.THRESH_BINARY)[1]
        thresh = cv2.dilate(thresh, None, iterations=2)
        
        contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        gesture = "none"
        for c in contours:
            if cv2.contourArea(c) > 5000:
                x, y, w, h = cv2.boundingRect(c)
                cx, cy = x + w//2, y + h//2
                fx, fy = frame.shape[1]//2, frame.shape[0]//2
                
                if abs(cx - fx) > abs(cy - fy):
                    gesture = "swipe_right" if cx > fx else "swipe_left"
                else:
                    gesture = "swipe_down" if cy > fy else "swipe_up"
                break
        
        self.prev_gray = gray
        return {"gesture": gesture, "hands": 1 if gesture != "none" else 0, "confidence": 0.7}


class VisionProcessor:
    """Advanced computer vision processing"""
    
    def __init__(self):
        self.face_cascade = None
        self.eye_cascade = None
        cascade_path = cv2.data.haarcascades
        face_file = os.path.join(cascade_path, 'haarcascade_frontalface_default.xml')
        eye_file = os.path.join(cascade_path, 'haarcascade_eye.xml')
        
        if os.path.exists(face_file):
            self.face_cascade = cv2.CascadeClassifier(face_file)
        if os.path.exists(eye_file):
            self.eye_cascade = cv2.CascadeClassifier(eye_file)
        
        self.hand_recognizer = HandGestureRecognizer()
    
    def detect_faces(self, image_data):
        img = self._decode_image(image_data)
        if img is None:
            return {"faces": [], "count": 0, "error": "Invalid image"}
        
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        
        if self.face_cascade is None:
            return {"faces": [], "count": 0, "error": "Face cascade not available", "mode": "basic"}
        
        faces = self.face_cascade.detectMultiScale(gray, 1.3, 5)
        
        result = []
        for (x, y, w, h) in faces:
            entry = {"x": int(x), "y": int(y), "width": int(w), "height": int(h), "confidence": 0.95}
            
            if self.eye_cascade:
                face_roi = gray[y:y+h, x:x+w]
                eyes = self.eye_cascade.detectMultiScale(face_roi)
                entry["eyes"] = len(eyes)
            
            result.append(entry)
        
        return {"faces": result, "count": len(result), "mode": "opencv"}
    
    def detect_objects(self, image_data):
        img = self._decode_image(image_data)
        if img is None:
            return {"objects": [], "error": "Invalid image"}
        
        hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)
        height, width = img.shape[:2]
        objects = []
        
        colors = {
            "red": ([0, 120, 70], [10, 255, 255]),
            "green": ([36, 100, 100], [86, 255, 255]),
            "blue": ([100, 100, 100], [130, 255, 255]),
            "yellow": ([20, 100, 100], [35, 255, 255]),
            "white": ([0, 0, 200], [180, 30, 255]),
            "black": ([0, 0, 0], [180, 255, 50])
        }
        
        for name, (lower, upper) in colors.items():
            mask = cv2.inRange(hsv, np.array(lower), np.array(upper))
            contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            for c in contours:
                if cv2.contourArea(c) > 1000:
                    x, y, w, h = cv2.boundingRect(c)
                    objects.append({"type": "color", "color": name, "bbox": [int(x), int(y), int(w), int(h)]})
        
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        edges = cv2.Canny(gray, 50, 150)
        contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        for c in contours:
            area = cv2.contourArea(c)
            if area < 500:
                continue
            peri = cv2.arcLength(c, True)
            approx = cv2.approxPolyDP(c, 0.02 * peri, True)
            vertices = len(approx)
            
            shape_map = {3: "triangle", 4: "rectangle"}
            if vertices in shape_map:
                shape = shape_map[vertices]
            elif vertices > 4:
                shape = "circle"
            else:
                continue
            
            x, y, w, h = cv2.boundingRect(approx)
            objects.append({"type": "shape", "shape": shape, "vertices": vertices, "bbox": [int(x), int(y), int(w), int(h)]})
        
        return {"objects": objects, "count": len(objects), "mode": "analysis"}
    
    def analyze_screen(self, image_data):
        img = self._decode_image(image_data)
        if img is None:
            return {"type": "unknown", "error": "Invalid image"}
        
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        height, width = gray.shape
        brightness = int(np.mean(gray))
        contrast = int(np.std(gray))
        
        edges = cv2.Canny(gray, 50, 150)
        lines = cv2.HoughLinesP(edges, 1, np.pi/180, 100, minLineLength=100, maxLineGap=10)
        
        text_regions = self._count_text_regions(gray)
        
        faces = []
        if self.face_cascade:
            faces = self.face_cascade.detectMultiScale(gray, 1.3, 5)
        
        content_type = "unknown"
        if text_regions > 10:
            content_type = "application"
        elif text_regions > 5:
            content_type = "document"
        elif len(faces) > 0:
            content_type = "video_call"
        elif brightness < 50:
            content_type = "dark_theme"
        elif lines is not None and len(lines) > 20:
            content_type = "interface"
        
        return {
            "type": content_type,
            "resolution": f"{width}x{height}",
            "text_regions": text_regions,
            "faces": len(faces),
            "brightness": brightness,
            "contrast": contrast,
            "has_ui_elements": lines is not None and len(lines) > 0
        }
    
    def detect_gesture(self, frame_data):
        img = self._decode_image(frame_data)
        if img is None:
            return {"gesture": "none", "error": "Invalid frame"}
        return self.hand_recognizer.detect(img)
    
    def _count_text_regions(self, gray):
        blur = cv2.GaussianBlur(gray, (5, 5), 0)
        edges = cv2.Canny(blur, 50, 150)
        contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        count = 0
        for contour in contours:
            x, y, w, h = cv2.boundingRect(contour)
            aspect = w / h if h > 0 else 0
            if 0.1 < aspect < 10 and w > 10 and h > 5:
                count += 1
        return count
    
    def _decode_image(self, image_data):
        if ',' in image_data:
            image_data = image_data.split(',')[1]
        try:
            img_bytes = base64.b64decode(image_data)
            img = Image.open(io.BytesIO(img_bytes))
            img = img.convert('RGB')
            return cv2.cvtColor(np.array(img), cv2.COLOR_RGB2BGR)
        except Exception:
            return None


class MLServer:
    """WebSocket server for ML processing"""
    
    def __init__(self, host='localhost', port=8765):
        self.host = host
        self.port = port
        self.vision = VisionProcessor()
        self.vision_model = VisionModel() if VisionModel else None
        self.clients = set()
        self.session_users = {}
    
    async def handler(self, websocket, path=None):
        self.clients.add(websocket)
        print(f"Client connected. Total: {len(self.clients)}")
        
        try:
            async for message in websocket:
                try:
                    data = json.loads(message)
                    response = await self.process_message(data)
                    await websocket.send(json.dumps(response))
                except json.JSONDecodeError:
                    await websocket.send(json.dumps({"error": "Invalid JSON"}))
                except Exception as e:
                    await websocket.send(json.dumps({"error": str(e)}))
        except websockets.exceptions.ConnectionClosed:
            pass
        finally:
            self.clients.discard(websocket)
            print(f"Client disconnected. Total: {len(self.clients)}")
    
    async def process_message(self, data):
        action = data.get('action', '')
        
        if action == 'detect_faces':
            return self.vision.detect_faces(data.get('image', ''))
        elif action == 'detect_objects':
            return self.vision.detect_objects(data.get('image', ''))
        elif action == 'analyze_screen':
            return self.vision.analyze_screen(data.get('image', ''))
        elif action == 'detect_gesture':
            return self.vision.detect_gesture(data.get('frame', ''))
        elif action == 'screen_understand':
            if self.vision_model:
                return self.vision_model.understand_screen(data.get('image', ''))
            return self.vision.analyze_screen(data.get('image', ''))
        elif action == 'analyze_camera':
            if self.vision_model:
                return self.vision_model.analyze_camera_frame(data.get('image', ''))
            return {"error": "Camera analysis not available"}
        elif action == 'ping':
            return {"pong": True, "capabilities": self.get_capabilities()}
        elif action == 'collaboration_join':
            return await self._handle_collaboration_join(data)
        elif action == 'collaboration_leave':
            return await self._handle_collaboration_leave(data)
        elif action == 'collaboration_event':
            return await self._handle_collaboration_event(data)
        else:
            return {"error": f"Unknown action: {action}"}
    
    def get_capabilities(self):
        return {
            "face_detection": self.vision.face_cascade is not None,
            "object_detection": True,
            "screen_analysis": True,
            "screen_understanding": self.vision_model is not None,
            "gesture_recognition": HAS_MEDIAPIPE or True,
            "hand_tracking": HAS_MEDIAPIPE,
            "mediapipe": HAS_MEDIAPIPE,
            "collaboration": True,
            "mode": "full" if HAS_MEDIAPIPE else "basic"
        }
    
    async def _handle_collaboration_join(self, data):
        session_id = data.get('session_id', 'default')
        user_id = data.get('user_id', '')
        username = data.get('username', 'Anonymous')
        
        if session_id not in self.session_users:
            self.session_users[session_id] = {}
        
        self.session_users[session_id][user_id] = {
            'username': username,
            'ws': data.get('ws_ref'),
            'cursor': {'x': 0, 'y': 0}
        }
        
        users = [
            {'id': uid, 'username': u['username']}
            for uid, u in self.session_users[session_id].items()
        ]
        
        return {
            'type': 'session_joined',
            'session_id': session_id,
            'users': users
        }
    
    async def _handle_collaboration_leave(self, data):
        session_id = data.get('session_id', 'default')
        user_id = data.get('user_id', '')
        
        if session_id in self.session_users and user_id in self.session_users[session_id]:
            del self.session_users[session_id][user_id]
            if not self.session_users[session_id]:
                del self.session_users[session_id]
        
        return {'type': 'session_left', 'success': True}
    
    async def _handle_collaboration_event(self, data):
        session_id = data.get('session_id', 'default')
        event_type = data.get('event_type', '')
        event_data = data.get('data', {})
        
        if session_id in self.session_users:
            for uid, user in self.session_users[session_id].items():
                if uid != data.get('user_id'):
                    pass
        
        return {'type': 'event_broadcast', 'success': True}
    
    async def start(self):
        print(f"Starting Ozayn ML Server on ws://{self.host}:{self.port}")
        print(f"Capabilities: {json.dumps(self.get_capabilities(), indent=2)}")
        async with websockets.serve(self.handler, self.host, self.port):
            await asyncio.Future()


def main():
    host = os.environ.get('ML_HOST', 'localhost')
    port = int(os.environ.get('ML_PORT', 8765))
    server = MLServer(host, port)
    asyncio.run(server.start())


if __name__ == '__main__':
    main()
