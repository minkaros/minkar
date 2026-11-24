/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Icon, cursor and appearance themes (a trial, the maintainer's, 24 Nov
 * 2026; the appearance's colors in NSSystemAccent.m): a theme
 * is a folder of icons laid out as /System/Library/Icons (GROUP/NAME.png),
 * or of cursors as /System/Library/Cursors (NAME.png, NAME@2x.png and
 * Cursors.plist), in Themes/Icons or Themes/Cursors of ~/Library or
 * /Library. MinkarIconTheme and MinkarCursorTheme (the global domain) name
 * the one chosen; what a theme lacks comes from the system's set. A change
 * is announced by org.minkar.ThemesChanged: each app forgets its standard
 * images and cursors and posts _NSSystemThemesChangedNotification for its
 * views to draw them again.
 */

#import <AppKit/NSImage.h>
#import <AppKit/NSCursor.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSView.h>
#import <Foundation/Foundation.h>

NSString *const _NSSystemThemesChangedNotification = @"_NSSystemThemesChangedNotification";
NSString *const _NSSystemThemesChangedDistributedNotification = @"org.minkar.ThemesChanged";

extern void _NSForgetStandardImages(void);
extern void _NSForgetCursorImages(void);

/* The folders a kind of theme ("Icons" or "Cursors") is looked for in,
   the user's first. */
static NSArray *themeFolders(NSString *kind) {
    NSString *user = [[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Themes"]
        stringByAppendingPathComponent:kind];
    return [NSArray arrayWithObjects:user,
        [@"/Library/Themes" stringByAppendingPathComponent:kind], nil];
}

/* The chosen theme's folder of that kind, or nil for the system's set. */
static NSString *chosenTheme(NSString *kind, NSString *key) {
    NSString *name = [[NSUserDefaults standardUserDefaults] stringForKey:key];
    BOOL isDirectory = NO;

    if ([name length] == 0 || [name rangeOfString:@"/"].location != NSNotFound)
        return nil;
    for (NSString *folder in themeFolders(kind)) {
        NSString *path = [folder stringByAppendingPathComponent:name];
        if ([[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&isDirectory] && isDirectory)
            return path;
    }
    return nil;
}

/* The themes of a kind found, by name, sorted. */
static NSArray *themeNames(NSString *kind) {
    NSMutableSet *names = [NSMutableSet set];
    NSFileManager *fm = [NSFileManager defaultManager];

    for (NSString *folder in themeFolders(kind))
        for (NSString *entry in [fm contentsOfDirectoryAtPath:folder error:NULL]) {
            BOOL isDirectory = NO;
            if (![entry hasPrefix:@"."] &&
                [fm fileExistsAtPath:[folder stringByAppendingPathComponent:entry] isDirectory:&isDirectory] &&
                isDirectory)
                [names addObject:entry];
        }
    return [[names allObjects] sortedArrayUsingSelector:@selector(localizedStandardCompare:)];
}

NSArray *_NSIconThemeNames(void) {
    return themeNames(@"Icons");
}

NSArray *_NSCursorThemeNames(void) {
    return themeNames(@"Cursors");
}

/* Appearance themes (Themes/Appearance/NAME/Appearance.plist: system
   colors for Light and Dark), applied in NSSystemAccent.m. */
NSArray *_NSAppearanceThemeNames(void) {
    return themeNames(@"Appearance");
}

/* An icon of the system's set (RELATIVE: GROUP/NAME.png), from the chosen
   theme when it has it. */
NSString *_NSSystemIconPath(NSString *relative) {
    NSString *theme = chosenTheme(@"Icons", @"MinkarIconTheme");

    if (theme != nil) {
        NSString *path = [theme stringByAppendingPathComponent:relative];
        if ([[NSFileManager defaultManager] fileExistsAtPath:path])
            return path;
    }
    return [@"/System/Library/Icons" stringByAppendingPathComponent:relative];
}

/* The folder cursors are read from: the chosen theme's when it has that
   cursor (NAME.png), else the system's. */
NSString *_NSSystemCursorFolder(NSString *name) {
    NSString *theme = chosenTheme(@"Cursors", @"MinkarCursorTheme");

    if (theme != nil && name != nil &&
        [[NSFileManager defaultManager] fileExistsAtPath:
            [theme stringByAppendingPathComponent:[name stringByAppendingPathExtension:@"png"]]])
        return theme;
    return @"/System/Library/Cursors";
}

@interface _NSSystemThemesWatcher : NSObject
@end

@implementation _NSSystemThemesWatcher

- (void)themesChanged:(NSNotification *)note {
    [[NSUserDefaults standardUserDefaults] synchronize];
    _NSForgetStandardImages();
    _NSForgetCursorImages();
    [[NSNotificationCenter defaultCenter] postNotificationName:_NSSystemThemesChangedNotification object:NSApp];
    for (NSWindow *window in [NSApp windows])
        [[[window contentView] superview] setNeedsDisplay:YES];
}

@end

/* Each app hears of a theme chosen (NSApplication starts it). */
void _NSWatchSystemThemes(void) {
    static _NSSystemThemesWatcher *watcher;

    if (watcher != nil)
        return;
    watcher = [_NSSystemThemesWatcher new];
    [[NSDistributedNotificationCenter defaultCenter] addObserver:watcher selector:@selector(themesChanged:)
        name:_NSSystemThemesChangedDistributedNotification object:nil
        suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
}
