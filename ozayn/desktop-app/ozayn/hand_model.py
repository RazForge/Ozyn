"""
Ozayn Hand Model — Geometric representation of hand anatomy.
Joint angles, finger states, distances, orientations.
Used by the gesture classifier for structured gesture recognition.
"""

import math
from dataclasses import dataclass, field
from typing import List, Optional


@dataclass
class Joint:
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0

    def distance_to(self, other: 'Joint') -> float:
        return math.sqrt(
            (self.x - other.x) ** 2 +
            (self.y - other.y) ** 2 +
            (self.z - other.z) ** 2
        )

    def angle_to(self, other: 'Joint') -> float:
        dx = other.x - self.x
        dy = other.y - self.y
        return math.atan2(dy, dx)


@dataclass
class Finger:
    """Represents one finger with its joints and computed state."""
    name: str
    mcp: Joint = field(default_factory=Joint)
    pip: Joint = field(default_factory=Joint)
    dip: Joint = field(default_factory=Joint)
    tip: Joint = field(default_factory=Joint)

    # Computed properties
    is_extended: bool = False
    is_flexed: bool = False
    bend_angle: float = 0.0       # Angle at PIP joint (0 = straight, 90+ = bent)
    extension_score: float = 0.0  # 0.0 = fully flexed, 1.0 = fully extended


@dataclass
class HandModel:
    """Complete geometric model of a single hand."""
    wrist: Joint = field(default_factory=Joint)
    palm_center: Joint = field(default_factory=Joint)

    thumb: Finger = field(default_factory=lambda: Finger("thumb"))
    index: Finger = field(default_factory=lambda: Finger("index"))
    middle: Finger = field(default_factory=lambda: Finger("middle"))
    ring: Finger = field(default_factory=lambda: Finger("ring"))
    pinky: Finger = field(default_factory=lambda: Finger("pinky"))

    # Hand-level properties
    palm_orientation: float = 0.0   # Angle of palm normal (radians)
    hand_orientation: float = 0.0   # Angle from wrist to middle MCP (radians)
    hand_velocity: float = 0.0      # Magnitude of velocity vector
    hand_acceleration: float = 0.0  # Change in velocity

    # Confidence
    confidence: float = 0.0

    @property
    def fingers(self) -> List[Finger]:
        return [self.thumb, self.index, self.middle, self.ring, self.pinky]

    @property
    def extended_fingers(self) -> List[Finger]:
        return [f for f in self.fingers if f.is_extended]

    @property
    def flexed_fingers(self) -> List[Finger]:
        return [f for f in self.fingers if f.is_flexed]

    @property
    def extended_count(self) -> int:
        return len(self.extended_fingers)

    @property
    def is_open_palm(self) -> bool:
        return self.extended_count >= 4

    @property
    def is_fist(self) -> bool:
        return self.extended_count == 0

    def finger_distance(self, f1: Finger, f2: Finger) -> float:
        return f1.tip.distance_to(f2.tip)

    def thumb_index_distance(self) -> float:
        return self.thumb.tip.distance_to(self.index.tip)


