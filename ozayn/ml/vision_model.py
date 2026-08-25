#!/usr/bin/env python3
"""
Ozayn Vision Model - Enhanced Screen Understanding
Provides structured screen descriptions using OpenCV analysis
"""

import numpy as np
import cv2
import base64
import io
import json
from PIL import Image


class VisionModel:
    """Enhanced vision model for screen understanding and scene analysis"""

    def __init__(self):
        self.face_cascade = None
        self.eye_cascade = None
        self.body_cascade = None
        self._load_cascades()

    def _load_cascades(self):
        cascade_path = cv2.data.haarcascades
        try:
            face_file = f"{cascade_path}/haarcascade_frontalface_default.xml"
            eye_file = f"{cascade_path}/haarcascade_eye.xml"
            body_file = f"{cascade_path}/haarcascade_fullbody.xml"

            if cv2.os.path.exists(face_file):
                self.face_cascade = cv2.CascadeClassifier(face_file)
            if cv2.os.path.exists(eye_file):
                self.eye_cascade = cv2.CascadeClassifier(eye_file)
            if cv2.os.path.exists(body_file):
                self.body_cascade = cv2.CascadeClassifier(body_file)
        except Exception:
            pass

    def decode_image(self, image_data):
        if ',' in image_data:
            image_data = image_data.split(',')[1]
        try:
            img_bytes = base64.b64decode(image_data)
            img = Image.open(io.BytesIO(img_bytes))
            img = img.convert('RGB')
            return cv2.cvtColor(np.array(img), cv2.COLOR_RGB2BGR)
        except Exception:
            return None

    def understand_screen(self, image_data):
        img = self.decode_image(image_data)
        if img is None:
            return {"error": "Invalid image", "type": "unknown"}

        height, width = img.shape[:2]
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)

        brightness = float(np.mean(gray))
        contrast = float(np.std(gray))
        saturation = float(np.mean(hsv[:, :, 1]))

        color_analysis = self._analyze_colors(hsv, img)
        layout = self._analyze_layout(gray)
        regions = self._detect_ui_regions(gray)
        elements = self._detect_ui_elements(gray)
        faces = self._detect_faces(gray)
        text_density = self._estimate_text_density(gray)
        screen_type = self._classify_screen_type(
            brightness, contrast, saturation, color_analysis,
            layout, elements, faces, text_density
        )

        return {
            "type": screen_type,
            "resolution": f"{width}x{height}",
            "brightness": round(brightness, 1),
            "contrast": round(contrast, 1),
            "saturation": round(saturation, 1),
            "colors": color_analysis,
            "layout": layout,
            "regions": regions,
            "elements": elements,
            "faces": faces,
            "text_density": round(text_density, 3),
            "description": self._generate_description(
                screen_type, width, height, brightness,
                color_analysis, elements, faces, text_density
            )
        }

    def _analyze_colors(self, hsv, bgr):
        h, s, v = cv2.split(hsv)
        height, width = h.shape
        total_pixels = height * width

        color_ranges = {
            "red": {"lower": np.array([0, 70, 50]), "upper": np.array([10, 255, 255])},
            "orange": {"lower": np.array([10, 70, 50]), "upper": np.array([25, 255, 255])},
            "yellow": {"lower": np.array([25, 70, 50]), "upper": np.array([35, 255, 255])},
            "green": {"lower": np.array([35, 70, 50]), "upper": np.array([85, 255, 255])},
            "blue": {"lower": np.array([85, 70, 50]), "upper": np.array([130, 255, 255])},
            "purple": {"lower": np.array([130, 70, 50]), "upper": np.array([170, 255, 255])},
        }

        color_percentages = {}
        for name, bounds in color_ranges.items():
            mask = cv2.inRange(hsv, bounds["lower"], bounds["upper"])
            count = cv2.countNonZero(mask)
            pct = (count / total_pixels) * 100
            if pct > 1:
                color_percentages[name] = round(pct, 1)

        avg_bgr = cv2.mean(bgr)[:3]
        dominant_bgr = tuple(int(c) for c in avg_bgr)

        sorted_colors = sorted(color_percentages.items(), key=lambda x: x[1], reverse=True)

        return {
            "dominant": sorted_colors[0][0] if sorted_colors else "unknown",
            "dominant_rgb": {
                "b": dominant_bgr[0], "g": dominant_bgr[1], "r": dominant_bgr[2]
            },
            "distribution": dict(sorted_colors),
            "is_dark": float(np.mean(v)) < 100
        }

    def _analyze_layout(self, gray):
        edges = cv2.Canny(gray, 50, 150)
        lines = cv2.HoughLinesP(edges, 1, np.pi / 180, 100, minLineLength=100, maxLineGap=10)

        h_lines = 0
        v_lines = 0
        if lines is not None:
            for line in lines:
                x1, y1, x2, y2 = line[0]
                angle = abs(np.arctan2(y2 - y1, x2 - x1) * 180 / np.pi)
                if angle < 10 or angle > 170:
                    h_lines += 1
                elif 80 < angle < 100:
                    v_lines += 1

        height, width = gray.shape
        blocks = 8
        block_h = height // blocks
        block_w = width // blocks
        brightness_grid = []

        for by in range(blocks):
            row = []
            for bx in range(blocks):
                block = gray[by * block_h:(by + 1) * block_h, bx * block_w:(bx + 1) * block_w]
                row.append(float(np.mean(block)))
            brightness_grid.append(row)

        has_sidebar = False
        has_header = False
        has_footer = False

        left_avg = np.mean([row[0] for row in brightness_grid])
        right_avg = np.mean([row[-1] for row in brightness_grid])
        top_avg = np.mean(brightness_grid[0])
        bottom_avg = np.mean(brightness_grid[-1])
        center_avg = np.mean([brightness_grid[i][j] for i in range(2, 6) for j in range(2, 6)])

        if abs(left_avg - center_avg) > 30 or abs(right_avg - center_avg) > 30:
            has_sidebar = True
        if abs(top_avg - center_avg) > 20:
            has_header = True
        if abs(bottom_avg - center_avg) > 20:
            has_footer = True

        return {
            "horizontal_lines": h_lines,
            "vertical_lines": v_lines,
            "has_sidebar": has_sidebar,
            "has_header": has_header,
            "has_footer": has_footer,
            "complexity": round((h_lines + v_lines) / (width * height / 10000), 3),
            "grid": brightness_grid
        }

    def _detect_ui_regions(self, gray):
        height, width = gray.shape
        block_size = 32
        regions = []

        for by in range(0, height - block_size, block_size):
            for bx in range(0, width - block_size, block_size):
                block = gray[by:by + block_size, bx:bx + block_size]
                avg = float(np.mean(block))
                std = float(np.std(block))

                if std > 30:
                    regions.append({
                        "x": bx, "y": by,
                        "width": block_size, "height": block_size,
                        "brightness": round(avg, 1),
                        "contrast": round(std, 1),
                        "type": "high_detail"
                    })

        merged = self._merge_regions(regions)
        return merged[:20]

    def _merge_regions(self, regions):
        if not regions:
            return []

        merged = []
        used = set()

        for i, r1 in enumerate(regions):
            if i in used:
                continue

            group = [r1]
            for j, r2 in enumerate(regions):
                if j in used or i == j:
                    continue

                if (abs(r1["x"] - r2["x"]) < 64 and abs(r1["y"] - r2["y"]) < 64):
                    group.append(r2)
                    used.add(j)

            if len(group) >= 2:
                xs = [r["x"] for r in group]
                ys = [r["y"] for r in group]
                merged.append({
                    "x": min(xs), "y": min(ys),
                    "width": max(r["x"] + r["width"] for r in group) - min(xs),
                    "height": max(r["y"] + r["height"] for r in group) - min(ys),
                    "area": len(group),
                    "type": "ui_region"
                })
                used.add(i)

        return merged

    def _detect_ui_elements(self, gray):
        edges = cv2.Canny(gray, 50, 150)
        contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        elements = []
        for contour in contours:
            area = cv2.contourArea(contour)
            if area < 500:
                continue

            x, y, w, h = cv2.boundingRect(contour)
            aspect = w / h if h > 0 else 0
            peri = cv2.arcLength(contour, True)
            approx = cv2.approxPolyDP(contour, 0.02 * peri, True)
            vertices = len(approx)

            elem_type = "unknown"
            if 0.8 < aspect < 1.2 and vertices > 4:
                elem_type = "button"
            elif aspect > 3 and h < 30:
                elem_type = "text_line"
            elif aspect > 5 and h < 20:
                elem_type = "separator"
            elif vertices == 4:
                elem_type = "rectangle"
            elif vertices == 3:
                elem_type = "triangle"

            confidence = min(0.95, area / 10000)
            elements.append({
                "type": elem_type,
                "bbox": {"x": int(x), "y": int(y), "width": int(w), "height": int(h)},
                "aspect_ratio": round(aspect, 2),
                "vertices": vertices,
                "confidence": round(confidence, 2)
            })

        return elements[:30]

    def _detect_faces(self, gray):
        if self.face_cascade is None:
            return []

        faces = self.face_cascade.detectMultiScale(gray, 1.3, 5)
        result = []
        for (x, y, w, h) in faces:
            entry = {
                "bbox": {"x": int(x), "y": int(y), "width": int(w), "height": int(h)},
                "confidence": 0.95
            }
            if self.eye_cascade:
                face_roi = gray[y:y + h, x:x + w]
                eyes = self.eye_cascade.detectMultiScale(face_roi)
                entry["eyes"] = len(eyes)
            result.append(entry)

        return result

    def _estimate_text_density(self, gray):
        blur = cv2.GaussianBlur(gray, (5, 5), 0)
        edges = cv2.Canny(blur, 50, 150)
        contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        text_like = 0
        total = len(contours)

        for contour in contours:
            x, y, w, h = cv2.boundingRect(contour)
            aspect = w / h if h > 0 else 0
            if 0.1 < aspect < 10 and w > 10 and h > 5 and h < 50:
                text_like += 1

        return text_like / max(total, 1)

    def _classify_screen_type(self, brightness, contrast, saturation, colors, layout, elements, faces, text_density):
        if faces:
            return "video_call"
        if brightness < 40:
            return "terminal"
        if brightness < 80 and colors.get("is_dark"):
            if text_density > 0.3:
                return "code_editor"
            return "dark_application"
        if text_density > 0.4:
            return "document"
        if layout["horizontal_lines"] > 5 or layout["vertical_lines"] > 5:
            return "spreadsheet"
        if len(elements) > 10:
            return "application"
        if layout["has_header"] and layout["has_sidebar"]:
            return "dashboard"
        return "desktop"

    def _generate_description(self, screen_type, width, height, brightness, colors, elements, faces, text_density):
        descriptions = {
            "video_call": f"Video call interface with {len(faces)} person(s) detected. Resolution {width}x{height}.",
            "terminal": f"Terminal/command line interface. Dark theme with low brightness ({brightness:.0f}/255).",
            "code_editor": f"Code editor with dark theme. High text density ({text_density:.1%}). Dominant color: {colors.get('dominant', 'unknown')}.",
            "document": f"Document/text content with high text density ({text_density:.1%}). Resolution {width}x{height}.",
            "application": f"Application interface with {len(elements)} UI elements detected. Resolution {width}x{height}.",
            "dashboard": f"Dashboard interface with header and sidebar layout. {len(elements)} elements detected.",
            "spreadsheet": f"Spreadsheet/table interface with grid structure. Resolution {width}x{height}.",
            "desktop": f"Desktop/home screen. Resolution {width}x{height}. Brightness: {brightness:.0f}/255.",
            "dark_application": f"Dark-themed application. Resolution {width}x{height}. Brightness: {brightness:.0f}/255."
        }

        return descriptions.get(screen_type, f"Screen content detected. Resolution {width}x{height}.")

    def analyze_camera_frame(self, image_data):
        img = self.decode_image(image_data)
        if img is None:
            return {"error": "Invalid image"}

        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        result = {
            "faces": self._detect_faces(gray),
            "brightness": float(np.mean(gray)),
            "has_motion": self._detect_motion(gray)
        }
        return result

    def _detect_motion(self, gray):
        edges = cv2.Canny(gray, 50, 150)
        edge_count = cv2.countNonZero(edges)
        total = gray.shape[0] * gray.shape[1]
        return (edge_count / total) > 0.05
