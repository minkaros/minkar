/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Files' preferences (FilesPreferences.h): one window, its controls laid
 * out by hand, each written to the defaults as it changes.
 */

#import "FilesPreferences.h"
#include <os/log.h>

NSString *const FilesPreferencesChangedNotification = @"FilesPreferencesChangedNotification";

static NSString *const kNewWindowTarget = @"NewWindowTarget";
static NSString *const kWarnOnEmptyTrash = @"WarnOnEmptyTrash";
static NSString *const kFoldersFirst = @"_FXSortFoldersFirst";
static NSString *const kShowAllFiles = @"AppleShowAllFiles";
static NSString *const kShowHardDrives = @"ShowHardDrivesOnDesktop";
static NSString *const kShowExternal = @"ShowExternalHardDrivesOnDesktop";

static BOOL flag(NSString *key, BOOL byDefault)
{
    id value = [[NSUserDefaults standardUserDefaults] objectForKey:key];
    return value != nil ? [value boolValue] : byDefault;
}

/* NewWindowTarget's values, the reference's: home, desktop, documents,
   computer. */
static NSArray<NSArray *> *targets(void)
{
    return @[ @[ @"PfHm", @"Home" ], @[ @"PfDe", @"Desktop" ], @[ @"PfDo", @"Documents" ],
              @[ @"PfCm", @"Computer" ] ];
}

NSURL *FilesNewWindowFolder(void)
{
    NSString *target = [[NSUserDefaults standardUserDefaults] stringForKey:kNewWindowTarget];
    NSString *home = NSHomeDirectory();
    NSString *path = home;

    if ([target isEqualToString:@"PfDe"])
        path = [home stringByAppendingPathComponent:@"Desktop"];
    else if ([target isEqualToString:@"PfDo"])
        path = [home stringByAppendingPathComponent:@"Documents"];
    else if ([target isEqualToString:@"PfCm"])
        path = @"/";
    BOOL dir = NO;
    if (![[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&dir] || !dir)
        path = home;
    return [NSURL fileURLWithPath:path isDirectory:YES];
}

BOOL FilesWarnsOnEmptyTrash(void) { return flag(kWarnOnEmptyTrash, YES); }
BOOL FilesSortsFoldersFirst(void) { return flag(kFoldersFirst, YES); }
BOOL FilesShowsHiddenFiles(void) { return flag(kShowAllFiles, NO); }
BOOL FilesDesktopShowsStartupDisk(void) { return flag(kShowHardDrives, NO); }
BOOL FilesDesktopShowsVolumes(void) { return flag(kShowExternal, YES); }

void FilesSetShowsHiddenFiles(BOOL shown)
{
    [[NSUserDefaults standardUserDefaults] setBool:shown forKey:kShowAllFiles];
    [[NSNotificationCenter defaultCenter] postNotificationName:FilesPreferencesChangedNotification object:nil];
}

@implementation FilesPreferences {
    NSWindow *_window;
    NSPopUpButton *_target;
}

+ (FilesPreferences *)shared
{
    static FilesPreferences *shared;
    if (shared == nil)
        shared = [[FilesPreferences alloc] init];
    return shared;
}

+ (void)show
{
    FilesPreferences *p = [self shared];
    [p build];
    [p->_window center];
    [p->_window makeKeyAndOrderFront:nil];
}

static NSTextField *label(NSString *text, NSRect frame, BOOL right)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
    [field setStringValue:text];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setBordered:NO];
    [field setDrawsBackground:NO];
    [field setAlignment:right ? NSRightTextAlignment : NSLeftTextAlignment];
    return field;
}

- (NSButton *)check:(NSString *)title key:(NSString *)key byDefault:(BOOL)byDefault frame:(NSRect)frame
{
    NSButton *box = [[NSButton alloc] initWithFrame:frame];
    [box setButtonType:NSSwitchButton];
    [box setTitle:title];
    [box setState:flag(key, byDefault) ? NSControlStateValueOn : NSControlStateValueOff];
    [box setIdentifier:key];
    [box setTarget:self];
    [box setAction:@selector(toggled:)];
    return box;
}

- (void)build
{
    if (_window != nil)
        return;
    const CGFloat width = 440, height = 236, left = 170;
    _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, width, height)
                                          styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                            backing:NSBackingStoreBuffered defer:NO];
    [_window setTitle:NSLocalizedString(@"Files Preferences", nil)];
    [_window setReleasedWhenClosed:NO];
    [_window setFrameAutosaveName:@"Files Preferences"];
    NSView *content = [_window contentView];
    CGFloat y = height - 40;

    [content addSubview:label(NSLocalizedString(@"Show these items on the desktop:", nil),
                              NSMakeRect(20, y, width - 40, 18), NO)];
    y -= 26;
    [content addSubview:[self check:NSLocalizedString(@"Hard disks", nil) key:kShowHardDrives byDefault:NO
                              frame:NSMakeRect(40, y, width - 60, 20)]];
    y -= 22;
    [content addSubview:[self check:NSLocalizedString(@"External disks", nil) key:kShowExternal byDefault:YES
                              frame:NSMakeRect(40, y, width - 60, 20)]];
    y -= 38;

    [content addSubview:label(NSLocalizedString(@"New windows show:", nil), NSMakeRect(20, y + 3, left - 28, 18), YES)];
    _target = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(left, y, 200, 26) pullsDown:NO];
    NSString *current = [[NSUserDefaults standardUserDefaults] stringForKey:kNewWindowTarget] ?: @"PfHm";
    for (NSArray *t in targets()) {
        [_target addItemWithTitle:NSLocalizedString(t[1], nil)];
        [[_target lastItem] setRepresentedObject:t[0]];
        if ([t[0] isEqualToString:current])
            [_target selectItem:[_target lastItem]];
    }
    [_target setTarget:self];
    [_target setAction:@selector(targetChosen:)];
    [content addSubview:_target];
    y -= 40;

    [content addSubview:[self check:NSLocalizedString(@"Show warning before emptying the Trash", nil)
                                key:kWarnOnEmptyTrash byDefault:YES frame:NSMakeRect(20, y, width - 40, 20)]];
    y -= 24;
    [content addSubview:[self check:NSLocalizedString(@"Keep folders on top", nil) key:kFoldersFirst byDefault:YES
                              frame:NSMakeRect(20, y, width - 40, 20)]];
    y -= 24;
    [content addSubview:[self check:NSLocalizedString(@"Show hidden files", nil) key:kShowAllFiles byDefault:NO
                              frame:NSMakeRect(20, y, width - 40, 20)]];
}

- (void)toggled:(NSButton *)sender
{
    [[NSUserDefaults standardUserDefaults] setBool:[sender state] == NSControlStateValueOn forKey:[sender identifier]];
    os_log_info(OS_LOG_DEFAULT, "preference %{public}s %d", [[sender identifier] UTF8String],
                [sender state] == NSControlStateValueOn);
    [[NSNotificationCenter defaultCenter] postNotificationName:FilesPreferencesChangedNotification object:nil];
}

- (void)targetChosen:(NSPopUpButton *)sender
{
    [[NSUserDefaults standardUserDefaults] setObject:[[sender selectedItem] representedObject] forKey:kNewWindowTarget];
}

@end
