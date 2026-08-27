"""
Ozayn Gesture Engine — Wraps hand_model + gesture_classifier for backward compatibility.
Processes MediaPipe landmarks through the geometric pipeline.
"""

from ozayn.hand_model import build_hand_model, HandModel
from ozayn.gesture_classifier import GestureClassifier, GestureCommand, Gesture


class GestureEngine:
    """Processes hand landmarks and returns gesture commands."""

    GESTURE_NONE = Gesture.NONE
    GESTURE_POINTER = Gesture.POINTER
    GESTURE_PINCH = Gesture.PINCH
    GESTURE_TWO_FINGERS = Gesture.TWO_FINGERS
    GESTURE_OPEN_PALM = Gesture.OPEN_PALM
    GESTURE_FIST = Gesture.FIST
    GESTURE_THUMB_UP = Gesture.THUMB_UP
    GESTURE_THUMB_DOWN = Gesture.THUMB_DOWN
    GESTURE_SWIPE = "SWIPE"

    def __init__(self):
        self._classifier = GestureClassifier()
        self._last_cmd = GestureCommand()

    @property
    def gesture(self):
        return self._last_cmd.gesture

    @property
    def mode(self):
        return self._last_cmd.mode

    @property
    def is_locked(self):
        return self._classifier.is_locked

    @property
    def is_dragging(self):
        return self._classifier.is_dragging

    def process(self, hand_landmarks, screen_w, screen_h):
        """Process single hand landmarks and return command dict."""
        hand = build_hand_model(hand_landmarks)
        cmd = self._classifier.classify(hand, screen_w, screen_h)
        self._last_cmd = cmd
        return self._to_dict(cmd)

    def process_two_hands(self, left_landmarks, right_landmarks, screen_w, screen_h):
        """Process two hands and return command dict."""
        left = build_hand_model(left_landmarks)
        right = build_hand_model(right_landmarks)
        cmd = self._classifier.classify_two_hands(left, right, screen_w, screen_h)
        self._last_cmd = cmd
        return self._to_dict(cmd)

    def _to_dict(self, cmd: GestureCommand) -> dict:
        return {
            "cursor_x": cmd.cursor_x,
            "cursor_y": cmd.cursor_y,
            "click": cmd.click,
            "right_click": cmd.right_click,
            "drag_start": cmd.drag_start,
            "drag_end": cmd.drag_end,
            "scroll_delta": cmd.scroll_delta,
            "zoom_delta": cmd.zoom_delta,
            "swipe": cmd.swipe,
            "gesture": cmd.gesture,
            "mode": cmd.mode,
            "locked": cmd.locked,
        }

    def reset(self):
        self._classifier.reset()
        self._last_cmd = GestureCommand()
