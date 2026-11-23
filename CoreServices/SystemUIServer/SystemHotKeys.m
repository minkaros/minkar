/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The window server recognizes the symbolic hotkeys (WSHotKeys.m) and
 * sends each to its owner. SystemUIServer owns these, as on the
 * reference:
 *   28 Shift-Command-3: the screen, to a file (the screencapture domain's
 *      "location", the Desktop with none);
 *   29 Control-Shift-Command-3: the screen, to the clipboard;
 *   60 Control-Space: the input source selected before this one;
 *   61 Control-Option-Space: the next one in the input menu.
 * When the session starts, the user's own keys (System Preferences'
 * Keyboard Shortcuts, the com.apple.symbolichotkeys domain) are given to
 * the server: it reads only the computer's at its own start.
 */

#import "SystemHotKeys.h"
#import <AppKit/AppKit.h>
#include <Carbon/Carbon.h>
#include <SkyLight/SkyLight.h>
#include <os/log.h>

static CFStringRef const kHotKeysDomain = CFSTR("com.apple.symbolichotkeys");

@implementation SystemHotKeys {
    NSString *_currentSource, *_previousSource;
}

static NSString *
sourceID(TISInputSourceRef source)
{
    return source != NULL ? (__bridge NSString *)TISGetInputSourceProperty(source, kTISPropertyInputSourceID) : nil;
}

static NSString *
currentSourceID(void)
{
    TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();
    NSString *identifier = [sourceID(source) copy];
    if (source != NULL)
        CFRelease(source);
    return identifier;
}

/* The user's keys, each { enabled; value = { parameters = (character,
   key code, modifier flags) } }; numbers the server does not have, or
   keeps fixed, it refuses. */
static void
applyUserHotKeys(void)
{
    NSDictionary *table = CFBridgingRelease(CFPreferencesCopyAppValue(CFSTR("AppleSymbolicHotKeys"), kHotKeysDomain));

    /* The computer's first: another user's, before, are not kept. */
    SLSSetSymbolicHotKey(0, 0, 0, 0);
    if (![table isKindOfClass:[NSDictionary class]])
        return;
    for (NSString *number in table) {
        NSDictionary *entry = table[number];
        if (![number isKindOfClass:[NSString class]] || ![entry isKindOfClass:[NSDictionary class]])
            continue;
        uint32_t hotKey = (uint32_t)[number intValue];
        uint16_t keyCode = 0;
        uint32_t modifiers = 0;
        int enabled = 0;
        if (hotKey == 0 || SLSGetSymbolicHotKey(hotKey, &keyCode, &modifiers, &enabled) != kSLSErrorSuccess)
            continue;
        if ([entry[@"enabled"] respondsToSelector:@selector(boolValue)])
            enabled = [entry[@"enabled"] boolValue];
        NSArray *parameters = [entry[@"value"] isKindOfClass:[NSDictionary class]] ? entry[@"value"][@"parameters"] : nil;
        if ([parameters isKindOfClass:[NSArray class]] && [parameters count] >= 3 &&
            [parameters[1] unsignedIntValue] != 0xffff) {
            keyCode = (uint16_t)[parameters[1] unsignedIntValue];
            modifiers = [parameters[2] unsignedIntValue];
        }
        int err = SLSSetSymbolicHotKey(hotKey, keyCode, modifiers, enabled);
        os_log_info(OS_LOG_DEFAULT, "hotkey %u from the user: %d", hotKey, err);
    }
}

- (void)start
{
    static const uint32_t owned[] = { kSLSHotKeyScreenshot, kSLSHotKeyScreenshotToClipboard,
                                      kSLSHotKeyPreviousInputSource, kSLSHotKeyNextInputSource };

    applyUserHotKeys();
    _currentSource = currentSourceID();
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(sourceChanged:)
        name:(__bridge NSString *)kTISNotifySelectedKeyboardInputSourceChanged object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(hotKey:)
        name:@"_NSSkyLightSymbolicHotKeyNotification" object:nil];
    for (size_t i = 0; i < sizeof(owned) / sizeof(owned[0]); i++) {
        int err = SLSClaimSymbolicHotKey(owned[i], 1);
        if (err != kSLSErrorSuccess)
            os_log_error(OS_LOG_DEFAULT, "hotkey %u not claimed: %d", owned[i], err);
    }
}

