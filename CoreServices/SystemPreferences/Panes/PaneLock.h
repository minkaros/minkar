/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The lock of the panes whose settings are the system's (Printers, Date &
 * Time, Users, Network; the maintainer's choice, 2026-11-23): a pane's
 * content above a strip with SecurityInterface's SFAuthorizationView, set to
 * the pane's right (system.preferences.printing, ...). Closed, its controls
 * are off; a click asks for an administrator's name and password (the
 * Authorization Agent's window). The tools that change the system then run
 * as root through the lock's authorization (AuthorizationExecuteWithPrivileges,
 * Security's trampoline), or directly when the session is root's.
 */

#import <AppKit/AppKit.h>
#import <SecurityInterface/SFAuthorizationView.h>
#include <unistd.h>

static const CGFloat kLockStrip = 44;

/* The pane's view: its content, the lock below it. */
static inline NSView *paneWithLock(NSView *content, NSString *right, id delegate, SFAuthorizationView *__strong *lockOut)
{
    NSRect frame = [content frame];
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, NSWidth(frame), NSHeight(frame) + kLockStrip)];
    [content setFrameOrigin:NSMakePoint(0, kLockStrip)];
    [view addSubview:content];
    NSBox *line = [[NSBox alloc] initWithFrame:NSMakeRect(20, kLockStrip - 1, NSWidth(frame) - 40, 1)];
    [line setBoxType:NSBoxSeparator];
    [view addSubview:line];
    SFAuthorizationView *lock = [[SFAuthorizationView alloc] initWithFrame:NSMakeRect(20, 6, 360, 32)];
    [lock setString:[right UTF8String]];
    [lock setDelegate:delegate];
    [lock setAutoupdate:YES];
    [view addSubview:lock];
    *lockOut = lock;
    return view;
}

/* Whether the pane's system settings may change now: root's session, or
   the lock open. */
static inline BOOL paneUnlocked(SFAuthorizationView *lock)
{
    return geteuid() == 0 || [lock authorizationState] == SFAuthorizationViewUnlockedState;
}

/* A tool run as root with the lock's authorization (directly for root);
   its exit status, or -1 when it could not run. */
static inline int paneRunTool(SFAuthorizationView *lock, NSString *path, NSArray<NSString *> *arguments)
{
    if (geteuid() == 0) {
        NSTask *task = [[NSTask alloc] init];
        [task setLaunchPath:path];
        [task setArguments:arguments];
        @try {
            [task launch];
        } @catch (NSException *e) {
            return -1;
        }
        [task waitUntilExit];
        return [task terminationStatus];
    }
    /* Through the shell, which says the tool's status last. */
    NSMutableArray<NSString *> *all = [NSMutableArray arrayWithObjects:@"-c", @"\"$0\" \"$@\" < /dev/null; echo \"status=$?\"", path, nil];
    [all addObjectsFromArray:arguments];
    char **argv = calloc([all count] + 1, sizeof(char *));
    for (NSUInteger i = 0; i < [all count]; i++)
        argv[i] = strdup([all[i] UTF8String]);
    FILE *pipe = NULL;
    OSStatus status = AuthorizationExecuteWithPrivileges([[lock authorization] authorizationRef], "/bin/sh",
                                                         kAuthorizationFlagDefaults, argv, &pipe);
    for (NSUInteger i = 0; i < [all count]; i++)
        free(argv[i]);
    free(argv);
    if (status != errAuthorizationSuccess || pipe == NULL)
        return -1;
    char line[1024];
    int exitStatus = -1;
    while (fgets(line, sizeof(line), pipe) != NULL)
        if (strncmp(line, "status=", 7) == 0)
            exitStatus = atoi(line + 7);
    fclose(pipe);
    return exitStatus;
}
