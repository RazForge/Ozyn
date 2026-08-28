/**
 * Ozayn Voice Engine — Speech recognition + command parsing.
 * Uses ALSA for audio capture. Vosk for offline recognition.
 * Supports English and Amharic.
 */

#include "voice.h"
#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ── Amharic + English command map ── */
static const voice_cmd_t CMD_MAP[] = {
    /* English */
    {"login",        "LOGIN"},     {"sign in",      "LOGIN"},
    {"home",         "HOME"},      {"go home",      "HOME"},
    {"back",         "BACK"},      {"go back",      "BACK"},
    {"next",         "NEXT"},      {"submit",       "SUBMIT"},
    {"confirm",      "CONFIRM"},   {"ok",           "CONFIRM"},
    {"yes",          "CONFIRM"},   {"cancel",       "CANCEL"},
    {"no",           "CANCEL"},    {"close",        "CLOSE"},
    {"exit",         "CLOSE"},     {"keyboard",     "KEYBOARD"},
    {"settings",     "SETTINGS"},  {"click",        "CLICK"},
    {"scroll up",    "SCROLL_UP"}, {"scroll down",  "SCROLL_DOWN"},
    {"lock",         "LOCK"},      {"unlock",       "UNLOCK"},
    {"chat",         "PAGE_CHAT"}, {"projects",     "PAGE_PROJECTS"},
    {"tasks",        "PAGE_TASKS"}, {"dashboard",   "PAGE_DASHBOARD"},

    /* Amharic (Ge'ez) */
    {"\u1275\u122b\u1295",         "LOGIN"},     /* ክፈት - open/login */
    {"\u1273\u1271\u122b",         "LOGIN"},     /* መግባት - login */
    {"\u1265\u1270\u121b",         "CONFIRM"},   /* አዎ - yes */
    {"\u1275\u123d\u1272",         "CLOSE"},     /* ዝጋ - close */
    {"\u1273\u120b\u1295",         "KEYBOARD"},  /* መሳፍያ - keyboard */
    {"\u1295\u1233\u1275",         "SETTINGS"},  /* ቅንብር - settings */
    {"\u1273\u1225",               "CLICK"},     /* ጠቅ - click */
    {"\u1260\u1275\u123d",         "SCROLL_UP"}, /* ወደ ላይ - scroll up */
    {"\u1275\u120b\u1295",         "PAGE_CHAT"}, /* ውይይት - chat */
    {"\u1273\u122a",               "PAGE_TASKS"},/* ስራ - tasks */
    {"\u1270\u1275\u1235",         "MODE_FAST"}, /* ፈጣን - fast */

    {NULL, NULL}
};

static const char *match_command(const char *text)
{
    if (!text) return NULL;

    /* Exact match */
    for (int i = 0; CMD_MAP[i].phrase; i++) {
        if (strcmp(text, CMD_MAP[i].phrase) == 0)
            return CMD_MAP[i].command;
    }

    /* Substring match */
    for (int i = 0; CMD_MAP[i].phrase; i++) {
        if (strstr(text, CMD_MAP[i].phrase))
            return CMD_MAP[i].command;
    }

    return NULL;
}

static void *voice_thread(void *arg)
{
    voice_ctx_t *ctx = (voice_ctx_t *)arg;

    printf("[VOICE] Listening... (English + Amharic)\n");

    /* Main listening loop — reads audio, sends to recognizer */
    while (ctx->active) {
        /*
         * In production: read from ALSA → feed to Vosk recognizer
         * Vosk returns partial/final text. We match against CMD_MAP.
         *
         * For now: stub that simulates recognition cycle.
         * Replace with real Vosk integration:
         *   vosk_recognizer_accept_waveform(rec, buf, len);
         *   const char *result = vosk_recognizer_result(rec);
         */

        usleep(100000); /* 100ms loop */
    }

    printf("[VOICE] Stopped\n");
    return NULL;
}

int voice_init(voice_ctx_t *ctx, int ipc_fd)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->ipc_fd = ipc_fd;
    ctx->sample_rate = 16000;
    ctx->buffer_size = 4096;
    ctx->active = false;
    return 0;
}

int voice_start(voice_ctx_t *ctx)
{
    if (ctx->active) return 0;
    ctx->active = true;
    return pthread_create(&ctx->thread, NULL, voice_thread, ctx);
}

void voice_stop(voice_ctx_t *ctx)
{
    ctx->active = false;
    pthread_join(ctx->thread, NULL);
}

void voice_destroy(voice_ctx_t *ctx)
{
    voice_stop(ctx);
}
