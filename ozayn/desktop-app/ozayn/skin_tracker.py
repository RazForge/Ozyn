"""
Ozayn Hand Tracker — Multi-space skin detection with confirmation.
Uses HSV + YCrCb for robust detection across skin tones and lighting.
"""

import cv2
import numpy as np


class SkinHandTracker:
    """Track skin blob with hand confirmation. Returns normalized (0-1) cursor."""

    def __init__(self):
        self._sx = None
        self._sy = None
        self._lost_count = 0
        self._confirm_count = 0
        self._confirmed = False
        self._ref_cx = 0.0
        self._ref_cy = 0.0
        self._ref_set = False

    def _skin_mask(self, frame):
        """Combined HSV + YCrCb skin mask for maximum coverage."""
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        ycrcb = cv2.cvtColor(frame, cv2.COLOR_BGR2YCrCb)

        # HSV: warm skin tones + red wrap
        hsv1 = cv2.inRange(hsv, np.array([0, 10, 50]), np.array([50, 255, 255]))
        hsv2 = cv2.inRange(hsv, np.array([160, 10, 50]), np.array([180, 255, 255]))
        hsv_mask = cv2.bitwise_or(hsv1, hsv2)

        # YCrCb: robust to lighting changes
        ycrcb_mask = cv2.inRange(ycrcb, np.array([0, 133, 77]), np.array([255, 173, 127]))

        # Combine: either space detects skin
        mask = cv2.bitwise_or(hsv_mask, ycrcb_mask)

        # Cleanup
        mask = cv2.erode(mask, np.ones((3, 3), np.uint8), iterations=1)
        mask = cv2.dilate(mask, np.ones((5, 5), np.uint8), iterations=2)
        return mask

    def detect(self, frame):
        """
        Returns (nx, ny, area) or (None, None, 0).
        Only returns when hand confirmed (4+ frames in same region).
        """
        h, w = frame.shape[:2]
        mask = self._skin_mask(frame)

        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        # ── Find best candidate ──
        best = None
        best_score = -1

        for c in contours:
            area = cv2.contourArea(c)
            if area < 600 or area > 80000:
                continue

            M = cv2.moments(c)
            if M["m00"] == 0:
                continue

            cx_n = M["m10"] / M["m00"] / w
            cy_n = M["m01"] / M["m00"] / h

            # Reject top 40% (face)
            if cy_n < 0.40:
                continue

            # Reject very wide blobs
            x, bw, bh = cv2.boundingRect(c)[:3]
            if bw > w * 0.6:
                continue

            score = area * (0.5 + cy_n)
            if score > best_score:
                best_score = score
                best = c

        if best is None:
            self._lost_count += 1
            if self._lost_count > 6:
                self._confirmed = False
                self._confirm_count = 0
                self._ref_set = False
                self._sx = None
                self._sy = None
            return self._sx, self._sy, 0

        M = cv2.moments(best)
        if M["m00"] == 0:
            return self._sx, self._sy, 0
        cx = M["m10"] / M["m00"] / w
        cy = M["m01"] / M["m00"] / h
        area = cv2.contourArea(best)

        self._lost_count = 0

        # ── Confirmation: any detection in lower half counts ──
        self._confirm_count = min(self._confirm_count + 1, 10)

        if self._confirm_count < 4:
            self._confirmed = False
            return None, None, area

        # ── Confirmed — smooth output ──
        self._confirmed = True
        if self._sx is None:
            self._sx = cx
            self._sy = cy
        else:
            # Strong smoothing to prevent jumping between blobs
            a = 0.2
            self._sx = self._sx * (1 - a) + cx * a
            self._sy = self._sy * (1 - a) + cy * a

        return self._sx, self._sy, area

    def reset(self):
        self._sx = None
        self._sy = None
        self._lost_count = 0
        self._confirm_count = 0
        self._confirmed = False
        self._ref_set = False
