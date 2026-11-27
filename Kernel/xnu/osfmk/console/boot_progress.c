/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The boot screen's progress bar, after the loader. The loader
 * (efiloader's bootscreen.c) draws Minkar's logo and the bar on black and
 * hands the kernel a graphics console without -v; from there the bar is
 * drawn here, in the same place and colours (the design at 1920x1080
 * scaled to the screen's height), up to the percentage the kernel's
 * stages and userland (kern.boot_progress) give.
 *
 * The logo is taken from the screen when the kernel starts: a display
 * driver that changes the mode (and tells the console, initialize_screen)
 * gets the whole boot screen drawn again on its framebuffer. While the
 * console is off (PCI configuration turns it off for a while) nothing is
 * drawn; turned on again, the screen is drawn whole. It ends at 100 % (the
 * window server sets it as it takes the display) or when the console
 * turns to text.
 */

#include <kern/kalloc.h>
#include <kern/simple_lock.h>
#include <console/video_console.h>
#include <console/boot_progress.h>
#include <pexpert/pexpert.h>
#include <string.h>

#define DESIGN_HEIGHT   1080
#define LOGO_BOX        156
#define LOGO_CENTRE_Y   494
#define BAR_WIDTH       320
#define BAR_HEIGHT      5
#define BAR_TOP         635

#define TRACK_COLOUR    0x00555559u
#define FILL_COLOUR     0x00f0f0f2u

static struct {
	boolean_t       active;
	boolean_t       paused;         /* the console off for a while */
	unsigned int    percent;
	struct vc_info  screen;
	uint32_t        *logo;          /* logoSize x logoSize pixels */
	unsigned int    logoSize;
} bp;

decl_simple_lock_data(static, bp_lock);

static unsigned int
scale(unsigned int value, unsigned int height)
{
	return (value * height + DESIGN_HEIGHT / 2) / DESIGN_HEIGHT;
}

static volatile uint32_t *
pixel_row(const struct vc_info *v, unsigned int y)
{
	return (volatile uint32_t *)(v->v_baseaddr + (unsigned long)y * v->v_rowbytes);
}

static boolean_t
usable(const struct vc_info *v)
{
	return v->v_baseaddr != 0 && v->v_depth == 32 && v->v_width > 0 && v->v_height > 0;
}

static void
logo_box(const struct vc_info *v, unsigned int *left, unsigned int *top, unsigned int *size)
{
	unsigned int centre = scale(LOGO_CENTRE_Y, v->v_height);

	*size = scale(LOGO_BOX, v->v_height);
	if (*size > v->v_width) {
		*size = v->v_width;
	}
	*left = (v->v_width - *size) / 2;
	*top = centre > *size / 2 ? centre - *size / 2 : 0;
	if (*top + *size > v->v_height) {
		*size = v->v_height - *top;
	}
}

/* A capsule (round ends), filled from the left to percent; each pixel
 * sampled 4x4 times (in eighths of a pixel) over black. */
static void
draw_bar(const struct vc_info *v, unsigned int percent)
{
	unsigned int width = scale(BAR_WIDTH, v->v_height), height = scale(BAR_HEIGHT, v->v_height);
	unsigned int left, top = scale(BAR_TOP, v->v_height);

	if (height < 3) {
		height = 3;
	}
	if (width > v->v_width || top + height > v->v_height) {
		return;
	}
	left = (v->v_width - width) / 2;

	int radius = (int)height * 4;
	int leftCentre = (int)left * 8 + radius, rightCentre = (int)(left + width) * 8 - radius;
	int middle = (int)top * 8 + (int)height * 4;
	int fillEnd = (int)left * 8 + (int)(width * 8 * percent / 100);

	for (unsigned int y = 0; y < height; y++) {
		volatile uint32_t *row = pixel_row(v, top + y);
		for (unsigned int x = 0; x < width; x++) {
			unsigned int r = 0, g = 0, b = 0;
			for (int j = 0; j < 4; j++) {
				for (int i = 0; i < 4; i++) {
					int sx = (int)(left + x) * 8 + 2 * i + 1, sy = (int)(top + y) * 8 + 2 * j + 1;
					int dx = sx < leftCentre ? leftCentre - sx : sx > rightCentre ? sx - rightCentre : 0;
					int dy = sy - middle;
					if (dx * dx + dy * dy > radius * radius) {
						continue;
					}
					uint32_t c = sx < fillEnd ? FILL_COLOUR : TRACK_COLOUR;
					r += (c >> 16) & 0xff;
					g += (c >> 8) & 0xff;
					b += c & 0xff;
				}
			}
			row[left + x] = ((r / 16) << 16) | ((g / 16) << 8) | (b / 16);
		}
	}
}

/* The whole boot screen: black, the logo (the one taken from the screen,
 * resized to this screen's box by its nearest pixels), the bar. */
static void
draw_screen(const struct vc_info *v)
{
	unsigned int left, top, size;

	for (unsigned int y = 0; y < v->v_height; y++) {
		volatile uint32_t *row = pixel_row(v, y);
		for (unsigned int x = 0; x < v->v_width; x++) {
			row[x] = 0;
		}
	}
	if (bp.logo != NULL) {
		logo_box(v, &left, &top, &size);
		for (unsigned int y = 0; y < size; y++) {
			volatile uint32_t *row = pixel_row(v, top + y);
			const uint32_t *from = bp.logo + (y * bp.logoSize / size) * bp.logoSize;
			for (unsigned int x = 0; x < size; x++) {
				row[left + x] = from[x * bp.logoSize / size];
			}
		}
	}
	draw_bar(v, bp.percent);
}

void
boot_progress_start(const struct vc_info *v)
{
	unsigned int left, top, size;
	int verbose = 0;

	simple_lock_init(&bp_lock, 0);
	if (PE_parse_boot_argn("-v", &verbose, sizeof(verbose)) || !usable(v)) {
		return;
	}
	logo_box(v, &left, &top, &size);
	bp.logo = (uint32_t *)kalloc((vm_size_t)size * size * sizeof(uint32_t));
	if (bp.logo != NULL) {
		bp.logoSize = size;
		for (unsigned int y = 0; y < size; y++) {
			volatile uint32_t *row = pixel_row(v, top + y);
			for (unsigned int x = 0; x < size; x++) {
				bp.logo[y * size + x] = row[left + x] & 0x00ffffff;
			}
		}
	}
	bp.screen = *v;
	bp.percent = 10;
	bp.active = TRUE;
	boot_progress_set(15);	/* drawn now, or when the console is on again */
}

void
boot_progress_screen(const struct vc_info *v)
{
	if (!bp.active) {
		return;
	}
	simple_lock(&bp_lock, LCK_GRP_NULL);
	if (bp.active && usable(v) &&
	    (v->v_baseaddr != bp.screen.v_baseaddr || v->v_width != bp.screen.v_width ||
	    v->v_height != bp.screen.v_height || v->v_rowbytes != bp.screen.v_rowbytes)) {
		bp.screen = *v;
		if (!bp.paused) {
			draw_screen(&bp.screen);
		}
	}
	simple_unlock(&bp_lock);
}

void
boot_progress_set(unsigned int percent)
{
	if (!bp.active) {
		return;
	}
	if (percent > 100) {
		percent = 100;
	}
	simple_lock(&bp_lock, LCK_GRP_NULL);
	if (bp.active && percent > bp.percent) {
		bp.percent = percent;
		if (!bp.paused) {
			draw_bar(&bp.screen, percent);
		}
		if (percent == 100) {
			bp.active = FALSE;
		}
	}
	simple_unlock(&bp_lock);
}

void
boot_progress_pause(boolean_t pause)
{
	if (!bp.active) {
		bp.paused = pause;	/* kept for when it starts */
		return;
	}
	simple_lock(&bp_lock, LCK_GRP_NULL);
	if (bp.active && bp.paused != pause) {
		bp.paused = pause;
		if (!pause) {
			draw_screen(&bp.screen);
		}
	}
	simple_unlock(&bp_lock);
}

unsigned int
boot_progress_get(void)
{
	return bp.percent;
}

void
boot_progress_stop(void)
{
	if (!bp.active) {
		return;
	}
	simple_lock(&bp_lock, LCK_GRP_NULL);
	bp.active = FALSE;
	simple_unlock(&bp_lock);
}
