/**
 * Ozayn Hand Data — Landmark processing and feature extraction.
 */

#include "hand_data.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float dist2d(landmark_t a, landmark_t b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

static float angle_deg(landmark_t a, landmark_t b, landmark_t c)
{
    float ba_x = a.x - b.x, ba_y = a.y - b.y;
    float bc_x = c.x - b.x, bc_y = c.y - b.y;
    float dot = ba_x * bc_x + ba_y * bc_y;
    float mag_ba = sqrtf(ba_x * ba_x + ba_y * ba_y);
    float mag_bc = sqrtf(bc_x * bc_x + bc_y * bc_y);
    if (mag_ba < 0.001f || mag_bc < 0.001f) return 0.0f;
    float cos_a = dot / (mag_ba * mag_bc);
    if (cos_a > 1.0f) cos_a = 1.0f;
    if (cos_a < -1.0f) cos_a = -1.0f;
    return acosf(cos_a) * 180.0f / (float)M_PI;
}

void hand_state_init(hand_state_t *hs)
{
    memset(hs, 0, sizeof(*hs));
}

void hand_system_init(hand_system_t *sys)
{
    memset(sys, 0, sizeof(*sys));
    hand_state_init(&sys->hands[0]);
    hand_state_init(&sys->hands[1]);
}

void hand_compute_palm(hand_state_t *hs)
{
    /* Palm center = average of wrist (0), index MCP (5), middle MCP (9),
     * ring MCP (13), pinky MCP (17) */
    landmark_t *lm = hs->landmarks;
    hs->palm.center.x = (lm[0].x + lm[5].x + lm[9].x + lm[13].x + lm[17].x) / 5.0f;
    hs->palm.center.y = (lm[0].y + lm[5].y + lm[9].y + lm[13].y + lm[17].y) / 5.0f;
    hs->palm.center.z = (lm[0].z + lm[5].z + lm[9].z + lm[13].z + lm[17].z) / 5.0f;

    /* Palm width = distance between index MCP and pinky MCP */
    hs->palm.width = dist2d(lm[LM_INDEX_MCP], lm[LM_PINKY_MCP]);

    /* Palm height = distance from wrist to middle MCP */
    hs->palm.height = dist2d(lm[LM_WRIST], lm[LM_MIDDLE_MCP]);

    /* Palm orientation: angle from wrist to middle MCP (0=up, CW positive) */
    float dx = lm[LM_MIDDLE_MCP].x - lm[LM_WRIST].x;
    float dy = lm[LM_MIDDLE_MCP].y - lm[LM_WRIST].y;
    hs->palm.orientation = atan2f(dx, -dy) * 180.0f / (float)M_PI;

    /* Facing camera: average z of fingertips (lower z = closer to camera) */
    float avg_z = (lm[LM_INDEX_TIP].z + lm[LM_MIDDLE_TIP].z +
                   lm[LM_RING_TIP].z + lm[LM_PINKY_TIP].z) / 4.0f;
    hs->palm.facing_camera = 1.0f - (avg_z + 0.1f) / 0.2f;
    if (hs->palm.facing_camera < 0.0f) hs->palm.facing_camera = 0.0f;
    if (hs->palm.facing_camera > 1.0f) hs->palm.facing_camera = 1.0f;
}

void hand_compute_fingers(hand_state_t *hs)
{
    landmark_t *lm = hs->landmarks;

    /* ── Thumb: compare tip x vs IP x (mirrored for camera) ── */
    hs->fingers[FINGER_THUMB].extended = (lm[LM_THUMB_TIP].x < lm[LM_THUMB_IP].x);
    hs->fingers[FINGER_THUMB].angles[0] = angle_deg(lm[LM_THUMB_CMC], lm[LM_THUMB_MCP], lm[LM_THUMB_IP]);
    hs->fingers[FINGER_THUMB].angles[1] = angle_deg(lm[LM_THUMB_MCP], lm[LM_THUMB_IP], lm[LM_THUMB_TIP]);
    hs->fingers[FINGER_THUMB].tip_dist = dist2d(lm[LM_THUMB_TIP], lm[LM_THUMB_MCP]);

    /* ── Index: tip y < pip y → extended ── */
    static const int mcp_ids[FINGER_COUNT - 1] = {5, 9, 13, 17};
    static const int pip_ids[FINGER_COUNT - 1] = {6, 10, 14, 18};
    static const int dip_ids[FINGER_COUNT - 1] = {7, 11, 15, 19};
    static const int tip_ids[FINGER_COUNT - 1] = {8, 12, 16, 20};

    for (int f = 1; f < FINGER_COUNT; f++) {
        int fi = f - 1;
        hs->fingers[f].extended = (lm[tip_ids[fi]].y < lm[pip_ids[fi]].y);
        hs->fingers[f].angles[0] = angle_deg(lm[LM_WRIST], lm[mcp_ids[fi]], lm[pip_ids[fi]]);
        hs->fingers[f].angles[1] = angle_deg(lm[mcp_ids[fi]], lm[pip_ids[fi]], lm[dip_ids[fi]]);
        hs->fingers[f].angles[2] = angle_deg(lm[pip_ids[fi]], lm[dip_ids[fi]], lm[tip_ids[fi]]);
        hs->fingers[f].tip_dist = dist2d(lm[tip_ids[fi]], lm[mcp_ids[fi]]);
    }
}