- (void)sourceChanged:(NSNotification *)note
{
    NSString *now = currentSourceID();
    if (now != nil && ![now isEqualToString:_currentSource]) {
        _previousSource = _currentSource;
        _currentSource = now;
    }
}

- (void)hotKey:(NSNotification *)note
{
    switch ([note.userInfo[@"hotKey"] unsignedIntValue]) {
    case kSLSHotKeyScreenshot:
        [self screenshotToClipboard:NO];
        break;
    case kSLSHotKeyScreenshotToClipboard:
        [self screenshotToClipboard:YES];
        break;
    case kSLSHotKeyPreviousInputSource:
        [self selectSource:_previousSource];
        break;
    case kSLSHotKeyNextInputSource:
        [self selectNextSource];
        break;
    }
}

/* The folder the screencapture domain names, or the Desktop. */
static NSString *
screenshotFolder(void)
{
    NSString *location = CFBridgingRelease(CFPreferencesCopyAppValue(CFSTR("location"), CFSTR("com.apple.screencapture")));
    BOOL folder = NO;

    if ([location isKindOfClass:[NSString class]]) {
        location = [location stringByExpandingTildeInPath];
        if ([[NSFileManager defaultManager] fileExistsAtPath:location isDirectory:&folder] && folder)
            return location;
    }
    return [NSHomeDirectory() stringByAppendingPathComponent:@"Desktop"];
}

/* "Screenshot 2026-11-22 at 19.38.12.png", as the reference names it. */
- (void)screenshotToClipboard:(BOOL)clipboard
{
    NSMutableArray *arguments = [NSMutableArray arrayWithObject:@"-x"];

    if (clipboard)
        [arguments addObject:@"-c"];
    else {
        NSDateFormatter *day = [[NSDateFormatter alloc] init], *time = [[NSDateFormatter alloc] init];
        [day setDateFormat:@"yyyy-MM-dd"];
        [time setDateFormat:@"HH.mm.ss"];
        NSDate *now = [NSDate date];
        NSString *name = [NSString stringWithFormat:NSLocalizedString(@"Screenshot %@ at %@", nil),
                          [day stringFromDate:now], [time stringFromDate:now]];
        [arguments addObject:[[screenshotFolder() stringByAppendingPathComponent:name]
                              stringByAppendingPathExtension:@"png"]];
    }
    NSTask *task = [[NSTask alloc] init];
    [task setLaunchPath:@"/usr/sbin/screencapture"];
    [task setArguments:arguments];
    @try {
        [task launch];
    } @catch (NSException *e) {
        os_log_error(OS_LOG_DEFAULT, "screencapture not started: %{public}s", [[e reason] UTF8String]);
    }
}

/* The keyboard sources in the input menu's order. */
static NSArray *
selectableSources(void)
{
    NSDictionary *filter = @{ (__bridge NSString *)kTISPropertyInputSourceCategory:
                                  (__bridge NSString *)kTISCategoryKeyboardInputSource,
                              (__bridge NSString *)kTISPropertyInputSourceIsSelectCapable: @YES };
    return CFBridgingRelease(TISCreateInputSourceList((__bridge CFDictionaryRef)filter, false));
}

- (void)selectSource:(NSString *)identifier
{
    if (identifier == nil)
        return;
    for (id object in selectableSources())
        if ([sourceID((__bridge TISInputSourceRef)object) isEqualToString:identifier]) {
            TISSelectInputSource((__bridge TISInputSourceRef)object);
            return;
        }
}

- (void)selectNextSource
{
    NSArray *sources = selectableSources();
    NSUInteger count = [sources count];

    for (NSUInteger i = 0; i < count; i++)
        if ([sourceID((__bridge TISInputSourceRef)sources[i]) isEqualToString:_currentSource]) {
            if (count > 1)
                TISSelectInputSource((__bridge TISInputSourceRef)sources[(i + 1) % count]);
            return;
        }
    if (count > 0)
        TISSelectInputSource((__bridge TISInputSourceRef)sources[0]);
}

@end
