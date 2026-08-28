"""
Ozayn Hand Tracker — Motion + Skin hybrid.
Motion: detects intentional movement (ignores static face).
Skin: pinpoints hand position when still.
Only cursor moves when USER is actively controlling.
"""

import cv2
import numpy as np


class SkinHandTracker:
    """Hybrid motion+skin tracker. Returns normalized (0-1) cursor position."""

    def __init__(self):
        self._prev_gray = None
        self._sx = None
        self._sy = None
        self._lost_count = 0
        self._motion_active = False   # True when user is actively moving
        self._motion_frames = 0       # consecutive frames with motion

    def _skin_centroid(self, frame):
        """Find skin blob centroid. Returns (nx, ny, area) or None."""
        h, w = frame.shape[:2]
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(hsv, np.array([0, 10, 50]), np.array([50, 255, 255]))
        mask2 = cv2.inRange(hsv, np.array([160, 10, 50]), np.array([180, 255, 255]))
        mask = cv2.bitwise_or(mask, mask2)

        # YCrCb backup
        ycrcb = cv2.cvtColor(frame, cv2.COLOR_BGR2YCrCb)
        ymask = cv2.inRange(ycrcb, np.array([0, 133, 77]), np.array([255, 173, 127]))
        mask = cv2.bitwise_or(mask, ymask)

        mask = cv2.erode(mask, np.ones((3, 3), np.uint8), iterations=1)
        mask = cv2.dilate(mask, np.ones((5, 5), np.uint8), iterations=2)

        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        if not contours:
            return None

        biggest = max(contours, key=cv2.contourArea)
        area = cv2.contourArea(biggest)
        if area < 600:
            return None

        M = cv2.moments(biggest)
        if M["m00"] == 0:
            return None

        cx = M["m10"] / M["m00"] / w
        cy = M["m01"] / M["m00"] / h

        # Reject top 40% (face)
        if cy < 0.40:
            return None

        return cx, cy, area

    def detect(self, frame):
        """
        Returns (nx, ny, area) or (None, None, 0).
        Motion decides IF we track. Skin decides WHERE.
        """
        h, w = frame.shape[:2]
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        gray = cv2.GaussianBlur(gray, (21, 21), 0)

        # ── Motion detection ──
        has_motion = False
        motion_cx, motion_cy = 0.5, 0.5

        if self._prev_gray is not None:
            diff = cv2.absdiff(self._prev_gray, gray)
            _, motion_mask = cv2.threshold(diff, 20, 255, cv2.THRESH_BINARY)
            motion_mask = cv2.erode(motion_mask, np.ones((5, 5), np.uint8), iterations=1)
            motion_mask = cv2.dilate(motion_mask, np.ones((7, 7), np.uint8), iterations=2)

            motion_contours, _ = cv2.findContours(motion_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            if motion_contours:
                biggest_motion = max(motion_contours, key=cv2.contourArea)
                if cv2.contourArea(biggest_motion) > 300:
                    has_motion = True
                    M = cv2.moments(biggest_motion)
                    if M["m00"] > 0:
                        motion_cx = M["m10"] / M["m00"] / w
                        motion_cy = M["m01"] / M["m00"] / h

        self._prev_gray = gray

        # ── Track motion state ──
        if has_motion:
            self._motion_frames = min(self._motion_frames + 1, 10)
        else:
            self._motion_frames = max(self._motion_frames - 1, 0)

        self._motion_active = self._motion_frames >= 2

        # ── Skin detection (for precise position) ──
        skin = self._skin_centroid(frame)

        # ── Decision logic ──
        if self._motion_active and skin:
            # Both motion AND skin detected — best case, use skin position
            cx, cy, area = skin
            self._lost_count = 0
        elif self._motion_active:
            # Motion but no skin (maybe glove/dark hand) — use motion centroid
            cx, cy = motion_cx, motion_cy
            area = 500
            self._lost_count = 0
        elif skin:
            # Skin but no motion — user is holding still, keep last position
            if self._sx is not None:
                return self._sx, self._sy, 0
            return None, None, 0
        else:
            # Nothing detected
            self._lost_count += 1
            if self._lost_count < 10 and self._sx is not None:
                return self._sx, self._sy, 0
            return None, None, 0

        # ── Smooth output ──
        if self._sx is None:
            self._sx = cx
            self._sy = cy
        else:
            a = 0.35
            self._sx = self._sx * (1 - a) + cx * a
            self._sy = self._sy * (1 - a) + cy * a

        return self._sx, self._sy, area

    def reset(self):
        self._prev_gray = None
        self._sx = None
        self._sy = None
        self._lost_count = 0
        self._motion_frames = 0
        self._motion_active = False
