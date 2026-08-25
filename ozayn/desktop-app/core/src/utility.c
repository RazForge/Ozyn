/**
 * Ozayn Core — Utility Functions
 * Timestamps, system info, formatting
 */

#include "ozayn_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <sys/utsname.h>
#include <unistd.h>
#endif

int ozayn_util_timestamp(char* buffer, int buffer_len) {
    if (!buffer || buffer_len < 26) return -1;

    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);
    strftime(buffer, buffer_len, "%Y-%m-%dT%H:%M:%SZ", tm_info);
    return 0;
}

int ozayn_util_system_info(char* buffer, int buffer_len) {
    if (!buffer || buffer_len <= 0) return -1;

    int offset = 0;

#ifdef __linux__
    struct utsname uts;
    uname(&uts);

    offset += snprintf(buffer + offset, buffer_len - offset,
        "{\"os\":\"%s\",\"hostname\":\"%s\",\"arch\":\"%s\",\"kernel\":\"%s\"",
        uts.sysname, uts.nodename, uts.machine, uts.release);

    /* Uptime */
    FILE* f = fopen("/proc/uptime", "r");
    if (f) {
        double uptime;
        fscanf(f, "%lf", &uptime);
        fclose(f);
        int hours = (int)(uptime / 3600);
        int mins = (int)((uptime - hours * 3600) / 60);
        offset += snprintf(buffer + offset, buffer_len - offset,
            ",\"uptime_seconds\":%.0f,\"uptime_formatted\":\"%dh %dm\"", uptime, hours, mins);
    }

    /* CPU info */
    f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[256];
        char model[128] = "unknown";
        int cores = 0;
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "model name : %127[^\n]", model) == 1) continue;
            if (strstr(line, "processor")) cores++;
        }
        fclose(f);
        offset += snprintf(buffer + offset, buffer_len - offset,
            ",\"cpu_model\":\"%s\",\"cpu_cores\":%d", model, cores);
    }

    /* Memory */
    long used, total;
    if (ozayn_sys_get_memory_usage(&used, &total) == 0) {
        offset += snprintf(buffer + offset, buffer_len - offset,
            ",\"mem_used\":%ld,\"mem_total\":%ld", used, total);
    }

    /* PHP version (for ARWE integration) */
    offset += snprintf(buffer + offset, buffer_len - offset, ",\"core_version\":\"%s\"", OZAYN_CORE_VERSION);

    offset += snprintf(buffer + offset, buffer_len - offset, "}");
#else
    offset += snprintf(buffer + offset, buffer_len - offset,
        "{\"error\":\"unsupported platform\"}");
#endif

    return offset;
}
