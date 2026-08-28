"""
Ozayn 3D Hand Avatar — Renders a stylized 3D hand from MediaPipe landmarks.
Glass/transparent appearance with depth shading, lighting, and glow.
No real camera footage shown — only the 3D representation.
"""

import math
from PyQt6.QtCore import Qt, QPointF, QRectF
from PyQt6.QtGui import (
    QPainter, QColor, QPen, QBrush, QRadialGradient,
    QLinearGradient, QPainterPath, QConicalGradient,
)


# MediaPipe hand connections (bone pairs)
HAND_CONNECTIONS = [
    (0, 1), (1, 2), (2, 3), (3, 4),       # Thumb
    (0, 5), (5, 6), (6, 7), (7, 8),       # Index
    (0, 9), (9, 10), (10, 11), (11, 12),  # Middle
    (0, 13), (13, 14), (14, 15), (15, 16), # Ring
    (0, 17), (17, 18), (18, 19), (19, 20),# Pinky
    (5, 9), (9, 13), (13, 17),            # Palm
]

# Fingertip indices
FINGERTIPS = [4, 8, 12, 16, 20]
FINGER_NAMES = ["Thumb", "Index", "Middle", "Ring", "Pinky"]


def _lerp_color(c1: QColor, c2: QColor, t: float) -> QColor:
    t = max(0.0, min(1.0, t))
    return QColor(
        int(c1.red() + (c2.red() - c1.red()) * t),
        int(c1.green() + (c2.green() - c1.green()) * t),
        int(c1.blue() + (c2.blue() - c1.blue()) * t),
        int(c1.alpha() + (c2.alpha() - c1.alpha()) * t),
    )


def _depth_shade(z: float, base_color: QColor) -> QColor:
    """Shade color based on depth (z). Negative z = closer = brighter."""
    depth_factor = max(0.5, min(1.5, 1.0 - z * 2.0))
    return QColor(
        min(255, int(base_color.red() * depth_factor)),
        min(255, int(base_color.green() * depth_factor)),
        min(255, int(base_color.blue() * depth_factor)),
        base_color.alpha(),
    )


