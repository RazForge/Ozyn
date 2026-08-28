/**
 * Ozayn Control Room — Main Header
 * C/C++ controller for voice, gesture, and keyboard input.
 */

#ifndef OZAYN_CORE_H
#define OZAYN_CORE_H

#include <stdint.h>
#include <stdbool.h>

/* Version */
#define OZAYN_VERSION "1.0.0"

/* Command types sent over IPC */
#define CMD_VOICE_START     0x01
#define CMD_VOICE_STOP      0x02
#define CMD_VOICE_TEXT      0x03  /* Voice recognized text */
#define CMD_GESTURE_START   0x10
#define CMD_GESTURE_STOP    0x11
#define CMD_GESTURE_MOVE    0x12  /* Cursor position */
#define CMD_GESTURE_CLICK   0x13
#define CMD_GESTURE_SCROLL  0x14
#define CMD_KEYBOARD_SHOW   0x20
#define CMD_KEYBOARD_HIDE   0x21
#define CMD_KEYBOARD_KEY    0x22  /* Key press */
#define CMD_STATUS          0xF0
#define CMD_SHUTDOWN        0xFF

/* Response codes */
#define RESP_OK             0x00
#define RESP_ERROR          0x01
#define RESP_DATA           0x02

/* IPC message structure */
typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t length;
    uint32_t seq;
    char     data[4096];
} ozayn_msg_t;

/* Control room context */
typedef struct {
    int  ipc_fd;           /* IPC socket fd */
    bool voice_active;
    bool gesture_active;
    bool keyboard_visible;
    int  gesture_x;        /* Current cursor X (normalized 0-65535) */
    int  gesture_y;        /* Current cursor Y (normalized 0-65535) */
    int  gesture_gear;     /* 0=STOP 1=PRECISION 2=NORMAL 3=FAST 4=TURBO */
    float dwell_progress;  /* 0.0 - 1.0 */
    bool dwell_click;
} ozayn_ctx_t;

/* Initialize control room */
int  ozayn_init(ozayn_ctx_t *ctx);

/* Shutdown control room */
void ozayn_shutdown(ozayn_ctx_t *ctx);

/* Run main loop (blocks) */
int  ozayn_run(ozayn_ctx_t *ctx);

#endif /* OZAYN_CORE_H */
