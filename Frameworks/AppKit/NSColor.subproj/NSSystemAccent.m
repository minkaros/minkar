/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The system's accent and highlight colors (System Preferences' General
 * pane), under the reference's keys in the global domain:
 * AppleAccentColor (-1 graphite, 0 red, 1 orange, 2 yellow, 3 green,
 * absent blue, 5 purple, 6 pink) and AppleHighlightColor ("R G B Name",
 * the selected text's background; absent, the accent's own). The System
 * color list's accent colors are set from them when an app starts and
 * again when AppleColorPreferencesChangedNotification comes (a distributed
 * notification, as on the reference); then every window is drawn again
 * and NSSystemColorsDidChangeNotification posted. The tones are Atlas's
 * own palette.
 *
 * The appearance, the same way: AppleInterfaceStyle Dark (or, with
 * AppleInterfaceStyleSwitchesAutomatically, the night: 19:00 to 7:00
 * local time) gives the System color list Atlas's dark neutrals, the
 * light ones kept to come back to; AppleInterfaceThemeChangedNotification
 * says it changed. Colors views and windows already took from the list
 * (a window's background, a text view's) are given the new ones.
 */

#import <AppKit/NSColor.h>
#import <AppKit/NSColorList.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSView.h>
#import <AppKit/NSMatrix.h>
#import <AppKit/NSButtonCell.h>
#import <Foundation/NSDistributedNotificationCenter.h>
#import <Foundation/NSNotification.h>
#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/NSTimer.h>
#import <Foundation/NSHashTable.h>
#import <objc/runtime.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dispatch/dispatch.h>
#import <Foundation/NSPathUtilities.h>

NSString *const _NSColorPreferencesChangedNotification = @"AppleColorPreferencesChangedNotification";

/* An accent: the color itself, a selected row in a focused list (deeper),
   and selected text's background (lighter). */
typedef struct {
    int value;
    CGFloat accent[3], list[3], selection[3];
} NSAccentTones;

static const NSAccentTones kAccents[] = {
    { 4, { 0.13, 0.47, 0.95 }, { 0.10, 0.38, 0.83 }, { 0.70, 0.84, 1.00 } },    /* blue, the default */
    { 5, { 0.58, 0.34, 0.80 }, { 0.48, 0.26, 0.69 }, { 0.88, 0.80, 0.98 } },    /* purple */
    { 6, { 0.93, 0.33, 0.62 }, { 0.80, 0.24, 0.51 }, { 0.99, 0.80, 0.90 } },    /* pink */
    { 0, { 0.91, 0.27, 0.27 }, { 0.78, 0.19, 0.19 }, { 1.00, 0.80, 0.79 } },    /* red */
    { 1, { 0.96, 0.53, 0.13 }, { 0.84, 0.43, 0.07 }, { 1.00, 0.87, 0.72 } },    /* orange */
    { 2, { 0.95, 0.75, 0.10 }, { 0.80, 0.60, 0.03 }, { 1.00, 0.93, 0.68 } },    /* yellow */
    { 3, { 0.28, 0.69, 0.29 }, { 0.20, 0.56, 0.22 }, { 0.80, 0.94, 0.76 } },    /* green */
    { -1, { 0.54, 0.54, 0.57 }, { 0.44, 0.44, 0.47 }, { 0.85, 0.85, 0.87 } },   /* graphite */
};

static NSColor *rgb(const CGFloat c[3], CGFloat alpha) {
    return [NSColor colorWithCalibratedRed:c[0] green:c[1] blue:c[2] alpha:alpha];
}

