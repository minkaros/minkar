/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * fontcache: at boot, as root, has CoreText load the system's fonts, which
 * writes their cache in /System/Library/Caches/com.apple.FontRegistry
 * (CTFontManager.c). Sandboxed processes, which may not write a cache of
 * their own or read the user's, read that one: without it each would
 * read every font file before drawing text.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include <stdio.h>

int main(void) {
	CFArrayRef families = CTFontManagerCopyAvailableFontFamilyNames();

	if (families == NULL) {
		fprintf(stderr, "fontcache: no fonts\n");
		return 1;
	}
	fprintf(stderr, "fontcache: %ld font families\n", (long)CFArrayGetCount(families));
	CFRelease(families);
	return 0;
}
