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
 * NSRunningApplication over LaunchServices' session (WindowServer plan,
 * WS4a): the daemon's list of running applications, and the messages each
 * app gets about the others (NSApplication passes them here). One object
 * per process, kept up to date by those messages. The rest of the class
 * (icon, hide, terminate, owning the menu bar) comes with the Dock (D0).
 */

#import <AppKit/NSRunningApplication.h>
#import <AppKit/NSWorkspace.h>
#import "NSRunningApplication_private.h"
#include <LaunchServices/LSSession.h>
#include <unistd.h>

static NSMutableDictionary *applications;	/* pid -> NSRunningApplication */
static NSLock *applicationsLock;

@implementation NSRunningApplication

+ (void)initialize {
    if (self == [NSRunningApplication class]) {
        applications = [NSMutableDictionary new];
        applicationsLock = [NSLock new];
    }
}

- (void)_updateWithSessionApp:(const LSSessionApp *)app {
    NSString *path = [NSString stringWithUTF8String:app->path];

    _processIdentifier = app->pid;
    [_bundleIdentifier release];
    _bundleIdentifier = app->bundleIdentifier[0]
        ? [[NSString alloc] initWithUTF8String:app->bundleIdentifier] : nil;
    [_localizedName release];
    _localizedName = app->name[0] ? [[NSString alloc] initWithUTF8String:app->name] : nil;
    [_bundleURL release];
    [_executableURL release];
    _bundleURL = nil;
    _executableURL = nil;
    if ([path length] > 0) {
        NSURL *url = [NSURL fileURLWithPath:path];
        if ([[path pathExtension] isEqualToString:@"app"]) {
            _bundleURL = [url retain];
            _executableURL = [[[NSBundle bundleWithPath:path] executableURL] retain];
        } else
            _executableURL = [url retain];
    }
    if (_launchDate == nil && app->launchTime > 0)
        _launchDate = [[NSDate alloc] initWithTimeIntervalSince1970:(NSTimeInterval)app->launchTime];
    _activationPolicy = app->activationPolicy;
    _active = (app->flags & kLSSessionAppActive) != 0;
}

/* The object for an app the daemon described, made or brought up to date. */
+ (NSRunningApplication *)_applicationForSessionApp:(const LSSessionApp *)app {
    NSNumber *key = [NSNumber numberWithInt:app->pid];
    NSRunningApplication *result;

    [applicationsLock lock];
    result = [applications objectForKey:key];
    if (result == nil) {
        result = [[NSRunningApplication alloc] init];
        [applications setObject:result forKey:key];
        [result release];
    }
    [result _updateWithSessionApp:app];
    [[result retain] autorelease];
    [applicationsLock unlock];
    return result;
}

+ (NSRunningApplication *)_applicationTerminated:(const LSSessionApp *)app {
    NSRunningApplication *result = [self _applicationForSessionApp:app];

    [applicationsLock lock];
    result->_terminated = YES;
    result->_active = NO;
    [applications removeObjectForKey:[NSNumber numberWithInt:app->pid]];
    [applicationsLock unlock];
    return result;
}

+ (NSArray *)_runningApplications {
    LSSessionApp *apps = NULL;
    uint32_t count = 0;
    NSMutableArray *result = [NSMutableArray array];

    if (LSSessionCopyApps(&apps, &count) != kLSSessionSuccess)
        return result;
    for (uint32_t i = 0; i < count; i++)
        [result addObject:[self _applicationForSessionApp:&apps[i]]];
    free(apps);
    return result;
}

+ (instancetype)runningApplicationWithProcessIdentifier:(pid_t)pid {
    for (NSRunningApplication *app in [self _runningApplications])
        if (app->_processIdentifier == pid)
            return app;
    return nil;
}

+ (NSArray *)runningApplicationsWithBundleIdentifier:(NSString *)bundleIdentifier {
    NSMutableArray *result = [NSMutableArray array];

    for (NSRunningApplication *app in [self _runningApplications])
        if ([app->_bundleIdentifier isEqualToString:bundleIdentifier])
            [result addObject:app];
    return result;
}

+ (instancetype)currentApplication {
    NSRunningApplication *app = [self runningApplicationWithProcessIdentifier:getpid()];
    LSSessionApp me;

    if (app != nil)
        return app;
    /* Not checked in (no session daemon, or not an NSApplication). */
    _NSRunningApplicationDescribeSelf(&me, NSApplicationActivationPolicyProhibited);
    me.pid = getpid();
    return [self _applicationForSessionApp:&me];
}

