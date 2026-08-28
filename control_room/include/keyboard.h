/**
 * Ozayn Keyboard Engine — On-screen keyboard + key simulation.
 * Uses X11 for key press simulation and window management.
 */

#ifndef OZAYN_KEYBOARD_H
#define OZAYN_KEYBOARD_H

#include "ozayn_core.h"
#include <pthread.h>

/* Keyboard layout */
typedef struct {
    const char *name;        /* "qwerty", "amharic", etc. */
    const char *rows[4];     /* Key rows */
    int         num_rows;
} kbd_layout_t;

/* Keyboard context */
typedef struct {
    bool      active;
    pthread_t thread;
    int       ipc_fd;
    void     *x_display;     /* X11 Display */
    void     *window;        /* X11 Window */
    int       win_x;
    int       win_y;
    int       win_w;
    int       win_h;
    const kbd_layout_t *layout;
    bool      shift_on;
    bool      ctrl_on;
    bool      alt_on;
    bool      keyboard_visible;
} keyboard_ctx_t;

/* Initialize keyboard engine */
int  keyboard_init(keyboard_ctx_t *ctx, int ipc_fd);

/* Show keyboard */
int  keyboard_show(keyboard_ctx_t *ctx);

/* Hide keyboard */
void keyboard_hide(keyboard_ctx_t *ctx);

/* Simulate key press */
void keyboard_press(keyboard_ctx_t *ctx, const char *key);

/* Cleanup */
void keyboard_destroy(keyboard_ctx_t *ctx);

#endif /* OZAYN_KEYBOARD_H */
