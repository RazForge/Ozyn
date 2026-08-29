/**
 * Ozayn Gesture Classifier — Geometric gesture recognition.
 *
 * Pipeline: hand_state_t → gesture_result_t → command_t
 *
 * Classification rules use:
 * - Which fingers are extended (fingers_up bitmap)
 * - Thumb-index distance (pinch detection)
 * - Hand velocity (for swipes, fast pointer)
 * - Gesture duration (for confirmation)
 * - Two-hand relations (for two-hand gestures)
 */

#include "gesture_classifier.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* ── Thresholds ── */
#define PINCH_DIST         0.08f   /* Thumb-index distance for pinch */
#define TWO_FINGER_DIST    0.06f   /* Index-middle distance for right click */
#define SWIPE_SPEED        0.15f   /* Minimum speed for swipe */
#define CONFIRM_TIME       0.30f   /* Hold time for confirmation (seconds) */
#define FAST_POINTER_SPEED 0.15f   /* Speed threshold for fast pointer */
#define PRECISION_MAX      0.06f   /* Max speed for precision mode */

/* ── Finger bitmaps ── */
#define F_THUMB  0x01
#define F_INDEX  0x02
#define F_MIDDLE 0x04
#define F_RING   0x08
#define F_PINKY  0x10

/* ── Single hand gesture classification ── */
gesture_result_t gesture_classify_single(const hand_state_t *hand)
{
    gesture_result_t r = { GESTURE_NONE, 0.0f, 0.0f, false };
    if (!hand->valid) return r;

    uint8_t fu = 0;
    if (hand->fingers[FINGER_THUMB].extended)  fu |= F_THUMB;
    if (hand->fingers[FINGER_INDEX].extended)  fu |= F_INDEX;
    if (hand->fingers[FINGER_MIDDLE].extended) fu |= F_MIDDLE;
    if (hand->fingers[FINGER_RING].extended)   fu |= F_RING;
    if (hand->fingers[FINGER_PINKY].extended)  fu |= F_PINKY;

    float speed = hand->speed;
    float pinch = hand->thumb_index_dist;
    float idx_mid = hand->index_middle_dist;

    /* ── LOCKED STATE ── */
    /* Fist = all fingers down → LOCK already handled in state machine */

    /* ── GESTURE CLASSIFICATION ── */

    /* PINCH: thumb + index tips close, others may be flexed */
    if (pinch < PINCH_DIST && (fu & F_THUMB) && (fu & F_INDEX)) {
        r.type = GESTURE_LEFT_CLICK;
        r.confidence = 1.0f - (pinch / PINCH_DIST);
        return r;
    }

    /* RIGHT CLICK: index + middle extended, thumb-index close enough */
    if ((fu & F_INDEX) && (fu & F_MIDDLE) && !(fu & F_RING) && !(fu & F_PINKY)) {
        if (idx_mid < TWO_FINGER_DIST) {
            r.type = GESTURE_RIGHT_CLICK;
            r.confidence = 0.9f;
            return r;
        }
        /* Two finger zoom */
        r.type = GESTURE_ZOOM_IN;
        r.confidence = 0.7f;
        return r;
    }

    /* THUMB UP: only thumb extended */
    if (fu == F_THUMB) {
        /* Check orientation: thumb should point upward */
        float thumb_angle = hand->palm.orientation;
        if (thumb_angle > -60.0f && thumb_angle < 60.0f) {
            r.type = GESTURE_THUMB_UP;
            r.confidence = 0.9f;
            return r;
        } else {
            r.type = GESTURE_THUMB_DOWN;
            r.confidence = 0.9f;
            return r;
        }
    }

    /* INDEX ONLY: pointer */
    if (fu == F_INDEX) {
        if (speed > FAST_POINTER_SPEED) {
            r.type = GESTURE_FAST_POINTER;
            r.confidence = 0.85f;
        } else if (speed < PRECISION_MAX) {
            r.type = GESTURE_PRECISION_POINTER;
            r.confidence = 0.85f;
        } else {
            r.type = GESTURE_POINTER;
            r.confidence = 0.9f;
        }
        return r;
    }

    /* INDEX + PINKY: command mode */
    if (fu == (F_INDEX | F_PINKY)) {
        r.type = GESTURE_COMMAND_MODE;
        r.confidence = 0.85f;
        return r;
    }

    /* INDEX + MIDDLE + RING: window command */
    if (fu == (F_INDEX | F_MIDDLE | F_RING)) {
        r.type = GESTURE_WINDOW_COMMAND;
        r.confidence = 0.85f;
        return r;
    }

    /* INDEX + MIDDLE + RING + PINKY: menu (4 fingers) */
    if (fu == (F_INDEX | F_MIDDLE | F_RING | F_PINKY)) {
        r.type = GESTURE_MENU;
        r.confidence = 0.85f;
        return r;
    }

    /* INDEX + MIDDLE V shape: task switch */
    if (fu == (F_INDEX | F_MIDDLE) && idx_mid > TWO_FINGER_DIST) {
        r.type = GESTURE_TASK_SWITCH;
        r.confidence = 0.8f;
        return r;
    }

    /* ALL FIVE fingers: open palm → STOP or SWIPE */
    if (fu == 0x1F) {
        if (speed > SWIPE_SPEED) {
            /* Determine swipe direction from velocity */
            if (fabsf(hand->vel_x) > fabsf(hand->vel_y)) {
                r.type = (hand->vel_x > 0) ? GESTURE_SWIPE_RIGHT : GESTURE_SWIPE_LEFT;
            } else {
                r.type = (hand->vel_y > 0) ? GESTURE_SWIPE_DOWN : GESTURE_SWIPE_UP;
            }
            r.confidence = 0.85f;
        } else {
            r.type = GESTURE_STOP;
            r.confidence = 0.8f;
        }
        return r;
    }

    /* CLOSED FIST: all fingers down */
    if (fu == 0x00) {
        r.type = GESTURE_LOCK;
        r.confidence = 0.9f;
        return r;
    }

    /* FLAT HAND rotation detection (all extended + wrist rotation) */
    if (fu == 0x1F) {
        float orient = hand->palm.orientation;
        if (orient > 45.0f && orient < 135.0f) {
            r.type = GESTURE_ROTATE_RIGHT;
            r.confidence = 0.7f;
        } else if (orient < -45.0f && orient > -135.0f) {
            r.type = GESTURE_ROTATE_LEFT;
            r.confidence = 0.7f;
        }
        return r;
    }

    return r;
}

