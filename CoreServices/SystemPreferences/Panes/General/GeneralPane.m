/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The General pane, laid out as the reference's, writing the global
 * domain's keys the reference writes (measured on it):
 *   Appearance        AppleInterfaceStyle (Dark, else absent),
 *                     AppleInterfaceStyleSwitchesAutomatically (Auto)
 *   Accent color      AppleAccentColor (-1 graphite, 0 red, 1 orange,
 *                     2 yellow, 3 green, 5 purple, 6 pink; blue: absent),
 *                     AppleAquaColorVariant (6 graphite, else 1) and the
 *                     accent's AppleHighlightColor
 *   Highlight color   AppleHighlightColor "R G B Name" (blue: absent)
 *   Sidebar icon size NSTableViewDefaultSizeMode 1, 2, 3
 *   Hide the menu bar _HIHideMenuBar
 *   Show scroll bars  AppleShowScrollBars Automatic, WhenScrolling, Always
 *   Click in the bar  AppleScrollerPagingBehavior (1: the spot clicked)
 *   Default browser   LaunchServices' handler of http and https
 *   Ask to keep...    NSCloseAlwaysConfirmsChanges
 *   Close windows...  NSQuitAlwaysKeepsWindows (the inverse)
 *   Recent items      NSRecentDocumentMaximum (AppKit's own key)
 *   Font smoothing    AppleFontSmoothing 0 when off (on: absent)
 * then posts what the reference posts: AppleColorPreferencesChangedNotification
 * (accent, highlight) and AppleInterfaceThemeChangedNotification
 * (appearance), distributed notifications. The view is made in code.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>

/* AppKit's: a color of the appearance theme shown (Atlas's accent). */
extern NSColor *_NSAppearanceThemeColor(NSString *key);
#include <CoreServices/CoreServices.h>
#include <os/log.h>

@interface GeneralPane : NSPreferencePane
@end

static const CGFloat kWidth = 668, kHeight = 606;
static const CGFloat kLabelWidth = 200, kLeft = 220;

/* The accents in the reference's order, its AppleAccentColor values, and
   each one's highlight color (Atlas's tones, as AppKit draws them). */
typedef struct {
    NSString *name;
    int value;          /* 4: blue, written as no key */
    CGFloat accent[3], highlight[3];
} Accent;

static const Accent kAccents[] = {
    { @"Blue", 4, { 0.13, 0.47, 0.95 }, { 0.70, 0.84, 1.00 } },
    { @"Purple", 5, { 0.58, 0.34, 0.80 }, { 0.88, 0.80, 0.98 } },
    { @"Pink", 6, { 0.93, 0.33, 0.62 }, { 0.99, 0.80, 0.90 } },
    { @"Red", 0, { 0.91, 0.27, 0.27 }, { 1.00, 0.80, 0.79 } },
    { @"Orange", 1, { 0.96, 0.53, 0.13 }, { 1.00, 0.87, 0.72 } },
    { @"Yellow", 2, { 0.95, 0.75, 0.10 }, { 1.00, 0.93, 0.68 } },
    { @"Green", 3, { 0.28, 0.69, 0.29 }, { 0.80, 0.94, 0.76 } },
    { @"Graphite", -1, { 0.54, 0.54, 0.57 }, { 0.85, 0.85, 0.87 } },
};
static const NSUInteger kAccentCount = sizeof(kAccents) / sizeof(kAccents[0]);

static const int kRecentCounts[] = { 0, 5, 10, 15, 20, 30, 50 };

static NSColor *rgb(const CGFloat c[3])
{
    return [NSColor colorWithCalibratedRed:c[0] green:c[1] blue:c[2] alpha:1];
}

static NSString *highlightString(const Accent *a)
{
    return [NSString stringWithFormat:@"%f %f %f %@", a->highlight[0], a->highlight[1], a->highlight[2], a->name];
}

static id globalValue(NSString *key)
{
    return CFBridgingRelease(CFPreferencesCopyValue((__bridge CFStringRef)key, kCFPreferencesAnyApplication,
                                                    kCFPreferencesCurrentUser, kCFPreferencesAnyHost));
}

static void setGlobal(NSString *key, id value)
{
    CFPreferencesSetValue((__bridge CFStringRef)key, (__bridge CFPropertyListRef)value, kCFPreferencesAnyApplication,
                          kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
}

static void syncGlobal(void)
{
    CFPreferencesSynchronize(kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
}

static void postDistributed(NSString *name)
{
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:name object:nil userInfo:nil
                                                       deliverImmediately:YES];
}

static NSTextField *label(NSString *text, NSRect frame, NSTextAlignment alignment)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
    [field setStringValue:text];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setBordered:NO];
    [field setDrawsBackground:NO];
    [field setAlignment:alignment];
    [field setFont:[NSFont systemFontOfSize:13]];
    return field;
}

