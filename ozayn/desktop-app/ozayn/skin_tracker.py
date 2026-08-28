"""
Ozayn Hand Tracker — Stable cursor control from hand position.
Only moves cursor when hand area is large enough and position changes significantly.
"""

import cv2
import numpy as np


class SkinHandTracker:
    """Tracks hand using skin color detection. Stable cursor output."""

    def __init__(self):
        self._smooth_x = None
        self._smooth_y = None
        self._last_move_x = None
        self._last_move_y = None
        self._no_hand_count = 0

    def detect(self, frame):
        """
        Detect hand in frame.
        Returns (index_x, index_y, contour) or (None, None, None).
        Coordinates are normalized 0-1.
        """
        h, w = frame.shape[:2]
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

        # Wide skin ranges
        mask1 = cv2.inRange(hsv, np.array([0, 5, 40]), np.array([30, 255, 255]))
        mask2 = cv2.inRange(hsv, np.array([170, 5, 40]), np.array([180, 255, 255]))
        mask = cv2.bitwise_or(mask1, mask2)

        # Brightness filter
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        bright_mask = cv2.inRange(gray, 60, 230)
        mask = cv2.bitwise_and(mask, bright_mask)

        # Light cleanup
        kernel = np.ones((3, 3), np.uint8)
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel, iterations=1)
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel, iterations=2)

        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        if not contours:
            self._no_hand_count += 1
            # Keep last position for a few frames (avoid cursor jump on brief loss)
            if self._no_hand_count < 5 and self._smooth_x is not None:
                return self._smooth_x, self._smooth_y, None
            self._smooth_x = None
            self._smooth_y = None
            return None, None, None

        self._no_hand_count = 0
        biggest = max(contours, key=cv2.contourArea)
        area = cv2.contourArea(biggest)

        # Minimum area: ignore small noise blobs
        if area < 5000:
            if self._smooth_x is not None:
                return self._smooth_x, self._smooth_y, None
            return None, None, None

        # Get hull and find fingertip
        hull = cv2.convexHull(biggest)
        hull_pts = hull.reshape(-1, 2)

        # Find the topmost hull point (index fingertip when pointing up)
        topmost_idx = hull_pts[:, 1].argmin()
        fingertip = hull_pts[topmost_idx]

        # Normalize
        nx = fingertip[0] / w
        ny = fingertip[1] / h

        # Deadzone: don't update if position barely changed from LAST output
        if self._smooth_x is not None:
            dx = abs(nx - self._smooth_x)
            dy = abs(ny - self._smooth_y)
            if dx < 0.03 and dy < 0.03:
                return self._smooth_x, self._smooth_y, biggest

        self._last_move_x = nx
        self._last_move_y = ny

        # Smooth with very low alpha for maximum stability
        if self._smooth_x is None:
            self._smooth_x = nx
            self._smooth_y = ny
        else:
            alpha = 0.15
            self._smooth_x = self._smooth_x * (1 - alpha) + nx * alpha
            self._smooth_y = self._smooth_y * (1 - alpha) + ny * alpha

        return self._smooth_x, self._smooth_y, biggest

    def reset(self):
        self._smooth_x = None
        self._smooth_y = None
        self._last_move_x = None
        self._last_move_y = None
        self._no_hand_count = 0
