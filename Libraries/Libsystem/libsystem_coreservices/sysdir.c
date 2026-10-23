/*
 * Re-implementation of the sysdir API based on
 *    https://www.manpagez.com/man/3/sysdir/ and
 *    https://github.com/artichoke/sysdir-rs/blob/trunk/src/sys.rs
 * Copyright (C) 2026 Zoe Knox <zoe@pixin.net>
 * Copyright (c) 2026 The ravynOS contributors (enumeration rewrite)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#include "sysdir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>


static const char* _base_paths[__SYSDIR_DIRECTORY_MAX] = {
    "NULL",
    "Applications",
    "Applications/Demos",
    "Library/Developer/Applications",
    "Applications/Utilities",
    "Library",
    "Library/Developer",
    "Users",
    "Library/Documentation",
    "Documents",
    "Library/CoreServices",
    "Library/Autosave Information",
    "Desktop",
    "Library/Caches",
    "Library/Application Support",
    "Downloads",
    "Library/Input Methods",
    "Movies",
    "Music",
    "Pictures",
    "Library/Printers/PPDs",
    "Public",
    "Library/Preference Panes",
    "__special__", /* handled specially */
    "__special__" /* handled specially */
};

static char*
_domain_path(sysdir_search_path_domain_mask_t mask);

static unsigned int
_prepend_next_root(char* path);

sysdir_search_path_enumeration_state
sysdir_start_search_path_enumeration(
        sysdir_search_path_directory_t dir,
        sysdir_search_path_domain_mask_t domain_mask);

sysdir_search_path_enumeration_state
sysdir_get_next_search_path_enumeration(
        sysdir_search_path_enumeration_state state,
        char* path);



static char*
_domain_path(sysdir_search_path_domain_mask_t mask)
{
    switch (mask & 0x000F) {
        case SYSDIR_DOMAIN_MASK_USER: return (char*)"~";
        case SYSDIR_DOMAIN_MASK_LOCAL: return (char*)""; /* /, but avoid doubles */
        case SYSDIR_DOMAIN_MASK_NETWORK: return (char*)"/Network";
        case SYSDIR_DOMAIN_MASK_SYSTEM: return (char*)"/System";
        case SYSDIR_DOMAIN_MASK_ALL: break;
    }

    return NULL;
}

static unsigned int
_prepend_next_root(char* path)
{
    const char* next_root = getenv("NEXT_ROOT");
    unsigned int path_len = 0;

    if (!next_root)
        return 0;

    if (getuid() != geteuid() || getgid() != getegid())
        /* FIXME: or if codesigned with entitlements */
        return 0;

    path_len = strlen(next_root);
    memcpy(path, next_root, path_len);
    char* ch = &path[path_len];
    if (*ch != '/')
        path[path_len++] = '/';

    return path_len;
}
    

/*
 * Enumeration state (ravynOS rewrite; same API as macOS):
 *
 *   bits  0-15  the directory asked for (never 0, so a state is never 0)
 *   bits 16-19  the domains still to visit (SYSDIR_DOMAIN_MASK_*)
 *   bits 20-23  for ALL_APPLICATIONS / ALL_LIBRARIES, the next directory
 *               of the list within the current domain
 *
 * get_next fills path and returns a nonzero state while there is a result,
 * and returns 0 (path left empty) once there are no more; callers loop on
 * while ((state = sysdir_get_next_...(state, path)) != 0).
 */
#define STATE_DIR(s)		((s) & 0xFFFF)
#define STATE_DOMAINS(s)	(((s) >> 16) & 0xF)
#define STATE_INDEX(s)		(((s) >> 20) & 0xF)
#define MAKE_STATE(dir, domains, index) \
	((sysdir_search_path_enumeration_state)(dir) | ((domains) & 0xF) << 16 | \
	 ((index) & 0xF) << 20)

/* Domains in the order macOS visits them. */
static const sysdir_search_path_domain_mask_t _domain_order[] = {
    SYSDIR_DOMAIN_MASK_USER, SYSDIR_DOMAIN_MASK_LOCAL,
    SYSDIR_DOMAIN_MASK_NETWORK, SYSDIR_DOMAIN_MASK_SYSTEM,
};

static const sysdir_search_path_directory_t _all_applications[] = {
    SYSDIR_DIRECTORY_APPLICATION, SYSDIR_DIRECTORY_ADMIN_APPLICATION,
    SYSDIR_DIRECTORY_DEVELOPER_APPLICATION, SYSDIR_DIRECTORY_DEMO_APPLICATION,
};

static const sysdir_search_path_directory_t _all_libraries[] = {
    SYSDIR_DIRECTORY_LIBRARY, SYSDIR_DIRECTORY_DEVELOPER,
};

/* The directories a request covers: one, or the ALL_ list. */
static size_t
_directories(sysdir_search_path_directory_t dir,
             const sysdir_search_path_directory_t **list)
{
    static sysdir_search_path_directory_t single;

    if (dir == SYSDIR_DIRECTORY_ALL_APPLICATIONS) {
        *list = _all_applications;
        return sizeof(_all_applications) / sizeof(_all_applications[0]);
    }
    if (dir == SYSDIR_DIRECTORY_ALL_LIBRARIES) {
        *list = _all_libraries;
        return sizeof(_all_libraries) / sizeof(_all_libraries[0]);
    }
    single = dir;
    *list = &single;
    return 1;
}

