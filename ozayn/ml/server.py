#!/usr/bin/env python3
"""
Ozayn ML Backend Server
Provides computer vision, gesture recognition, and screen understanding
Uses WebSocket for real-time communication with PHP frontend
"""

import asyncio
import json
import base64
import io
import sys
import os
from pathlib import Path

try:
    import websockets
except ImportError:
    print("Installing websockets...")
    os.system(f"{sys.executable} -m pip install websockets")
    import websockets

try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False
    print("NumPy not available - using basic mode")

try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False
    print("Pillow not available - using basic mode")

try:
    import cv2
    HAS_CV2 = True
except ImportError:
    HAS_CV2 = False
    print("OpenCV not available - using basic mode")


class VisionProcessor:
    """Computer vision processing"""
    
    def __init__(self):
        self.face_cascade = None
        if HAS_CV2:
            cascade_path = cv2.data.haarcascades + 'haarcascade_frontalface_default.xml'
            if os.path.exists(cascade_path):
                self.face_cascade = cv2.CascadeClassifier(cascade_path)
    
    def detect_faces(self, image_data):
        """Detect faces in image"""
        if not HAS_CV2 or not self.face_cascade:
            return {"faces": [], "count": 0, "mode": "basic"}
        
        try:
            img = self._decode_image(image_data)
            if img is None:
                return {"faces": [], "count": 0, "error": "Invalid image"}
            
            gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
            faces = self.face_cascade.detectMultiScale(gray, 1.3, 5)
            
            result = []
            for (x, y, w, h) in faces:
                result.append({
                    "x": int(x), "y": int(y),
                    "width": int(w), "height": int(h),
                    "confidence": 0.95
                })
            
            return {"faces": result, "count": len(result), "mode": "opencv"}
        except Exception as e:
            return {"faces": [], "count": 0, "error": str(e)}
    
    def detect_objects(self, image_data):
        """Basic object detection using color analysis"""
        if not HAS_CV2:
            return {"objects": [], "mode": "basic"}
        
        try:
            img = self._decode_image(image_data)
            if img is None:
                return {"objects": [], "error": "Invalid image"}
            
            hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)
            height, width = img.shape[:2]
            
            objects = []
            # Detect dominant colors
            for color_name, (lower, upper) in self._get_color_ranges().items():
                mask = cv2.inRange(hsv, np.array(lower), np.array(upper))
                pixels = cv2.countNonZero(mask)
                if pixels > (height * width * 0.05):
                    objects.append({
                        "type": "color",
                        "name": color_name,
                        "coverage": round(pixels / (height * width) * 100, 1)
                    })
            
            return {"objects": objects, "mode": "color_analysis"}
        except Exception as e:
            return {"objects": [], "error": str(e)}
    
    def analyze_screen(self, image_data):
        """Basic screen content analysis"""
        if not HAS_CV2:
            return {"type": "unknown", "mode": "basic"}
        
        try:
            img = self._decode_image(image_data)
            if img is None:
                return {"type": "unknown", "error": "Invalid image"}
            
            gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
            height, width = gray.shape
            
            # Analyze layout
            edges = cv2.Canny(gray, 50, 150)
            lines = cv2.HoughLinesP(edges, 1, np.pi/180, 100, minLineLength=100, maxLineGap=10)
            
            has_text = self._detect_text_regions(gray)
            has_ui = len(lines) > 20 if lines is not None else False
            
            content_type = "unknown"
            if has_text and has_ui:
                content_type = "application"
            elif has_text:
                content_type = "document"
            elif has_ui:
                content_type = "interface"
            
            return {
                "type": content_type,
                "has_text": has_text,
                "has_ui_elements": has_ui,
                "resolution": f"{width}x{height}",
                "mode": "opencv"
            }
        except Exception as e:
            return {"type": "unknown", "error": str(e)}
    
    def _decode_image(self, image_data):
        """Decode base64 image to OpenCV format"""
        if not HAS_CV2 or not HAS_PIL:
            return None
        
        try:
            if ',' in image_data:
                image_data = image_data.split(',')[1]
            
            img_bytes = base64.b64decode(image_data)
            img = Image.open(io.BytesIO(img_bytes))
            img = img.convert('RGB')
            return cv2.cvtColor(np.array(img), cv2.COLOR_RGB2BGR)
        except:
            return None
    
    def _detect_text_regions(self, gray):
        """Detect text-like regions"""
        if not HAS_CV2:
            return False
        
        blur = cv2.GaussianBlur(gray, (5, 5), 0)
        edges = cv2.Canny(blur, 50, 150)
        contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        text_like = 0
        for contour in contours:
            x, y, w, h = cv2.boundingRect(contour)
            aspect = w / h if h > 0 else 0
            if 0.1 < aspect < 10 and w > 10 and h > 5:
                text_like += 1
        
        return text_like > 10
    
    def _get_color_ranges(self):
        """HSV color ranges for detection"""
        return {
            "red": ([0, 120, 70], [10, 255, 255]),
            "green": ([36, 100, 100], [86, 255, 255]),
            "blue": ([100, 100, 100], [130, 255, 255]),
            "yellow": ([20, 100, 100], [35, 255, 255]),
            "white": ([0, 0, 200], [180, 30, 255]),
            "black": ([0, 0, 0], [180, 255, 50])
        }