static NSTextField *note(NSString *text, NSRect frame)
{
    NSTextField *field = label(text, frame, NSLeftTextAlignment);
    [field setFont:[NSFont systemFontOfSize:11]];
    [field setTextColor:[NSColor secondaryLabelColor]];
    return field;
}

static NSButton *button(NSString *title, NSRect frame, NSButtonType type, id target, SEL action)
{
    NSButton *b = [[NSButton alloc] initWithFrame:frame];
    [b setButtonType:type];
    [b setTitle:title];
    [b setTarget:target];
    [b setAction:action];
    return b;
}

static NSBox *separator(CGFloat y)
{
    NSBox *line = [[NSBox alloc] initWithFrame:NSMakeRect(20, y, kWidth - 40, 1)];
    [line setBoxType:NSBoxSeparator];
    return line;
}

/* An accent's swatch: a dot of its color, ringed when chosen. */
/* A clickable sample: a view of its own, with a target and an action. */
@interface GeneralClickable : NSView
@property (nonatomic, weak) id target;
@property (nonatomic) SEL action;
@property (nonatomic) NSInteger tag;
@end

@implementation GeneralClickable
@synthesize tag = _clickTag;
- (void)mouseDown:(NSEvent *)event
{
    [NSApp sendAction:self.action to:self.target from:self];
}
@end

@interface GeneralSwatch : GeneralClickable
@property (nonatomic, strong) NSColor *color;
@property (nonatomic) BOOL chosen;
@end

@implementation GeneralSwatch
- (void)drawRect:(NSRect)dirty
{
    NSRect dot = NSInsetRect(self.bounds, 3, 3);
    [self.color setFill];
    [[NSBezierPath bezierPathWithOvalInRect:dot] fill];
    [[self.color shadowWithLevel:0.25] setStroke];
    [[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(dot, 0.5, 0.5)] stroke];
    if (self.chosen) {
        [[NSColor whiteColor] setFill];
        [[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(dot, NSWidth(dot) / 2 - 2.5, NSHeight(dot) / 2 - 2.5)] fill];
    }
}
@end

/* An appearance's sample: a small window drawn light or dark (Auto: half
   of each), framed in the accent when chosen. */
@interface GeneralAppearanceSample : GeneralClickable
@property (nonatomic) NSInteger kind;   /* 0 light, 1 dark, 2 auto */
@property (nonatomic) BOOL chosen;
@end

@implementation GeneralAppearanceSample
- (void)drawRect:(NSRect)dirty
{
    NSRect card = NSInsetRect(self.bounds, 3, 3);
    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:card xRadius:6 yRadius:6];
    NSColor *light = [NSColor colorWithCalibratedWhite:0.93 alpha:1];
    NSColor *dark = [NSColor colorWithCalibratedWhite:0.20 alpha:1];

    [NSGraphicsContext saveGraphicsState];
    [shape addClip];
    [(self.kind == 1 ? dark : light) setFill];
    NSRectFill(card);
    if (self.kind == 2) {
        [dark setFill];
        NSRectFill(NSMakeRect(NSMidX(card), NSMinY(card), NSWidth(card) / 2, NSHeight(card)));
    }
    /* A title bar and two lines of text. */
    [[NSColor colorWithCalibratedWhite:(self.kind == 1 ? 0.30 : 0.82) alpha:1] setFill];
    NSRectFill(NSMakeRect(NSMinX(card), NSMaxY(card) - 10, NSWidth(card), 10));
    [[NSColor controlAccentColor] setFill];
    NSRectFill(NSMakeRect(NSMinX(card) + 8, NSMaxY(card) - 22, NSWidth(card) * 0.45, 5));
    [NSGraphicsContext restoreGraphicsState];
    [(self.chosen ? [NSColor controlAccentColor] : [NSColor colorWithCalibratedWhite:0.65 alpha:1]) setStroke];
    [shape setLineWidth:self.chosen ? 3 : 1];
    [shape stroke];
}
@end

