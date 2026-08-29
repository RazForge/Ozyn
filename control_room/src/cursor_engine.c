/**
 * Ozayn Cursor Engine — Velocity-based acceleration with gear system.
 *
 * Pipeline: hand velocity → dead zone → acceleration curve → speed limit → cursor
 *
 * 5 gears: STOP, PRECISION, NORMAL, FAST, TURBO
 * Auto-gears based on hand speed.
 */

#include "cursor_engine.h"
#include <math.h>
#include <string.h>

void cursor_init(cursor_state_t *c, int screen_w, int screen_h)
{
    memset(c, 0, sizeof(*c));
    c->screen_w = screen_w;
    c->screen_h = screen_h;
    c->gear = GEAR_NORMAL;
    c->auto_gear = true;
    c->frame_reduction = 50;
    c->has_pos = false;
}

/* ── Compute acceleration factor from speed ── */
static float acceleration_curve(float speed)
{
    if (speed < VEL_DEADZONE) return 0.0f;
    if (speed < VEL_PRECISION) return speed * SENS_PRECISION;
    if (speed < VEL_NORMAL_MAX) return speed * SENS_NORMAL;
    if (speed < VEL_FAST_MAX) return speed * SENS_FAST;
    return speed * SENS_TURBO;
}

/* ── Auto-detect gear from speed ── */
void cursor_auto_gear(cursor_state_t *c, float hand_speed)
{
    if (hand_speed < VEL_DEADZONE) {
        c->gear = GEAR_STOP;
    } else if (hand_speed < VEL_PRECISION) {
        c->gear = GEAR_PRECISION;
    } else if (hand_speed < VEL_NORMAL_MAX) {
        c->gear = GEAR_NORMAL;
    } else if (hand_speed < VEL_FAST_MAX) {
        c->gear = GEAR_FAST;
    } else {
        c->gear = GEAR_TURBO;
    }
}

void cursor_set_gear(cursor_state_t *c, cursor_gear_t gear)
{
    c->gear = gear;
    c->auto_gear = false;
}

void cursor_update(cursor_state_t *c, float hand_x, float hand_y,
                   float hand_vx, float hand_vy)
{
    if (c->locked) return;
    if (c->gear == GEAR_STOP) return;

    /* Apply frame reduction zone */
    float fr = c->frame_reduction / (float)c->screen_w;
    float nx = hand_x, ny = hand_y;
    if (nx < fr) nx = fr;
    if (nx > 1.0f - fr) nx = 1.0f - fr;
    if (ny < fr) ny = fr;
    if (ny > 1.0f - fr) ny = 1.0f - fr;

    /* Remap from frame zone to full range */
    float mapped_x = (nx - fr) / (1.0f - 2.0f * fr);
    float mapped_y = (ny - fr) / (1.0f - 2.0f * fr);

    /* Auto-gear if enabled */
    float speed = sqrtf(hand_vx * hand_vx + hand_vy * hand_vy);
    if (c->auto_gear) {
        cursor_auto_gear(c, speed);
    }

    /* Apply gear sensitivity */
    float sensitivity = SENS_NORMAL;
    switch (c->gear) {
    case GEAR_STOP:      sensitivity = 0.0f; break;
    case GEAR_PRECISION: sensitivity = SENS_PRECISION; break;
    case GEAR_NORMAL:    sensitivity = SENS_NORMAL; break;
    case GEAR_FAST:      sensitivity = SENS_FAST; break;
    case GEAR_TURBO:     sensitivity = SENS_TURBO; break;
    default: break;
    }

    /* Compute target with acceleration */
    float accel = acceleration_curve(speed);
    float target_x, target_y;

    if (sensitivity > 0.0f && accel > 0.0f) {
        /* Velocity-based: add acceleration to position */
        target_x = c->x + hand_vx * sensitivity * accel * 50.0f;
        target_y = c->y + hand_vy * sensitivity * accel * 50.0f;

        /* Clamp */
        if (target_x < 0.0f) target_x = 0.0f;
        if (target_x > 1.0f) target_x = 1.0f;
        if (target_y < 0.0f) target_y = 0.0f;
        if (target_y > 1.0f) target_y = 1.0f;
    } else {
        /* Direct mapping for precision/normal */
        target_x = mapped_x;
        target_y = mapped_y;
    }

    /* Smoothing */
    if (!c->has_pos) {
        c->smooth_x = target_x;
        c->smooth_y = target_y;
        c->has_pos = true;
    } else {
        c->smooth_x = c->smooth_x + (target_x - c->smooth_x) / (float)SMOOTH_FACTOR;
        c->smooth_y = c->smooth_y + (target_y - c->smooth_y) / (float)SMOOTH_FACTOR;
    }

    /* Store velocity */
    c->vx = hand_vx;
    c->vy = hand_vy;
    c->speed = speed;

    c->x = c->smooth_x;
    c->y = c->smooth_y;
}

int cursor_px(const cursor_state_t *c)
{
    return (int)(c->x * c->screen_w);
}

int cursor_py(const cursor_state_t *c)
{
    return (int)(c->y * c->screen_h);
}

void cursor_lock(cursor_state_t *c, bool locked)
{
    c->locked = locked;
}

const char *cursor_gear_name(cursor_gear_t g)
{
    static const char *names[] = { "STOP", "PRECISION", "NORMAL", "FAST", "TURBO" };
    if (g >= 0 && g < GEAR_COUNT) return names[g];
    return "?";
}