class GestureProcessor:
    """Gesture recognition using basic motion detection"""
    
    def __init__(self):
        self.prev_frame = None
    
    def detect_gesture(self, frame_data):
        """Detect motion-based gestures"""
        if not HAS_CV2:
            return {"gesture": "none", "confidence": 0, "mode": "basic"}
        
        try:
            img = self._decode_image(frame_data)
            if img is None:
                return {"gesture": "none", "error": "Invalid frame"}
            
            gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
            gray = cv2.GaussianBlur(gray, (21, 21), 0)
            
            if self.prev_frame is None:
                self.prev_frame = gray
                return {"gesture": "none", "confidence": 0, "mode": "waiting"}
            
            frame_diff = cv2.absdiff(self.prev_frame, gray)
            thresh = cv2.threshold(frame_diff, 25, 255, cv2.THRESH_BINARY)[1]
            thresh = cv2.dilate(thresh, None, iterations=2)
            
            contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            
            gesture = "none"
            confidence = 0
            
            for contour in contours:
                if cv2.contourArea(contour) < 500:
                    continue
                
                x, y, w, h = cv2.boundingRect(contour)
                center_x = x + w // 2
                center_y = y + h // 2
                frame_center_x = gray.shape[1] // 2
                frame_center_y = gray.shape[0] // 2
                
                dx = center_x - frame_center_x
                dy = center_y - frame_center_y
                
                if abs(dx) > abs(dy):
                    gesture = "swipe_right" if dx > 0 else "swipe_left"
                else:
                    gesture = "swipe_down" if dy > 0 else "swipe_up"
                
                confidence = min(cv2.contourArea(contour) / 10000, 1.0)
                break
            
            self.prev_frame = gray
            return {"gesture": gesture, "confidence": round(confidence, 2), "mode": "motion"}
        except Exception as e:
            return {"gesture": "none", "error": str(e)}
    
    def _decode_image(self, image_data):
        """Decode base64 image"""
        if not HAS_CV2 or not HAS_PIL:
            return None
        try:
            if ',' in image_data:
                image_data = image_data.split(',')[1]
            img_bytes = base64.b64decode(image_data)
            img = Image.open(io.BytesIO(img_bytes))
            img = img.convert('RGB')
            return cv2.cvtColor(np.array(img), cv2.COLOR_RGB2BGR)
        except:
            return None


class MLServer:
    """WebSocket server for ML processing"""
    
    def __init__(self, host='localhost', port=8765):
        self.host = host
        self.port = port
        self.vision = VisionProcessor()
        self.gesture = GestureProcessor()
        self.clients = set()
    
    async def handler(self, websocket, path=None):
        """Handle WebSocket connections"""
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
        """Process incoming messages"""
        action = data.get('action', '')
        
        if action == 'detect_faces':
            return self.vision.detect_faces(data.get('image', ''))
        
        elif action == 'detect_objects':
            return self.vision.detect_objects(data.get('image', ''))
        
        elif action == 'analyze_screen':
            return self.vision.analyze_screen(data.get('image', ''))
        
        elif action == 'detect_gesture':
            return self.gesture.detect_gesture(data.get('frame', ''))
        
        elif action == 'ping':
            return {"pong": True, "capabilities": self.get_capabilities()}
        
        else:
            return {"error": f"Unknown action: {action}"}
    
    def get_capabilities(self):
        """Return available capabilities"""
        return {
            "vision": HAS_CV2 and HAS_PIL,
            "numpy": HAS_NUMPY,
            "face_detection": self.vision.face_cascade is not None,
            "gesture_detection": HAS_CV2,
            "screen_analysis": HAS_CV2,
            "mode": "full" if HAS_CV2 else "basic"
        }
    
    async def start(self):
        """Start the WebSocket server"""
        print(f"Starting Ozayn ML Server on ws://{self.host}:{self.port}")
        print(f"Capabilities: {json.dumps(self.get_capabilities(), indent=2)}")
        
        async with websockets.serve(self.handler, self.host, self.port):
            await asyncio.Future()  # Run forever


def main():
    """Main entry point"""
    host = os.environ.get('ML_HOST', 'localhost')
    port = int(os.environ.get('ML_PORT', 8765))
    
    server = MLServer(host, port)
    asyncio.run(server.start())


if __name__ == '__main__':
    main()