/* ── Two-hand gesture classification ── */
gesture_result_t gesture_classify_two_hands(const hand_system_t *sys)
{
    gesture_result_t r = { GESTURE_NONE, 0.0f, 0.0f, false };
    if (sys->num_hands < 2) return r;

    const hand_state_t *a = &sys->hands[0];
    const hand_state_t *b = &sys->hands[1];

    /* Both open palms */
    bool a_open = (a->fingers[FINGER_INDEX].extended && a->fingers[FINGER_MIDDLE].extended &&
                   a->fingers[FINGER_RING].extended && a->fingers[FINGER_PINKY].extended);
    bool b_open = (b->fingers[FINGER_INDEX].extended && b->fingers[FINGER_MIDDLE].extended &&
                   b->fingers[FINGER_RING].extended && b->fingers[FINGER_PINKY].extended);

    /* Both pinch */
    bool a_pinch = (a->thumb_index_dist < PINCH_DIST);
    bool b_pinch = (b->thumb_index_dist < PINCH_DIST);

    /* Both fists */
    bool a_fist = !a_open && !a_pinch && a->speed < 0.02f;
    bool b_fist = !b_open && !b_pinch && b->speed < 0.02f;

    /* Both fists → SYSTEM LOCK */
    if (a_fist && b_fist) {
        r.type = GESTURE_SYSTEM_LOCK;
        r.confidence = 0.85f;
        return r;
    }

    /* Both open palms → system ready, zoom, swipe, etc. */
    if (a_open && b_open) {
        float vel = sys->hands_velocity;

        if (fabsf(vel) > 0.01f) {
            if (vel > 0) {
                r.type = GESTURE_HANDS_APART;
                r.confidence = 0.85f;
            } else {
                r.type = GESTURE_HANDS_TOGETHER;
                r.confidence = 0.85f;
            }
            return r;
        }

        /* Vertical movement */
        if (a->vel_y < -0.05f && b->vel_y < -0.05f) {
            r.type = GESTURE_TWO_HAND_UP;
            r.confidence = 0.8f;
            return r;
        }
        if (a->vel_y > 0.05f && b->vel_y > 0.05f) {
            r.type = GESTURE_TWO_HAND_DOWN;
            r.confidence = 0.8f;
            return r;
        }

        /* Both stationary → system ready */
        if (a->speed < 0.02f && b->speed < 0.02f) {
            r.type = GESTURE_TWO_HAND_READY;
            r.confidence = 0.8f;
            return r;
        }
    }

    /* Both pinch → object control */
    if (a_pinch && b_pinch) {
        if (sys->hands_velocity > 0.01f) {
            r.type = GESTURE_SCALE_UP;
            r.confidence = 0.85f;
        } else if (sys->hands_velocity < -0.01f) {
            r.type = GESTURE_SCALE_DOWN;
            r.confidence = 0.85f;
        } else {
            r.type = GESTURE_BOTH_PINCH;
            r.confidence = 0.8f;
        }
        return r;
    }

    return r;
}