void hand_compute_distances(hand_state_t *hs)
{
    landmark_t *lm = hs->landmarks;
    hs->thumb_index_dist   = dist2d(lm[LM_THUMB_TIP], lm[LM_INDEX_TIP]);
    hs->thumb_middle_dist  = dist2d(lm[LM_THUMB_TIP], lm[LM_MIDDLE_TIP]);
    hs->index_middle_dist  = dist2d(lm[LM_INDEX_TIP], lm[LM_MIDDLE_TIP]);
    hs->middle_ring_dist   = dist2d(lm[LM_MIDDLE_TIP], lm[LM_RING_TIP]);
    hs->ring_pinky_dist    = dist2d(lm[LM_RING_TIP], lm[LM_PINKY_TIP]);
}

void hand_compute_velocity(hand_state_t *hs)
{
    if (!hs->has_history) {
        hs->prev_palm_x = hs->palm.center.x;
        hs->prev_palm_y = hs->palm.center.y;
        hs->has_history = true;
        return;
    }

    hs->vel_x = hs->palm.center.x - hs->prev_palm_x;
    hs->vel_y = hs->palm.center.y - hs->prev_palm_y;
    hs->speed = sqrtf(hs->vel_x * hs->vel_x + hs->vel_y * hs->vel_y);
    hs->accel = hs->speed - hs->prev_speed;

    hs->prev_palm_x = hs->palm.center.x;
    hs->prev_palm_y = hs->palm.center.y;
    hs->prev_speed = hs->speed;
}

void hand_state_update(hand_state_t *hs)
{
    if (!hs->valid) return;
    hand_compute_palm(hs);
    hand_compute_fingers(hs);
    hand_compute_distances(hs);
    hand_compute_velocity(hs);
}

void hand_system_update(hand_system_t *sys)
{
    /* Update each hand */
    for (int i = 0; i < sys->num_hands; i++)
        hand_state_update(&sys->hands[i]);

    /* Two-hand relations */
    if (sys->num_hands >= 2) {
        hand_state_t *a = &sys->hands[0];
        hand_state_t *b = &sys->hands[1];

        sys->hands_distance = dist2d(a->palm.center, b->palm.center);

        float dx = b->palm.center.x - a->palm.center.x;
        float dy = b->palm.center.y - a->palm.center.y;
        sys->hands_angle = atan2f(dx, -dy) * 180.0f / (float)M_PI;

        if (sys->has_two_hand_history) {
            sys->hands_velocity = sys->hands_distance - sys->prev_hands_distance;
        }
        sys->prev_hands_distance = sys->hands_distance;
        sys->has_two_hand_history = true;
    }
}

void hand_state_from_raw(hand_state_t *hs, const uint16_t *raw_xy,
                         uint8_t fingers_up, uint8_t handedness)
{
    hs->valid = true;
    hs->handedness = (handedness_t)handedness;

    for (int i = 0; i < NUM_LANDMARKS; i++) {
        hs->landmarks[i].x = raw_xy[i * 2] / 65535.0f;
        hs->landmarks[i].y = raw_xy[i * 2 + 1] / 65535.0f;
        hs->landmarks[i].z = 0.0f;
    }

    /* Reconstruct fingers_up into finger states */
    hs->fingers[FINGER_THUMB].extended  = (fingers_up & 0x01) != 0;
    hs->fingers[FINGER_INDEX].extended  = (fingers_up & 0x02) != 0;
    hs->fingers[FINGER_MIDDLE].extended = (fingers_up & 0x04) != 0;
    hs->fingers[FINGER_RING].extended   = (fingers_up & 0x08) != 0;
    hs->fingers[FINGER_PINKY].extended  = (fingers_up & 0x10) != 0;
}
