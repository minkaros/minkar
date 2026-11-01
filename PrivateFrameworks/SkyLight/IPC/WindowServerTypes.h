/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* C types of the MIG interface (IPC/WindowServer.defs). Private. */

#ifndef SKYLIGHT_WINDOWSERVERTYPES_H
#define SKYLIGHT_WINDOWSERVERTYPES_H

#include <stdint.h>
#include "SkyLight.h"

typedef uint32_t ws_display_list_t[32];
typedef SLSDisplayMode ws_display_mode_list_t[kSLSMaxDisplayModes];
typedef SLSDisplayConfigOp ws_display_config_list_t[kSLSMaxDisplayConfigOps];
/* Red, then green, then blue: count entries each. */
typedef float ws_gamma_table_t[3 * kSLSGammaTableCapacity];
typedef SLSWindowID ws_window_list_t[kSLSMaxWindowList];
typedef SLSIntRect ws_damage_t[kSLSMaxDamageRects];
typedef SLSEvent ws_event_list_t[kSLSMaxEvents];

/* The message the server sends a client's port when its event queue
 * stops being empty: a header only. */
#define WS_EVENTS_PENDING_ID	0x534c5345	/* 'SLSE' */

#endif
