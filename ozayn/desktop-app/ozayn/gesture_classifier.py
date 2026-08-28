"""
Ozayn Gesture Classifier — Classifies hand gestures from geometric measurements.
Structured pipeline: Detection → Classification → State → Command → Action.
"""

import time
import math
from dataclasses import dataclass, field
from typing import Optional, Tuple, List

from ozayn.hand_model import HandModel, Finger


# ── Gesture Definitions ──────────────────────────────────────────────────────

class Gesture:
    NONE = "NONE"
    # Single-hand
    POINTER = "POINTER"
    FAST_POINTER = "FAST_POINTER"
    PRECISION_POINTER = "PRECISION_POINTER"
    OPEN_PALM = "OPEN_PALM"
    PINCH = "PINCH"
    TWO_FINGERS = "TWO_FINGERS"
    THUMB_UP = "THUMB_UP"
    THUMB_DOWN = "THUMB_DOWN"
    FIST = "FIST"
    INDEX_MIDDLE_V = "INDEX_MIDDLE_V"
    THREE_FINGERS = "THREE_FINGERS"
    FOUR_FINGERS = "FOUR_FINGERS"
    INDEX_PINKY = "INDEX_PINKY"
    INDEX_THUMB_POINT = "INDEX_THUMB_POINT"
    # Movement
    SWIPE_RIGHT = "SWIPE_RIGHT"
    SWIPE_LEFT = "SWIPE_LEFT"
    SWIPE_UP = "SWIPE_UP"
    SWIPE_DOWN = "SWIPE_DOWN"
    # Two-hand
    TWO_HAND_OPEN = "TWO_HAND_OPEN"
    HANDS_APART = "HANDS_APART"
    HANDS_TOGETHER = "HANDS_TOGETHER"
    BOTH_FIST = "BOTH_FIST"
    BOTH_THUMBS_UP = "BOTH_THUMBS_UP"
    BOTH_THUMBS_DOWN = "BOTH_THUMBS_DOWN"


# ── Command Output ───────────────────────────────────────────────────────────

@dataclass
class GestureCommand:
    gesture: str = Gesture.NONE
    confidence: float = 0.0
    cursor_x: Optional[int] = None
    cursor_y: Optional[int] = None
    click: bool = False
    right_click: bool = False
    drag_start: bool = False
    drag_end: bool = False
    scroll_delta: int = 0
    zoom_delta: int = 0
    swipe: Optional[str] = None
    mode: str = "NORMAL"
    locked: bool = False
    # Dwell-to-click
    dwell_progress: float = 0.0   # 0.0 to 1.0
    dwell_click: bool = False     # True when dwell completes
    dwell_active: bool = False    # True when dwell timer is running
    # Cursor gear (0=STOP 1=PRECISION 2=NORMAL 3=FAST 4=TURBO)
    cursor_gear: int = 2


# ── Gesture State Machine ────────────────────────────────────────────────────

@dataclass
class GestureState:
    """Tracks gesture stability over time for confirmation."""
    gesture: str = Gesture.NONE
    first_seen: float = 0.0
    last_seen: float = 0.0
    frame_count: int = 0
    confidence_sum: float = 0.0
    confirmed: bool = False

    @property
    def avg_confidence(self) -> float:
        return self.confidence_sum / max(1, self.frame_count)

    @property
    def duration_ms(self) -> float:
        return (self.last_seen - self.first_seen) * 1000

    def update(self, gesture: str, confidence: float, now: float):
        if gesture == self.gesture:
            self.frame_count += 1
            self.last_seen = now
            self.confidence_sum += confidence
        else:
            self.gesture = gesture
            self.first_seen = now
            self.last_seen = now
            self.frame_count = 1
            self.confidence_sum = confidence
            self.confirmed = False

    def is_stable(self, min_frames: int = 3, min_ms: float = 100) -> bool:
        return (self.frame_count >= min_frames and
                self.duration_ms >= min_ms and
                self.avg_confidence >= 0.7)


