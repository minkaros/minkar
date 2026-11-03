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
 * pbpaste: a pasteboard's contents (the general one, or the one -pboard
 * names) to standard output: its plain text, or with -Prefer rtf or ps
 * that type when it has it.
 */

#import <AppKit/NSPasteboard.h>
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <string.h>

static void
usage(void)
{
	fprintf(stderr, "usage: pbpaste [-help] [-pboard {general | ruler | find | font}] "
	    "[-Prefer {txt | rtf | ps}]\n");
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
		NSMutableArray *order = [NSMutableArray arrayWithObject:NSPasteboardTypeString];

		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "-pboard") == 0 && i + 1 < argc) {
				if ((name = pasteboardNamed(argv[++i])) == nil)
					usage();
			} else if (strcmp(argv[i], "-Prefer") == 0 && i + 1 < argc) {
				const char *p = argv[++i];
				if (strcmp(p, "rtf") == 0)
					[order insertObject:NSPasteboardTypeRTF atIndex:0];
				else if (strcmp(p, "ps") == 0)
					[order insertObject:NSPostScriptPboardType atIndex:0];
				else if (strcmp(p, "txt") != 0)
					usage();
			} else
				usage();
		}
		NSPasteboard *pb = [NSPasteboard pasteboardWithName:name];
		NSString *type = [pb availableTypeFromArray:order];
		NSData *data = type != nil ? [pb dataForType:type] : nil;
		if (data != nil)
			fwrite([data bytes], 1, [data length], stdout);
	}
	return 0;
}
