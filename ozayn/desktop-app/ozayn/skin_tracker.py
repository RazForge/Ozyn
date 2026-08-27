"""
Ozayn Hand Tracker — Motion + contour based hand tracking.
Works with low-saturation cameras. Finds the largest moving skin blob
and tracks its topmost point as the index fingertip.
"""

import cv2
import numpy as np


class SkinHandTracker:
    """Tracks hand using background subtraction + skin color."""

    def __init__(self):
        self._bg_subtractor = cv2.createBackgroundSubtractorMOG2(
            history=500, varThreshold=50, detectShadows=False)
        self._smooth_x = None
        self._smooth_y = None
        self._frame_count = 0

    def detect(self, frame):
        """
        Detect hand in frame.
        Returns (index_x, index_y, contour) or (None, None, None).
        Coordinates are normalized 0-1.
        """
        self._frame_count += 1
        h, w = frame.shape[:2]
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

        # Very wide skin color ranges (works with low saturation)
        # Range 1: H 0-30 (covers warm tones)
        mask1 = cv2.inRange(hsv, np.array([0, 5, 40]), np.array([30, 255, 255]))
        # Range 2: Hue near 0 (wrap around for very red skin)
        mask2 = cv2.inRange(hsv, np.array([170, 5, 40]), np.array([180, 255, 255]))
        mask = cv2.bitwise_or(mask1, mask2)

        # Also try grayscale brightness filter for very desaturated images
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        bright_mask = cv2.inRange(gray, 60, 230)
        mask = cv2.bitwise_and(mask, bright_mask)

        # Clean up — light morph to keep skin blob
        kernel = np.ones((3, 3), np.uint8)
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel, iterations=1)
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel, iterations=2)
        mask = cv2.GaussianBlur(mask, (5, 5), 0)
        _, mask = cv2.threshold(mask, 127, 255, cv2.THRESH_BINARY)

        # Find contours
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        if not contours:
            self._smooth_x = None
            self._smooth_y = None
            return None, None, None

        # Get the largest contour
        biggest = max(contours, key=cv2.contourArea)
        area = cv2.contourArea(biggest)

        if area < 2000:
            self._smooth_x = None
            self._smooth_y = None
            return None, None, None

        # Get hull and find topmost point (fingertip)
        hull = cv2.convexHull(biggest)
        hull_pts = hull.reshape(-1, 2)

        # Topmost point = lowest y value = fingertip when pointing
        topmost_idx = hull_pts[:, 1].argmin()
        fingertip = hull_pts[topmost_idx]

        # Also get the centroid for reference
        M = cv2.moments(biggest)
        if M["m00"] > 0:
            centroid_x = int(M["m10"] / M["m00"])
            centroid_y = int(M["m01"] / M["m00"])
        else:
            centroid_x, centroid_y = fingertip

        # Use the point that is highest (lowest y) and near the center-x
        # This is more robust than just the absolute topmost
        center_x = w // 2
        best_point = fingertip
        min_score = float('inf')
        for pt in hull_pts:
            # Score: y position + distance from center
            score = pt[1] + abs(pt[0] - center_x) * 0.3
            if score < min_score:
                min_score = score
                best_point = pt

        # Normalize to 0-1
        nx = best_point[0] / w
        ny = best_point[1] / h

        # Smooth
        if self._smooth_x is None:
            self._smooth_x = nx
            self._smooth_y = ny
        else:
            alpha = 0.5
            self._smooth_x = self._smooth_x * (1 - alpha) + nx * alpha
            self._smooth_y = self._smooth_y * (1 - alpha) + ny * alpha

        return self._smooth_x, self._smooth_y, biggest

    def reset(self):
        self._smooth_x = None
        self._smooth_y = None
