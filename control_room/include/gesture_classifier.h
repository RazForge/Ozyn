/**
 * Ozayn Gesture Classifier — Geometric gesture recognition.
 * Takes hand_state_t → classifies into gesture_t.
 */

#ifndef OZAYN_GESTURE_CLASSIFIER_H
#define OZAYN_GESTURE_CLASSIFIER_H

#include "hand_data.h"

/* ── Gesture types (single hand) ── */
typedef enum {
    GESTURE_NONE = 0,

    /* Cursor control */
    GESTURE_POINTER,             /* Index only → move cursor */
    GESTURE_FAST_POINTER,        /* Index + fast → accelerated cursor */
    GESTURE_PRECISION_POINTER,   /* Index + slow → precise cursor */
    GESTURE_STOP,                /* Open palm stationary → freeze cursor */

    /* Clicks */
    GESTURE_LEFT_CLICK,          /* Pinch (thumb+index touch) */
    GESTURE_DRAG,                /* Pinch hold + move */
    GESTURE_DROP,                /* Pinch release */
    GESTURE_RIGHT_CLICK,         /* Two-finger (index+middle) pinch */

    /* Zoom */
    GESTURE_ZOOM_IN,             /* Two-finger spread */
    GESTURE_ZOOM_OUT,            /* Two-finger contract */

    /* Swipes */
    GESTURE_SWIPE_LEFT,          /* Open hand, fast horizontal ← */
    GESTURE_SWIPE_RIGHT,         /* Open hand, fast horizontal → */
    GESTURE_SWIPE_UP,            /* Open hand, fast vertical ↑ */
    GESTURE_SWIPE_DOWN,          /* Open hand, fast vertical ↓ */

    /* Confirmation */
    GESTURE_THUMB_UP,            /* Thumb extended up, hold 300ms */
    GESTURE_THUMB_DOWN,          /* Thumb extended down, hold 300ms */

    /* System */
    GESTURE_LOCK,                /* Closed fist → disable gestures */
    GESTURE_UNLOCK,              /* Fist → open palm → re-enable */

    /* Navigation */
    GESTURE_TASK_SWITCH,         /* Index + middle V shape */
    GESTURE_WINDOW_COMMAND,      /* Three fingers open */
    GESTURE_MENU,                /* Four fingers open */
    GESTURE_COMMAND_MODE,        /* Index + pinky */
    GESTURE_SELECT_MODE,         /* Index + thumb point */

    /* Rotation */
    GESTURE_ROTATE_RIGHT,        /* Flat hand, wrist rotate CW */
    GESTURE_ROTATE_LEFT,         /* Flat hand, wrist rotate CCW */

    /* Two-hand gestures */
    GESTURE_TWO_HAND_READY,      /* Both open palms, stationary */
    GESTURE_HANDS_APART,         /* Both open, moving apart */
    GESTURE_HANDS_TOGETHER,      /* Both open, moving together */
    GESTURE_TWO_HAND_UP,         /* Both move up */
    GESTURE_TWO_HAND_DOWN,       /* Both move down */
    GESTURE_EXPAND,              /* Both open, pull outward */
    GESTURE_COLLAPSE,            /* Both open, push inward */
    GESTURE_TWO_HAND_ROTATE_L,   /* Left up + right down */
    GESTURE_TWO_HAND_ROTATE_R,   /* Left down + right up */
    GESTURE_BOTH_PINCH,          /* Both hands pinch */
    GESTURE_SCALE_UP,            /* Both pinch, apart */
    GESTURE_SCALE_DOWN,          /* Both pinch, together */
    GESTURE_TWO_THUMBS_UP,       /* Both thumbs up */
    GESTURE_TWO_THUMBS_DOWN,     /* Both thumbs down */
    GESTURE_SYSTEM_LOCK,         /* Both fists */
    GESTURE_SYSTEM_UNLOCK,       /* Both open palms hold 1s */
    GESTURE_EMERGENCY_STOP,      /* Crossed hands */
    GESTURE_FULL_SCREEN,         /* Hands above head */
    GESTURE_EXIT_FULLSCREEN,     /* Hands below waist */

    GESTURE_COUNT
} gesture_type_t;

/* ── Gesture result ── */
typedef struct {
    gesture_type_t type;
    float          confidence;   /* 0.0 - 1.0 */
    float          duration;     /* How long gesture held (seconds) */
    bool           confirmed;    /* Passed confirmation threshold? */
} gesture_result_t;

/* ── Gesture state machine ── */
typedef struct {
    gesture_type_t current;
    gesture_type_t previous;
    float          hold_time;      /* How long current gesture held */
    float          confirm_time;   /* Required hold time to confirm */
    float          confidence;
    bool           is_confirmed;
    bool           lock_active;    /* System locked? */
} gesture_state_t;

/* ── Command types (what the system actually does) ── */
typedef enum {
    CMD_NONE = 0,
    CMD_MOVE_CURSOR,
    CMD_LEFT_CLICK,
    CMD_RIGHT_CLICK,
    CMD_DRAG_START,
    CMD_DRAG_END,
    CMD_ZOOM_IN,
    CMD_ZOOM_OUT,
    CMD_SCROLL_UP,
    CMD_SCROLL_DOWN,
    CMD_SCROLL_LEFT,
    CMD_SCROLL_RIGHT,
    CMD_SWIPE_LEFT,
    CMD_SWIPE_RIGHT,
    CMD_TASK_SWITCH,
    CMD_MENU,
    CMD_CONFIRM,
    CMD_CANCEL,
    CMD_LOCK,
    CMD_UNLOCK,
    CMD_EMERGENCY_STOP,
    CMD_FULLSCREEN,
    CMD_SELECT,
    CMD_ROTATE_LEFT,
    CMD_ROTATE_RIGHT,
    CMD_SCALE_UP,
    CMD_SCALE_DOWN,
    CMD_COUNT
} command_type_t;

typedef struct {
    command_type_t type;
    float          param_f;    /* Optional float param (e.g., speed) */
    int            param_i;    /* Optional int param */
} command_t;

/* ── Functions ── */

/* Initialize classifier state */
void gesture_state_init(gesture_state_t *gs);

/* Classify gesture from single hand */
gesture_result_t gesture_classify_single(const hand_state_t *hand);

/* Classify gesture from two hands */
gesture_result_t gesture_classify_two_hands(const hand_system_t *sys);

/* Update state machine (call every frame) */
void gesture_state_update(gesture_state_t *gs, gesture_result_t result,
                          float dt);

/* Map confirmed gesture to command */
command_t gesture_to_command(const gesture_state_t *gs);

/* Get gesture name string */
const char *gesture_name(gesture_type_t g);

/* Get command name string */
const char *command_name(command_type_t c);

#endif /* OZAYN_GESTURE_CLASSIFIER_H */
