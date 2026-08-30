#ifndef OZAYN_PLATFORM_DETECT_H
#define OZAYN_PLATFORM_DETECT_H

/*
 * platform_detect.h — Compile-time platform detection.
 *
 * Defines OZAYN_OS and OZAYN_ARCH for the current target.
 * Only one OS and one architecture should be active.
 */

/* ---- Operating System ---- */

#if defined(_WIN32) || defined(_WIN64)
    #define OZAYN_OS_WINDOWS 1
    #define OZAYN_OS_NAME    "Windows"
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #define OZAYN_OS_MACOS 1
    #define OZAYN_OS_NAME  "macOS"
#elif defined(__linux__)
    #define OZAYN_OS_LINUX 1
    #define OZAYN_OS_NAME  "Linux"
#else
    #define OZAYN_OS_UNKNOWN 1
    #define OZAYN_OS_NAME    "Unknown"
#endif

/* ---- Architecture ---- */

#if defined(__x86_64__) || defined(_M_X64)
    #define OZAYN_ARCH_X86_64 1
    #define OZAYN_ARCH_NAME   "x86_64"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define OZAYN_ARCH_ARM64 1
    #define OZAYN_ARCH_NAME  "aarch64"
#elif defined(__i386__) || defined(_M_IX86)
    #define OZAYN_ARCH_X86 1
    #define OZAYN_ARCH_NAME "x86"
#else
    #define OZAYN_ARCH_UNKNOWN 1
    #define OZAYN_ARCH_NAME    "unknown"
#endif

#endif
