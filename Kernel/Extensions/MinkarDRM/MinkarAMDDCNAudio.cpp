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
 * Audio over HDMI on Navi 23 (DCN 3.0.2; Docs/GPU-RDNA2.md, R2), the
 * display's half, as amdgpu's dce_audio.c, dcn30_afmt.c and the DCN
 * stream encoders (MIT) program it: the HD audio codec's endpoint told
 * what the sink plays (its ELD, from the EDID's CEA audio blocks) and
 * enabled, the audio DTO at the pipe's pixel clock, and the HDMI encoder
 * sending audio samples, clock regeneration (N/CTS) and the audio
 * InfoFrame. The audio half is MinkarHDA's, on the GPU's HD audio
 * function, through the endpoint's pin.
 */

#include <IOKit/IOLib.h>
#include "MinkarAMDFramebuffer.h"
#include "amd_dcn302_regs.h"

#define LOG(fmt, ...)	IOLog("MinkarAMDFramebuffer: " fmt "\n", ##__VA_ARGS__)

#define M(reg, f)	DCN302_##reg##__##f##_MASK
#define S(reg, f)	DCN302_##reg##__##f##__SHIFT
#define PUT(reg, f, x)	(((uint32_t)(x) << S(reg, f)) & M(reg, f))

/* The endpoint's own registers, through its index and data pair
 * (dce_11_0_d.h and dce_11_0_sh_mask.h, which amdgpu uses on DCN too). */
enum {
	ixChannelSpeaker = 0x25, ixACPData = 0x27, ixAudioDescriptor0 = 0x28,
	ixResponseLipsync = 0x37, ixResponseHBR = 0x38, ixSinkInfo0 = 0x3a,
	ixHotPlugControl = 0x54,
	kHotPlugClockGatingDisable = 0x1, kHotPlugAudioEnabled = 0x80000000,
	kSpeakerAllocationMask = 0x7f, kHDMIConnection = 0x10000, kDPConnection = 0x20000,
	kExtraConnectionInfoMask = 0xfc0000, kLFEPlaybackLevelMask = 0x3000000,
};

uint32_t
MinkarAMDFramebuffer::azRead(unsigned endpoint, uint32_t index)
{
	writeReg(dcn302_AZF0ENDPOINT_AZALIA_F0_CODEC_ENDPOINT_INDEX[endpoint], index);
	return readReg(dcn302_AZF0ENDPOINT_AZALIA_F0_CODEC_ENDPOINT_DATA[endpoint]);
}

void
MinkarAMDFramebuffer::azWrite(unsigned endpoint, uint32_t index, uint32_t value)
{
	writeReg(dcn302_AZF0ENDPOINT_AZALIA_F0_CODEC_ENDPOINT_INDEX[endpoint], index);
	writeReg(dcn302_AZF0ENDPOINT_AZALIA_F0_CODEC_ENDPOINT_DATA[endpoint], value);
}

static void
update(MinkarAMDFramebuffer *fb, uint32_t reg, uint32_t mask, uint32_t value)
{
	fb->writeReg(reg, (fb->readReg(reg) & ~mask) | (value & mask));
}

/* The EDID's CEA-861 audio: its short audio descriptors (3 bytes each, at
 * most 16) and its speaker allocation. */
static unsigned
edidAudio(const uint8_t *e, unsigned bytes, uint8_t sads[16][3], uint8_t *speakers)
{
	unsigned n = 0;

	*speakers = 0;
	for (unsigned b = 128; b + 128 <= bytes; b += 128) {
		const uint8_t *x = e + b;
		if (x[0] != 0x02 || x[2] < 4 || x[2] > 127)
			continue;
		if (!(x[3] & 0x40))
			continue;		/* "basic audio" not supported */
		for (unsigned p = 4; p < x[2]; ) {
			unsigned tag = x[p] >> 5, len = x[p] & 0x1f;
			if (p + 1 + len > x[2])
				break;
			if (tag == 1)
				for (unsigned k = 0; k + 3 <= len && n < 16; k += 3, n++)
					bcopy(x + p + 1 + k, sads[n], 3);
			if (tag == 4 && len >= 1)
				*speakers = x[p + 1];
			p += 1 + len;
		}
	}
	return n;
}

/*
 * The endpoint configured for the display and enabled (dce_aud_az_configure
 * and dce_aud_az_enable): what the HD audio codec reports of the sink, its
 * ELD, comes from these.
 */
bool
MinkarAMDFramebuffer::configureAudioEndpointDCN(unsigned ep, unsigned *channels)
{
	uint8_t sads[16][3], speakers;
	unsigned n = edidAudio(fEDID, fEDIDBytes, sads, &speakers);
	uint32_t hp = azRead(ep, ixHotPlugControl);

	*channels = 0;
	if (n == 0)
		return false;
	azWrite(ep, ixHotPlugControl, hp | kHotPlugClockGatingDisable);

	uint32_t v = azRead(ep, ixChannelSpeaker);
	v &= ~(kSpeakerAllocationMask | kHDMIConnection | kDPConnection | kLFEPlaybackLevelMask);
	v &= ~(0x1u << 18);		/* EXTRA_CONNECTION_INFO's bit 0 */
	v |= (speakers & kSpeakerAllocationMask) | kHDMIConnection;
	azWrite(ep, ixChannelSpeaker, v);
	azWrite(ep, ixACPData, azRead(ep, ixACPData) & ~0x40u);	/* no AI support */

	/* One descriptor a format (1 LPCM to 14), the one with most channels. */
	for (unsigned format = 1; format <= 14; format++) {
		int best = -1;
		uint32_t value = 0;
		if (format == 9 || format == 13)	/* one-bit audio, DST: left out */
			continue;
		for (unsigned i = 0; i < n; i++)
			if (((sads[i][0] >> 3) & 0xf) == format &&
			    (best < 0 || (sads[i][0] & 7) > (sads[best][0] & 7)))
				best = (int)i;
		if (best >= 0) {
			const uint8_t *d = sads[best];
			value = (d[0] & 7) | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16);
			if (format == 1) {
				value |= (uint32_t)d[1] << 24;	/* stereo rates: all of them */
				*channels = (d[0] & 7) + 1u;
			}
		}
		azWrite(ep, ixAudioDescriptor0 + format - 1, value);
	}
	azWrite(ep, ixResponseHBR, azRead(ep, ixResponseHBR) & ~0x1u);	/* no HBR at HDMI 1.4 */
	azWrite(ep, ixResponseLipsync, azRead(ep, ixResponseLipsync) & ~0xffffu);

	/* The sink: its maker and product, and its name from the EDID. */
	char name[18];
	unsigned length = 0;
	bzero(name, sizeof(name));
	for (unsigned d = 54; d + 18 <= 126; d += 18)
		if (fEDID[d] == 0 && fEDID[d + 1] == 0 && fEDID[d + 3] == 0xfc)
			for (unsigned k = 0; k < 13 && fEDID[d + 5 + k] != 0x0a; k++)
				name[length++] = (char)fEDID[d + 5 + k];
	azWrite(ep, ixSinkInfo0, (uint32_t)(fEDID[8] | fEDID[9] << 8) |
	    (uint32_t)(fEDID[10] | fEDID[11] << 8) << 16);
	azWrite(ep, ixSinkInfo0 + 1, length);
	azWrite(ep, ixSinkInfo0 + 2, 0);	/* port id 0: the display's index */
	azWrite(ep, ixSinkInfo0 + 3, 0);
	for (unsigned k = 0; k < 5; k++)	/* SINK_INFO4..8: four characters each */
		azWrite(ep, ixSinkInfo0 + 4 + k, (uint32_t)(uint8_t)name[4 * k] |
		    (uint32_t)(uint8_t)name[4 * k + 1] << 8 | (uint32_t)(uint8_t)name[4 * k + 2] << 16 |
		    (uint32_t)(uint8_t)(k < 4 ? name[4 * k + 3] : 0) << 24);

	/* Enabled, the clock gating as it was. */
	hp = azRead(ep, ixHotPlugControl);
	azWrite(ep, ixHotPlugControl, hp | kHotPlugClockGatingDisable | kHotPlugAudioEnabled);
	azWrite(ep, ixHotPlugControl, (hp | kHotPlugAudioEnabled) & ~kHotPlugClockGatingDisable);
	LOG("audio: endpoint %u for \"%s\": %u descriptors, %u channels of PCM, speakers 0x%02x",
	    ep, name, n, *channels, speakers);
	return *channels != 0;
}