class HandAvatar3D:
    """Renders a 3D hand skeleton from MediaPipe landmarks."""

    def __init__(self):
        self._smoothed_landmarks = None
        self._smooth_alpha = 0.4

    def render(self, painter: QPainter, landmarks, w: int, h: int,
               gear: int = 2, dwell_progress: float = 0.0,
               dwell_active: bool = False, gesture: str = ""):
        """
        Render the 3D hand avatar onto the QPainter.

        landmarks: list of 21 MediaPipe landmark objects (x, y, z) normalized 0-1
        w, h: widget dimensions
        """
        if not landmarks or len(landmarks) < 21:
            self._draw_idle_hand(painter, w, h)
            return

        # Smooth landmarks for stability
        pts = self._smooth(landmarks, w, h)

        # ── Draw palm fill ──
        self._draw_palm(painter, pts)

        # ── Draw bones ──
        self._draw_bones(painter, pts)

        # ── Draw joints ──
        self._draw_joints(painter, pts)

        # ── Draw fingertips with glow ──
        self._draw_fingertips(painter, pts)

        # ── Draw wrist ring ──
        self._draw_wrist_ring(painter, pts[0])

        # ── Draw dwell thinking ring ──
        if dwell_active:
            self._draw_dwell_ring(painter, pts[8], dwell_progress)

        # ── Draw gesture label ──
        if gesture and gesture != "NONE":
            self._draw_gesture_label(painter, pts[8], gesture, gear)

    def _smooth(self, landmarks, w, h):
        """Convert landmarks to screen coords with smoothing."""
        pts = []
        for i, lm in enumerate(landmarks):
            # Mirror X for natural feel
            x = (1.0 - lm.x) * w
            y = lm.y * h
            z = getattr(lm, 'z', 0.0)

            if self._smoothed_landmarks and i < len(self._smoothed_landmarks):
                sx, sy, sz = self._smoothed_landmarks[i]
                a = self._smooth_alpha
                x = sx + (x - sx) * a
                y = sy + (y - sy) * a
                z = sz + (z - sz) * a

            pts.append((x, y, z))

        self._smoothed_landmarks = [(p[0], p[1], p[2]) for p in pts]
        return pts

    def _draw_palm(self, painter: QPainter, pts):
        """Draw semi-transparent palm polygon with depth gradient."""
        # Palm: wrist(0), index_mcp(5), middle_mcp(9), ring_mcp(13), pinky_mcp(17)
        palm_indices = [0, 5, 9, 13, 17]
        palm_pts = [QPointF(pts[i][0], pts[i][1]) for i in palm_indices]

        # Create gradient across palm
        if len(palm_pts) >= 3:
            grad = QLinearGradient(
                palm_pts[0].x(), palm_pts[0].y(),
                palm_pts[2].x(), palm_pts[2].y()
            )
            grad.setColorAt(0.0, QColor(0, 180, 255, 25))
            grad.setColorAt(0.5, QColor(0, 220, 255, 15))
            grad.setColorAt(1.0, QColor(0, 150, 255, 25))

            path = QPainterPath()
            path.moveTo(palm_pts[0])
            for p in palm_pts[1:]:
                path.lineTo(p)
            path.closeSubpath()

            painter.setBrush(QBrush(grad))
            painter.setPen(Qt.PenStyle.NoPen)
            painter.drawPath(path)

    def _draw_bones(self, painter: QPainter, pts):
        """Draw hand bones with depth-based thickness and color."""
        for a, b in HAND_CONNECTIONS:
            ax, ay, az = pts[a]
            bx, by, bz = pts[b]

            # Average depth
            avg_z = (az + bz) / 2.0
            depth = max(0.4, min(1.2, 1.0 - avg_z * 2))

            # Color: cyan base, brighter when closer
            r = int(0 * depth)
            g = int(200 * depth)
            b_val = int(255 * depth)

            # Glow layer (thicker, transparent)
            glow_pen = QPen(QColor(r, g, b_val, 40))
            glow_pen.setWidthF(5.0 * depth)
            glow_pen.setCapStyle(Qt.PenCapStyle.RoundCap)
            painter.setPen(glow_pen)
            painter.drawLine(QPointF(ax, ay), QPointF(bx, by))

            # Core line (thin, bright)
            core_pen = QPen(QColor(r, g, b_val, 180))
            core_pen.setWidthF(2.0 * depth)
            core_pen.setCapStyle(Qt.PenCapStyle.RoundCap)
            painter.setPen(core_pen)
            painter.drawLine(QPointF(ax, ay), QPointF(bx, by))

    def _draw_joints(self, painter: QPainter, pts):
        """Draw joint spheres with depth shading."""
        for i, (x, y, z) in enumerate(pts):
            if i in FINGERTIPS:
                continue  # Drawn separately with glow

            depth = max(0.4, min(1.2, 1.0 - z * 2))
            size = 4.0 * depth

            # Outer glow
            grad = QRadialGradient(x, y, size * 2)
            grad.setColorAt(0.0, QColor(0, 200, 255, int(80 * depth)))
            grad.setColorAt(0.5, QColor(0, 180, 255, int(30 * depth)))
            grad.setColorAt(1.0, QColor(0, 150, 255, 0))
            painter.setBrush(QBrush(grad))
            painter.setPen(Qt.PenStyle.NoPen)
            painter.drawEllipse(QPointF(x, y), size * 2, size * 2)

            # Inner core
            painter.setBrush(QBrush(QColor(200, 240, 255, int(200 * depth))))
            painter.setPen(Qt.PenStyle.NoPen)
            painter.drawEllipse(QPointF(x, y), size * 0.6, size * 0.6)

    def _draw_fingertips(self, painter: QPainter, pts):
        """Draw fingertips with bright glow and ring."""
        for tip_idx in FINGERTIPS:
            x, y, z = pts[tip_idx]
            depth = max(0.5, min(1.3, 1.0 - z * 2))

            # Large glow
            glow = QRadialGradient(x, y, 18 * depth)
            glow.setColorAt(0.0, QColor(255, 255, 255, int(180 * depth)))
            glow.setColorAt(0.2, QColor(0, 255, 220, int(120 * depth)))
            glow.setColorAt(0.5, QColor(0, 200, 255, int(40 * depth)))
            glow.setColorAt(1.0, QColor(0, 150, 255, 0))
            painter.setBrush(QBrush(glow))
            painter.setPen(Qt.PenStyle.NoPen)
            painter.drawEllipse(QPointF(x, y), 18 * depth, 18 * depth)

            # Bright core
            painter.setBrush(QBrush(QColor(255, 255, 255, int(220 * depth))))
            painter.setPen(Qt.PenStyle.NoPen)
            painter.drawEllipse(QPointF(x, y), 4 * depth, 4 * depth)

            # Ring
            ring_pen = QPen(QColor(0, 255, 200, int(100 * depth)))
            ring_pen.setWidthF(1.5)
            painter.setPen(ring_pen)
            painter.setBrush(Qt.BrushStyle.NoBrush)
            painter.drawEllipse(QPointF(x, y), 10 * depth, 10 * depth)

    def _draw_wrist_ring(self, painter: QPainter, wrist):
        """Draw rotating ring at wrist position."""
        x, y, z = wrist
        ring_pen = QPen(QColor(0, 180, 255, 60))
        ring_pen.setWidthF(1.5)
        painter.setPen(ring_pen)
        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.drawEllipse(QPointF(x, y), 14, 14)

        # Cross hairs
        ch_pen = QPen(QColor(0, 180, 255, 40))
        ch_pen.setWidthF(1.0)
        painter.setPen(ch_pen)
        painter.drawLine(QPointF(x - 8, y), QPointF(x + 8, y))
        painter.drawLine(QPointF(x, y - 8), QPointF(x, y + 8))

    def _draw_dwell_ring(self, painter: QPainter, fingertip, progress):
        """Draw dwell progress ring around index fingertip."""
        x, y, _ = fingertip

        # Background ring
        bg_pen = QPen(QColor(0, 180, 255, 40))
        bg_pen.setWidthF(3.0)
        painter.setPen(bg_pen)
        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.drawEllipse(QPointF(x, y), 22, 22)

        # Progress arc
        color = _lerp_color(
            QColor(0, 200, 255),
            QColor(255, 100, 50) if progress > 0.7 else QColor(0, 255, 200),
            progress
        )
        arc_pen = QPen(color)
        arc_pen.setWidthF(3.5)
        arc_pen.setCapStyle(Qt.PenCapStyle.RoundCap)
        painter.setPen(arc_pen)

        # Draw arc manually (QPainterPath)
        path = QPainterPath()
        start_angle = -90  # Start from top
        span_angle = 360 * progress
        rect = QRectF(x - 22, y - 22, 44, 44)
        path.arcTo(rect, start_angle, span_angle)
        painter.drawPath(path)

        # Center dot
        painter.setBrush(QBrush(color))
        painter.setPen(Qt.PenStyle.NoPen)
        painter.drawEllipse(QPointF(x, y), 3, 3)

    def _draw_gesture_label(self, painter: QPainter, fingertip, gesture, gear):
        """Draw gesture label above the hand."""
        x, y, _ = fingertip
        gear_names = ["STOP", "PRECISION", "NORMAL", "FAST", "TURBO"]
        gear_name = gear_names[gear] if 0 <= gear < len(gear_names) else "NORMAL"

        painter.setPen(QPen(QColor(0, 200, 255, 180)))
        font = painter.font()
        font.setPointSize(10)
        font.setBold(True)
        painter.setFont(font)
        painter.drawText(QPointF(x + 25, y - 25), gesture)
        painter.setPen(QPen(QColor(0, 180, 255, 120)))
        font.setPointSize(8)
        font.setBold(False)
        painter.setFont(font)
        painter.drawText(QPointF(x + 25, y - 10), gear_name)

    def _draw_idle_hand(self, painter: QPainter, w: int, h: int):
        """Draw a ghost hand outline when no hand is detected."""
        cx, cy = w // 2, h // 2

        # Ghost hand outline
        ghost_pen = QPen(QColor(0, 180, 255, 25))
        ghost_pen.setWidthF(1.5)
        painter.setPen(ghost_pen)
        painter.setBrush(Qt.BrushStyle.NoBrush)

        # Simplified hand shape
        path = QPainterPath()
        path.moveTo(cx, cy + 40)  # Wrist
        path.lineTo(cx - 25, cy + 10)  # Left palm
        path.lineTo(cx - 30, cy - 20)  # Pinky
        path.lineTo(cx - 15, cy - 35)  # Ring
        path.lineTo(cx, cy - 45)       # Middle
        path.lineTo(cx + 15, cy - 35)  # Index
        path.lineTo(cx + 30, cy - 15)  # Thumb area
        path.lineTo(cx + 25, cy + 10)  # Right palm
        path.closeSubpath()
        painter.drawPath(path)

        # Dots at fingertips
        painter.setBrush(QBrush(QColor(0, 180, 255, 30)))
        for dx, dy in [(-30, -20), (-15, -35), (0, -45), (15, -35), (30, -15)]:
            painter.drawEllipse(QPointF(cx + dx, cy + dy), 4, 4)

    def reset(self):
        self._smoothed_landmarks = None
