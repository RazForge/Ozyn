/**
 * Ozayn Core — System Control
 * CPU, memory, disk, process management, file operations
 */

#include "ozayn_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#endif

int ozayn_sys_get_cpu_usage(void) {
#ifdef __linux__
    FILE* f = fopen("/proc/stat", "r");
    if (!f) return -1;

    char line[256];
    fgets(line, sizeof(line), f); /* skip "cpu " header */

    long user, nice, system, idle, iowait, irq, softirq, steal;
    sscanf(line, "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
    fclose(f);

    long total = user + nice + system + idle + iowait + irq + softirq + steal;
    long active = total - idle - iowait;

    /* Simple instantaneous reading (not ideal, but works for quick checks) */
    int usage = (int)((active * 100) / (total > 0 ? total : 1));
    return usage > 100 ? 100 : (usage < 0 ? 0 : usage);
#else
    return -1;
#endif
}

int ozayn_sys_get_memory_usage(long* used_bytes, long* total_bytes) {
#ifdef __linux__
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return -1;

    long total = 0, available = 0;
    char line[256];

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "MemTotal: %ld kB", &total) == 1) continue;
        if (sscanf(line, "MemAvailable: %ld kB", &available) == 1) continue;
    }
    fclose(f);

    if (total_bytes) *total_bytes = total * 1024;
    if (used_bytes) *used_bytes = (total - available) * 1024;
    return 0;
#else
    return -1;
#endif
}

int ozayn_sys_get_disk_usage(const char* path, long* used_bytes, long* total_bytes) {
#ifdef __linux__
    struct statvfs stat;
    if (statvfs(path ? path : "/", &stat) != 0) return -1;

    if (total_bytes) *total_bytes = (long)stat.f_blocks * stat.f_frsize;
    if (used_bytes) *used_bytes = (long)(stat.f_blocks - stat.f_bfree) * stat.f_frsize;
    return 0;
#else
    return -1;
#endif
}

int ozayn_sys_list_processes(const char* sort_by, int limit, char* buffer, int buffer_len) {
    if (!buffer || buffer_len <= 0) return -1;

#ifdef __linux__
    char cmd[256];
    if (!sort_by || strcmp(sort_by, "cpu") == 0) {
        snprintf(cmd, sizeof(cmd), "ps aux --sort=-%%cpu | head -%d", limit + 1);
    } else if (strcmp(sort_by, "mem") == 0) {
        snprintf(cmd, sizeof(cmd), "ps aux --sort=-%%mem | head -%d", limit + 1);
    } else {
        snprintf(cmd, sizeof(cmd), "ps -eo pid,%%cpu,%%mem,comm --sort=-%%cpu | head -%d", limit + 1);
    }

    FILE* pipe = popen(cmd, "r");
    if (!pipe) return -1;

    int offset = 0;
    offset += snprintf(buffer + offset, buffer_len - offset, "[");

    char line[512];
    int first = 1;
    int line_num = 0;

    while (fgets(line, sizeof(line), pipe) && offset < buffer_len - 100) {
        line_num++;
        if (line_num == 1) continue; /* skip header */

        /* Parse: PID %CPU %MEM COMMAND */
        int pid;
        float cpu, mem;
        char comm[256];

        if (sscanf(line, "%d %f %f %255[^\n]", &pid, &cpu, &mem, comm) >= 3) {
            if (!first) offset += snprintf(buffer + offset, buffer_len - offset, ",");
            /* Trim leading spaces from comm */
            char* p = comm;
            while (*p == ' ') p++;
            offset += snprintf(buffer + offset, buffer_len - offset,
                "{\"pid\":%d,\"cpu\":%.1f,\"mem\":%.1f,\"command\":\"%.200s\"}",
                pid, cpu, mem, p);
            first = 0;
        }
    }

    offset += snprintf(buffer + offset, buffer_len - offset, "]");
    pclose(pipe);
    return offset;
#else
    return -1;
#endif
}

int ozayn_sys_run_command(const char* cmd, int timeout_seconds, char* output, int output_len) {
    if (!cmd || !output || output_len <= 0) return -1;

#ifdef __linux__
    char full_cmd[4096];
    snprintf(full_cmd, sizeof(full_cmd), "%s", cmd);

    FILE* pipe = popen(full_cmd, "r");
    if (!pipe) return -1;

    int n = fread(output, 1, output_len - 1, pipe);
    output[n] = '\0';

    int status = pclose(pipe);
    return WEXITSTATUS(status);
#else
    return -1;
#endif
}

long ozayn_sys_get_uptime(void) {
#ifdef __linux__
    FILE* f = fopen("/proc/uptime", "r");
    if (!f) return -1;

    double uptime;
    fscanf(f, "%lf", &uptime);
    fclose(f);
    return (long)uptime;
#else
    return -1;
#endif
}

int ozayn_sys_get_hostname(char* buffer, int buffer_len) {
    if (!buffer || buffer_len <= 0) return -1;
    return gethostname(buffer, buffer_len) == 0 ? 0 : -1;
}

int ozayn_sys_list_files(const char* path, int show_hidden, char* buffer, int buffer_len) {
    if (!path || !buffer || buffer_len <= 0) return -1;

#ifdef __linux__
    DIR* dir = opendir(path);
    if (!dir) return -1;

    int offset = 0;
    offset += snprintf(buffer + offset, buffer_len - offset, "[");

    struct dirent* entry;
    int first = 1;
    int count = 0;

    while ((entry = readdir(dir)) != NULL && offset < buffer_len - 200) {
        if (!show_hidden && entry->d_name[0] == '.') continue;

        char fullpath[2048];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        struct stat st;
        stat(fullpath, &st);

        if (!first) offset += snprintf(buffer + offset, buffer_len - offset, ",");

        const char* type = S_ISDIR(st.st_mode) ? "directory" : "file";
        offset += snprintf(buffer + offset, buffer_len - offset,
            "{\"name\":\"%s\",\"type\":\"%s\",\"size\":%ld}",
            entry->d_name, type, (long)st.st_size);

        first = 0;
        count++;
    }

    offset += snprintf(buffer + offset, buffer_len - offset, "]");
    closedir(dir);
    return offset;
#else
    return -1;
#endif
}
