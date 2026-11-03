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
 * pbcopy: standard input to a pasteboard (the general one, or the one
 * -pboard names), as RTF when it is RTF, else as plain text.
 */

#import <AppKit/NSPasteboard.h>
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <string.h>

static void
usage(void)
{
	fprintf(stderr, "usage: pbcopy [-help] [-pboard {general | ruler | find | font}]\n");
	exit(1);
}

static NSString *
pasteboardNamed(const char *name)
{
	if (strcmp(name, "general") == 0)
		return NSPasteboardNameGeneral;
	if (strcmp(name, "ruler") == 0)
		return NSPasteboardNameRuler;
	if (strcmp(name, "find") == 0)
		return NSPasteboardNameFind;
	if (strcmp(name, "font") == 0)
		return NSPasteboardNameFont;
	return nil;
}

int
main(int argc, char *argv[])
{
	@autoreleasepool {
		NSString *name = NSPasteboardNameGeneral;

		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "-pboard") == 0 && i + 1 < argc) {
				if ((name = pasteboardNamed(argv[++i])) == nil)
					usage();
			} else
				usage();
		}
		NSData *data = [[NSFileHandle fileHandleWithStandardInput] readDataToEndOfFile];
		NSPasteboard *pb = [NSPasteboard pasteboardWithName:name];
		static const char rtf[] = "{\\rtf";
		BOOL isRTF = [data length] >= sizeof(rtf) - 1 &&
		    memcmp([data bytes], rtf, sizeof(rtf) - 1) == 0;
		[pb clearContents];
		if (![pb setData:data forType:isRTF ? NSPasteboardTypeRTF : NSPasteboardTypeString]) {
			fprintf(stderr, "pbcopy: the pasteboard server did not take it\n");
			return 1;
		}
	}
	return 0;
}