@implementation GeneralPane {
    NSArray<GeneralAppearanceSample *> *_appearances;
    NSArray<GeneralSwatch *> *_swatches;
    NSTextField *_accentName;
    NSPopUpButton *_highlight, *_sidebar, *_browser, *_recent;
    NSPopUpButton *_appearanceTheme, *_iconTheme, *_cursorTheme;
    NSButton *_hideMenuBar, *_scrollAuto, *_scrollWhen, *_scrollAlways, *_pageNext, *_pageSpot;
    NSButton *_askKeep, *_closeWindows, *_smoothing;
    NSArray<NSString *> *_browsers;
}

- (NSString *)text:(NSString *)english
{
    return [[self bundle] localizedStringForKey:english value:english table:nil];
}

- (NSTextField *)rowLabel:(NSString *)text y:(CGFloat)y
{
    return label([self text:text], NSMakeRect(20, y, kLabelWidth - 8, 20), NSRightTextAlignment);
}

- (NSView *)loadMainView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];
    CGFloat y = kHeight - 30;

    /* Appearance. */
    [view addSubview:[self rowLabel:@"Appearance:" y:y]];
    NSMutableArray *samples = [NSMutableArray array];
    NSArray *names = @[ @"Light", @"Dark", @"Auto" ];
    for (NSInteger i = 0; i < 3; i++) {
        GeneralAppearanceSample *s = [[GeneralAppearanceSample alloc]
            initWithFrame:NSMakeRect(kLeft + i * 90, y - 34, 72, 50)];
        s.kind = i;
        [s setTag:i];
        [s setTarget:self];
        [s setAction:@selector(chooseAppearance:)];
        [view addSubview:s];
        [samples addObject:s];
        [view addSubview:note([self text:names[i]], NSMakeRect(kLeft + i * 90, y - 52, 72, 16))];
        [[view.subviews lastObject] setAlignment:NSCenterTextAlignment];
    }
    _appearances = samples;
    y -= 76;

    /* Accent color. */
    [view addSubview:[self rowLabel:@"Accent color:" y:y]];
    NSMutableArray *swatches = [NSMutableArray array];
    for (NSUInteger i = 0; i < kAccentCount; i++) {
        GeneralSwatch *s = [[GeneralSwatch alloc] initWithFrame:NSMakeRect(kLeft + i * 26, y - 2, 24, 24)];
        s.color = rgb(kAccents[i].accent);
        [s setTag:i];
        [s setTarget:self];
        [s setAction:@selector(chooseAccent:)];
        [view addSubview:s];
        [swatches addObject:s];
    }
    _swatches = swatches;
    _accentName = note(@"", NSMakeRect(kLeft, y - 20, 200, 16));
    [view addSubview:_accentName];
    y -= 44;

    /* Highlight color. */
    [view addSubview:[self rowLabel:@"Highlight color:" y:y]];
    _highlight = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(kLeft, y - 4, 160, 26) pullsDown:NO];
    for (NSUInteger i = 0; i < kAccentCount; i++)
        [_highlight addItemWithTitle:[self text:kAccents[i].name]];
    [_highlight setTarget:self];
    [_highlight setAction:@selector(chooseHighlight:)];
    [view addSubview:_highlight];
    y -= 34;

    /* Sidebar icon size. */
    [view addSubview:[self rowLabel:@"Sidebar icon size:" y:y]];
    _sidebar = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(kLeft, y - 4, 160, 26) pullsDown:NO];
    [_sidebar addItemsWithTitles:@[ [self text:@"Small"], [self text:@"Medium"], [self text:@"Large"] ]];
    [_sidebar setTarget:self];
    [_sidebar setAction:@selector(chooseSidebar:)];
    [view addSubview:_sidebar];
    y -= 34;

    /* Icon and cursor themes (a trial): the themes put in Themes/Icons
       and Themes/Cursors of /Library or ~/Library, or Atlas's own. */
    [view addSubview:[self rowLabel:@"Themes:" y:y]];
    NSArray *kinds = @[ @"Appearance", @"Icons", @"Cursors" ];
    NSMutableArray *popups = [NSMutableArray array];
    for (NSUInteger i = 0; i < [kinds count]; i++) {
        NSPopUpButton *p = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(kLeft + i * 142, y - 4, 134, 26)
                                                      pullsDown:NO];
        [p setTarget:self];
        [p setAction:@selector(chooseTheme:)];
        [view addSubview:p];
        [view addSubview:note([self text:kinds[i]], NSMakeRect(kLeft + i * 142 + 4, y - 22, 126, 16))];
        [popups addObject:p];
    }
    _appearanceTheme = popups[0];
    _iconTheme = popups[1];
    _cursorTheme = popups[2];
    y -= 54;

    _hideMenuBar = button([self text:@"Automatically hide and show the menu bar"], NSMakeRect(kLeft, y, 400, 22),
                          NSSwitchButton, self, @selector(hideMenuBarChanged:));
    [view addSubview:_hideMenuBar];
    y -= 14;
    [view addSubview:separator(y)];
    y -= 26;

    /* Scroll bars. */
    [view addSubview:[self rowLabel:@"Show scroll bars:" y:y]];
    _scrollAuto = button([self text:@"Automatically based on mouse or trackpad"], NSMakeRect(kLeft, y, 400, 22),
                         NSRadioButton, self, @selector(chooseScrollBars:));
    _scrollWhen = button([self text:@"When scrolling"], NSMakeRect(kLeft, y - 22, 400, 22), NSRadioButton, self,
                         @selector(chooseScrollBars:));
    _scrollAlways = button([self text:@"Always"], NSMakeRect(kLeft, y - 44, 400, 22), NSRadioButton, self,
                           @selector(chooseScrollBars:));
    for (NSButton *b in @[ _scrollAuto, _scrollWhen, _scrollAlways ])
        [view addSubview:b];
    y -= 76;
    [view addSubview:[self rowLabel:@"Click in the scroll bar to:" y:y]];
    _pageNext = button([self text:@"Jump to the next page"], NSMakeRect(kLeft, y, 400, 22), NSRadioButton, self,
                       @selector(choosePaging:));
    _pageSpot = button([self text:@"Jump to the spot that’s clicked"], NSMakeRect(kLeft, y - 22, 400, 22),
                       NSRadioButton, self, @selector(choosePaging:));
    [view addSubview:_pageNext];
    [view addSubview:_pageSpot];
    y -= 36;
    [view addSubview:separator(y)];
    y -= 28;

    /* Default web browser. */
    [view addSubview:[self rowLabel:@"Default web browser:" y:y]];
    _browser = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(kLeft, y - 4, 220, 26) pullsDown:NO];
    [_browser setTarget:self];
    [_browser setAction:@selector(chooseBrowser:)];
    [view addSubview:_browser];
    y -= 14;
    [view addSubview:separator(y)];
    y -= 26;

    /* Documents. */
    _askKeep = button([self text:@"Ask to keep changes when closing documents"], NSMakeRect(kLeft, y, 420, 22),
                      NSSwitchButton, self, @selector(documentsChanged:));
    _closeWindows = button([self text:@"Close windows when quitting an app"], NSMakeRect(kLeft, y - 24, 420, 22),
                           NSSwitchButton, self, @selector(documentsChanged:));
    [view addSubview:_askKeep];
    [view addSubview:_closeWindows];
    [view addSubview:note([self text:@"When selected, open documents and windows will not be restored when you re-open an app."],
                          NSMakeRect(kLeft + 18, y - 54, kWidth - kLeft - 38, 28))];
    y -= 76;

    /* Recent items. */
    [view addSubview:[self rowLabel:@"Recent items:" y:y]];
    _recent = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(kLeft, y - 4, 80, 26) pullsDown:NO];
    for (size_t i = 0; i < sizeof(kRecentCounts) / sizeof(kRecentCounts[0]); i++)
        [_recent addItemWithTitle:kRecentCounts[i] == 0 ? [self text:@"None"]
                                                        : [NSString stringWithFormat:@"%d", kRecentCounts[i]]];
    [_recent setTarget:self];
    [_recent setAction:@selector(chooseRecent:)];
    [view addSubview:_recent];
    [view addSubview:label([self text:@"Documents, Apps, and Servers"], NSMakeRect(kLeft + 88, y, 260, 20),
                           NSLeftTextAlignment)];
    y -= 28;
    _smoothing = button([self text:@"Use font smoothing when available"], NSMakeRect(kLeft, y, 400, 22),
                        NSSwitchButton, self, @selector(smoothingChanged:));
    [view addSubview:_smoothing];

    [self reload];
    [self setMainView:view];
    return view;
}