static id globalValue(CFStringRef key) {
    CFPropertyListRef value = CFPreferencesCopyValue(key, kCFPreferencesAnyApplication,
        kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    return [(id)value autorelease];
}

/* The accent the global domain names; blue when it names none. */
static const NSAccentTones *currentAccent(void) {
    id value = globalValue(CFSTR("AppleAccentColor"));
    int accent = 4;

    if ([value respondsToSelector:@selector(intValue)])
        accent = [value intValue];
    for (size_t i = 0; i < sizeof(kAccents) / sizeof(kAccents[0]); i++)
        if (kAccents[i].value == accent)
            return &kAccents[i];
    return &kAccents[0];
}

/* "R G B Name": the highlight color chosen, else nil. */
static NSColor *currentHighlight(void) {
    id value = globalValue(CFSTR("AppleHighlightColor"));
    NSArray *parts;

    if (![value isKindOfClass:[NSString class]])
        return nil;
    parts = [value componentsSeparatedByString:@" "];
    if ([parts count] < 3)
        return nil;
    return [NSColor colorWithCalibratedRed:[[parts objectAtIndex:0] doubleValue]
                                     green:[[parts objectAtIndex:1] doubleValue]
                                      blue:[[parts objectAtIndex:2] doubleValue] alpha:1];
}

/* Atlas's dark neutrals: a grey, then alpha (1 when absent). Chosen by
   the maintainer, 23 Nov 2026: windows and menus 0.176, content and text
   fields 0.20, the tab bar 0.153, borders 0.09, a chosen row 0.13 (darker
   than its list), text 0.93. */
typedef struct {
    NSString *name;
    CGFloat white;
} _NSAtlasDarkGray;

static const _NSAtlasDarkGray kDark[] = {
    { @"labelColor", 0.93 }, { @"secondaryLabelColor", 0.66 }, { @"tertiaryLabelColor", 0.46 },
    { @"quaternaryLabelColor", 0.34 }, { @"textColor", 0.93 }, { @"textBackgroundColor", 0.2 },
    { @"selectedTextColor", 0.96 }, { @"unemphasizedSelectedTextColor", 0.92 },
    { @"unemphasizedSelectedTextBackgroundColor", 0.13 }, { @"placeholderTextColor", 0.46 },
    { @"headerTextColor", 0.93 }, { @"windowBackgroundColor", 0.176 }, { @"underPageBackgroundColor", 0.153 },
    { @"windowFrameColor", 0.176 }, { @"windowFrameTextColor", 0.88 }, { @"controlBackgroundColor", 0.2 },
    { @"controlAlternatingRowColor", 0.22 }, { @"alternatingContentBackgroundColor", 0.22 },
    { @"unemphasizedSelectedContentBackgroundColor", 0.13 }, { @"gridColor", 0.12 },
    { @"separatorColor", 0.09 }, { @"controlColor", 0.36 }, { @"controlTextColor", 0.93 },
    { @"disabledControlTextColor", 0.46 }, { @"selectedControlTextColor", 0.96 },
    { @"secondarySelectedControlColor", 0.13 }, { @"controlHighlightColor", 0.40 },
    { @"controlLightHighlightColor", 0.44 }, { @"controlShadowColor", 0.12 },
    { @"controlDarkShadowColor", 0.06 }, { @"highlightColor", 0.44 }, { @"scrollBarColor", 0.2 },
    { @"knobColor", 0.46 }, { @"selectedKnobColor", 0.56 }, { @"menuBackgroundColor", 0.176 },
    { @"menuItemTextColor", 0.93 },
};
static const size_t kDarkCount = sizeof(kDark) / sizeof(kDark[0]);

static BOOL appearanceDark = NO;
static NSMutableDictionary *lightColors;    /* name -> the list's light color */

/* The night, for Auto. */
static BOOL nightNow(void) {
    time_t now = time(NULL);
    struct tm local;

    localtime_r(&now, &local);
    return local.tm_hour >= 19 || local.tm_hour < 7;
}

BOOL _NSSystemAppearanceIsDark(void) {
    return appearanceDark;
}

static BOOL wantsDark(void) {
    if ([globalValue(CFSTR("AppleInterfaceStyleSwitchesAutomatically")) boolValue])
        return nightNow();
    return [globalValue(CFSTR("AppleInterfaceStyle")) isEqual:@"Dark"];
}

/* An appearance theme (a trial, the maintainer's, 24 Nov 2026): the
   folder Themes/Appearance/NAME of ~/Library or /Library that
   MinkarAppearanceTheme names, its Appearance.plist giving, for Light and
   for Dark, system colors by name: "0.176" a grey (gamma 2.2), "r g b" or
   "r g b a" (sRGB). Laid over Atlas's own; what it lacks stays Atlas's. */
static NSDictionary *atlasAppearance(BOOL dark) {
    static NSDictionary *plist;

    if (plist == nil)
        plist = [[NSDictionary alloc] initWithContentsOfFile:
            [[NSBundle bundleForClass:[NSColorList class]] pathForResource:@"AtlasAppearance" ofType:@"plist"]];
    id colors = [plist objectForKey:dark ? @"Dark" : @"Light"];
    return [colors isKindOfClass:[NSDictionary class]] ? colors : nil;
}

static NSDictionary *appearanceTheme(BOOL dark) {
    NSString *name = globalValue(CFSTR("MinkarAppearanceTheme"));

    /* None chosen (or not found): Atlas's own look, approved by the
       maintainer on 25 Nov 2026 (AtlasAppearance.plist): light #ececec,
       dark #555555, defined control borders, a blue accent, a green
       switch, an orange level. */
    if (![name isKindOfClass:[NSString class]] || [name length] == 0 ||
        [name rangeOfString:@"/"].location != NSNotFound)
        return atlasAppearance(dark);
    for (NSString *base in [NSArray arrayWithObjects:
            [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Themes/Appearance"],
            @"/Library/Themes/Appearance", nil]) {
        NSDictionary *plist = [NSDictionary dictionaryWithContentsOfFile:
            [[base stringByAppendingPathComponent:name] stringByAppendingPathComponent:@"Appearance.plist"]];
        if (plist != nil) {
            id colors = [plist objectForKey:dark ? @"Dark" : @"Light"];
            return [colors isKindOfClass:[NSDictionary class]] ? colors : nil;
        }
    }
    return atlasAppearance(dark);
}

static NSColor *themeColor(id value) {
    if (![value isKindOfClass:[NSString class]])
        return nil;
    NSArray *parts = [value componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    if ([parts count] == 1)
        return [NSColor colorWithGenericGamma22White:[value doubleValue] alpha:1];
    /* sRGB, as a color picked from the screen or written in hex is (the
       calibrated space, gamma 1.8, showed them lighter). */
    if ([parts count] >= 3)
        return [NSColor colorWithSRGBRed:[[parts objectAtIndex:0] doubleValue]
            green:[[parts objectAtIndex:1] doubleValue] blue:[[parts objectAtIndex:2] doubleValue]
            alpha:[parts count] > 3 ? [[parts objectAtIndex:3] doubleValue] : 1];
    return nil;
}

/* The system colors a theme changed, as they were before it: put back
   when the theme no longer gives them. */
static NSMutableDictionary *themeReplaced;

/* Whether an appearance theme gives colors now: apps that draw with their
   own greys (Files, the Dock) take the system's colors then. */
static BOOL themeActive;

BOOL _NSAppearanceThemeActive(void) {
    return themeActive;
}

/* The appearance theme's colors for the appearance shown. */
static NSDictionary *currentTheme;

/* A color the appearance theme gives by name, nil when there is no theme
   or it does not give it: besides the system colors, the controls' own
   (MinkarControlBorderColor, MinkarControlFaceBottomColor,
   MinkarControlTrackColor, MinkarProgressColor, MinkarSliderFillColor,
   MinkarSwitchOnColor, MinkarLevelColor, MinkarMenuBarColor,
   MinkarMenuBarLineColor, MinkarMenuBarTextColor, MinkarAccentColor,
   MinkarSidebarSelectionColor). */
NSColor *_NSAppearanceThemeColor(NSString *key) {
    return themeActive ? themeColor([currentTheme objectForKey:key]) : nil;
}

/* The neutrals for the appearance: dark ones, or the light ones kept. */
static void applyAppearance(BOOL dark) {
    NSColorList *list = [NSColorList colorListNamed:@"System"];

    if (list == nil)
        return;
    if (lightColors == nil) {
        lightColors = [NSMutableDictionary new];
        for (size_t i = 0; i < kDarkCount; i++) {
            NSColor *c = [NSColor colorWithCatalogName:@"System" colorName:kDark[i].name];
            if (c != nil)
                [lightColors setObject:c forKey:kDark[i].name];
        }
    }
    for (size_t i = 0; i < kDarkCount; i++) {
        NSColor *c = dark ? [NSColor colorWithGenericGamma22White:kDark[i].white alpha:1]
                          : [lightColors objectForKey:kDark[i].name];
        if (c != nil)
            [list setColor:c forKey:kDark[i].name];
    }
    /* The theme's colors over them; others it changed before, back. */
    NSDictionary *theme = appearanceTheme(dark);
    themeActive = [theme count] > 0;
    [currentTheme release];
    currentTheme = [theme retain];
    if (themeReplaced == nil)
        themeReplaced = [NSMutableDictionary new];
    for (NSString *key in [themeReplaced allKeys])
        if ([theme objectForKey:key] == nil) {
            BOOL neutral = NO;
            for (size_t i = 0; i < kDarkCount && !neutral; i++)
                neutral = [kDark[i].name isEqualToString:key];
            if (!neutral)
                [list setColor:[themeReplaced objectForKey:key] forKey:key];
            [themeReplaced removeObjectForKey:key];
        }
    for (NSString *key in theme) {
        NSColor *c = themeColor([theme objectForKey:key]);
        if (c == nil)
            continue;
        if ([themeReplaced objectForKey:key] == nil) {
            NSColor *was = [list colorWithKey:key];
            if (was != nil)
                [themeReplaced setObject:was forKey:key];
        }
        [list setColor:c forKey:key];
    }
    appearanceDark = dark;
}

void _NSApplySystemAccentColors(void) {
    NSColorList *list = [NSColorList colorListNamed:@"System"];
    const NSAccentTones *tones = currentAccent();
    NSColor *accent = rgb(tones->accent, 1), *rows = rgb(tones->list, 1);
    NSColor *selection = currentHighlight() ?: rgb(tones->selection, 1);
    NSColor *themeAccent = _NSAppearanceThemeColor(@"MinkarAccentColor");

    /* An appearance theme's own accent (Atlas's blue) while none is chosen
       in General: the controls, chosen rows and (unless a highlight is
       chosen) the text's selection, lighter. One chosen there wins. */
    if (themeAccent != nil && globalValue(CFSTR("AppleAccentColor")) == nil) {
        accent = rows = themeAccent;
        if (currentHighlight() == nil)
            selection = [themeAccent blendedColorWithFraction:0.6 ofColor:[NSColor whiteColor]];
    }

    /* In the dark, the highlight half as bright: light text stays legible. */
    if (appearanceDark)
        selection = [selection blendedColorWithFraction:0.55 ofColor:[NSColor blackColor]];

    if (list == nil)
        return;
    [list setColor:accent forKey:@"controlAccentColor"];
    [list setColor:accent forKey:@"selectedMenuItemColor"];
    [list setColor:accent forKey:@"textInsertionPointColor"];
    [list setColor:[accent colorWithAlphaComponent:0.5] forKey:@"keyboardFocusIndicatorColor"];
    [list setColor:rows forKey:@"selectedContentBackgroundColor"];
    [list setColor:rows forKey:@"alternateSelectedControlColor"];
    [list setColor:selection forKey:@"selectedTextBackgroundColor"];
    [list setColor:selection forKey:@"selectedControlColor"];
}

NSArray *_NSGraphicsStyleAccentChanged(void);

/* A button's cells holding an old check box or radio button image get the
   new one; then its subviews. */
static void swapToggleImages(NSView *view, NSArray *pairs) {
    NSArray *cells = nil;

    if ([view isKindOfClass:[NSMatrix class]])
        cells = [(NSMatrix *)view cells];
    else if ([view isKindOfClass:[NSControl class]] && [[(NSControl *)view cell] isKindOfClass:[NSButtonCell class]])
        cells = [NSArray arrayWithObject:[(NSControl *)view cell]];
    for (id cell in cells) {
        if (![cell isKindOfClass:[NSButtonCell class]])
            continue;
        for (NSUInteger i = 0; i + 1 < [pairs count]; i += 2) {
            if ([cell image] == [pairs objectAtIndex:i])
                [cell setImage:[pairs objectAtIndex:i + 1]];
            if ([cell alternateImage] == [pairs objectAtIndex:i])
                [cell setAlternateImage:[pairs objectAtIndex:i + 1]];
        }
    }
    for (NSView *sub in [view subviews])
        swapToggleImages(sub, pairs);
}

/* The System list's colors now, by name: compared before and after a
   change, to find the copies views took. */
static NSDictionary *systemColorsNow(void) {
    NSColorList *list = [NSColorList colorListNamed:@"System"];
    NSMutableDictionary *now = [NSMutableDictionary dictionary];

    for (NSString *key in [list allKeys]) {
        NSColor *c = [NSColor colorWithCatalogName:@"System" colorName:key];
        if (c != nil)
            [now setObject:c forKey:key];
    }
    return now;
}

/* The same color, by its RGBA components (copies of a color are not
   isEqual: to it). */
static BOOL sameColor(NSColor *a, NSColor *b) {
    NSColor *x = [a colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
    NSColor *y = [b colorUsingColorSpaceName:NSCalibratedRGBColorSpace];

    if (x == nil || y == nil)
        return NO;
    return fabs([x redComponent] - [y redComponent]) < 0.002 &&
           fabs([x greenComponent] - [y greenComponent]) < 0.002 &&
           fabs([x blueComponent] - [y blueComponent]) < 0.002 &&
           fabs([x alphaComponent] - [y alphaComponent]) < 0.002;
}

/* Colors a view or window holds that were the System list's: the new ones
   of the same names. Names that did not change, and the accent's (other
   names share its value), are left alone. */
static NSColor *replacement(NSColor *color, NSDictionary *before, NSDictionary *after) {
    if (color == nil)
        return nil;
    for (size_t i = 0; i < kDarkCount; i++) {
        NSColor *old = [before objectForKey:kDark[i].name], *new = [after objectForKey:kDark[i].name];
        if (old != nil && new != nil && !sameColor(old, new) && sameColor(old, color))
            return new;
    }
    return nil;
}

/* The colors given in this pass: a control and its cell share theirs, and
   one already given must not be taken for an old one again. */
static NSHashTable *givenColors;

static void swapColors(id object, NSDictionary *before, NSDictionary *after) {
    static const char *const pairs[][2] = {
        { "backgroundColor", "setBackgroundColor:" }, { "textColor", "setTextColor:" },
        { "fillColor", "setFillColor:" }, { "borderColor", "setBorderColor:" },
        { "gridColor", "setGridColor:" }, { "insertionPointColor", "setInsertionPointColor:" },
    };

    for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
        SEL get = sel_registerName(pairs[i][0]), set = sel_registerName(pairs[i][1]);
        if ([object respondsToSelector:get] && [object respondsToSelector:set]) {
            id value = [object performSelector:get];
            if (value == nil || [givenColors containsObject:value])
                continue;
            NSColor *new = [value isKindOfClass:[NSColor class]] ? replacement(value, before, after) : nil;
            if (new != nil) {
                [object performSelector:set withObject:new];
                [givenColors addObject:new];
            }
        }
    }
    if ([object isKindOfClass:[NSView class]]) {
        if ([object isKindOfClass:[NSControl class]])
            swapColors([(NSControl *)object cell], before, after);
        for (NSView *sub in [(NSView *)object subviews])
            swapColors(sub, before, after);
    }
}

@interface _NSSystemAccentWatcher : NSObject
@end

@implementation _NSSystemAccentWatcher

/* The appearance changed, or Auto's hour came: as the colors do. */
- (void)themeChanged:(NSNotification *)note {
    [self colorsChanged:note];
}

/* The keys General writes, as they are in the global domain now. */
static NSString *colorSettings(void) {
    return [NSString stringWithFormat:@"%d %@ %@ %@", wantsDark(), globalValue(CFSTR("AppleAccentColor")),
        globalValue(CFSTR("AppleHighlightColor")), globalValue(CFSTR("MinkarAppearanceTheme"))];
}

/* Seconds until Auto's next hour (7:00 or 19:00), and a moment more. */
static NSTimeInterval untilNextSwitch(void) {
    time_t now = time(NULL);
    struct tm local;

    localtime_r(&now, &local);
    int hour = local.tm_hour < 7 ? 7 : local.tm_hour < 19 ? 19 : 24 + 7;
    long seconds = (long)(hour - local.tm_hour) * 3600 - local.tm_min * 60 - local.tm_sec;
    return (NSTimeInterval)(seconds > 0 ? seconds : 1) + 1;
}

/* Auto's next hour: one timer, set again each time. */
- (void)scheduleSwitch {
    static NSTimer *timer;

    [timer invalidate];
    [timer release];
    timer = [[NSTimer scheduledTimerWithTimeInterval:untilNextSwitch() target:self selector:@selector(tick:)
        userInfo:nil repeats:NO] retain];
}

/* When ~/Library/Preferences changes (a write there, the preferences'
   files being written whole and renamed in), and at Auto's hours: the
   global preferences' file looked at (a stat), and when it changed, read
   again; an appearance, accent or highlight changed with no notification
   (defaults write) is followed as a notified one is. Nothing runs while
   nothing changes. */
- (void)tick:(NSTimer *)timer {
    static NSString *file;
    static struct timespec seen;
    static NSString *settings;
    struct stat st;

    if (file == nil)
        file = [[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Preferences/.GlobalPreferences.plist"] retain];
    if (stat([file fileSystemRepresentation], &st) == 0 &&
        (st.st_mtimespec.tv_sec != seen.tv_sec || st.st_mtimespec.tv_nsec != seen.tv_nsec)) {
        BOOL first = seen.tv_sec == 0 && seen.tv_nsec == 0;
        seen = st.st_mtimespec;
        CFPreferencesAppSynchronize(kCFPreferencesAnyApplication);
        NSString *now = colorSettings();
        if (!first && settings != nil && ![now isEqualToString:settings]) {
            [settings release];
            settings = [now retain];
            [self colorsChanged:nil];
            return;
        }
        [settings release];
        settings = [now retain];
    }
    if (wantsDark() != appearanceDark)
        [self colorsChanged:nil];
    if (timer != nil)
        [self scheduleSwitch];
}

/* The preferences' folder watched (kqueue, through dispatch); with no
   folder to watch, a look every two seconds as before. */
- (void)watchPreferences {
    NSString *folder = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Preferences"];
    int fd = open([folder fileSystemRepresentation], O_RDONLY | O_EVTONLY);

    if (fd < 0) {
        [NSTimer scheduledTimerWithTimeInterval:2 target:self selector:@selector(tick:) userInfo:nil repeats:YES];
        return;
    }
    dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, (uintptr_t)fd,
        DISPATCH_VNODE_WRITE | DISPATCH_VNODE_LINK | DISPATCH_VNODE_RENAME | DISPATCH_VNODE_DELETE,
        dispatch_get_main_queue());
    dispatch_source_set_event_handler(source, ^{
        [self tick:nil];
    });
    dispatch_source_set_cancel_handler(source, ^{
        close(fd);
    });
    dispatch_resume(source);
    [self scheduleSwitch];
}

/* The pane changed them: read again, then every window drawn anew. */
- (void)colorsChanged:(NSNotification *)note {
    NSDictionary *before = systemColorsNow();
    BOOL wasDark = appearanceDark;

    CFPreferencesAppSynchronize(kCFPreferencesAnyApplication);
    applyAppearance(wantsDark());
    _NSApplySystemAccentColors();
    NSDictionary *after = systemColorsNow();
    NSArray *pairs = _NSGraphicsStyleAccentChanged();
    if (wasDark != appearanceDark) {
        [NSApp willChangeValueForKey:@"effectiveAppearance"];
        [NSApp didChangeValueForKey:@"effectiveAppearance"];
    }
    givenColors = [NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
    for (NSWindow *window in [NSApp windows]) {
        NSView *top = [[window contentView] superview];
        NSColor *background = replacement([window backgroundColor], before, after);
        if (background != nil)
            [window setBackgroundColor:background];
        swapColors(top != nil ? top : [window contentView], before, after);
        swapToggleImages(top != nil ? top : [window contentView], pairs);
        if ([window isVisible])
            [window display];
    }
    givenColors = nil;
    [[NSNotificationCenter defaultCenter] postNotificationName:NSSystemColorsDidChangeNotification object:nil];
}

@end

void _NSWatchSystemAccentColors(void) {
    static _NSSystemAccentWatcher *watcher;

    if (watcher != nil)
        return;
    watcher = [_NSSystemAccentWatcher new];
    applyAppearance(wantsDark());
    _NSApplySystemAccentColors();
    if (appearanceDark)
        _NSGraphicsStyleAccentChanged();
    [[NSDistributedNotificationCenter defaultCenter] addObserver:watcher selector:@selector(colorsChanged:)
        name:_NSColorPreferencesChangedNotification object:nil];
    [[NSDistributedNotificationCenter defaultCenter] addObserver:watcher selector:@selector(themeChanged:)
        name:@"AppleInterfaceThemeChangedNotification" object:nil];
    /* An appearance theme chosen (General's Themes, with icons and cursors). */
    [[NSDistributedNotificationCenter defaultCenter] addObserver:watcher selector:@selector(themeChanged:)
        name:@"org.minkar.ThemesChanged" object:nil];
    [watcher tick:nil];
    [watcher watchPreferences];
}