/* N/CTS for a sample rate at the pixel clock: the HDMI specification's
 * recommended N, CTS = pixel clock * N / (128 * rate). */
static void
acr(uint32_t pixelKHz, uint32_t rate, uint32_t n, uint32_t *ctsOut, uint32_t *nOut)
{
	*nOut = n;
	*ctsOut = (uint32_t)((uint64_t)pixelKHz * 1000 * n / (128ULL * rate));
}

/* The audio DTO and the HDMI encoder's audio (dce_aud_wall_dto_setup,
 * enc3_se_hdmi_audio_setup, afmt3_*), and the samples sent. */
bool
MinkarAMDFramebuffer::setupAudioDCN()
{
	unsigned ep = 0, channels;
	unsigned d = (unsigned)fDIGFE;

	if (fDIGFE < 0 || fDIGMode != 3 /* HDMI: DVI carries no audio */ || fPixelKHz == 0 ||
	    fEDIDBytes <= 128)
		return false;
	if (!configureAudioEndpointDCN(ep, &channels)) {
		LOG("audio: the display plays no PCM");
		return false;
	}
	update(this, dcn302_AZALIA_F0_CODEC_FUNCTION_PARAMETER_SUPPORTED_SIZE_RATES,
	    M(AZALIA_F0_CODEC_FUNCTION_PARAMETER_SUPPORTED_SIZE_RATES, AUDIO_RATE_CAPABILITIES),
	    PUT(AZALIA_F0_CODEC_FUNCTION_PARAMETER_SUPPORTED_SIZE_RATES, AUDIO_RATE_CAPABILITIES, 0x70));

	/* DTO0: 24 MHz (in 100 Hz) over the pixel clock (in 100 Hz), from
	 * the pipe's timing generator. Source and select first. */
	update(this, dcn302_DCCG_AUDIO_DTO_SOURCE,
	    M(DCCG_AUDIO_DTO_SOURCE, DCCG_AUDIO_DTO0_SOURCE_SEL) | M(DCCG_AUDIO_DTO_SOURCE, DCCG_AUDIO_DTO_SEL),
	    PUT(DCCG_AUDIO_DTO_SOURCE, DCCG_AUDIO_DTO0_SOURCE_SEL, fCRTC));
	writeReg(dcn302_DCCG_AUDIO_DTO0_MODULE, fPixelKHz * 10);
	writeReg(dcn302_DCCG_AUDIO_DTO0_PHASE, 24 * 10000);

	/* The encoder. */
	update(this, dcn302_DIG_AFMT_CNTL[d], M(DIG_AFMT_CNTL, AFMT_AUDIO_CLOCK_EN),
	    M(DIG_AFMT_CNTL, AFMT_AUDIO_CLOCK_EN));
	update(this, dcn302_AFMT_AFMT_AUDIO_PACKET_CONTROL[d], M(AFMT_AFMT_AUDIO_PACKET_CONTROL, AFMT_60958_CS_UPDATE),
	    M(AFMT_AFMT_AUDIO_PACKET_CONTROL, AFMT_60958_CS_UPDATE));
	update(this, dcn302_AFMT_AFMT_AUDIO_PACKET_CONTROL2[d],
	    M(AFMT_AFMT_AUDIO_PACKET_CONTROL2, AFMT_AUDIO_LAYOUT_OVRD) |
	    M(AFMT_AFMT_AUDIO_PACKET_CONTROL2, AFMT_60958_OSF_OVRD), 0);
	update(this, dcn302_AFMT_AFMT_60958_0[d],
	    M(AFMT_AFMT_60958_0, AFMT_60958_CS_CHANNEL_NUMBER_L) | M(AFMT_AFMT_60958_0, AFMT_60958_CS_CLOCK_ACCURACY),
	    PUT(AFMT_AFMT_60958_0, AFMT_60958_CS_CHANNEL_NUMBER_L, 1));
	update(this, dcn302_AFMT_AFMT_60958_1[d], M(AFMT_AFMT_60958_1, AFMT_60958_CS_CHANNEL_NUMBER_R),
	    PUT(AFMT_AFMT_60958_1, AFMT_60958_CS_CHANNEL_NUMBER_R, 2));
	update(this, dcn302_AFMT_AFMT_60958_2[d], 0x00FFFFFF,
	    PUT(AFMT_AFMT_60958_2, AFMT_60958_CS_CHANNEL_NUMBER_2, 3) |
	    PUT(AFMT_AFMT_60958_2, AFMT_60958_CS_CHANNEL_NUMBER_3, 4) |
	    PUT(AFMT_AFMT_60958_2, AFMT_60958_CS_CHANNEL_NUMBER_4, 5) |
	    PUT(AFMT_AFMT_60958_2, AFMT_60958_CS_CHANNEL_NUMBER_5, 6) |
	    PUT(AFMT_AFMT_60958_2, AFMT_60958_CS_CHANNEL_NUMBER_6, 7) |
	    PUT(AFMT_AFMT_60958_2, AFMT_60958_CS_CHANNEL_NUMBER_7, 8));
	update(this, dcn302_DIG_HDMI_AUDIO_PACKET_CONTROL[d], M(DIG_HDMI_AUDIO_PACKET_CONTROL, HDMI_AUDIO_DELAY_EN),
	    PUT(DIG_HDMI_AUDIO_PACKET_CONTROL, HDMI_AUDIO_DELAY_EN, 1));
	update(this, dcn302_DIG_HDMI_ACR_PACKET_CONTROL[d],
	    M(DIG_HDMI_ACR_PACKET_CONTROL, HDMI_ACR_AUTO_SEND) | M(DIG_HDMI_ACR_PACKET_CONTROL, HDMI_ACR_SOURCE) |
	    M(DIG_HDMI_ACR_PACKET_CONTROL, HDMI_ACR_AUDIO_PRIORITY), M(DIG_HDMI_ACR_PACKET_CONTROL, HDMI_ACR_AUTO_SEND));
	uint32_t cts, n;
	acr(fPixelKHz, 32000, 4096, &cts, &n);
	update(this, dcn302_DIG_HDMI_ACR_32_0[d], M(DIG_HDMI_ACR_32_0, HDMI_ACR_CTS_32), PUT(DIG_HDMI_ACR_32_0, HDMI_ACR_CTS_32, cts));
	update(this, dcn302_DIG_HDMI_ACR_32_1[d], M(DIG_HDMI_ACR_32_1, HDMI_ACR_N_32), PUT(DIG_HDMI_ACR_32_1, HDMI_ACR_N_32, n));
	acr(fPixelKHz, 44100, 6272, &cts, &n);
	update(this, dcn302_DIG_HDMI_ACR_44_0[d], M(DIG_HDMI_ACR_44_0, HDMI_ACR_CTS_44), PUT(DIG_HDMI_ACR_44_0, HDMI_ACR_CTS_44, cts));
	update(this, dcn302_DIG_HDMI_ACR_44_1[d], M(DIG_HDMI_ACR_44_1, HDMI_ACR_N_44), PUT(DIG_HDMI_ACR_44_1, HDMI_ACR_N_44, n));
	acr(fPixelKHz, 48000, 6144, &cts, &n);
	update(this, dcn302_DIG_HDMI_ACR_48_0[d], M(DIG_HDMI_ACR_48_0, HDMI_ACR_CTS_48), PUT(DIG_HDMI_ACR_48_0, HDMI_ACR_CTS_48, cts));
	update(this, dcn302_DIG_HDMI_ACR_48_1[d], M(DIG_HDMI_ACR_48_1, HDMI_ACR_N_48), PUT(DIG_HDMI_ACR_48_1, HDMI_ACR_N_48, n));

	/* The endpoint's samples into this encoder, the channels it carries. */
	writeReg(dcn302_AFMT_AFMT_AUDIO_SRC_CONTROL[d], PUT(AFMT_AFMT_AUDIO_SRC_CONTROL, AFMT_AUDIO_SRC_SELECT, ep));
	update(this, dcn302_AFMT_AFMT_AUDIO_PACKET_CONTROL2[d], M(AFMT_AFMT_AUDIO_PACKET_CONTROL2, AFMT_AUDIO_CHANNEL_ENABLE),
	    PUT(AFMT_AFMT_AUDIO_PACKET_CONTROL2, AFMT_AUDIO_CHANNEL_ENABLE, (1u << channels) - 1));
	update(this, dcn302_AFMT_AFMT_MEM_PWR[d], M(AFMT_AFMT_MEM_PWR, AFMT_MEM_PWR_FORCE), 0);

	/* The packets: general control and null every frame, the audio
	 * InfoFrame on line 2, no AV mute. */
	update(this, dcn302_DIG_HDMI_VBI_PACKET_CONTROL[d],
	    M(DIG_HDMI_VBI_PACKET_CONTROL, HDMI_GC_CONT) | M(DIG_HDMI_VBI_PACKET_CONTROL, HDMI_GC_SEND) |
	    M(DIG_HDMI_VBI_PACKET_CONTROL, HDMI_NULL_SEND) | M(DIG_HDMI_VBI_PACKET_CONTROL, HDMI_ACP_SEND),
	    M(DIG_HDMI_VBI_PACKET_CONTROL, HDMI_GC_CONT) | M(DIG_HDMI_VBI_PACKET_CONTROL, HDMI_GC_SEND) |
	    M(DIG_HDMI_VBI_PACKET_CONTROL, HDMI_NULL_SEND));
	update(this, dcn302_DIG_HDMI_INFOFRAME_CONTROL0[d], M(DIG_HDMI_INFOFRAME_CONTROL0, HDMI_AUDIO_INFO_SEND),
	    M(DIG_HDMI_INFOFRAME_CONTROL0, HDMI_AUDIO_INFO_SEND));
	update(this, dcn302_AFMT_AFMT_INFOFRAME_CONTROL0[d], M(AFMT_AFMT_INFOFRAME_CONTROL0, AFMT_AUDIO_INFO_UPDATE),
	    M(AFMT_AFMT_INFOFRAME_CONTROL0, AFMT_AUDIO_INFO_UPDATE));
	update(this, dcn302_DIG_HDMI_INFOFRAME_CONTROL1[d], M(DIG_HDMI_INFOFRAME_CONTROL1, HDMI_AUDIO_INFO_LINE),
	    PUT(DIG_HDMI_INFOFRAME_CONTROL1, HDMI_AUDIO_INFO_LINE, 2));
	update(this, dcn302_DIG_HDMI_GC[d], M(DIG_HDMI_GC, HDMI_GC_AVMUTE), 0);

	/* Unmuted: samples sent. */
	update(this, dcn302_AFMT_AFMT_AUDIO_PACKET_CONTROL[d], M(AFMT_AFMT_AUDIO_PACKET_CONTROL, AFMT_AUDIO_SAMPLE_SEND),
	    M(AFMT_AFMT_AUDIO_PACKET_CONTROL, AFMT_AUDIO_SAMPLE_SEND));
	acr(fPixelKHz, 48000, 6144, &cts, &n);
	LOG("audio: DIG %u from endpoint %u, DTO %u/%u, 48 kHz N %u CTS %u", d, ep, 240000,
	    fPixelKHz * 10, n, cts);
	setProperty("MinkarHDMIAudioEndpoint", ep, 32);
	return true;
}
