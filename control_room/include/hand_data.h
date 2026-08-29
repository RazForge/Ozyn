/**
 * Ozayn Hand Data — Structured hand representation.
 * 21 MediaPipe landmarks → geometric features.
 */

#ifndef OZAYN_HAND_DATA_H
#define OZAYN_HAND_DATA_H

#include <stdint.h>
#include <stdbool.h>

/* ── MediaPipe 21 landmarks ── */
/* IDs: 0=Wrist
 * Thumb: 1=CMC, 2=MCP, 3=IP, 4=TIP
 * Index: 5=MCP, 6=PIP, 7=DIP, 8=TIP
 * Middle: 9=MCP, 10=PIP, 11=DIP, 12=TIP
 * Ring: 13=MCP, 14=PIP, 15=DIP, 16=TIP
 * Pinky: 17=MCP, 18=PIP, 19=DIP, 20=TIP
 */
#define LM_WRIST       0
#define LM_THUMB_CMC   1
#define LM_THUMB_MCP   2
#define LM_THUMB_IP    3
#define LM_THUMB_TIP   4
#define LM_INDEX_MCP   5
#define LM_INDEX_PIP   6
#define LM_INDEX_DIP   7
#define LM_INDEX_TIP   8
#define LM_MIDDLE_MCP  9
#define LM_MIDDLE_PIP  10
#define LM_MIDDLE_DIP  11
#define LM_MIDDLE_TIP  12
#define LM_RING_MCP    13
#define LM_RING_PIP    14
#define LM_RING_DIP    15
#define LM_RING_TIP    16
#define LM_PINKY_MCP   17
#define LM_PINKY_PIP   18
#define LM_PINKY_DIP   19
#define LM_PINKY_TIP   20

#define NUM_LANDMARKS  21

/* ── Single landmark ── */
typedef struct {
    float x;          /* Normalized 0.0 - 1.0 */
    float y;          /* Normalized 0.0 - 1.0 */
    float z;          /* Depth (relative) */
} landmark_t;

/* ── Finger names ── */
typedef enum {
    FINGER_THUMB = 0,
    FINGER_INDEX,
    FINGER_MIDDLE,
    FINGER_RING,
    FINGER_PINKY,
    FINGER_COUNT
} finger_id_t;

/* ── Finger state ── */
typedef struct {
    bool extended;        /* Is finger extended? */
    float angles[3];      /* Joint angles (MCP, PIP, DIP) in degrees */
    float tip_dist;       /* Distance from MCP to TIP */
} finger_state_t;

/* ── Palm info ── */
typedef struct {
    landmark_t center;    /* Palm center (computed) */
    float width;          /* Palm width */
    float height;         /* Palm height */
    float orientation;    /* Angle in degrees (0=up, 90=right, 180=down, 270=left) */
    float facing_camera;  /* 0.0=away, 1.0=toward (from z coordinates) */
} palm_t;

/* ── Handedness ── */
typedef enum {
    HAND_RIGHT = 0,
    HAND_LEFT = 1
} handedness_t;

/* ── Complete hand state ── */
typedef struct {
    bool           valid;          /* Hand detected this frame? */
    handedness_t   handedness;     /* Left or right */
    landmark_t     landmarks[NUM_LANDMARKS];  /* 21 joints */
    finger_state_t fingers[FINGER_COUNT];     /* Per-finger state */
    palm_t         palm;           /* Palm info */

    /* Distances between specific fingers */
    float thumb_index_dist;        /* Thumb tip to index tip */
    float thumb_middle_dist;
    float index_middle_dist;       /* Index tip to middle tip */
    float middle_ring_dist;
    float ring_pinky_dist;

    /* Velocity (computed frame-to-frame) */
    float vel_x;                   /* Palm velocity X (normalized/frame) */
    float vel_y;                   /* Palm velocity Y (normalized/frame) */
    float speed;                   /* sqrt(vx^2 + vy^2) */
    float accel;                   /* Acceleration magnitude */

    /* History for velocity computation */
    float prev_palm_x;
    float prev_palm_y;
    float prev_speed;
    bool  has_history;
} hand_state_t;

/* ── Two-hand system state ── */
typedef struct {
    int         num_hands;
    hand_state_t hands[2];         /* [0]=primary, [1]=secondary */

    /* Two-hand relations */
    float hands_distance;          /* Distance between palm centers */
    float hands_angle;             /* Angle between hands */
    float hands_velocity;          /* Relative velocity */
    float prev_hands_distance;
    bool  has_two_hand_history;
} hand_system_t;

/* ── Functions ── */

/* Initialize hand state */
void hand_state_init(hand_state_t *hs);

/* Initialize hand system */
void hand_system_init(hand_system_t *sys);

/* Compute palm center from wrist + MCP joints */
void hand_compute_palm(hand_state_t *hs);

/* Compute finger states (extended? angles?) */
void hand_compute_fingers(hand_state_t *hs);

/* Compute finger-to-finger distances */
void hand_compute_distances(hand_state_t *hs);

/* Compute velocity from previous frame */
void hand_compute_velocity(hand_state_t *hs);

/* Full update: call after receiving new landmarks */
void hand_state_update(hand_state_t *hs);

/* Update two-hand relations */
void hand_system_update(hand_system_t *sys);

/* Process raw uint16 data from Python */
void hand_state_from_raw(hand_state_t *hs, const uint16_t *raw_xy,
                         uint8_t fingers_up, uint8_t handedness);

#endif /* OZAYN_HAND_DATA_H */
