/**
 * Ozayn Voice Engine — Speech recognition + command parsing.
 * Supports English and Amharic (am-ET).
 */

#ifndef OZAYN_VOICE_H
#define OZAYN_VOICE_H

#include "ozayn_core.h"
#include <pthread.h>

/* Voice engine context */
typedef struct {
    bool     active;
    pthread_t thread;
    int      ipc_fd;         /* Send recognized text here */
    int      sample_rate;    /* 16000 Hz */
    int      buffer_size;    /* 4096 samples */
    void     *model;         /* Vosk model handle */
    void     *recognizer;    /* Vosk recognizer handle */
    void     *microphone;    /* PaAudio stream */
} voice_ctx_t;

/* Command mapping */
typedef struct {
    const char *phrase;
    const char *command;
} voice_cmd_t;

/* Initialize voice engine */
int  voice_init(voice_ctx_t *ctx, int ipc_fd);

/* Start listening (spawns thread) */
int  voice_start(voice_ctx_t *ctx);

/* Stop listening */
void voice_stop(voice_ctx_t *ctx);

/* Cleanup */
void voice_destroy(voice_ctx_t *ctx);

#endif /* OZAYN_VOICE_H */