/* Whether dir exists in domain (the user's own folders are only in ~). */
static int
_exists_in(sysdir_search_path_directory_t dir, sysdir_search_path_domain_mask_t domain)
{
    switch (dir) {
        case SYSDIR_DIRECTORY_DOCUMENT:
        case SYSDIR_DIRECTORY_DESKTOP:
        case SYSDIR_DIRECTORY_AUTOSAVED_INFORMATION:
        case SYSDIR_DIRECTORY_DOWNLOADS:
        case SYSDIR_DIRECTORY_MOVIES:
        case SYSDIR_DIRECTORY_MUSIC:
        case SYSDIR_DIRECTORY_PICTURES:
        case SYSDIR_DIRECTORY_SHARED_PUBLIC:
            return domain == SYSDIR_DOMAIN_MASK_USER;
        case SYSDIR_DIRECTORY_USER:
            return domain == SYSDIR_DOMAIN_MASK_LOCAL || domain == SYSDIR_DOMAIN_MASK_NETWORK;
        default:
            return dir > 0 && dir < SYSDIR_DIRECTORY_ALL_APPLICATIONS &&
                _base_paths[dir] != NULL;
    }
}

sysdir_search_path_enumeration_state
sysdir_start_search_path_enumeration(
        sysdir_search_path_directory_t dir,
        sysdir_search_path_domain_mask_t domain_mask)
{
    return MAKE_STATE(dir, domain_mask & 0xF, 0);
}

sysdir_search_path_enumeration_state
sysdir_get_next_search_path_enumeration(
        sysdir_search_path_enumeration_state state,
        char* path)
{
    sysdir_search_path_directory_t dir = STATE_DIR(state);
    unsigned int domains = STATE_DOMAINS(state);
    unsigned int index = STATE_INDEX(state);
    const sysdir_search_path_directory_t *list;
    size_t count = _directories(dir, &list);

    if (path)
        memset(path, 0, PATH_MAX);

    for (size_t d = 0; d < sizeof(_domain_order) / sizeof(_domain_order[0]); d++) {
        sysdir_search_path_domain_mask_t domain = _domain_order[d];

        if (!(domains & domain))
            continue;
        for (; index < count; index++) {
            sysdir_search_path_directory_t sub = list[index];
            if (!_exists_in(sub, domain))
                continue;

            if (path) {
                unsigned int len = _prepend_next_root(path);
                snprintf(path + len, PATH_MAX - len, "%s/%s", _domain_path(domain),
                         _base_paths[sub]);
            }
            /* Next time: the following directory, or the next domain. */
            if (index + 1 < count)
                return MAKE_STATE(dir, domains, index + 1);
            return MAKE_STATE(dir, domains & ~domain, 0);
        }
        domains &= ~domain;
        index = 0;
    }
    return 0;
}

#if TEST
#include <assert.h>

/* Every result for dir in mask, space-separated, the way callers loop. */
static const char *
results(sysdir_search_path_directory_t dir, sysdir_search_path_domain_mask_t mask)
{
    static char all[4096];
    char path[PATH_MAX];
    sysdir_search_path_enumeration_state state;

    all[0] = '\0';
    state = sysdir_start_search_path_enumeration(dir, mask);
    while ((state = sysdir_get_next_search_path_enumeration(state, path)) != 0) {
        size_t n = strlen(all);
        snprintf(all + n, sizeof(all) - n, "%s%s", n > 0 ? " " : "", path);
    }
    return all;
}

static int failures;

static void
expect(const char *what, const char *got, const char *want)
{
    if (strcmp(got, want) != 0) {
        printf("FAIL %s\n  got  [%s]\n  want [%s]\n", what, got, want);
        failures++;
    }
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    unsetenv("NEXT_ROOT");

    expect("system library", results(SYSDIR_DIRECTORY_LIBRARY, SYSDIR_DOMAIN_MASK_SYSTEM),
        "/System/Library");
    expect("local+system library", results(SYSDIR_DIRECTORY_LIBRARY,
        SYSDIR_DOMAIN_MASK_LOCAL | SYSDIR_DOMAIN_MASK_SYSTEM), "/Library /System/Library");
    expect("all libraries", results(SYSDIR_DIRECTORY_LIBRARY, SYSDIR_DOMAIN_MASK_ALL),
        "~/Library /Library /Network/Library /System/Library");
    expect("desktop", results(SYSDIR_DIRECTORY_DESKTOP, SYSDIR_DOMAIN_MASK_ALL), "~/Desktop");
    expect("desktop, system", results(SYSDIR_DIRECTORY_DESKTOP, SYSDIR_DOMAIN_MASK_SYSTEM), "");
    expect("users", results(SYSDIR_DIRECTORY_USER, SYSDIR_DOMAIN_MASK_ALL),
        "/Users /Network/Users");
    expect("user caches", results(SYSDIR_DIRECTORY_CACHES, SYSDIR_DOMAIN_MASK_USER),
        "~/Library/Caches");
    expect("all applications, local", results(SYSDIR_DIRECTORY_ALL_APPLICATIONS,
        SYSDIR_DOMAIN_MASK_LOCAL), "/Applications /Applications/Utilities "
        "/Library/Developer/Applications /Applications/Demos");
    expect("all libraries, user+local", results(SYSDIR_DIRECTORY_ALL_LIBRARIES,
        SYSDIR_DOMAIN_MASK_USER | SYSDIR_DOMAIN_MASK_LOCAL),
        "~/Library ~/Library/Developer /Library /Library/Developer");
    expect("no domains", results(SYSDIR_DIRECTORY_LIBRARY, 0), "");

    setenv("NEXT_ROOT", "/Volumes/Other", 1);
    expect("NEXT_ROOT", results(SYSDIR_DIRECTORY_LIBRARY, SYSDIR_DOMAIN_MASK_SYSTEM),
        "/Volumes/Other//System/Library");

    printf("%s: %d failures\n", failures ? "FAIL" : "PASS", failures);
    return failures != 0;
}

#endif
