"""
Ozayn Hand Tracker — Motion + Skin hybrid.
Cursor ROCK SOLID when still. Follows hand when moving.
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
        self._motion_frames = 0

    def _skin_centroid(self, frame):
        """Find skin blob in lower half. Returns (nx, ny, area) or None."""
        h, w = frame.shape[:2]

        # Method 1: HSV (warm tones)
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        mask1 = cv2.inRange(hsv, np.array([0, 5, 40]), np.array([55, 255, 255]))
        mask2 = cv2.inRange(hsv, np.array([155, 5, 40]), np.array([180, 255, 255]))
        hsv_mask = cv2.bitwise_or(mask1, mask2)

        # Method 2: YCrCb (wide range for ALL skin tones)
        ycrcb = cv2.cvtColor(frame, cv2.COLOR_BGR2YCrCb)
        ycrcb_mask = cv2.inRange(ycrcb, np.array([0, 100, 60]), np.array([255, 180, 140]))

        # Method 3: Excess red — R > G and R > B (works for skin in any color space)
        b, g, r = cv2.split(frame)
        red_mask = cv2.inRange(r.astype(np.int16) - g.astype(np.int16), 15, 255)
        red_mask2 = cv2.inRange(r.astype(np.int16) - b.astype(np.int16), 15, 255)
        red_mask = cv2.bitwise_and(red_mask, red_mask2)

        # Combine all three
        mask = cv2.bitwise_or(hsv_mask, cv2.bitwise_or(ycrcb_mask, red_mask))

        mask = cv2.erode(mask, np.ones((3, 3), np.uint8), iterations=1)
        mask = cv2.dilate(mask, np.ones((5, 5), np.uint8), iterations=2)

        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        if not contours:
            return None

        biggest = max(contours, key=cv2.contourArea)
        area = cv2.contourArea(biggest)
        if area < 500:
            return None

        M = cv2.moments(biggest)
        if M["m00"] == 0:
            return None

        cx = M["m10"] / M["m00"] / w
        cy = M["m01"] / M["m00"] / h

        if cy < 0.40:
            return None

        return cx, cy, area

    def detect(self, frame):
        """
        Returns (nx, ny, area) or last known position.
        ALWAYS returns a position once tracking starts — never jumps to None.
        """
        h, w = frame.shape[:2]
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        gray = cv2.GaussianBlur(gray, (15, 15), 0)

        # ── Motion detection ──
        has_motion = False

        if self._prev_gray is not None:
            diff = cv2.absdiff(self._prev_gray, gray)

            # Low threshold — camera has tiny frame-to-frame differences
            _, motion_mask = cv2.threshold(diff, 12, 255, cv2.THRESH_BINARY)

            # Cleanup small noise but keep hand-sized regions
            motion_mask = cv2.erode(motion_mask, np.ones((3, 3), np.uint8), iterations=1)
            motion_mask = cv2.dilate(motion_mask, np.ones((5, 5), np.uint8), iterations=2)

            motion_contours, _ = cv2.findContours(
                motion_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

            if motion_contours:
                biggest_motion = max(motion_contours, key=cv2.contourArea)
                motion_area = cv2.contourArea(biggest_motion)
                # Hand-sized motion (>= 50 pixels), not tiny noise
                if motion_area > 50:
                    has_motion = True

        self._prev_gray = gray

        # ── Track motion state with hysteresis ──
        if has_motion:
            self._motion_frames = min(self._motion_frames + 1, 15)
        else:
            self._motion_frames = max(self._motion_frames - 1, 0)

        motion_on = self._motion_frames >= 2

        # ── Skin detection ──
        skin = self._skin_centroid(frame)

        # ── Decision: where to point cursor ──
        if motion_on:
            # User IS moving — find where hand is
            if skin:
                cx, cy, area = skin
            else:
                # No skin blob but motion detected — hold last position
                if self._sx is not None:
                    return self._sx, self._sy, 0
                return None, None, 0

            self._lost_count = 0
        else:
            # User is STILL — cursor must NOT move
            if self._sx is not None:
                return self._sx, self._sy, 0
            # First ever frame — try skin to get initial position
            if skin:
                cx, cy, area = skin
                self._sx = cx
                self._sy = cy
                return self._sx, self._sy, area
            return None, None, 0

        # ── Smooth output ──
        if self._sx is None:
            self._sx = cx
            self._sy = cy
        else:
            a = 0.3
            self._sx = self._sx * (1 - a) + cx * a
            self._sy = self._sy * (1 - a) + cy * a

        return self._sx, self._sy, area

    def reset(self):
        self._prev_gray = None
        self._sx = None
        self._sy = None
        self._lost_count = 0
        self._motion_frames = 0
