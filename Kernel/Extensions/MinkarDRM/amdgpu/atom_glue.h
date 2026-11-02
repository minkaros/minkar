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

/*
 * The AtomBIOS interpreter behind a plain C interface, so the IOKit
 * drivers (C++) never include Linux headers.
 */
#ifndef _MINKAR_ATOM_GLUE_H
#define _MINKAR_ATOM_GLUE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t (*minkar_reg_read_t)(void *gpu, uint32_t reg);
typedef void (*minkar_reg_write_t)(void *gpu, uint32_t reg, uint32_t value);

struct minkar_vbios_info {
	char part_number[64];
	char version[64];
	char date[32];
	char name[64];
};

struct minkar_atom;

/* NULL unless bios is an AtomBIOS image; bios stays the caller's and must
 * outlive the result. */
struct minkar_atom *minkar_atom_open(void *gpu, minkar_reg_read_t read,
    minkar_reg_write_t write, void *bios, struct minkar_vbios_info *info);
/* Runs command table index with params (dwords); 0 or a negative errno. */
int minkar_atom_execute(struct minkar_atom *a, int index, uint32_t *params, int size);
void minkar_atom_close(struct minkar_atom *a);

/* Display tables (DCE 6 and later), by crtc index; 0 or a negative errno. */
enum minkar_atom_table {
	MINKAR_ATOM_ENABLE_SCALER,
	MINKAR_ATOM_SET_CRTC_OVERSCAN,
	MINKAR_ATOM_UPDATE_CRTC_DOUBLE_BUFFER,
};
int minkar_atom_has_table(struct minkar_atom *a, enum minkar_atom_table t);
/* Holds (lock 1) or releases (0) the crtc's double-buffered registers, so
 * a set of changes takes effect in one frame. */
int minkar_atom_crtc_lock(struct minkar_atom *a, int crtc, int lock);
/* Borders around the scaled picture, in pixels of the display's mode. */
int minkar_atom_overscan(struct minkar_atom *a, int crtc, int left, int right, int top, int bottom);
/* Scales the viewport to the display's mode (expand), or passes it through. */
int minkar_atom_scaler(struct minkar_atom *a, int crtc, int expand);

#ifdef __cplusplus
}
#endif

#endif