/* ── State machine update ── */
void gesture_state_init(gesture_state_t *gs)
{
    memset(gs, 0, sizeof(*gs));
    gs->current = GESTURE_NONE;
    gs->previous = GESTURE_NONE;
    gs->confirm_time = CONFIRM_TIME;
}

void gesture_state_update(gesture_state_t *gs, gesture_result_t result, float dt)
{
    gs->previous = gs->current;

    if (result.type == GESTURE_NONE) {
        gs->current = GESTURE_NONE;
        gs->hold_time = 0.0f;
        gs->is_confirmed = false;
        gs->confidence = 0.0f;
        return;
    }

    if (result.type == gs->current) {
        /* Same gesture maintained */
        gs->hold_time += dt;
        gs->confidence = result.confidence;

        /* Confirm after hold time */
        if (!gs->is_confirmed && gs->hold_time >= gs->confirm_time &&
            gs->confidence >= 0.7f) {
            gs->is_confirmed = true;
        }
    } else {
        /* New gesture */
        gs->current = result.type;
        gs->hold_time = dt;
        gs->confidence = result.confidence;
        gs->is_confirmed = false;
    }
}

/* ── Map gesture → command ── */
command_t gesture_to_command(const gesture_state_t *gs)
{
    command_t cmd = { CMD_NONE, 0.0f, 0 };

    if (gs->lock_active && gs->current != GESTURE_UNLOCK)
        return cmd;

    if (!gs->is_confirmed && gs->current != GESTURE_POINTER &&
        gs->current != GESTURE_FAST_POINTER &&
        gs->current != GESTURE_PRECISION_POINTER &&
        gs->current != GESTURE_DRAG) {
        return cmd;
    }

    switch (gs->current) {
    case GESTURE_POINTER:
    case GESTURE_FAST_POINTER:
    case GESTURE_PRECISION_POINTER:
        cmd.type = CMD_MOVE_CURSOR;
        cmd.param_f = (gs->current == GESTURE_FAST_POINTER) ? 2.5f :
                      (gs->current == GESTURE_PRECISION_POINTER) ? 0.3f : 1.0f;
        break;
    case GESTURE_STOP:
        cmd.type = CMD_NONE;
        break;
    case GESTURE_LEFT_CLICK:
        cmd.type = CMD_LEFT_CLICK;
        break;
    case GESTURE_RIGHT_CLICK:
        cmd.type = CMD_RIGHT_CLICK;
        break;
    case GESTURE_DRAG:
        cmd.type = CMD_DRAG_START;
        break;
    case GESTURE_DROP:
        cmd.type = CMD_DRAG_END;
        break;
    case GESTURE_ZOOM_IN:
        cmd.type = CMD_ZOOM_IN;
        break;
    case GESTURE_ZOOM_OUT:
        cmd.type = CMD_ZOOM_OUT;
        break;
    case GESTURE_SWIPE_LEFT:
        cmd.type = CMD_SWIPE_LEFT;
        break;
    case GESTURE_SWIPE_RIGHT:
        cmd.type = CMD_SWIPE_RIGHT;
        break;
    case GESTURE_SWIPE_UP:
        cmd.type = CMD_SCROLL_UP;
        break;
    case GESTURE_SWIPE_DOWN:
        cmd.type = CMD_SCROLL_DOWN;
        break;
    case GESTURE_THUMB_UP:
        cmd.type = CMD_CONFIRM;
        break;
    case GESTURE_THUMB_DOWN:
        cmd.type = CMD_CANCEL;
        break;
    case GESTURE_LOCK:
        cmd.type = CMD_LOCK;
        break;
    case GESTURE_UNLOCK:
        cmd.type = CMD_UNLOCK;
        break;
    case GESTURE_EMERGENCY_STOP:
        cmd.type = CMD_EMERGENCY_STOP;
        break;
    case GESTURE_TASK_SWITCH:
        cmd.type = CMD_TASK_SWITCH;
        break;
    case GESTURE_MENU:
        cmd.type = CMD_MENU;
        break;
    case GESTURE_TWO_HAND_UP:
        cmd.type = CMD_SCROLL_UP;
        break;
    case GESTURE_TWO_HAND_DOWN:
        cmd.type = CMD_SCROLL_DOWN;
        break;
    case GESTURE_HANDS_APART:
        cmd.type = CMD_ZOOM_IN;
        break;
    case GESTURE_HANDS_TOGETHER:
        cmd.type = CMD_ZOOM_OUT;
        break;
    case GESTURE_SCALE_UP:
        cmd.type = CMD_SCALE_UP;
        break;
    case GESTURE_SCALE_DOWN:
        cmd.type = CMD_SCALE_DOWN;
        break;
    case GESTURE_TWO_THUMBS_UP:
        cmd.type = CMD_CONFIRM;
        break;
    case GESTURE_TWO_THUMBS_DOWN:
        cmd.type = CMD_CANCEL;
        break;
    case GESTURE_SYSTEM_LOCK:
        cmd.type = CMD_LOCK;
        break;
    case GESTURE_ROTATE_LEFT:
        cmd.type = CMD_ROTATE_LEFT;
        break;
    case GESTURE_ROTATE_RIGHT:
        cmd.type = CMD_ROTATE_RIGHT;
        break;
    case GESTURE_FULL_SCREEN:
        cmd.type = CMD_FULLSCREEN;
        break;
    default:
        break;
    }

    return cmd;
}

