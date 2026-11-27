/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The boot screen's progress bar (boot_progress.c).
 */
#ifndef _CONSOLE_BOOT_PROGRESS_H_
#define _CONSOLE_BOOT_PROGRESS_H_

#include <sys/cdefs.h>
#include <console/video_console.h>

__BEGIN_DECLS

/* The graphical boot goes on from the loader's screen (not with -v). */
void boot_progress_start(const struct vc_info *v);
/* The console's framebuffer changed: the boot screen drawn there again. */
void boot_progress_screen(const struct vc_info *v);
/* The bar to percent (it only moves forward; 100 ends it). */
void boot_progress_set(unsigned int percent);
unsigned int boot_progress_get(void);
/* The console turned off (pause) and on again. */
void boot_progress_pause(boolean_t pause);
/* The screen given to something else (text). */
void boot_progress_stop(void);

__END_DECLS

#endif /* _CONSOLE_BOOT_PROGRESS_H_ */
