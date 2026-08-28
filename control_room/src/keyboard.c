/**
 * Ozayn Keyboard Engine — On-screen keyboard + key simulation.
 * Uses X11 for key press simulation and window management.
 */

#include "keyboard.h"
#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* QWERTY layout */
static const kbd_layout_t LAYOUT_QWERTY = {
    .name = "qwerty",
    .rows = {
        "1234567890",
        "qwertyuiop",
        "asdfghjkl",
        "zxcvbnm,.",
    },
    .num_rows = 4,
};

/* Amharic layout (Ge'ez) */
static const kbd_layout_t LAYOUT_AMHARIC = {
    .name = "amharic",
    .rows = {
        "\u1362\u134a\u1361\u1365\u1384\u1357\u1343\u1363\u137d\u1345",
        "\u1260\u1273\u1275\u1270\u1275\u1295\u122b\u1275\u123d\u121b",
        "\u1273\u120b\u1295\u1270\u1275\u1295\u1273\u1275\u1275",
        "\u1275\u120b\u1295\u1273\u1272\u1273\u1225\u1273",
    },
    .num_rows = 4,
};

static void *keyboard_thread(void *arg)
{
    keyboard_ctx_t *ctx = (keyboard_ctx_t *)arg;

    printf("[KEYBOARD] On-screen keyboard active\n");

    /*
     * Production: create X11 window with transparency,
     * render key grid, handle clicks, simulate keypresses.
     *
     * XOpenDisplay → XCreateWindow → draw keys →
     * XGrabKey for hotkeys → XTestFakeKeyEvent for simulation
     */

    while (ctx->active) {
        usleep(100000); /* 100ms loop */
    }

    printf("[KEYBOARD] Stopped\n");
    return NULL;
}

int keyboard_init(keyboard_ctx_t *ctx, int ipc_fd)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->ipc_fd = ipc_fd;
    ctx->layout = &LAYOUT_QWERTY;
    ctx->win_x = 100;
    ctx->win_y = 500;
    ctx->win_w = 800;
    ctx->win_h = 300;
    ctx->active = false;
    return 0;
}

int keyboard_show(keyboard_ctx_t *ctx)
{
    if (ctx->active) return 0;
    ctx->active = true;
    ctx->keyboard_visible = true;
    return pthread_create(&ctx->thread, NULL, keyboard_thread, ctx);
}

void keyboard_hide(keyboard_ctx_t *ctx)
{
    ctx->active = false;
    ctx->keyboard_visible = false;
    pthread_join(ctx->thread, NULL);
}

void keyboard_press(keyboard_ctx_t *ctx, const char *key)
{
    if (!key || !ctx->active) return;

    /*
     * Production: XTestFakeKeyEvent(XStringToKeysym(key), True, 0);
     *            XTestFakeKeyEvent(XStringToKeysym(key), False, 0);
     */
    printf("[KEYBOARD] Press: %s\n", key);
}

void keyboard_destroy(keyboard_ctx_t *ctx)
{
    keyboard_hide(ctx);
}
