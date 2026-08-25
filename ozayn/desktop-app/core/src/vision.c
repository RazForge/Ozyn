/**
 * Ozayn Core — Vision System
 * Screen capture, face detection, gesture recognition
 */

#include "ozayn_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#endif

static int vision_initialized = 0;

int ozayn_vision_init(void) {
    if (vision_initialized) return 0;
    vision_initialized = 1;
    return 0;
}

void ozayn_vision_shutdown(void) {
    vision_initialized = 0;
}

int ozayn_vision_capture_screen(const char* output_path) {
    if (!output_path) return -1;

#ifdef __linux__
    /* Use scrot or import (ImageMagick) for screenshot */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "scrot '%s' 2>/dev/null || import -window root '%s' 2>/dev/null",
             output_path, output_path);
    return system(cmd) == 0 ? 0 : -1;
#elif __APPLE__
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "screencapture '%s'", output_path);
    return system(cmd) == 0 ? 0 : -1;
#else
    return -1;
#endif
}

int ozayn_vision_detect_faces(const char* image_path, int* count) {
    if (!image_path || !count) return -1;
    *count = 0;

    /* Use Python OpenCV for face detection via subprocess */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "python3 -c \""
        "import cv2; "
        "cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml'); "
        "img = cv2.imread('%s'); "
        "gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY); "
        "faces = cascade.detectMultiScale(gray, 1.3, 5); "
        "print(len(faces)); "
        "\" 2>/dev/null",
        image_path);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) return -1;

    char result[32];
    if (fgets(result, sizeof(result), pipe)) {
        *count = atoi(result);
    }
    pclose(pipe);
    return 0;
}

int ozayn_vision_analyze_image(const char* image_path, char* result, int result_len) {
    if (!image_path || !result || result_len <= 0) return -1;

    /* Basic image info using Python */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "python3 -c \""
        "import cv2, json; "
        "img = cv2.imread('%s'); "
        "h, w = img.shape[:2]; "
        "info = {'width': w, 'height': h, 'channels': img.shape[2] if len(img.shape) > 2 else 1}; "
        "print(json.dumps(info)); "
        "\" 2>/dev/null",
        image_path);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) return -1;

    int n = fread(result, 1, result_len - 1, pipe);
    result[n] = '\0';
    pclose(pipe);

    return n;
}

int ozayn_vision_detect_gesture(int camera_id, char* result, int result_len) {
    if (!result || result_len <= 0) return -1;

    /* Placeholder for gesture detection - would use mediapipe/OpenCV */
    snprintf(result, result_len,
        "{\"gesture\":\"none\",\"confidence\":0.0,\"camera\":%d}", camera_id);
    return strlen(result);
}
