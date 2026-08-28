"""
Ozayn Hand Tracker — Fast, reliable skin-color hand tracking.
Primary cursor controller. Works on any camera, any skin tone.
"""

import cv2
import numpy as np


class SkinHandTracker:
    """Tracks hand using skin color detection with convexity defect fingertips."""

    def __init__(self):
        self._smooth_x = None
        self._smooth_y = None
        self._no_hand_count = 0
        self._prev_x = None
        self._prev_y = None
        self._velocity_x = 0.0
        self._velocity_y = 0.0
        # Adaptive HSV ranges (initialized wide, narrows to user's skin)
        self._hsv_center = None
        self._calibrated = False
        self._calib_frames = 0

    def _calibrate(self, hsv_frame, mask):
        """Learn user's skin color from first few frames."""
        if self._calibrated or self._calib_frames > 30:
            self._calibrated = True
            return
        self._calib_frames += 1
        if mask is None:
            return
        pixels = hsv_frame[mask > 0]
        if len(pixels) < 100:
            return
        # Get median HSV of skin pixels
        median = np.median(pixels, axis=0).astype(np.uint8)
        self._hsv_center = median

    def detect(self, frame):
        """
        Detect hand in frame.
        Returns (index_x, index_y, contour) normalized 0-1.
        """
        h, w = frame.shape[:2]
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

        # ── Wide skin color mask (works for ALL skin tones) ──
        # Range 1: hue 0-30 (warm skin tones)
        mask1 = cv2.inRange(hsv, np.array([0, 10, 50]), np.array([35, 255, 255]))
        # Range 2: hue 170-180 (wraps around red)
        mask2 = cv2.inRange(hsv, np.array([170, 10, 50]), np.array([180, 255, 255]))
        # Range 3: wider yellow/brown tones
        mask3 = cv2.inRange(hsv, np.array([5, 20, 60]), np.array([40, 200, 255]))
        mask = cv2.bitwise_or(mask1, cv2.bitwise_or(mask2, mask3))

        # Brightness: allow dark to bright (camera in any lighting)
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        bright_mask = cv2.inRange(gray, 30, 240)
        mask = cv2.bitwise_and(mask, bright_mask)

        # Cleanup noise
        kernel = np.ones((5, 5), np.uint8)
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel, iterations=1)
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel, iterations=2)
        mask = cv2.dilate(mask, kernel, iterations=1)

        # Calibrate on first frames
        self._calibrate(hsv, mask)

        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        if not contours:
            self._no_hand_count += 1
            if self._no_hand_count < 8 and self._smooth_x is not None:
                return self._smooth_x, self._smooth_y, None
            self._smooth_x = None
            self._smooth_y = None
            return None, None, None

        self._no_hand_count = 0

        # Filter: only keep contours in lower 70% of frame (hands, not faces)
        h_frame, w_frame = frame.shape[:2]
        valid_contours = []
        for c in contours:
            area = cv2.contourArea(c)
            if area < 800:
                continue
            M = cv2.moments(c)
            if M["m00"] == 0:
                continue
            cx = int(M["m10"] / M["m00"])
            cy = int(M["m01"] / M["m00"])
            # Reject if center is in top 30% (likely face)
            if cy < h_frame * 0.3:
                continue
            valid_contours.append((c, area, cx, cy))

        if not valid_contours:
            self._no_hand_count += 1
            if self._no_hand_count < 8 and self._smooth_x is not None:
                return self._smooth_x, self._smooth_y, None
            self._smooth_x = None
            self._smooth_y = None
            return None, None, None

        # Use the biggest valid contour
        biggest, area, cx_contour, cy_contour = max(valid_contours, key=lambda x: x[1])

        # ── Fingertip: leftmost point in lower half of contour (index finger when pointing) ──
        hull_pts = cv2.convexHull(biggest).reshape(-1, 2)
        # Get the point that is leftmost in the lower 60% (fingertip when pointing at screen)
        lower_pts = hull_pts[hull_pts[:, 1] > cy_contour - 20]
        if len(lower_pts) > 0:
            # Among lower points, pick the one closest to left edge (pointing direction)
            leftmost_idx = lower_pts[:, 0].argmin()
            fingertip = lower_pts[leftmost_idx]
        else:
            fingertip = hull_pts[hull_pts[:, 1].argmin()]

        nx = fingertip[0] / w_frame
        ny = fingertip[1] / h_frame

        return self._smooth_and_output(nx, ny, biggest)

    def _smooth_and_output(self, nx, ny, contour):
        """Apply deadzone + smoothing and return cursor position."""
        # ── Deadzone: ignore tiny tremors ──
        if self._smooth_x is not None:
            dx = abs(nx - self._smooth_x)
            dy = abs(ny - self._smooth_y)
            if dx < 0.015 and dy < 0.015:
                return self._smooth_x, self._smooth_y, contour

        # ── Velocity-based deadzone: ignore low-velocity jitter ──
        if self._prev_x is not None:
            vx = (nx - self._prev_x)
            vy = (ny - self._prev_y)
            speed = abs(vx) + abs(vy)
            # Smooth velocity
            self._velocity_x = self._velocity_x * 0.5 + vx * 0.5
            self._velocity_y = self._velocity_y * 0.5 + vy * 0.5
            vel_mag = abs(self._velocity_x) + abs(self._velocity_y)
            # If velocity is very low, don't move (hand is still)
            if vel_mag < 0.005:
                return self._smooth_x if self._smooth_x else nx, self._smooth_y if self._smooth_y else ny, contour

        self._prev_x = nx
        self._prev_y = ny

        # ── Smooth with EMA ──
        if self._smooth_x is None:
            self._smooth_x = nx
            self._smooth_y = ny
        else:
            # Adaptive alpha: fast movement = more responsive, slow = smoother
            speed = abs(nx - self._smooth_x) + abs(ny - self._smooth_y)
            if speed > 0.1:
                alpha = 0.5   # Fast movement: responsive
            elif speed > 0.03:
                alpha = 0.3   # Medium: balanced
            else:
                alpha = 0.15  # Slow: very smooth
            self._smooth_x = self._smooth_x * (1 - alpha) + nx * alpha
            self._smooth_y = self._smooth_y * (1 - alpha) + ny * alpha

        return self._smooth_x, self._smooth_y, contour

    def reset(self):
        self._smooth_x = None
        self._smooth_y = None
        self._prev_x = None
        self._prev_y = None
        self._velocity_x = 0.0
        self._velocity_y = 0.0
        self._no_hand_count = 0
