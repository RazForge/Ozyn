#include "version.h"
#include "ozayn.h"
#include "logger.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* ---- Version comparison ---- */

int ozayn_semver_compare(const ozayn_semver_t *a, const ozayn_semver_t *b) {
    if (!a || !b) return 0;

    if (a->major != b->major) return OZAYN_VER_INCOMPATIBLE;
    if (a->major > b->major)  return OZAYN_VER_GREATER;
    if (a->major < b->major)  return OZAYN_VER_LESS;

    if (a->minor != b->minor) return (a->minor > b->minor) ? OZAYN_VER_GREATER : OZAYN_VER_LESS;
    if (a->patch != b->patch) return (a->patch > b->patch) ? OZAYN_VER_GREATER : OZAYN_VER_LESS;

    /* Compare prerelease: empty (stable) > non-empty (pre-release) */
    int a_has_pre = (a->prerelease[0] != '\0');
    int b_has_pre = (b->prerelease[0] != '\0');
    if (a_has_pre && !b_has_pre) return OZAYN_VER_LESS;  /* pre-release < stable */
    if (!a_has_pre && b_has_pre) return OZAYN_VER_GREATER;
    if (a_has_pre && b_has_pre) {
        int cmp = strcmp(a->prerelease, b->prerelease);
        if (cmp > 0) return OZAYN_VER_GREATER;
        if (cmp < 0) return OZAYN_VER_LESS;
    }

    return OZAYN_VER_EQUAL;
}

int ozayn_semver_major_compatible(const ozayn_semver_t *a, const ozayn_semver_t *b) {
    if (!a || !b) return 0;
    return (a->major == b->major) ? 0 : 1;
}

/* ---- Parse "MAJOR.MINOR.PATCH[-prerelease]" ---- */

int ozayn_semver_parse(ozayn_semver_t *ver, const char *str) {
    if (!ver || !str) return -1;

    memset(ver, 0, sizeof(ozayn_semver_t));

    /* Parse MAJOR.MINOR.PATCH */
    const char *p = str;
    char *end;

    ver->major = (uint16_t)strtoul(p, &end, 10);
    if (end == p || *end != '.') return -1;
    p = end + 1;

    ver->minor = (uint16_t)strtoul(p, &end, 10);
    if (end == p || *end != '.') return -1;
    p = end + 1;

    ver->patch = (uint16_t)strtoul(p, &end, 10);
    if (end == p) return -1;
    p = end;

    /* Parse optional [-prerelease] */
    if (*p == '-') {
        p++;
        const char *start = p;
        while (*p && *p != '+' && *p != ' ') p++;
        size_t len = (size_t)(p - start);
        if (len >= sizeof(ver->prerelease)) len = sizeof(ver->prerelease) - 1;
        memcpy(ver->prerelease, start, len);
        ver->prerelease[len] = '\0';
    }

    /* Parse optional [+build] */
    if (*p == '+') {
        p++;
        const char *start = p;
        while (*p && *p != ' ') p++;
        size_t len = (size_t)(p - start);
        if (len >= sizeof(ver->build)) len = sizeof(ver->build) - 1;
        memcpy(ver->build, start, len);
        ver->build[len] = '\0';
    }

    return 0;
}

int ozayn_semver_format(const ozayn_semver_t *ver, char *buf, size_t buflen) {
    if (!ver || !buf || buflen == 0) return -1;

    if (ver->prerelease[0] && ver->build[0]) {
        snprintf(buf, buflen, "%u.%u.%u-%s+%s",
                 ver->major, ver->minor, ver->patch,
                 ver->prerelease, ver->build);
    } else if (ver->prerelease[0]) {
        snprintf(buf, buflen, "%u.%u.%u-%s",
                 ver->major, ver->minor, ver->patch,
                 ver->prerelease);
    } else if (ver->build[0]) {
        snprintf(buf, buflen, "%u.%u.%u+%s",
                 ver->major, ver->minor, ver->patch,
                 ver->build);
    } else {
        snprintf(buf, buflen, "%u.%u.%u",
                 ver->major, ver->minor, ver->patch);
    }

    return 0;
}

/* ---- Build identity ---- */

static char s_build_id[64] = "dev";
static char s_build_date[32] = __DATE__ " " __TIME__;

void ozayn_build_identity_init(ozayn_build_identity_t *id) {
    if (!id) return;
    memset(id, 0, sizeof(ozayn_build_identity_t));

    /* Version from compile-time defines */
    snprintf(id->version, sizeof(id->version), "%u.%u.%u",
             OZAYN_MAJOR, OZAYN_MINOR, OZAYN_PATCH);

    /* Platform */
    snprintf(id->platform, sizeof(id->platform), "%s", OZAYN_PLATFORM);

    /* Architecture */
#if defined(__x86_64__) || defined(_M_X64)
    snprintf(id->arch, sizeof(id->arch), "x86_64");
#elif defined(__aarch64__) || defined(_M_ARM64)
    snprintf(id->arch, sizeof(id->arch), "aarch64");
#elif defined(__i386__) || defined(_M_IX86)
    snprintf(id->arch, sizeof(id->arch), "i386");
#else
    snprintf(id->arch, sizeof(id->arch), "unknown");
#endif

    /* Compiler identification */
#if defined(__GNUC__) && !defined(__clang__)
    snprintf(id->compiler, sizeof(id->compiler), "gcc");
    snprintf(id->compiler_version, sizeof(id->compiler_version),
             "%d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(__clang__)
    snprintf(id->compiler, sizeof(id->compiler), "clang");
    snprintf(id->compiler_version, sizeof(id->compiler_version),
             "%d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(_MSC_VER)
    snprintf(id->compiler, sizeof(id->compiler), "msvc");
    snprintf(id->compiler_version, sizeof(id->compiler_version), "%d", _MSC_VER);
#else
    snprintf(id->compiler, sizeof(id->compiler), "unknown");
    snprintf(id->compiler_version, sizeof(id->compiler_version), "0");
#endif

    /* Build mode */
#ifdef NDEBUG
    snprintf(id->build_mode, sizeof(id->build_mode), "release");
#else
    snprintf(id->build_mode, sizeof(id->build_mode), "debug");
#endif

    /* Build ID and date */
    snprintf(id->build_id, sizeof(id->build_id), "%s", s_build_id);
    snprintf(id->build_date, sizeof(id->build_date), "%s", s_build_date);
    snprintf(id->build_time, sizeof(id->build_time), "%s", s_build_date);

    /* Default commit/branch if not injected */
    snprintf(id->commit, sizeof(id->commit), "unknown");
    snprintf(id->branch, sizeof(id->branch), "unknown");

    /* Build timestamp */
    id->build_timestamp = (uint64_t)time(NULL);
}

const char *ozayn_version_string(void) {
    return OZAYN_VERSION;
}

const char *ozayn_build_id_string(void) {
    return s_build_id;
}

int ozayn_full_version_string(char *buf, size_t buflen) {
    if (!buf || buflen == 0) return -1;
    snprintf(buf, buflen, "%s (build %s, %s %s)",
             OZAYN_VERSION, s_build_id, OZAYN_PLATFORM,
#ifdef NDEBUG
             "release"
#else
             "debug"
#endif
    );
    return 0;
}

uint16_t ozayn_version_major(void) { return OZAYN_MAJOR; }
uint16_t ozayn_version_minor(void) { return OZAYN_MINOR; }
uint16_t ozayn_version_patch(void) { return OZAYN_PATCH; }
const char *ozayn_version_prerelease(void) { return ""; }