# ── Main Classifier ──────────────────────────────────────────────────────────

class GestureClassifier:
    """
    Classifies hand gestures from geometric hand model data.
    Uses structured hand measurements, not emoji or heuristics.
    """

    # Confirmation thresholds
    CLICK_CONFIRM_FRAMES = 3
    CLICK_CONFIRM_MS = 100
    DRAG_CONFIRM_FRAMES = 5
    DRAG_CONFIRM_MS = 200
    SWIPE_MIN_VELOCITY = 0.15
    SWIPE_CONFIRM_MS = 150

    def __init__(self):
        self._state = GestureState()
        self._prev_state = GestureState()
        self._prev_hand: Optional[HandModel] = None
        self._prev_time: float = 0.0
        self._swipe_start_pos: Optional[Tuple[float, float]] = None
        self._swipe_start_time: float = 0.0

        # Cursor state
        self._cursor_x: float = 0.0
        self._cursor_y: float = 0.0
        self._velocity_x: float = 0.0
        self._velocity_y: float = 0.0
        self._is_dragging: bool = False
        self._is_locked: bool = False
        self._gesture_mode: str = "NORMAL"

        # Dwell-to-click state
        self._dwell_start_x: float = 0.0
        self._dwell_start_y: float = 0.0
        self._dwell_start_time: float = 0.0
        self._dwell_threshold_px: float = 15.0  # pixels of movement before reset
        self._dwell_duration_s: float = 2.0     # seconds to trigger click

    @property
    def is_locked(self) -> bool:
        return self._is_locked

    @property
    def is_dragging(self) -> bool:
        return self._is_dragging

    @property
    def gesture(self) -> str:
        return self._state.gesture

    @property
    def mode(self) -> str:
        return self._gesture_mode

    def classify(self, hand: HandModel, screen_w: int, screen_h: int) -> GestureCommand:
        """
        Classify gesture from hand model and produce command.
        Main entry point for single-hand gesture recognition.
        """
        now = time.time()
        cmd = GestureCommand()

        if self._is_locked:
            cmd.locked = True
            cmd.gesture = Gesture.FIST
            # Check for unlock (fist → open palm)
            if hand.is_open_palm:
                if self._prev_hand and self._prev_hand.is_fist:
                    self._is_locked = False
                    cmd.locked = False
            self._prev_hand = hand
            self._prev_time = now
            return cmd

        # ── Classify gesture ──
        gesture, confidence = self._classify_gesture(hand)
        self._state.update(gesture, confidence, now)

        cmd.gesture = gesture
        cmd.confidence = confidence
        cmd.mode = self._gesture_mode

        # ── Cursor control (only for pointer gestures) ──
        if gesture in (Gesture.POINTER, Gesture.FAST_POINTER, Gesture.PRECISION_POINTER):
            cmd.cursor_x, cmd.cursor_y = self._compute_cursor(hand, screen_w, screen_h, gesture)
        elif gesture == Gesture.OPEN_PALM:
            # Stop cursor
            self._velocity_x = 0
            self._velocity_y = 0

        # ── Click detection (confirmed over N frames) ──
        if gesture == Gesture.PINCH:
            if self._state.is_stable(self.CLICK_CONFIRM_FRAMES, self.CLICK_CONFIRM_MS):
                if not self._state.confirmed:
                    # Check if this is a new pinch (not continuation of drag)
                    if not self._is_dragging:
                        cmd.click = True
                        self._state.confirmed = True
                    else:
                        # Was dragging, now pinching = release
                        cmd.drag_end = True
                        self._is_dragging = False
        elif gesture == Gesture.TWO_FINGERS:
            if self._state.is_stable(self.CLICK_CONFIRM_FRAMES, self.CLICK_CONFIRM_MS):
                if not self._state.confirmed:
                    cmd.right_click = True
                    self._state.confirmed = True
        else:
            # Gesture changed — if was pinching, it's a drag
            if self._is_dragging and gesture != Gesture.PINCH:
                cmd.drag_end = True
                self._is_dragging = False

        # ── Drag detection ──
        if gesture == Gesture.PINCH and self._state.frame_count >= self.DRAG_CONFIRM_FRAMES:
            if not self._is_dragging and self._state.duration_ms >= self.DRAG_CONFIRM_MS:
                self._is_dragging = True
                cmd.drag_start = True

        # ── Swipe detection ──
        swipe = self._detect_swipe(hand, now)
        if swipe:
            cmd.swipe = swipe

        # ── Scroll from swipe velocity ──
        if swipe == "up":
            cmd.scroll_delta = -3
        elif swipe == "down":
            cmd.scroll_delta = 3

        # ── Gesture-specific commands ──
        if gesture == Gesture.FIST and self._state.is_stable(5, 300):
            if not self._state.confirmed:
                self._is_locked = True
                cmd.locked = True
                self._state.confirmed = True
        elif gesture == Gesture.THUMB_UP and self._state.is_stable(3, 300):
            cmd.gesture = Gesture.THUMB_UP
        elif gesture == Gesture.THUMB_DOWN and self._state.is_stable(3, 300):
            cmd.gesture = Gesture.THUMB_DOWN

        # ── Dwell-to-click ──
        cx, cy, dwell_p, dwell_click, dwell_active = self._update_dwell(
            cmd.cursor_x, cmd.cursor_y, now)
        cmd.cursor_x = cx
        cmd.cursor_y = cy
        cmd.dwell_progress = dwell_p
        cmd.dwell_click = dwell_click
        cmd.dwell_active = dwell_active

        self._prev_hand = hand
        self._prev_time = now
        return cmd

    def classify_two_hands(self, left: HandModel, right: HandModel,
                           screen_w: int, screen_h: int) -> GestureCommand:
        """
        Classify two-hand gesture and produce command.
        """
        now = time.time()
        cmd = GestureCommand()

        # Calculate inter-hand distances
        left_center = left.palm_center
        right_center = right.palm_center
        hand_distance = left_center.distance_to(right_center)

        left_extended = left.extended_count
        right_extended = right.extended_count

        # Both open palms
        if left.is_open_palm and right.is_open_palm:
            if self._prev_hand is None:
                cmd.gesture = Gesture.TWO_HAND_OPEN
                cmd.confidence = 0.9
            else:
                # Check for apart/together movement
                prev_dist = getattr(self, '_prev_hand_distance', hand_distance)
                delta = hand_distance - prev_dist

                if delta > 0.02:
                    cmd.gesture = Gesture.HANDS_APART
                    cmd.zoom_delta = int(delta * 500)
                    cmd.confidence = 0.85
                elif delta < -0.02:
                    cmd.gesture = Gesture.HANDS_TOGETHER
                    cmd.zoom_delta = int(delta * 500)
                    cmd.confidence = 0.85
                else:
                    cmd.gesture = Gesture.TWO_HAND_OPEN
                    cmd.confidence = 0.9

            self._prev_hand_distance = hand_distance

        # Both fists
        elif left.is_fist and right.is_fist:
            cmd.gesture = Gesture.BOTH_FIST
            cmd.confidence = 0.9

        # Both thumbs up
        elif (left.thumb.is_extended and not left.index.is_extended and
              right.thumb.is_extended and not right.index.is_extended):
            cmd.gesture = Gesture.BOTH_THUMBS_UP
            cmd.confidence = 0.85

        # Both thumbs down
        elif (not left.thumb.is_extended and left.thumb.bend_angle < 60 and
              not right.thumb.is_extended and right.thumb.bend_angle < 60):
            cmd.gesture = Gesture.BOTH_THUMBS_DOWN
            cmd.confidence = 0.85

        else:
            cmd.gesture = Gesture.NONE
            cmd.confidence = 0.0

        cmd.mode = self._gesture_mode
        self._prev_hand = left
        self._prev_time = now
        return cmd

    def _classify_gesture(self, hand: HandModel) -> Tuple[str, float]:
        """Classify single-hand gesture from hand model. Returns (gesture, confidence)."""
        ext = hand.extended_count
        thumb_ext = hand.thumb.is_extended
        index_ext = hand.index.is_extended
        middle_ext = hand.middle.is_extended
        ring_ext = hand.ring.is_extended
        pinky_ext = hand.pinky.is_extended

        thumb_index_dist = hand.thumb_index_distance()

        # ── Open Palm (4+ fingers extended) ──
        if ext >= 4 and thumb_ext:
            return Gesture.OPEN_PALM, 0.9

        # ── Fist (0 fingers extended) ──
        if ext == 0:
            return Gesture.FIST, 0.9

        # ── Pinch (thumb tip near index tip, others flexed) ──
        if thumb_index_dist < 0.06 and not middle_ext and not ring_ext:
            return Gesture.PINCH, min(1.0, 0.8 + (0.06 - thumb_index_dist) * 10)

        # ── Index + Pinky (command mode) ──
        if index_ext and pinky_ext and not middle_ext and not ring_ext:
            return Gesture.INDEX_PINKY, 0.85

        # ── Index + Middle V (task switch) ──
        if index_ext and middle_ext and not ring_ext and not pinky_ext:
            # Check fingertips are separated (V shape)
            dist = hand.index.tip.distance_to(hand.middle.tip)
            if dist > 0.04:
                return Gesture.INDEX_MIDDLE_V, 0.85

        # ── Three fingers (index, middle, ring) ──
        if index_ext and middle_ext and ring_ext and not pinky_ext:
            return Gesture.THREE_FINGERS, 0.8

        # ── Four fingers (no thumb) ──
        if index_ext and middle_ext and ring_ext and pinky_ext and not thumb_ext:
            return Gesture.FOUR_FINGERS, 0.8

        # ── Thumb Up ──
        if thumb_ext and not index_ext and not middle_ext and not ring_ext and not pinky_ext:
            if hand.thumb.tip.y < hand.thumb.mcp.y:
                return Gesture.THUMB_UP, 0.85
            else:
                return Gesture.THUMB_DOWN, 0.85

        # ── Index Pointer (only index extended) ──
        if index_ext and not middle_ext and not ring_ext and not pinky_ext:
            return Gesture.POINTER, 0.9

        return Gesture.NONE, 0.0

    def _compute_cursor(self, hand: HandModel, screen_w: int, screen_h: int,
                        gesture: str) -> Tuple[int, int]:
        """Compute cursor position from hand with dynamic acceleration."""
        # Index fingertip position (mirrored X)
        tip = hand.index.tip
        raw_x = (1.0 - tip.x) * screen_w
        raw_y = tip.y * screen_h

        now = time.time()
        dt = now - self._prev_time if self._prev_time > 0 else 0.033

        if dt <= 0:
            dt = 0.033

        # Velocity from hand movement
        vx = (raw_x - self._cursor_x) / dt if dt > 0 else 0
        vy = (raw_y - self._cursor_y) / dt if dt > 0 else 0

        speed = math.sqrt(vx * vx + vy * vy)

        # ── Dead zone ──
        if speed < 50:
            return int(self._cursor_x), int(self._cursor_y)

        # ── Dynamic acceleration (5 gears) ──
        if gesture == Gesture.PRECISION_POINTER:
            sensitivity = 0.15
            self._gesture_mode = "PRECISION"
        elif gesture == Gesture.FAST_POINTER or speed > 2000:
            sensitivity = 2.5
            self._gesture_mode = "TURBO"
        elif speed > 800:
            sensitivity = 1.8
            self._gesture_mode = "FAST"
        elif speed > 300:
            sensitivity = 1.2
            self._gesture_mode = "NORMAL"
        else:
            sensitivity = 0.6
            self._gesture_mode = "PRECISION"

        # cursor_velocity = hand_velocity × sensitivity
        target_x = self._cursor_x + vx * sensitivity * dt
        target_y = self._cursor_y + vy * sensitivity * dt

        # Smooth (exponential moving average)
        alpha = 0.4
        self._cursor_x = self._cursor_x * (1 - alpha) + target_x * alpha
        self._cursor_y = self._cursor_y * (1 - alpha) + target_y * alpha

        # Clamp to screen
        self._cursor_x = max(0, min(screen_w - 1, self._cursor_x))
        self._cursor_y = max(0, min(screen_h - 1, self._cursor_y))

        return int(self._cursor_x), int(self._cursor_y)

    def _detect_swipe(self, hand: HandModel, now: float) -> Optional[str]:
        """Detect swipe gesture from hand velocity and direction."""
        vel = hand.hand_velocity
        if vel < self.SWIPE_MIN_VELOCITY:
            return None

        if self._prev_hand is None:
            self._swipe_start_pos = (hand.palm_center.x, hand.palm_center.y)
            self._swipe_start_time = now
            return None

        dt = now - self._swipe_start_time
        if dt < 0.05 or dt > 0.5:
            self._swipe_start_pos = (hand.palm_center.x, hand.palm_center.y)
            self._swipe_start_time = now
            return None

        if self._swipe_start_pos is None:
            return None

        dx = hand.palm_center.x - self._swipe_start_pos[0]
        dy = hand.palm_center.y - self._swipe_start_pos[1]

        dist = math.sqrt(dx * dx + dy * dy)
        if dist < 0.08:
            return None

        # Reset after detection
        self._swipe_start_pos = (hand.palm_center.x, hand.palm_center.y)
        self._swipe_start_time = now

        # Note: X is mirrored (camera view)
        if abs(dx) > abs(dy):
            return "right" if dx < 0 else "left"  # Mirrored
        else:
            return "down" if dy > 0 else "up"

    def _detect_zoom(self, hand: HandModel) -> int:
        """Detect zoom in/out from two-finger spread/contract."""
        if self._prev_hand is None:
            return 0

        curr_dist = hand.index.tip.distance_to(hand.middle.tip)
        prev_dist = self._prev_hand.index.tip.distance_to(self._prev_hand.middle.tip)

        delta = curr_dist - prev_dist
        if abs(delta) < 0.005:
            return 0

        return int(delta * 500)

    def _update_dwell(self, cursor_x, cursor_y, now: float):
        """
        Track cursor dwell. If cursor stays in same spot for 2s, trigger click.
        Returns (cursor_x, cursor_y, dwell_progress, dwell_click, dwell_active).
        """
        if cursor_x is None or cursor_y is None:
            self._dwell_start_time = 0
            return cursor_x, cursor_y, 0.0, False, False

        # First call — initialize
        if self._dwell_start_time == 0:
            self._dwell_start_x = cursor_x
            self._dwell_start_y = cursor_y
            self._dwell_start_time = now
            return cursor_x, cursor_y, 0.0, False, False

        # Check if cursor moved beyond threshold
        dx = cursor_x - self._dwell_start_x
        dy = cursor_y - self._dwell_start_y
        dist = math.sqrt(dx * dx + dy * dy)

        if dist > self._dwell_threshold_px:
            # Reset dwell — cursor moved
            self._dwell_start_x = cursor_x
            self._dwell_start_y = cursor_y
            self._dwell_start_time = now
            return cursor_x, cursor_y, 0.0, False, False

        # Cursor is stationary — update dwell
        elapsed = now - self._dwell_start_time
        progress = min(1.0, elapsed / self._dwell_duration_s)

        if progress >= 1.0:
            # Dwell complete — trigger click and reset
            self._dwell_start_x = cursor_x
            self._dwell_start_y = cursor_y
            self._dwell_start_time = now
            return cursor_x, cursor_y, 1.0, True, False

        # Dwell in progress
        active = progress > 0.15  # Show box after 15% (300ms)
        return cursor_x, cursor_y, progress, False, active

    def reset(self):
        """Reset all state."""
        self._prev_hand = None
        self._prev_time = 0
        self._velocity_x = 0
        self._velocity_y = 0
        self._state = GestureState()
        self._swipe_start_pos = None
