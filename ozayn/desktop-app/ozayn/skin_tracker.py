"""
Ozayn Skin Hand Tracker — One system to rule them all.
Tracks skin-colored blob center of mass. Moves cursor. Done.
"""

import cv2
import numpy as np


class SkinHandTracker:
    """Track skin blob centroid. Returns normalized (0-1) cursor position."""

    def __init__(self):
        self._sx = None
        self._sy = None
        self._lost_count = 0
        self._prev_t = 0

    def detect(self, frame):
        """Returns (nx, ny, area) or (None, None, 0). nx,ny are 0-1 normalized."""
        h, w = frame.shape[:2]
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

        # Wide skin mask — ALL skin tones
        mask = cv2.inRange(hsv, np.array([0, 15, 40]), np.array([40, 255, 255]))
        mask2 = cv2.inRange(hsv, np.array([165, 15, 40]), np.array([180, 255, 255]))
        mask = cv2.bitwise_or(mask, mask2)

        # Cleanup
        mask = cv2.erode(mask, np.ones((5, 5), np.uint8), iterations=1)
        mask = cv2.dilate(mask, np.ones((7, 7), np.uint8), iterations=2)

        # Find contours
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        if not contours:
            self._lost_count += 1
            if self._lost_count < 10 and self._sx is not None:
                return self._sx, self._sy, 0
            self._sx = None
            self._sy = None
            return None, None, 0

        # Pick best blob: prefer lower half (hands), reject top 35% (faces)
        best = None
        best_score = -1
        for c in contours:
            area = cv2.contourArea(c)
            if area < 600:
                continue
            M = cv2.moments(c)
            if M["m00"] == 0:
                continue
            cy = M["m01"] / M["m00"] / h  # 0=top, 1=bottom
            # Reject if center is in top 35% — that's the face
            if cy < 0.35:
                continue
            # Score: prefer blobs in the middle-bottom (where hands are)
            score = area * (0.5 + cy)
            if score > best_score:
                best_score = score
                best = c

        if best is None:
            self._lost_count += 1
            if self._lost_count < 10 and self._sx is not None:
                return self._sx, self._sy, 0
            self._sx = None
            self._sy = None
            return None, None, 0

        self._lost_count = 0
        area = cv2.contourArea(best)

        # Centroid — smooth and stable
        M = cv2.moments(best)
        if M["m00"] == 0:
            return self._sx, self._sy, area
        cx = M["m10"] / M["m00"] / w
        cy = M["m01"] / M["m00"] / h

        # Smooth
        if self._sx is None:
            self._sx = cx
            self._sy = cy
        else:
            a = 0.4
            self._sx = self._sx * (1 - a) + cx * a
            self._sy = self._sy * (1 - a) + cy * a

        return self._sx, self._sy, area

    def reset(self):
        self._sx = None
        self._sy = None
        self._lost_count = 0
