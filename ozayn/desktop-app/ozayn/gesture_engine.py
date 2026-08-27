"""
Ozayn Gesture Engine — Full hand gesture command system.
Velocity-based acceleration, speed modes, action gestures.
"""

import math
import time


class GestureEngine:
    """
    Processes hand landmarks into commands.

    TRACK → MOVE → ACCELERATE → CLICK → DRAG → SCROLL → WINDOW CONTROL → SYSTEM COMMAND
    """

    # Speed modes
    MODE_FINE = "FINE"
    MODE_NORMAL = "NORMAL"
    MODE_FAST = "FAST"
    MODE_TURBO = "TURBO"
    MODE_LOCKED = "LOCKED"

    # Gesture states
    GESTURE_NONE = "NONE"
    GESTURE_POINTER = "POINTER"
    GESTURE_PINCH = "PINCH"
    GESTURE_PINCH_HOLD = "PINCH_HOLD"
    GESTURE_TWO_FINGERS = "TWO_FINGERS"
    GESTURE_OPEN_PALM = "OPEN_PALM"
    GESTURE_FIST = "FIST"
    GESTURE_THUMB_UP = "THUMB_UP"
    GESTURE_THUMB_DOWN = "THUMB_DOWN"
    GESTURE_SWIPE = "SWIPE"
    GESTURE_ZOOM = "ZOOM"
    GESTURE_ROCK = "ROCK"
    GESTURE_TWO_FINGERS = "TWO_FINGERS"

    def __init__(self):
        # Hand tracking state
        self._prev_index = None
        self._prev_time = None
        self._prev_thumb = None
        self._prev_index_2 = None  # for zoom

        # Cursor state
        self._cursor_x = 0.0
        self._cursor_y = 0.0
        self._velocity_x = 0.0
        self._velocity_y = 0.0

        # Mode
        self._mode = self.MODE_NORMAL
        self._locked = False

        # Gesture detection
        self._current_gesture = self.GESTURE_NONE
        self._gesture_start_time = 0.0
        self._pinch_hold_start = 0.0
        self._is_dragging = False
        self._is_scroll_lock = False

        # Swipe detection
        self._swipe_positions = []
        self._swipe_time_window = 0.4  # seconds

        # Zoom detection
        self._zoom_start_dist = 0.0

        # Dead zone
        self._dead_zone = 0.008

        # Speed settings per mode
        self._speed_config = {
            self.MODE_FINE:   {"base": 0.5,  "accel": 0.0,  "max": 1.0},
            self.MODE_NORMAL: {"base": 1.0,  "accel": 0.3,  "max": 3.0},
            self.MODE_FAST:   {"base": 2.5,  "accel": 1.5,  "max": 8.0},
            self.MODE_TURBO:  {"base": 5.0,  "accel": 4.0,  "max": 20.0},
            self.MODE_LOCKED: {"base": 0.0,  "accel": 0.0,  "max": 0.0},
        }

    @property
    def mode(self):
        return self._mode

    @property
    def gesture(self):
        return self._current_gesture

    @property
    def is_dragging(self):
        return self._is_dragging

    @property
    def is_locked(self):
        return self._locked

    def set_mode(self, mode):
        if mode in self._speed_config:
            self._mode = mode

    def cycle_mode(self):
        """Cycle through speed modes."""
        modes = [self.MODE_FINE, self.MODE_NORMAL, self.MODE_FAST, self.MODE_TURBO]
        idx = modes.index(self._mode) if self._mode in modes else 1
        self._mode = modes[(idx + 1) % len(modes)]
        return self._mode

    def process(self, hand_landmarks, screen_w, screen_h):
        """
        Process hand landmarks and return command dict.

        Returns:
            dict with keys:
                cursor_x, cursor_y (absolute screen coords)
                click, right_click, drag_start, drag_end
                scroll_delta (negative=up, positive=down)
                zoom_delta (positive=in, negative=out)
                swipe_direction ("left","right","up","down") or None
                gesture (string)
                mode (string)
                locked (bool)
        """
        now = time.time()
        result = {
            "cursor_x": None,
            "cursor_y": None,
            "click": False,
            "right_click": False,
            "drag_start": False,
            "drag_end": False,
            "scroll_delta": 0,
            "zoom_delta": 0,
            "swipe": None,
            "gesture": self.GESTURE_NONE,
            "mode": self._mode,
            "locked": self._locked,
        }

        if not hand_landmarks:
            self._prev_index = None
            self._prev_time = None
            self._current_gesture = self.GESTURE_NONE
            if self._is_dragging:
                self._is_dragging = False
                result["drag_end"] = True
            return result

        lm = hand_landmarks

        # ── Extract key landmarks ──
        # Wrist (0), Index tip (8), Middle tip (12), Ring tip (16), Pinky tip (20)
        # Thumb tip (4), Index MCP (5), Index PIP (6), Index DIP (7)
        # Middle MCP (9), Ring MCP (13), Pinky MCP (17)
        wrist = lm[0]
        thumb_tip = lm[4]
        thumb_ip = lm[3]
        thumb_mcp = lm[2]
        index_tip = lm[8]
        index_pip = lm[6]
        index_mcp = lm[5]
        middle_tip = lm[12]
        middle_pip = lm[10]
        middle_mcp = lm[9]
        ring_tip = lm[16]
        ring_pip = lm[14]
        ring_mcp = lm[13]
        pinky_tip = lm[20]
        pinky_pip = lm[18]
        pinky_mcp = lm[17]

        # ── Detect current gesture ──
        gesture = self._detect_gesture(
            thumb_tip, thumb_ip, thumb_mcp,
            index_tip, index_pip, index_mcp,
            middle_tip, middle_pip, middle_mcp,
            ring_tip, ring_pip, ring_mcp,
            pinky_tip, pinky_pip, pinky_mcp,
            wrist
        )

        self._current_gesture = gesture
        result["gesture"] = gesture
        result["mode"] = self._mode
        result["locked"] = self._locked

        # ── Handle lock/unlock ──
        if gesture == self.GESTURE_FIST:
            self._locked = True
            self._mode = self.MODE_LOCKED
            return result
        elif gesture == self.GESTURE_OPEN_PALM and self._locked:
            self._locked = False
            self._mode = self.MODE_NORMAL
            return result

        if self._locked:
            return result

        # ── Handle mode switch gestures ──
        if gesture == self.GESTURE_ROCK:
            # Thumb + pinky → TURBO mode
            self._mode = self.MODE_TURBO
        elif gesture == self.GESTURE_TWO_FINGERS:
            # Two fingers → FAST mode
            self._mode = self.MODE_FAST
        elif gesture == self.GESTURE_POINTER:
            # Index only → NORMAL
            self._mode = self.MODE_NORMAL

        # ── Index finger position for cursor ──
        index_x = index_tip.x
        index_y = index_tip.y

        # ── FREEZE cursor during action gestures (click, right-click, etc.) ──
        # Only move cursor in POINTER mode (index finger up, no pinch)
        is_pointing = (gesture == self.GESTURE_POINTER)

        if is_pointing and self._prev_index is not None and self._prev_time is not None:
            # ── Calculate delta (velocity-based) ──
            dt = now - self._prev_time
            if dt > 0:
                dx = index_x - self._prev_index[0]
                dy = index_y - self._prev_index[1]
                dist = math.sqrt(dx * dx + dy * dy)

                # ── Dead zone ──
                if dist < self._dead_zone:
                    dx, dy = 0.0, 0.0
                else:
                    # ── Velocity ──
                    vx = dx / dt
                    vy = dy / dt
                    speed = math.sqrt(vx * vx + vy * vy)

                    # ── Acceleration curve ──
                    cfg = self._speed_config[self._mode]
                    base = cfg["base"]
                    accel = cfg["accel"]
                    max_speed = cfg["max"]

                    multiplier = base * (1.0 + accel * min(speed, 50.0) / 50.0)
                    multiplier = min(multiplier, max_speed)

                    dx = dx * multiplier
                    dy = dy * multiplier

                    # Smooth velocity
                    self._velocity_x = self._velocity_x * 0.6 + dx * 0.4
                    self._velocity_y = self._velocity_y * 0.6 + dy * 0.4

                    dx = self._velocity_x
                    dy = self._velocity_y

                # Map to screen coordinates (mirror X)
                self._cursor_x = (1.0 - index_x) * screen_w - dx * screen_w
                self._cursor_y = index_y * screen_h + dy * screen_h

                # Clamp to screen
                self._cursor_x = max(0, min(screen_w - 1, self._cursor_x))
                self._cursor_y = max(0, min(screen_h - 1, self._cursor_y))

                result["cursor_x"] = int(self._cursor_x)
                result["cursor_y"] = int(self._cursor_y)
        elif is_pointing:
            # First frame pointing — set initial position, no movement
            self._cursor_x = (1.0 - index_x) * screen_w
            self._cursor_y = index_y * screen_h
            result["cursor_x"] = int(self._cursor_x)
            result["cursor_y"] = int(self._cursor_y)

        # Always update prev_index for next frame (even during pinch)
        self._prev_index = (index_x, index_y)
        self._prev_time = now

        # ── Action gestures ──

        # Pinch → Left click
        if gesture == self.GESTURE_PINCH:
            if not self._is_dragging and self._pinch_hold_start == 0.0:
                self._pinch_hold_start = now
                result["click"] = True
            elif (now - self._pinch_hold_start) > 0.3 and self._pinch_hold_start > 0:
                # Hold → drag
                if not self._is_dragging:
                    self._is_dragging = True
                    result["drag_start"] = True
        elif gesture == self.GESTURE_TWO_FINGERS and self._prev_time and (now - self._gesture_start_time) > 0.1:
            # Two fingers → Right click (brief)
            if self._current_gesture != getattr(self, '_prev_action_gesture', None):
                result["right_click"] = True
        else:
            if self._is_dragging:
                self._is_dragging = False
                result["drag_end"] = True
            self._pinch_hold_start = 0.0

        self._prev_action_gesture = self._current_gesture

        # ── Swipe detection ──
        self._swipe_positions.append((index_x, index_y, now))
        # Keep only recent positions
        self._swipe_positions = [(x, y, t) for x, y, t in self._swipe_positions if now - t < self._swipe_time_window]

        if len(self._swipe_positions) >= 5:
            first = self._swipe_positions[0]
            last = self._swipe_positions[-1]
            sdx = last[0] - first[0]
            sdy = last[1] - first[1]
            sdist = math.sqrt(sdx * sdx + sdy * sdy)

            if sdist > 0.15:  # significant swipe
                angle = math.atan2(sdy, sdx)
                if abs(sdx) > abs(sdy):
                    result["swipe"] = "right" if sdx < 0 else "left"  # mirror
                else:
                    result["swipe"] = "down" if sdy > 0 else "up"
                self._swipe_positions.clear()

        # ── Scroll via vertical swipe with open palm ──
        if gesture == self.GESTURE_OPEN_PALM and len(self._swipe_positions) >= 3:
            first = self._swipe_positions[0]
            last = self._swipe_positions[-1]
            sdy = last[1] - first[1]
            if abs(sdy) > 0.05:
                result["scroll_delta"] = int(sdy * 500)

        # ── Zoom via two-finger spread/pinch ──
        if gesture == self.GESTURE_TWO_FINGERS:
            # Distance between index and middle tips
            d = math.sqrt(
                (index_tip.x - middle_tip.x) ** 2 +
                (index_tip.y - middle_tip.y) ** 2
            )
            if self._zoom_start_dist == 0.0:
                self._zoom_start_dist = d
            else:
                delta = d - self._zoom_start_dist
                if abs(delta) > 0.02:
                    result["zoom_delta"] = int(delta * 200)
                    self._zoom_start_dist = d
        else:
            self._zoom_start_dist = 0.0

        # ── Thumb up → confirm, Thumb down → cancel ──
        if gesture == self.GESTURE_THUMB_UP:
            result["click"] = True  # confirm
        elif gesture == self.GESTURE_THUMB_DOWN:
            result["right_click"] = True  # cancel

        # ── Rock (thumb+pinky) → home ──
        # Handled via mode switch above

        return result

    def _detect_gesture(self, thumb_tip, thumb_ip, thumb_mcp,
                         index_tip, index_pip, index_mcp,
                         middle_tip, middle_pip, middle_mcp,
                         ring_tip, ring_pip, ring_mcp,
                         pinky_tip, pinky_pip, pinky_mcp, wrist):
        """Detect which gesture is being performed."""

        # ── Finger extension checks ──
        index_up = index_tip.y < index_pip.y
        middle_up = middle_tip.y < middle_pip.y
        ring_up = ring_tip.y < ring_pip.y
        pinky_up = pinky_tip.y < pinky_pip.y
        thumb_up = thumb_tip.y < thumb_ip.y

        # ── Pinch: thumb + index close ──
        pinch_dist = math.sqrt(
            (thumb_tip.x - index_tip.x) ** 2 +
            (thumb_tip.y - index_tip.y) ** 2
        )
        is_pinch = pinch_dist < 0.06

        # ── Open palm: all fingers up ──
        is_palm = index_up and middle_up and ring_up and pinky_up

        # ── Fist: all fingers down ──
        is_fist = not index_up and not middle_up and not ring_up and not pinky_up

        # ── Two fingers: index + middle up, ring + pinky down ──
        is_two = index_up and middle_up and not ring_up and not pinky_up

        # ── Pointer: only index up ──
        is_pointer = index_up and not middle_up and not ring_up and not pinky_up

        # ── Thumb up: thumb up, all others down ──
        is_thumb_up = thumb_up and not index_up and not middle_up and not ring_up and not pinky_up
        # Refine: thumb must be significantly above wrist
        if is_thumb_up:
            is_thumb_up = thumb_tip.y < wrist.y - 0.15

        # ── Thumb down: thumb down, all others up (or most up) ──
        is_thumb_down = not thumb_up and index_up and middle_up and ring_up and pinky_up
        if is_thumb_down:
            is_thumb_down = thumb_tip.y > wrist.y + 0.1

        # ── Rock: thumb + pinky up, others down ──
        is_rock = thumb_up and pinky_up and not index_up and not middle_up and not ring_up

        # ── Priority order ──
        if is_fist:
            return self.GESTURE_FIST
        if is_palm:
            return self.GESTURE_OPEN_PALM
        if is_pinch:
            return self.GESTURE_PINCH
        if is_two:
            return self.GESTURE_two_fingers
        if is_rock:
            return self.GESTURE_ROCK
        if is_thumb_up:
            return self.GESTURE_THUMB_UP
        if is_thumb_down:
            return self.GESTURE_THUMB_DOWN
        if is_pointer:
            return self.GESTURE_POINTER

        return self.GESTURE_NONE

    def reset(self):
        """Reset all tracking state."""
        self._prev_index = None
        self._prev_time = None
        self._prev_thumb = None
        self._velocity_x = 0.0
        self._velocity_y = 0.0
        self._current_gesture = self.GESTURE_NONE
        self._is_dragging = False
        self._pinch_hold_start = 0.0
        self._swipe_positions.clear()
        self._zoom_start_dist = 0.0
        self._locked = False
        self._mode = self.MODE_NORMAL
