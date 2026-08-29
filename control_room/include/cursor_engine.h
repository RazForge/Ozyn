/**
 * Ozayn Cursor Engine — Velocity-based acceleration with gear system.
 * Hand velocity → dead zone → acceleration curve → speed limit → cursor position.
 */

#ifndef OZAYN_CURSOR_ENGINE_H
#define OZAYN_CURSOR_ENGINE_H

#include <stdbool.h>

/* ── Cursor gears ── */
typedef enum {
    GEAR_STOP = 0,      /* Cursor frozen */
    GEAR_PRECISION,     /* Very slow, accurate */
    GEAR_NORMAL,        /* Everyday navigation */
    GEAR_FAST,          /* Rapid movement */
    GEAR_TURBO,         /* Cross screen extremely quickly */
    GEAR_COUNT
} cursor_gear_t;

/* ── Velocity thresholds for auto-gearing ── */
#define VEL_DEADZONE     0.03f   /* Below this = no movement */
#define VEL_PRECISION    0.05f   /* 0.03 - 0.10 = precision */
#define VEL_NORMAL_MAX   0.20f   /* 0.10 - 0.30 = normal */
#define VEL_FAST_MAX     0.50f   /* 0.30 - 0.60 = fast */
/* Above 0.50 = turbo */

/* ── Sensitivity per gear ── */
#define SENS_PRECISION   0.3f
#define SENS_NORMAL      1.0f
#define SENS_FAST        2.5f
#define SENS_TURBO       5.0f

/* ── Max cursor speed (pixels per frame at 60fps) ── */
#define MAX_CURSOR_SPEED 80.0f

/* ── Smoothing ── */
#define SMOOTH_FACTOR    5

/* ── Cursor state ── */
typedef struct {
    /* Current position (0.0 - 1.0 normalized) */
    float x;
    float y;

    /* Screen dimensions */
    int   screen_w;
    int   screen_h;

    /* Velocity */
    float vx;
    float vy;
    float speed;

    /* Gear system */
    cursor_gear_t gear;
    bool          auto_gear;     /* Auto-switch gears based on speed? */

    /* Smoothing */
    float smooth_x;
    float smooth_y;
    bool  has_pos;

    /* Frame reduction zone (pixels from edges where movement is ignored) */
    int   frame_reduction;

    /* Drag state */
    bool  dragging;

    /* Gesture lock */
    bool  locked;
} cursor_state_t;

/* ── Functions ── */

/* Initialize cursor engine */
void cursor_init(cursor_state_t *c, int screen_w, int screen_h);

/* Update cursor from hand position (normalized 0-1) */
void cursor_update(cursor_state_t *c, float hand_x, float hand_y,
                   float hand_vx, float hand_vy);

/* Force specific gear */
void cursor_set_gear(cursor_state_t *c, cursor_gear_t gear);

/* Auto-detect gear from hand velocity */
void cursor_auto_gear(cursor_state_t *c, float hand_speed);

/* Get cursor pixel position */
int cursor_px(const cursor_state_t *c);
int cursor_py(const cursor_state_t *c);

/* Lock/unlock cursor movement */
void cursor_lock(cursor_state_t *c, bool locked);

/* Get gear name */
const char *cursor_gear_name(cursor_gear_t g);

#endif /* OZAYN_CURSOR_ENGINE_H */
