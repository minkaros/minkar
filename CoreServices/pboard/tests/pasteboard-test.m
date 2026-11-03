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
 * pasteboard-test: AppKit's NSPasteboard on the pasteboard server (pboard).
 * One line per check, "ok" or "FAIL", then "pasteboard-test: N passed, M
 * failed"; the exit status is the number of failures. Runs as root (the
 * last check becomes nobody in a child process). "pasteboard-test types"
 * lists the general pasteboard's types instead.
 */

#import <AppKit/NSPasteboard.h>
#import <Foundation/Foundation.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;
static int passed, failed;

static void
check(BOOL ok, const char *what)
{
	printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if (ok)
		passed++;
	else
		failed++;
}

/* An owner that writes its data only when asked. */
@interface LazyOwner : NSObject
@property int asked;
@end

@implementation LazyOwner
- (void)pasteboard:(NSPasteboard *)sender provideDataForType:(NSString *)type
{
	self.asked++;
	[sender setString:@"given when asked" forType:type];
}
@end

static int
run(char *const argv[])
{
	pid_t pid;
	int status = -1;

	if (posix_spawn(&pid, argv[0], NULL, NULL, argv, environ) != 0)
		return -1;
	waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int
main(int argc, char *argv[])
{
	@autoreleasepool {
		setvbuf(stdout, NULL, _IOLBF, 0);
		/* The other process: 0 if it reads what the first one wrote. */
		if (argc == 3 && strcmp(argv[1], "read") == 0) {
			NSString *s = [[NSPasteboard pasteboardWithName:@"minkar.test"]
			    stringForType:NSPasteboardTypeString];
			return [s isEqualToString:@(argv[2])] ? 0 : 2;
		}
		/* The general pasteboard's types, one a line (for the image tests). */
		if (argc == 2 && strcmp(argv[1], "types") == 0) {
			for (NSString *t in [[NSPasteboard generalPasteboard] types])
				printf("%s\n", [t UTF8String]);
			return 0;
		}
		/* Another user: 0 if it sees nothing of root's. */
		if (argc == 2 && strcmp(argv[1], "stranger") == 0) {
			if (setgid((gid_t)-2) != 0 || setuid((uid_t)-2) != 0)
				return 4;
			NSPasteboard *pb = [NSPasteboard pasteboardWithName:@"minkar.test"];
			return [pb stringForType:NSPasteboardTypeString] == nil && [pb changeCount] == 0 ?
			    0 : 2;
		}

		NSPasteboard *pb = [NSPasteboard pasteboardWithName:@"minkar.test"];
		NSInteger before = [pb changeCount];
		NSInteger count = [pb clearContents];
		check(count > before && [pb changeCount] == count, "clearContents makes a new change count");
		check([pb setString:@"hola" forType:NSPasteboardTypeString] &&
		    [[pb stringForType:NSPasteboardTypeString] isEqualToString:@"hola"],
		    "a string, written and read back");
		check([[pb stringForType:NSStringPboardType] isEqualToString:@"hola"] &&
		    [[pb types] containsObject:NSPasteboardTypeString] &&
		    [[pb types] containsObject:NSStringPboardType],
		    "NSStringPboardType is the same type, and types lists both names");
		check([[pb availableTypeFromArray:@[ NSPasteboardTypeRTF, NSStringPboardType ]]
		    isEqualToString:NSStringPboardType], "availableTypeFromArray: answers in the asker's names");

		/* Objects. */
		[pb clearContents];
		NSURL *url = [NSURL fileURLWithPath:@"/tmp/archivo.txt"];
		check([pb writeObjects:@[ url ]], "writeObjects: takes a file URL");
		NSArray *urls = [pb readObjectsForClasses:@[ [NSURL class] ] options:nil];
		check([urls count] == 1 && [[urls[0] path] isEqualToString:@"/tmp/archivo.txt"],
		    "readObjectsForClasses: gives it back");
		NSArray *strings = [pb readObjectsForClasses:@[ [NSString class] ] options:nil];
		check([strings count] == 1, "and as a string too");

		/* Ownership: a writer after another's clear loses. */
		NSPasteboard *other = [NSPasteboard pasteboardWithUniqueName];
		NSInteger mine = [pb declareTypes:@[ NSPasteboardTypeString ] owner:nil];
		check(mine > 0, "declareTypes:owner: makes a change count");
		[[NSPasteboard pasteboardWithName:@"minkar.test"] clearContents];
		check(![pb setString:@"tarde" forType:NSPasteboardTypeString] ||
		    [pb changeCount] != mine, "after another clear, the old owner's write does not land");
		[other releaseGlobally];

		/* Promised data. */
		LazyOwner *owner = [[LazyOwner alloc] init];
		[pb declareTypes:@[ NSPasteboardTypeString ] owner:owner];
		check([[pb types] containsObject:NSPasteboardTypeString] && owner.asked == 0,
		    "a declared type is listed before its data");
		check([[pb stringForType:NSPasteboardTypeString] isEqualToString:@"given when asked"] &&
		    owner.asked == 1, "its owner gives the data when it is asked for");
		[owner release];

		/* Property lists and size. */
		[pb clearContents];
		NSArray *files = @[ @"/a", @"/b" ];
		check([pb setPropertyList:files forType:NSFilenamesPboardType] &&
		    [[pb propertyListForType:NSFilenamesPboardType] isEqual:files],
		    "a property list, written and read back");
		NSMutableData *big = [NSMutableData dataWithLength:8 << 20];
		((uint8_t *)[big mutableBytes])[(8 << 20) - 1] = 0x5a;
		check([pb setData:big forType:@"org.minkar.test.big"] &&
		    [[pb dataForType:@"org.minkar.test.big"] isEqualToData:big], "8 MB, written and read back");

		/* Other processes. */
		[pb clearContents];
		[pb setString:@"de un proceso a otro" forType:NSPasteboardTypeString];
		char *reader[] = { argv[0], "read", "de un proceso a otro", NULL };
		check(run(reader) == 0, "another process of the same user reads it");
		char *stranger[] = { argv[0], "stranger", NULL };
		check(run(stranger) == 0, "another user sees none of it");

		/* Unique pasteboards. */
		NSPasteboard *unique = [NSPasteboard pasteboardWithUniqueName];
		[unique clearContents];
		[unique setString:@"x" forType:NSPasteboardTypeString];
		NSString *uniqueName = [[[unique name] copy] autorelease];
		[unique releaseGlobally];
		check(![uniqueName isEqualToString:[pb name]] &&
		    [[NSPasteboard pasteboardWithName:uniqueName] changeCount] == 0,
		    "a unique pasteboard, released, is gone");

		printf("pasteboard-test: %d passed, %d failed\n", passed, failed);
	}
	return failed;
}
