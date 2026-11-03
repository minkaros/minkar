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

/*
 * One DisplayPort AUX transaction through ProcessAuxChannelTransaction
 * (amdgpu_atombios_dp_aux_transfer): request is the DP AUX request (native
 * read 0x9, native write 0x8, I2C read 0x1, I2C write 0x0, | 0x4 for
 * I2C middle-of-transaction), at most 16 bytes. Returns the bytes read
 * (or written), a negative errno on a timeout or an error; *reply is the
 * sink's reply (bits 1:0 native ACK/NACK/DEFER, bits 3:2 I2C's).
 */
int minkar_atom_aux(struct minkar_atom *a, uint8_t channel, uint8_t hpd, uint8_t request,
    uint32_t address, uint8_t *buffer, uint8_t size, uint8_t *reply);
/*
 * One hardware I2C transaction through ProcessI2cChannelTransaction
 * (amdgpu_atombios_i2c_process_i2c_ch), on DDC line (the connector's I2C
 * id) to slave (7-bit): a write of reg and up to 2 more bytes, or a read
 * of up to 255 bytes from reg on into buffer (the table writes reg
 * first). 0, or a negative errno.
 */
int minkar_atom_i2c(struct minkar_atom *a, uint8_t line, uint8_t slave, int write, uint8_t reg,
    const uint8_t *more, uint8_t moreBytes, uint8_t *buffer, uint8_t readBytes);

/* The last AUX transaction's interpreter result and data length, for
 * bring-up. */
void minkar_atom_aux_last(struct minkar_atom *a, int *execute, uint8_t *dataOut);
/* Register accesses of the tables while tracing is on, for bring-up:
 * each entry is reg | (write ? 1u << 31 : 0), then the value. */
#define MINKAR_ATOM_TRACE_MAX	512
void minkar_atom_trace(struct minkar_atom *a, int on);
unsigned minkar_atom_trace_get(struct minkar_atom *a, uint32_t *entries, unsigned max);

/*
 * A display path and a mode on it, for setting the mode through the
 * VBIOS's tables (SetPixelClock 1.6, SetCRTC_UsingDTDTiming, EnableCRTC,
 * BlankCRTC, SelectCRTC_Source 1.2, DIGxEncoderControl 1.4,
 * UNIPHYTransmitterControl 1.5), with the parameters amdgpu's
 * atombios_crtc.c and atombios_encoders.c give them for a TMDS sink.
 */
struct minkar_atom_output {
	uint8_t crtc;			/* 0-5 */
	uint8_t pll;			/* ATOM_PPLL1 0, PPLL2 1, PPLL0 2 */
	uint8_t encoderID;		/* the encoder object's id (UNIPHY 0x1e, 0x20, 0x21) */
	uint8_t linkB;			/* the encoder object's enum is 2 */
	uint8_t connectorID;		/* the connector object's id */
	uint8_t hpd;			/* 0-5 */
	uint8_t hdmi;			/* else DVI */
};

struct minkar_atom_timing {
	uint32_t clock;			/* kHz */
	uint16_t hactive, hfront, hsync, hback;
	uint16_t vactive, vfront, vsync, vback;
	uint8_t  hpositive, vpositive;
	uint16_t refDiv, fbDiv, fbFrac, postDiv;	/* fbFrac in tenths */
};

/* The DIG front end Linux picks for an encoder (dce_v6_0_pick_dig_encoder). */
int minkar_atom_dig_for(const struct minkar_atom_output *o);
int minkar_atom_crtc_blank(struct minkar_atom *a, int crtc, int blank);
int minkar_atom_crtc_enable(struct minkar_atom *a, int crtc, int enable);
int minkar_atom_set_pixel_clock(struct minkar_atom *a, const struct minkar_atom_output *o,
    const struct minkar_atom_timing *t);
int minkar_atom_set_timing(struct minkar_atom *a, int crtc, const struct minkar_atom_timing *t);
int minkar_atom_select_crtc_source(struct minkar_atom *a, const struct minkar_atom_output *o);
int minkar_atom_dig_setup(struct minkar_atom *a, const struct minkar_atom_output *o, uint32_t clock);
/* ATOM_TRANSMITTER_ACTION_ENABLE (1) or _DISABLE (0). */
int minkar_atom_transmitter(struct minkar_atom *a, const struct minkar_atom_output *o, int action,
    uint32_t clock);

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
