/**
 * Ozayn Control Room — Main entry point.
 * C/C++ controller: voice + gesture + keyboard.
 * Communicates with Ozayn system via Unix domain sockets.
 */

#include "ozayn_core.h"
#include "ipc.h"
#include "voice.h"
#include "gesture.h"
#include "keyboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static volatile int g_running = 1;

static void on_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

static void handle_command(ozayn_ctx_t *ctx, const ozayn_msg_t *msg)
{
    switch (msg->type) {
    case CMD_VOICE_START:
        printf("[CTRL] Voice started\n");
        ctx->voice_active = true;
        break;
    case CMD_VOICE_STOP:
        printf("[CTRL] Voice stopped\n");
        ctx->voice_active = false;
        break;
    case CMD_VOICE_TEXT:
        printf("[CTRL] Voice: %.*s\n", msg->length, msg->data);
        break;
    case CMD_GESTURE_START:
        printf("[CTRL] Gesture started\n");
        ctx->gesture_active = true;
        break;
    case CMD_GESTURE_STOP:
        printf("[CTRL] Gesture stopped\n");
        ctx->gesture_active = false;
        break;
    case CMD_GESTURE_MOVE:
        if (msg->length >= 8) {
            ctx->gesture_x = (msg->data[0] << 8) | msg->data[1];
            ctx->gesture_y = (msg->data[2] << 8) | msg->data[3];
            ctx->gesture_gear = msg->data[4];
            ctx->dwell_progress = (float)msg->data[5] / 100.0f;
        }
        break;
    case CMD_GESTURE_CLICK:
        printf("[CTRL] Gesture click\n");
        break;
    case CMD_KEYBOARD_SHOW:
        printf("[CTRL] Keyboard shown\n");
        ctx->keyboard_visible = true;
        break;
    case CMD_KEYBOARD_HIDE:
        printf("[CTRL] Keyboard hidden\n");
        ctx->keyboard_visible = false;
        break;
    case CMD_KEYBOARD_KEY:
        printf("[CTRL] Key: %.*s\n", msg->length, msg->data);
        break;
    case CMD_SHUTDOWN:
        printf("[CTRL] Shutdown requested\n");
        g_running = 0;
        break;
    default:
        break;
    }
}

int ozayn_init(ozayn_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->gesture_x = 32768;
    ctx->gesture_y = 32768;
    ctx->gesture_gear = 2;
    return 0;
}

void ozayn_shutdown(ozayn_ctx_t *ctx)
{
    ipc_close(ctx->ipc_fd);
    ctx->ipc_fd = -1;
}

int ozayn_run(ozayn_ctx_t *ctx)
{
    voice_ctx_t    voice;
    gesture_ctx_t  gesture;
    keyboard_ctx_t keyboard;

    /* Initialize subsystems */
    voice_init(&voice, ctx->ipc_fd);
    gesture_init(&gesture, ctx->ipc_fd);
    keyboard_init(&keyboard, ctx->ipc_fd);

    /* Start all three */
    voice_start(&voice);
    gesture_start(&gesture);
    keyboard_show(&keyboard);

    printf("[CTRL] Control room active — voice + gesture + keyboard\n");
    printf("[CTRL] Press Ctrl+C to stop\n");

    /* Main loop: receive commands from Ozayn system */
    while (g_running) {
        ozayn_msg_t msg;
        if (ipc_recv(ctx->ipc_fd, &msg) == 0) {
            handle_command(ctx, &msg);
        }
    }

    /* Cleanup */
    voice_destroy(&voice);
    gesture_destroy(&gesture);
    keyboard_hide(&keyboard);
    ozayn_shutdown(ctx);

    printf("[CTRL] Control room shut down\n");
    return 0;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    printf("╔══════════════════════════════════════╗\n");
    printf("║     OZAYN Control Room v%s         ║\n", OZAYN_VERSION);
    printf("║  Voice + Gesture + Keyboard (C/C++) ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    ozayn_ctx_t ctx;
    ozayn_init(&ctx);

    /* Start IPC server */
    ctx.ipc_fd = ipc_server_init();
    if (ctx.ipc_fd < 0) {
        fprintf(stderr, "[ERROR] Failed to start IPC server\n");
        return 1;
    }

    return ozayn_run(&ctx);
}
