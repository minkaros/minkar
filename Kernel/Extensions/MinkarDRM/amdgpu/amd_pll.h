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
 * Display PLL dividers: the reference, feedback (with a fractional part
 * in tenths) and post dividers that bring a PLL's reference clock closest
 * to a pixel clock, within the limits the VBIOS gives (the PLL's input
 * and VCO ranges). An exhaustive search: for each post divider, the VCO
 * it needs, then each reference divider whose input is in range, the
 * nearest feedback divider; the closest result wins, a higher VCO on a
 * tie. Plain C, for the driver and host tests. All clocks in kHz.
 */

#ifndef _MINKAR_AMD_PLL_H
#define _MINKAR_AMD_PLL_H

#include <stdint.h>

struct amd_pll_limits {
	uint32_t reference;		/* kHz */
	uint32_t inMin, inMax;		/* the PLL's input, reference / ref_div */
	uint32_t vcoMin, vcoMax;
	uint32_t refDivMin, refDivMax;
	uint32_t fbDivMin, fbDivMax;	/* the integer part */
	uint32_t postDivMin, postDivMax;
};

struct amd_pll_dividers {
	uint32_t refDiv, fbDiv, fbFrac, postDiv;	/* fbFrac: tenths */
	uint32_t clock;				/* what they give, kHz */
};

/* 0 when no dividers are within the limits. */
static inline int
amd_pll_compute(const struct amd_pll_limits *l, uint32_t target, struct amd_pll_dividers *out)
{
	uint64_t bestError = UINT64_MAX, bestVCO = 0;
	int found = 0;

	for (uint32_t post = l->postDivMin; post <= l->postDivMax; post++) {
		uint64_t vco = (uint64_t)target * post;
		if (vco < l->vcoMin || vco > l->vcoMax)
			continue;
		for (uint32_t ref = l->refDivMin; ref <= l->refDivMax; ref++) {
			uint64_t in = l->reference / ref;
			if (in < l->inMin || in > l->inMax)
				continue;
			/* feedback in tenths: vco = reference * fb / ref */
			uint64_t fb10 = (vco * ref * 10 + l->reference / 2) / l->reference;
			uint32_t fb = (uint32_t)(fb10 / 10), frac = (uint32_t)(fb10 % 10);
			if (fb < l->fbDivMin || fb > l->fbDivMax)
				continue;
			uint64_t gotVCO = (uint64_t)l->reference * fb10 / (10 * ref);
			uint64_t got = (gotVCO + post / 2) / post;
			uint64_t error = got > target ? got - target : target - got;
			if (error < bestError || (error == bestError && gotVCO > bestVCO)) {
				bestError = error;
				bestVCO = gotVCO;
				out->refDiv = ref;
				out->fbDiv = fb;
				out->fbFrac = frac;
				out->postDiv = post;
				out->clock = (uint32_t)got;
				found = 1;
			}
		}
	}
	return found;
}

#endif