- (void)willSelect
{
    [self reload];
}

- (NSInteger)accentIndex
{
    id value = globalValue(@"AppleAccentColor");
    int accent = [value respondsToSelector:@selector(intValue)] ? [value intValue] : 4;

    for (NSUInteger i = 0; i < kAccentCount; i++)
        if (kAccents[i].value == accent)
            return i;
    return 0;
}

- (NSInteger)highlightIndex
{
    NSString *value = globalValue(@"AppleHighlightColor");
    NSString *name = [value isKindOfClass:[NSString class]] ? [[value componentsSeparatedByString:@" "] lastObject] : nil;

    for (NSUInteger i = 0; i < kAccentCount; i++)
        if ([kAccents[i].name isEqual:name])
            return i;
    return 0;
}

/* The controls as the global domain is now. */
- (void)reload
{
    CFPreferencesSynchronize(kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    NSInteger appearance = [globalValue(@"AppleInterfaceStyleSwitchesAutomatically") boolValue] ? 2
                         : [globalValue(@"AppleInterfaceStyle") isEqual:@"Dark"] ? 1 : 0;
    for (GeneralAppearanceSample *s in _appearances) {
        s.chosen = s.kind == appearance;
        [s setNeedsDisplay:YES];
    }
    NSInteger accent = self.accentIndex;
    /* Blue is the appearance theme's own accent when it has one (Atlas's
       #31679e, the maintainer's choice), the color blue gives. */
    NSColor *themeBlue = _NSAppearanceThemeColor(@"MinkarAccentColor");
    for (GeneralSwatch *s in _swatches) {
        s.chosen = [s tag] == accent;
        if (kAccents[[s tag]].value == 4)
            s.color = themeBlue ?: rgb(kAccents[[s tag]].accent);
        [s setNeedsDisplay:YES];
    }
    [_accentName setStringValue:[self text:kAccents[accent].name]];
    [_highlight selectItemAtIndex:self.highlightIndex];

    id size = globalValue(@"NSTableViewDefaultSizeMode");
    NSInteger mode = [size respondsToSelector:@selector(integerValue)] ? [size integerValue] : 2;
    [_sidebar selectItemAtIndex:MAX(0, MIN(2, mode - 1))];
    [_hideMenuBar setState:[globalValue(@"_HIHideMenuBar") boolValue] ? NSOnState : NSOffState];

    NSString *bars = globalValue(@"AppleShowScrollBars");
    [_scrollWhen setState:[bars isEqual:@"WhenScrolling"] ? NSOnState : NSOffState];
    [_scrollAlways setState:[bars isEqual:@"Always"] ? NSOnState : NSOffState];
    [_scrollAuto setState:[_scrollWhen state] == NSOffState && [_scrollAlways state] == NSOffState ? NSOnState : NSOffState];
    BOOL spot = [globalValue(@"AppleScrollerPagingBehavior") boolValue];
    [_pageSpot setState:spot ? NSOnState : NSOffState];
    [_pageNext setState:spot ? NSOffState : NSOnState];

    [self reloadBrowsers];
    [self reloadThemes];

    [_askKeep setState:[globalValue(@"NSCloseAlwaysConfirmsChanges") boolValue] ? NSOnState : NSOffState];
    id keeps = globalValue(@"NSQuitAlwaysKeepsWindows");
    [_closeWindows setState:keeps != nil && [keeps boolValue] ? NSOffState : NSOnState];

    id recent = globalValue(@"NSRecentDocumentMaximum");
    int count = recent != nil ? [recent intValue] : 10;
    NSInteger index = 2;
    for (size_t i = 0; i < sizeof(kRecentCounts) / sizeof(kRecentCounts[0]); i++)
        if (kRecentCounts[i] == count)
            index = i;
    [_recent selectItemAtIndex:index];

    id smoothing = globalValue(@"AppleFontSmoothing");
    [_smoothing setState:smoothing != nil && [smoothing intValue] == 0 ? NSOffState : NSOnState];
}

/* The apps that open http, by name, the default one chosen. */
- (void)reloadBrowsers
{
    NSArray *ids = CFBridgingRelease(LSCopyAllHandlersForURLScheme(CFSTR("http")));
    NSString *chosen = CFBridgingRelease(LSCopyDefaultHandlerForURLScheme(CFSTR("http")));
    NSMutableArray *kept = [NSMutableArray array];

    [_browser removeAllItems];
    for (NSString *identifier in ids) {
        if ([kept containsObject:identifier])
            continue;
        NSURL *url = [[NSWorkspace sharedWorkspace] URLForApplicationWithBundleIdentifier:identifier];
        NSString *name = url != nil ? [[NSFileManager defaultManager] displayNameAtPath:[url path]] : identifier;
        if ([[name pathExtension] isEqual:@"app"])
            name = [name stringByDeletingPathExtension];
        [_browser addItemWithTitle:name];
        [kept addObject:identifier];
    }
    _browsers = kept;
    if (kept.count == 0)
        [_browser addItemWithTitle:[self text:@"None"]];
    [_browser setEnabled:kept.count > 0];
    for (NSUInteger i = 0; i < kept.count; i++)
        if ([kept[i] caseInsensitiveCompare:chosen ?: @""] == NSOrderedSame)
            [_browser selectItemAtIndex:i];
}

- (void)chooseAppearance:(GeneralAppearanceSample *)sender
{
    setGlobal(@"AppleInterfaceStyle", sender.kind == 1 ? @"Dark" : nil);
    setGlobal(@"AppleInterfaceStyleSwitchesAutomatically", sender.kind == 2 ? @YES : nil);
    syncGlobal();
    postDistributed(@"AppleInterfaceThemeChangedNotification");
    [self reload];
    os_log_info(OS_LOG_DEFAULT, "general appearance=%ld", (long)sender.kind);
}

/* An accent brings its highlight color with it, as on the reference. */
- (void)chooseAccent:(GeneralSwatch *)sender
{
    const Accent *a = &kAccents[[sender tag]];
    BOOL blue = a->value == 4;

    setGlobal(@"AppleAccentColor", blue ? nil : @(a->value));
    setGlobal(@"AppleAquaColorVariant", blue ? nil : @(a->value == -1 ? 6 : 1));
    setGlobal(@"AppleHighlightColor", blue ? nil : highlightString(a));
    syncGlobal();
    postDistributed(@"AppleColorPreferencesChangedNotification");
    [self reload];
    os_log_info(OS_LOG_DEFAULT, "general accent=%d", a->value);
}

- (void)chooseHighlight:(id)sender
{
    const Accent *a = &kAccents[[_highlight indexOfSelectedItem]];

    setGlobal(@"AppleHighlightColor", a->value == 4 ? nil : highlightString(a));
    syncGlobal();
    postDistributed(@"AppleColorPreferencesChangedNotification");
}

/* A theme popup: Atlas's own first, then the themes found; the chosen one
   selected (one no longer there: Atlas's). */
- (void)fillThemes:(NSPopUpButton *)popup names:(NSArray *)names key:(NSString *)key
{
    NSString *chosen = globalValue(key);

    [popup removeAllItems];
    [popup addItemWithTitle:[self text:@"Atlas (standard)"]];
    [[popup lastItem] setRepresentedObject:@""];
    for (NSString *name in names) {
        [popup addItemWithTitle:name];
        [[popup lastItem] setRepresentedObject:name];
    }
    NSInteger index = [chosen isKindOfClass:[NSString class]] ? [popup indexOfItemWithRepresentedObject:chosen] : -1;
    [popup selectItemAtIndex:index > 0 ? index : 0];
}

- (void)reloadThemes
{
    extern NSArray *_NSIconThemeNames(void);
    extern NSArray *_NSCursorThemeNames(void);
    extern NSArray *_NSAppearanceThemeNames(void);

    [self fillThemes:_appearanceTheme names:_NSAppearanceThemeNames() key:@"MinkarAppearanceTheme"];
    [self fillThemes:_iconTheme names:_NSIconThemeNames() key:@"MinkarIconTheme"];
    [self fillThemes:_cursorTheme names:_NSCursorThemeNames() key:@"MinkarCursorTheme"];
}

- (void)chooseTheme:(NSPopUpButton *)sender
{
    NSString *key = sender == _appearanceTheme ? @"MinkarAppearanceTheme"
                  : sender == _iconTheme ? @"MinkarIconTheme" : @"MinkarCursorTheme";
    NSString *name = [[sender selectedItem] representedObject];

    setGlobal(key, [name length] ? name : nil);
    syncGlobal();
    postDistributed(@"org.minkar.ThemesChanged");
}

- (void)chooseSidebar:(id)sender
{
    setGlobal(@"NSTableViewDefaultSizeMode", @([_sidebar indexOfSelectedItem] + 1));
    syncGlobal();
}

- (void)hideMenuBarChanged:(id)sender
{
    setGlobal(@"_HIHideMenuBar", @([_hideMenuBar state] == NSOnState));
    syncGlobal();
}

- (void)chooseScrollBars:(NSButton *)sender
{
    for (NSButton *b in @[ _scrollAuto, _scrollWhen, _scrollAlways ])
        [b setState:b == sender ? NSOnState : NSOffState];
    setGlobal(@"AppleShowScrollBars", sender == _scrollWhen ? @"WhenScrolling" : sender == _scrollAlways ? @"Always"
                                                                                                      : @"Automatic");
    syncGlobal();
    postDistributed(@"AppleShowScrollBarsSettingChanged");
}

- (void)choosePaging:(NSButton *)sender
{
    [_pageNext setState:sender == _pageNext ? NSOnState : NSOffState];
    [_pageSpot setState:sender == _pageSpot ? NSOnState : NSOffState];
    setGlobal(@"AppleScrollerPagingBehavior", @(sender == _pageSpot));
    syncGlobal();
}

- (void)chooseBrowser:(id)sender
{
    NSInteger i = [_browser indexOfSelectedItem];

    if (i < 0 || (NSUInteger)i >= _browsers.count)
        return;
    LSSetDefaultHandlerForURLScheme(CFSTR("http"), (__bridge CFStringRef)_browsers[i]);
    LSSetDefaultHandlerForURLScheme(CFSTR("https"), (__bridge CFStringRef)_browsers[i]);
    os_log_info(OS_LOG_DEFAULT, "general browser=%{public}s", [_browsers[i] UTF8String]);
}

- (void)documentsChanged:(id)sender
{
    setGlobal(@"NSCloseAlwaysConfirmsChanges", @([_askKeep state] == NSOnState));
    setGlobal(@"NSQuitAlwaysKeepsWindows", @([_closeWindows state] != NSOnState));
    syncGlobal();
}

- (void)chooseRecent:(id)sender
{
    setGlobal(@"NSRecentDocumentMaximum", @(kRecentCounts[[_recent indexOfSelectedItem]]));
    syncGlobal();
}

- (void)smoothingChanged:(id)sender
{
    setGlobal(@"AppleFontSmoothing", [_smoothing state] == NSOnState ? nil : @0);
    syncGlobal();
}

@end