def build_hand_model(landmarks, timestamp_ms: float = 0) -> HandModel:
    """
    Build a HandModel from MediaPipe hand landmarks.

    MediaPipe hand landmark indices:
    0: WRIST
    1: THUMB_CMC, 2: THUMB_MCP, 3: THUMB_IP, 4: THUMB_TIP
    5: INDEX_MCP, 6: INDEX_PIP, 7: INDEX_DIP, 8: INDEX_TIP
    9: MIDDLE_MCP, 10: MIDDLE_PIP, 11: MIDDLE_DIP, 12: MIDDLE_TIP
    13: RING_MCP, 14: RING_PIP, 15: RING_DIP, 16: RING_TIP
    17: PINKY_MCP, 18: PINKY_PIP, 19: PINKY_DIP, 20: PINKY_TIP
    """
    hand = HandModel()

    def j(idx):
        pt = landmarks[idx]
        return Joint(x=pt.x, y=pt.y, z=getattr(pt, 'z', 0.0))

    # Wrist & palm
    hand.wrist = j(0)
    hand.palm_center = Joint(
        x=(landmarks[0].x + landmarks[9].x) / 2,
        y=(landmarks[0].y + landmarks[9].y) / 2,
        z=(getattr(landmarks[0], 'z', 0) + getattr(landmarks[9], 'z', 0)) / 2
    )

    # Fingers
    for finger, mcp_i, pip_i, dip_i, tip_i in [
        (hand.thumb, 1, 2, 3, 4),
        (hand.index, 5, 6, 7, 8),
        (hand.middle, 9, 10, 11, 12),
        (hand.ring, 13, 14, 15, 16),
        (hand.pinky, 17, 18, 19, 20),
    ]:
        finger.mcp = j(mcp_i)
        finger.pip = j(pip_i)
        finger.dip = j(dip_i)
        finger.tip = j(tip_i)

    # Compute finger states
    _compute_finger_states(hand)

    # Hand orientation
    hand.hand_orientation = hand.wrist.angle_to(hand.middle.mcp)

    # Palm orientation (approximate from cross product of wrist->middle and wrist->index)
    dx1 = hand.middle.mcp.x - hand.wrist.x
    dy1 = hand.middle.mcp.y - hand.wrist.y
    dx2 = hand.index.mcp.x - hand.wrist.x
    dy2 = hand.index.mcp.y - hand.wrist.y
    hand.palm_orientation = math.atan2(dy1 * dx2 - dx1 * dy2, dx1 * dx2 + dy1 * dy2)

    return hand


def _compute_finger_states(hand: HandModel):
    """Determine if each finger is extended or flexed based on joint angles."""
    for finger in hand.fingers:
        # Skip thumb — different anatomy
        if finger.name == "thumb":
            # Thumb extended: IP joint angle is large (nearly straight)
            dx = finger.tip.x - finger.mcp.x
            dy = finger.tip.y - finger.mcp.y
            tip_dist = math.sqrt(dx * dx + dy * dy)

            dx2 = finger.ip.x - finger.mcp.x
            dy2 = finger.ip.y - finger.mcp.y
            ip_dist = math.sqrt(dx2 * dx2 + dy2 * dy2)

            finger.extension_score = tip_dist / max(ip_dist, 0.001) if ip_dist > 0.01 else 0.0
            finger.extension_score = min(1.0, finger.extension_score)

            # Thumb is extended if tip is far from palm center
            thumb_tip_dist = finger.tip.distance_to(hand.palm_center)
            index_mcp_dist = hand.index.mcp.distance_to(hand.palm_center)
            finger.is_extended = thumb_tip_dist > index_mcp_dist * 0.8
            finger.is_flexed = not finger.is_extended

            # Bend angle from MCP
            finger.bend_angle = _angle_between(finger.mcp, finger.ip, finger.tip)
            continue

        # For other fingers: angle at PIP joint
        finger.bend_angle = _angle_between(finger.mcp, finger.pip, finger.dip)

        # Extended: PIP angle > 150 degrees (nearly straight)
        # Also check: tip y < pip y (higher on screen = more extended, for typical hand pose)
        tip_above_pip = finger.tip.y < finger.pip.y
        tip_above_mcp = finger.tip.y < finger.mcp.y

        if finger.bend_angle > 150 and tip_above_pip:
            finger.is_extended = True
            finger.is_flexed = False
            finger.extension_score = min(1.0, (finger.bend_angle - 140) / 40)
        elif finger.bend_angle < 100 or not tip_above_mcp:
            finger.is_extended = False
            finger.is_flexed = True
            finger.extension_score = max(0.0, finger.bend_angle / 140)
        else:
            # Ambiguous zone
            finger.is_extended = finger.bend_angle > 130
            finger.is_flexed = finger.bend_angle < 120
            finger.extension_score = finger.bend_angle / 180


def _angle_between(a: Joint, b: Joint, c: Joint) -> float:
    """Calculate angle at point b formed by a-b-c. Returns degrees."""
    dx1 = a.x - b.x
    dy1 = a.y - b.y
    dx2 = c.x - b.x
    dy2 = c.y - b.y

    dot = dx1 * dx2 + dy1 * dy2
    mag1 = math.sqrt(dx1 * dx1 + dy1 * dy1)
    mag2 = math.sqrt(dx2 * dx2 + dy2 * dy2)

    if mag1 < 1e-6 or mag2 < 1e-6:
        return 0.0

    cos_angle = max(-1.0, min(1.0, dot / (mag1 * mag2)))
    return math.degrees(math.acos(cos_angle))