- (void)dealloc {
    [_bundleIdentifier release];
    [_localizedName release];
    [_bundleURL release];
    [_executableURL release];
    [_launchDate release];
    [super dealloc];
}

- (BOOL)isTerminated {
    return _terminated;
}

- (BOOL)isFinishedLaunching {
    return !_terminated;
}

- (BOOL)isHidden {
    return NO;
}

- (BOOL)isActive {
    return _active;
}

- (NSApplicationActivationPolicy)activationPolicy {
    return _activationPolicy;
}

- (NSString *)localizedName {
    return _localizedName;
}

- (NSString *)bundleIdentifier {
    return _bundleIdentifier;
}

- (NSURL *)bundleURL {
    return _bundleURL;
}

- (NSURL *)executableURL {
    return _executableURL;
}

- (pid_t)processIdentifier {
    return _processIdentifier;
}

- (NSDate *)launchDate {
    return _launchDate;
}

- (BOOL)activateWithOptions:(NSApplicationActivationOptions)options {
    if (_terminated)
        return NO;
    return LSSessionActivate(_processIdentifier, (uint32_t)options) == kLSSessionSuccess;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p pid=%d %@>", [self class], self,
        _processIdentifier, _bundleIdentifier ? _bundleIdentifier : _localizedName];
}

@end

void _NSRunningApplicationDescribeSelf(LSSessionApp *app, NSInteger policy) {
    NSBundle *bundle = [NSBundle mainBundle];
    NSString *bundleID = [bundle bundleIdentifier];
    NSString *name = [bundle objectForInfoDictionaryKey:@"CFBundleDisplayName"];
    NSString *path = [bundle bundlePath];

    if (name == nil)
        name = [bundle objectForInfoDictionaryKey:@"CFBundleName"];
    if (name == nil)
        name = [[NSProcessInfo processInfo] processName];
    /* A program outside a bundle: its executable. */
    if (![[path pathExtension] isEqualToString:@"app"])
        path = [bundle executablePath];
    memset(app, 0, sizeof(*app));
    if (bundleID != nil)
        strlcpy(app->bundleIdentifier, [bundleID UTF8String], sizeof(app->bundleIdentifier));
    if (name != nil)
        strlcpy(app->name, [name UTF8String], sizeof(app->name));
    if (path != nil)
        strlcpy(app->path, [path fileSystemRepresentation], sizeof(app->path));
    app->activationPolicy = (int32_t)policy;
    app->launchTime = (uint64_t)time(NULL);
}

void _NSRunningApplicationSessionMessage(const LSSessionMessage *msg) {
    NSRunningApplication *app;
    NSString *name = nil;

    switch (msg->kind) {
        case kLSSessionAppLaunched:
            app = [NSRunningApplication _applicationForSessionApp:&msg->app];
            name = NSWorkspaceDidLaunchApplicationNotification;
            break;
        case kLSSessionAppTerminated:
            app = [NSRunningApplication _applicationTerminated:&msg->app];
            name = NSWorkspaceDidTerminateApplicationNotification;
            break;
        case kLSSessionAppActivated:
            app = [NSRunningApplication _applicationForSessionApp:&msg->app];
            name = NSWorkspaceDidActivateApplicationNotification;
            break;
        case kLSSessionAppDeactivated:
            app = [NSRunningApplication _applicationForSessionApp:&msg->app];
            name = NSWorkspaceDidDeactivateApplicationNotification;
            break;
        default:
            return;
    }

    NSMutableDictionary *info = [NSMutableDictionary dictionaryWithObject:app
        forKey:NSWorkspaceApplicationKey];
    /* The keys from before NSRunningApplication, still in these two. */
    if (msg->kind == kLSSessionAppLaunched || msg->kind == kLSSessionAppTerminated) {
        [info setObject:[NSNumber numberWithInt:app.processIdentifier]
            forKey:@"NSApplicationProcessIdentifier"];
        if (app.localizedName != nil)
            [info setObject:app.localizedName forKey:@"NSApplicationName"];
        if (app.bundleIdentifier != nil)
            [info setObject:app.bundleIdentifier forKey:@"NSApplicationBundleIdentifier"];
        NSURL *url = app.bundleURL ? app.bundleURL : app.executableURL;
        if (url != nil)
            [info setObject:[url path] forKey:@"NSApplicationPath"];
    }
    [[[NSWorkspace sharedWorkspace] notificationCenter] postNotificationName:name
        object:[NSWorkspace sharedWorkspace] userInfo:info];
}
