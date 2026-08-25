/**
 * Ozayn Core — ML Engine
 * Model loading, inference, and management
 */

#include "ozayn_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_MODELS 16
#define MAX_MODEL_PATH 512

typedef struct {
    int id;
    char path[MAX_MODEL_PATH];
    int input_size;
    int output_size;
    int loaded;
    void* data;
} ModelEntry;

static ModelEntry models[MAX_MODELS];
static int model_count = 0;

const char* ozayn_version(void) {
    return OZAYN_CORE_VERSION;
}

void* ozayn_ml_load_model(const char* path) {
    if (!path || model_count >= MAX_MODELS) return NULL;

    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return NULL;
    }

    void* data = malloc(size);
    if (!data) {
        fclose(f);
        return NULL;
    }

    fread(data, 1, size, f);
    fclose(f);

    ModelEntry* entry = &models[model_count++];
    entry->id = model_count;
    strncpy(entry->path, path, MAX_MODEL_PATH - 1);
    entry->data = data;
    entry->loaded = 1;
    entry->input_size = 0;
    entry->output_size = 0;

    return (void*)(long)entry->id;
}

int ozayn_ml_predict(void* model, const float* input, int input_len,
                     float* output, int output_len) {
    if (!model || !input || !output) return -1;

    int id = (int)(long)model;
    if (id < 1 || id > model_count) return -1;

    ModelEntry* entry = &models[id - 1];
    if (!entry->loaded) return -1;

    /* Placeholder: actual inference would use TensorFlow/ONNX Runtime/etc.
     * For now, copy input to output as passthrough */
    int copy_len = input_len < output_len ? input_len : output_len;
    memcpy(output, input, copy_len * sizeof(float));

    return copy_len;
}

void ozayn_ml_free_model(void* model) {
    if (!model) return;

    int id = (int)(long)model;
    if (id < 1 || id > model_count) return;

    ModelEntry* entry = &models[id - 1];
    if (entry->data) {
        free(entry->data);
        entry->data = NULL;
    }
    entry->loaded = 0;
}

int ozayn_ml_list_models(char* buffer, int buffer_len) {
    if (!buffer || buffer_len <= 0) return -1;

    int offset = 0;
    offset += snprintf(buffer + offset, buffer_len - offset, "[");

    for (int i = 0; i < model_count; i++) {
        if (models[i].loaded) {
            if (i > 0) offset += snprintf(buffer + offset, buffer_len - offset, ",");
            offset += snprintf(buffer + offset, buffer_len - offset,
                "{\"id\":%d,\"path\":\"%s\",\"loaded\":%s}",
                models[i].id, models[i].path,
                models[i].loaded ? "true" : "false");
        }
    }

    offset += snprintf(buffer + offset, buffer_len - offset, "]");
    return offset;
}
