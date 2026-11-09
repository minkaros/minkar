/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#include "HIToolbox.h"
#include <pthread.h>
#include <stdlib.h>

typedef struct Mode {
    OptionBits options;
    struct Mode *below;
} Mode;

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static Mode *sTop;

void *PushSymbolicHotKeyMode(OptionBits inOptions) {
    Mode *mode = calloc(1, sizeof(Mode));
    if (mode == NULL)
        return NULL;
    pthread_mutex_lock(&sLock);
    mode->options = inOptions;
    mode->below = sTop;
    sTop = mode;
    pthread_mutex_unlock(&sLock);
    return mode;
}

void PopSymbolicHotKeyMode(void *inToken) {
    pthread_mutex_lock(&sLock);
    for (Mode **link = &sTop; *link != NULL; link = &(*link)->below)
        if (*link == inToken) {
            Mode *mode = *link;
            *link = mode->below;
            free(mode);
            break;
        }
    pthread_mutex_unlock(&sLock);
}

OptionBits GetSymbolicHotKeyMode(void) {
    OptionBits options;
    pthread_mutex_lock(&sLock);
    options = (sTop != NULL) ? sTop->options : kHIHotKeyModeAllEnabled;
    pthread_mutex_unlock(&sLock);
    return options;
}