/* ── Name strings ── */
const char *gesture_name(gesture_type_t g)
{
    static const char *names[] = {
        "NONE",
        "POINTER", "FAST_POINTER", "PRECISION_POINTER", "STOP",
        "LEFT_CLICK", "DRAG", "DROP", "RIGHT_CLICK",
        "ZOOM_IN", "ZOOM_OUT",
        "SWIPE_LEFT", "SWIPE_RIGHT", "SWIPE_UP", "SWIPE_DOWN",
        "THUMB_UP", "THUMB_DOWN",
        "LOCK", "UNLOCK",
        "TASK_SWITCH", "WINDOW_COMMAND", "MENU", "COMMAND_MODE", "SELECT_MODE",
        "ROTATE_RIGHT", "ROTATE_LEFT",
        "TWO_HAND_READY", "HANDS_APART", "HANDS_TOGETHER",
        "TWO_HAND_UP", "TWO_HAND_DOWN",
        "EXPAND", "COLLAPSE",
        "TWO_HAND_ROTATE_L", "TWO_HAND_ROTATE_R",
        "BOTH_PINCH", "SCALE_UP", "SCALE_DOWN",
        "TWO_THUMBS_UP", "TWO_THUMBS_DOWN",
        "SYSTEM_LOCK", "SYSTEM_UNLOCK", "EMERGENCY_STOP",
        "FULL_SCREEN", "EXIT_FULLSCREEN"
    };
    if (g >= 0 && g < GESTURE_COUNT) return names[g];
    return "UNKNOWN";
}

const char *command_name(command_type_t c)
{
    static const char *names[] = {
        "NONE",
        "MOVE_CURSOR", "LEFT_CLICK", "RIGHT_CLICK",
        "DRAG_START", "DRAG_END",
        "ZOOM_IN", "ZOOM_OUT",
        "SCROLL_UP", "SCROLL_DOWN", "SCROLL_LEFT", "SCROLL_RIGHT",
        "SWIPE_LEFT", "SWIPE_RIGHT",
        "TASK_SWITCH", "MENU",
        "CONFIRM", "CANCEL",
        "LOCK", "UNLOCK", "EMERGENCY_STOP", "FULLSCREEN",
        "SELECT", "ROTATE_LEFT", "ROTATE_RIGHT",
        "SCALE_UP", "SCALE_DOWN"
    };
    if (c >= 0 && c < CMD_COUNT) return names[c];
    return "UNKNOWN";
}
