/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef __HITOOLBOX__
#define __HITOOLBOX__

#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Symbolic hot keys (the system's own shortcuts) disabled while a mode
   is pushed, so an application can take the keys. Minkar's shortcuts
   are the window server's and the menu bar's: the modes are counted and
   handed out; nothing is disabled yet. */
enum {
    kHIHotKeyModeAllEnabled = 0,
    kHIHotKeyModeAllDisabled = (1 << 0),
    kHIHotKeyModeAllDisabledExceptUniversalAccess = (1 << 1)
};

void *PushSymbolicHotKeyMode(OptionBits inOptions);
void PopSymbolicHotKeyMode(void *inToken);
OptionBits GetSymbolicHotKeyMode(void);

#ifdef __cplusplus
}
#endif

#endif
