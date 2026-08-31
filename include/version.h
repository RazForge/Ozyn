#ifndef OZAYN_VERSION_H
#define OZAYN_VERSION_H

#include <stdint.h>
#include <time.h>

/*
 * version.h — Semantic Versioning & Build Identity (Stage 30).
 *
 * Self-contained header — no circular includes.
 * Provides version comparison, build identity, and release identification.
 *
 * Versioning follows SemVer: MAJOR.MINOR.PATCH[-prerelease][+build]
 *
 * Every release is uniquely identifiable by version + build ID + commit.
 */

/* ---- Semantic version ---- */

typedef struct {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
    char     prerelease[32];  /* e.g. "rc1", "beta2", "" for stable */
    char     build[64];       /* build metadata, e.g. "20260831.abc1234" */
} ozayn_semver_t;

/* ---- Version comparison result ---- */

typedef enum {
    OZAYN_VER_EQUAL        =  0,
    OZAYN_VER_GREATER      =  1,
    OZAYN_VER_LESS         = -1,
    OZAYN_VER_INCOMPATIBLE = -2,  /* different major */
} ozayn_ver_cmp_t;

/* ---- Build identity ---- */

typedef struct {
    char     version[32];     /* e.g. "0.1.0" */
    char     build_id[64];    /* unique build identifier */
    char     commit[16];      /* git commit hash (short) */
    char     branch[64];      /* git branch name */
    char     build_date[32];  /* ISO 8601 build timestamp */
    char     build_time[32];  /* human-readable build time */
    uint64_t build_timestamp; /* unix epoch of build */
    char     platform[32];    /* e.g. "linux", "macos", "windows" */
    char     arch[16];        /* e.g. "x86_64", "aarch64" */
    char     compiler[64];    /* e.g. "gcc-13.2.0" */
    char     compiler_version[32];
    char     build_mode[16];  /* "debug" or "release" */
    uint32_t source_hash;     /* hash of source tree for reproducibility */
} ozayn_build_identity_t;

/* ---- Version comparison ---- */

/* Compare two semantic versions. Returns OZAYN_VER_EQUAL, _GREATER, _LESS. */
int ozayn_semver_compare(const ozayn_semver_t *a, const ozayn_semver_t *b);

/* Compare major version only: 0=compatible, 1=incompatible */
int ozayn_semver_major_compatible(const ozayn_semver_t *a, const ozayn_semver_t *b);

/* Parse "MAJOR.MINOR.PATCH" string into semver struct */
int ozayn_semver_parse(ozayn_semver_t *ver, const char *str);

/* Format semver into string buffer */
int ozayn_semver_format(const ozayn_semver_t *ver, char *buf, size_t buflen);

/* ---- Build identity ---- */

/* Initialize build identity from compile-time defines */
void ozayn_build_identity_init(ozayn_build_identity_t *id);

/* Get current version string */
const char *ozayn_version_string(void);

/* Get build ID string */
const char *ozayn_build_id_string(void);

/* Get full version string (version+build metadata) */
int ozayn_full_version_string(char *buf, size_t buflen);

/* ---- Version queries ---- */

uint16_t ozayn_version_major(void);
uint16_t ozayn_version_minor(void);
uint16_t ozayn_version_patch(void);
const char *ozayn_version_prerelease(void);

#endif /* OZAYN_VERSION_H */
