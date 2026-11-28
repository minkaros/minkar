/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * System Preferences' window (ROADMAP Phase 5, item 6): a bar as Files'
 * (the window's buttons in it, then Files' back and forward pair, Show
 * All as nine dots, the title once, a search field at the right, a line
 * under it) over either the panes found on
 * disk, in titled sections (their icon at the left of their
 * NSPrefPaneIconLabel; the search field's text keeps those whose name has
 * it) or one pane's view, loaded
 * from its bundle through NSPreferencePane. The window takes the pane's
 * size; the panes' order is the reference platform's for those it knows.
 *
 * Each of Minkar's panes shows its icon from the system's icon package
 * (MinkarPrefPaneSystemIcon); a pane with no icon gets a drawn one, a
 * rounded square in its MinkarPrefPaneIconColor with its label's initial.
 */

#import "PrefsWindowController.h"
#import <PreferencePanes/PreferencePanes.h>
#import "FilesSegmentedControl.h"
#import "FilesColors.h"
#include <os/log.h>

/* The window's buttons in the bar, as Files has them. */
@interface NSWindow (MinkarTitlebar)
- (void)_setTitlebarHeight:(CGFloat)height;
@end

static const CGFloat kBarHeight = 52, kCellHeight = 52, kIcon = 32;
static const CGFloat kGridWidth = 668;

/* ---- Panes on disk ---- */

@implementation PrefsPaneInfo

- (instancetype)initWithBundle:(NSBundle *)bundle
{
    if ((self = [super init]) == nil)
        return nil;
    _bundle = bundle;
    _identifier = [[bundle bundleIdentifier] copy] ?: [[[bundle bundlePath] lastPathComponent] copy];
    NSString *label = [bundle objectForInfoDictionaryKey:@"NSPrefPaneIconLabel"];
    if (![label isKindOfClass:[NSString class]])
        label = [bundle objectForInfoDictionaryKey:@"CFBundleName"];
    /* The label in the user's language: the pane's InfoPlist.strings, as
       on the reference platform. */
    _label = [[bundle localizedStringForKey:@"NSPrefPaneIconLabel" value:label table:@"InfoPlist"] copy];
    _icon = [self loadIcon];
    return self;
}

- (NSImage *)loadIcon
{
    /* Minkar's own panes name their icon in the system's icon package
       (/System/Library/Icons/<group>/<name>.png); others bring theirs. */
    NSString *system = [_bundle objectForInfoDictionaryKey:@"MinkarPrefPaneSystemIcon"];
    if ([system isKindOfClass:[NSString class]] && [system length] && [system rangeOfString:@".."].location == NSNotFound) {
        extern NSString *_NSSystemIconPath(NSString *relative);
        NSString *path = _NSSystemIconPath([system stringByAppendingPathExtension:@"png"]);
        NSImage *image = [[NSImage alloc] initWithContentsOfFile:path];
        if (image != nil)
            return image;
    }
    NSString *file = [_bundle objectForInfoDictionaryKey:@"NSPrefPaneIconFile"];
    if ([file length]) {
        NSString *path = [_bundle pathForResource:[file stringByDeletingPathExtension] ofType:[file pathExtension]];
        NSImage *image = path ? [[NSImage alloc] initWithContentsOfFile:path] : nil;
        if (image != nil)
            return image;
    }
    NSArray *rgb = [[_bundle objectForInfoDictionaryKey:@"MinkarPrefPaneIconColor"]
        componentsSeparatedByString:@" "];
    NSColor *color = [rgb count] == 3 ?
        [NSColor colorWithSRGBRed:[rgb[0] doubleValue] green:[rgb[1] doubleValue] blue:[rgb[2] doubleValue] alpha:1] :
        [NSColor colorWithGenericGamma22White:0.5 alpha:1];
    NSString *initial = [_label length] ? [[_label substringToIndex:1] uppercaseString] : @"?";
    NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(128, 128)];

    [image lockFocus];
    [color setFill];
    [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(8, 8, 112, 112) xRadius:26 yRadius:26] fill];
    NSDictionary *attributes = @{ NSFontAttributeName: [NSFont boldSystemFontOfSize:64],
                                  NSForegroundColorAttributeName: [NSColor whiteColor] };
    NSSize size = [initial sizeWithAttributes:attributes];
    [initial drawAtPoint:NSMakePoint(64 - size.width / 2, 64 - size.height / 2) withAttributes:attributes];
    [image unlockFocus];
    return image;
}

@end

/* The reference platform's order for the panes it has. */
static NSArray<NSString *> *knownOrder(void)
{
    return @[ @"org.minkar.preference.general", @"org.minkar.preference.dock",
              @"org.minkar.preference.notifications", @"org.minkar.preference.applications",
              @"org.minkar.preference.users",
              @"org.minkar.preference.displays", @"org.minkar.preference.keyboard",
              @"org.minkar.preference.mouse", @"org.minkar.preference.sound",
              @"org.minkar.preference.printers",
              @"org.minkar.preference.network", @"org.minkar.preference.datetime" ];
}

static NSArray<PrefsPaneInfo *> *findPanes(void)
{
    NSMutableArray<PrefsPaneInfo *> *panes = [NSMutableArray array];
    NSMutableSet *seen = [NSMutableSet set];

    for (NSString *dir in @[ @"/System/Library/PreferencePanes", @"/Library/PreferencePanes",
                             [@"~/Library/PreferencePanes" stringByExpandingTildeInPath] ]) {
        for (NSString *name in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:dir error:NULL]) {
            if (![[name pathExtension] isEqualToString:@"prefPane"])
                continue;
            NSBundle *bundle = [NSBundle bundleWithPath:[dir stringByAppendingPathComponent:name]];
            if (bundle == nil)
                continue;
            PrefsPaneInfo *info = [[PrefsPaneInfo alloc] initWithBundle:bundle];
            if ([seen containsObject:info.identifier])
                continue;
            [seen addObject:info.identifier];
            [panes addObject:info];
        }
    }
    NSArray *order = knownOrder();
    [panes sortUsingComparator:^NSComparisonResult(PrefsPaneInfo *a, PrefsPaneInfo *b) {
        NSUInteger ia = [order indexOfObject:a.identifier], ib = [order indexOfObject:b.identifier];
        if (ia != ib)
            return ia < ib ? NSOrderedAscending : NSOrderedDescending;
        return [a.label localizedStandardCompare:b.label];
    }];
    return panes;
}

/* ---- The grid ---- */

/* The sections, as the maintainer chose (23 Nov 2026): titled groups, four
   columns, each pane's icon at the left of its name. A pane Minkar does
   not know goes in Other. */
static NSArray<NSString *> *sectionTitles(void)
{
    return @[ @"Personal", @"Hardware", @"Network", @"System", @"Other" ];
}

static NSUInteger sectionOf(PrefsPaneInfo *pane)
{
    static NSDictionary *sections;
    if (sections == nil)
        sections = @{ @"org.minkar.preference.general": @0, @"org.minkar.preference.dock": @0,
                      @"org.minkar.preference.notifications": @0,
                      @"org.minkar.preference.applications": @0,
                      @"org.minkar.preference.displays": @1, @"org.minkar.preference.keyboard": @1,
                      @"org.minkar.preference.mouse": @1, @"org.minkar.preference.sound": @1,
                      @"org.minkar.preference.printers": @1,
                      @"org.minkar.preference.network": @2,
                      @"org.minkar.preference.users": @3, @"org.minkar.preference.datetime": @3 };
    NSNumber *n = sections[pane.identifier];
    return n != nil ? [n unsignedIntegerValue] : 4;
}

static const NSUInteger kColumns = 4;
static const CGFloat kSectionHeight = 34, kSectionGap = 6, kGridTop = 6, kGridBottom = 14, kGridSide = 18;

@class PrefsGridView;

@protocol PrefsGridViewDelegate <NSObject>
- (void)gridView:(PrefsGridView *)view choosePane:(PrefsPaneInfo *)pane;
@end

@interface PrefsGridView : NSView
@property (nonatomic, copy) NSArray<PrefsPaneInfo *> *panes;
@property (nonatomic, copy) NSString *filter;       /* the search field's text */
@property (nonatomic, weak) id<PrefsGridViewDelegate> delegate;
+ (CGFloat)heightForPanes:(NSArray<PrefsPaneInfo *> *)panes;
- (NSArray<PrefsPaneInfo *> *)shownPanes;
@end

@implementation PrefsGridView {
    NSInteger _pressed;
}

/* The panes of each section, in order; empty sections left out. */
static NSArray<NSArray<PrefsPaneInfo *> *> *bySection(NSArray<PrefsPaneInfo *> *panes)
{
    NSMutableArray *groups = [NSMutableArray array];
    for (NSUInteger i = 0; i < [sectionTitles() count]; i++)
        [groups addObject:[NSMutableArray array]];
    for (PrefsPaneInfo *pane in panes)
        [groups[sectionOf(pane)] addObject:pane];
    return groups;
}

+ (CGFloat)heightForPanes:(NSArray<PrefsPaneInfo *> *)panes
{
    CGFloat height = kGridTop + kGridBottom;
    for (NSArray *group in bySection(panes))
        if ([group count] > 0)
            height += kSectionHeight + ceil((double)[group count] / kColumns) * kCellHeight + kSectionGap;
    return height;
}

- (BOOL)isFlipped
{
    return YES;
}

- (NSArray<PrefsPaneInfo *> *)shownPanes
{
    if ([self.filter length] == 0)
        return self.panes;
    NSMutableArray *shown = [NSMutableArray array];
    for (PrefsPaneInfo *pane in self.panes)
        if ([pane.label rangeOfString:self.filter options:NSCaseInsensitiveSearch | NSDiacriticInsensitiveSearch].location != NSNotFound)
            [shown addObject:pane];
    return shown;
}

/* Each shown pane's cell, and each section's title, top down. */
- (void)layout:(void (^)(NSString *title, NSRect rect))section cell:(void (^)(PrefsPaneInfo *pane, NSRect rect))cell
{
    CGFloat width = (NSWidth([self bounds]) - 2 * kGridSide) / kColumns, y = kGridTop;
    NSArray *titles = sectionTitles();
    NSArray *groups = bySection([self shownPanes]);

    for (NSUInteger s = 0; s < [groups count]; s++) {
        NSArray *group = groups[s];
        if ([group count] == 0)
            continue;
        if (section != nil)
            section(NSLocalizedString(titles[s], nil), NSMakeRect(kGridSide, y, NSWidth([self bounds]) - 2 * kGridSide, kSectionHeight));
        y += kSectionHeight;
        for (NSUInteger i = 0; i < [group count]; i++)
            if (cell != nil)
                cell(group[i], NSMakeRect(kGridSide + (i % kColumns) * width, y + (i / kColumns) * kCellHeight,
                                          width, kCellHeight));
        y += ceil((double)[group count] / kColumns) * kCellHeight + kSectionGap;
    }
}

- (void)drawRect:(NSRect)dirty
{
    NSDictionary *heading = @{ NSFontAttributeName: [NSFont boldSystemFontOfSize:13],
                               NSForegroundColorAttributeName: [NSColor labelColor] };
    NSDictionary *name = @{ NSFontAttributeName: [NSFont systemFontOfSize:13],
                            NSForegroundColorAttributeName: [NSColor labelColor] };
    NSInteger pressed = _pressed;
    NSArray *panes = self.panes;

    /* In the appearance's colors (General): the window's background. */
    [[NSColor windowBackgroundColor] set];
    NSRectFill(dirty);
    [self layout:^(NSString *title, NSRect rect) {
        [title drawAtPoint:NSMakePoint(NSMinX(rect), NSMaxY(rect) - 24) withAttributes:heading];
    } cell:^(PrefsPaneInfo *pane, NSRect rect) {
        NSRect icon = NSMakeRect(NSMinX(rect) + 8, NSMidY(rect) - kIcon / 2, kIcon, kIcon);
        BOOL down = pressed >= 0 && (NSUInteger)pressed < [panes count] && panes[(NSUInteger)pressed] == pane;
        [pane.icon drawInRect:icon fromRect:NSZeroRect operation:NSCompositeSourceOver
                     fraction:down ? 0.6 : 1 respectFlipped:YES hints:nil];
        /* Two lines when one is not enough ("Usuarios y grupos" in
           Spanish showed "Usuarios y" only), centred by the icon. */
        CGFloat width = NSMaxX(rect) - NSMaxX(icon) - 14;
        NSSize size = [pane.label sizeWithAttributes:name];
        CGFloat height = size.height;
        if (size.width > width) {
            NSRect needed = [pane.label boundingRectWithSize:NSMakeSize(width, 2 * size.height + 4)
                                                     options:NSStringDrawingUsesLineFragmentOrigin
                                                  attributes:name];
            height = MIN(ceil(NSHeight(needed)), 2 * size.height);
        }
        [pane.label drawInRect:NSMakeRect(NSMaxX(icon) + 10, NSMidY(rect) - height / 2, width, height + 2)
                withAttributes:name];
    }];
}

- (NSInteger)indexAtPoint:(NSPoint)p
{
    __block NSInteger found = -1;
    NSArray *panes = self.panes;
    [self layout:nil cell:^(PrefsPaneInfo *pane, NSRect rect) {
        if (NSPointInRect(p, rect))
            found = (NSInteger)[panes indexOfObjectIdenticalTo:pane];
    }];
    return found;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return YES;
}

- (void)mouseDown:(NSEvent *)event
{
    _pressed = [self indexAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]];
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)event
{
    NSInteger index = [self indexAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]];

    if (index >= 0 && index == _pressed)
        [self.delegate gridView:self choosePane:self.panes[(NSUInteger)index]];
    _pressed = -1;
    [self setNeedsDisplay:YES];
}

@end

/* ---- The bar ---- */

/* Files' bar: its grey, and a line under it between it and the pane. */
@interface PrefsBarView : NSView
@end

@implementation PrefsBarView

- (void)drawRect:(NSRect)dirty
{
    /* With an appearance theme, the theme's title bar and lines. */
    BOOL theme = _NSAppearanceThemeActive();
    [(theme ? [NSColor windowFrameColor] : FilesDark() ? [NSColor colorWithGenericGamma22White:0.17 alpha:1]
                                                        : FilesGray(0.82, 1)) set];
    NSRectFill(dirty);
    [(theme ? [NSColor separatorColor] : FilesDark() ? [NSColor colorWithGenericGamma22White:0.09 alpha:1]
                                                      : FilesGray(0.74, 1)) set];
    NSRectFill(NSMakeRect(0, 0, NSWidth([self bounds]), 1));
}

@end

/* ---- The window ---- */

@interface PrefsWindowController () <PrefsGridViewDelegate, NSWindowDelegate>
@end

@implementation PrefsWindowController {
    NSWindow *_window;
    NSView *_content;          /* below the bar */
    PrefsBarView *_bar;
    FilesSegmentedControl *_navigation, *_all;
    NSTextField *_title;
    NSSearchField *_search;
    PrefsGridView *_grid;
    NSArray<PrefsPaneInfo *> *_panes;
    NSMutableDictionary<NSString *, NSPreferencePane *> *_loaded;
    NSPreferencePane *_current;
    NSMutableArray<NSString *> *_history, *_future;   /* pane identifiers; @"" the grid */
}

- (instancetype)init
{
    if ((self = [super init]) == nil)
        return nil;
    _panes = findPanes();
    _loaded = [NSMutableDictionary dictionary];
    _history = [NSMutableArray array];
    _future = [NSMutableArray array];

    CGFloat gridHeight = [PrefsGridView heightForPanes:_panes];
    _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(180, 160, kGridWidth, gridHeight + kBarHeight)
        styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask |
                  NSWindowStyleMaskFullSizeContentView
          backing:NSBackingStoreBuffered defer:NO];
    /* The title once, in the bar; the window's buttons in it too. */
    [_window setTitleVisibility:NSWindowTitleHidden];
    [_window setTitlebarAppearsTransparent:YES];
    if ([_window respondsToSelector:@selector(_setTitlebarHeight:)])
        [_window _setTitlebarHeight:kBarHeight];
    [_window setReleasedWhenClosed:NO];
    [_window setDelegate:self];
    /* Centered the first time; then where it was left, as on the
       reference: its top left corner (its height is the pane's). */
    NSRect initial = [_window frame];
    if ([_window setFrameUsingName:@"System Preferences"]) {
        NSPoint topLeft = NSMakePoint(NSMinX([_window frame]), NSMaxY([_window frame]));
        [_window setFrame:initial display:NO];
        [_window setFrameTopLeftPoint:topLeft];
    } else
        [_window center];
    NSView *root = [[NSView alloc] initWithFrame:[[_window contentView] frame]];
    [_window setContentView:root];

    _bar = [[PrefsBarView alloc] initWithFrame:NSZeroRect];
    _navigation = [[FilesSegmentedControl alloc] initWithGlyphs:@[ @(FilesGlyphBack), @(FilesGlyphForward) ]
        actions:@[ @"goBack:", @"goForward:" ] target:self];
    [_navigation setLabel:NSLocalizedString(@"Back", nil) forSegment:0];
    [_navigation setLabel:NSLocalizedString(@"Forward", nil) forSegment:1];
    _all = [[FilesSegmentedControl alloc] initWithGlyphs:@[ @(FilesGlyphShowAll) ]
        actions:@[ @"showAll:" ] target:self];
    [_all setLabel:NSLocalizedString(@"Show All", nil) forSegment:0];
    _title = [[NSTextField alloc] initWithFrame:NSZeroRect];
    /* A label: the window moves by it, as by the rest of the bar. */
    [_title setEditable:NO];
    [_title setSelectable:NO];
    [_title setBordered:NO];
    [_title setDrawsBackground:NO];
    [_title setFont:[NSFont boldSystemFontOfSize:13]];
    _search = [[NSSearchField alloc] initWithFrame:NSZeroRect];
    [[_search cell] setPlaceholderString:NSLocalizedString(@"Search", nil)];
    [_search setDelegate:(id)self];
    [_search setTarget:self];
    [_search setAction:@selector(searched:)];
    for (NSView *v in @[ _navigation, _all, _title, _search ])
        [_bar addSubview:v];
    [root addSubview:_bar];

    _grid = [[PrefsGridView alloc] initWithFrame:NSZeroRect];
    _grid.panes = _panes;
    _grid.delegate = self;
    [self showView:_grid title:NSLocalizedString(@"System Preferences", nil) size:NSMakeSize(kGridWidth, gridHeight)];
    [self updateButtons];
    return self;
}

- (NSWindow *)window
{
    return _window;
}

/* The window resized to the view (its top left kept), the bar above it. */
- (void)showView:(NSView *)view title:(NSString *)title size:(NSSize)size
{
    NSRect frame = [_window frame];
    NSRect content = [_window contentRectForFrameRect:frame];
    NSRect visible = [[_window screen] ?: [NSScreen mainScreen] visibleFrame];
    CGFloat top = NSMaxY(content);
    NSRect newContent = NSMakeRect(NSMinX(content), top - size.height - kBarHeight, size.width, size.height + kBarHeight);
    /* Never past the bottom of the screen's usable area (above the Dock). */
    NSRect newFrame = [_window frameRectForContentRect:newContent];
    if (NSMinY(newFrame) < NSMinY(visible) + 80)
        newContent.origin.y += NSMinY(visible) + 80 - NSMinY(newFrame);
    /* Nor past its top: the title bar always shows (on a small screen the
       window may cover the Dock's room instead). */
    newFrame = [_window frameRectForContentRect:newContent];
    if (NSMaxY(newFrame) > NSMaxY(visible))
        newContent.origin.y -= NSMaxY(newFrame) - NSMaxY(visible);

    [_content removeFromSuperview];
    _content = view;
    [_window setFrame:[_window frameRectForContentRect:newContent] display:NO];
    NSView *root = [_window contentView];
    /* After the window's buttons (as in Files): back and forward, Show
       All, the title; the search field at the right. */
    [_bar setFrame:NSMakeRect(0, size.height, size.width, kBarHeight)];
    NSSize nav = [_navigation naturalSize], all = [_all naturalSize];
    CGFloat mid = floor(kBarHeight / 2);
    [_navigation setFrame:NSMakeRect(92, mid - nav.height / 2, nav.width, nav.height)];
    [_all setFrame:NSMakeRect(92 + nav.width + 10, mid - all.height / 2, all.width, all.height)];
    /* The title in the middle of the window, clear of the controls on
       either side (as Files'). */
    CGFloat side = MAX(NSMaxX([_all frame]) + 14, 200);
    [_title setFrame:NSMakeRect(side, mid - 10, MAX(size.width - 2 * side, 0), 20)];
    [_title setAlignment:NSCenterTextAlignment];
    [_search setFrame:NSMakeRect(size.width - 190, mid - 12, 178, 24)];
    [_title setStringValue:title];
    [view setFrame:NSMakeRect(0, 0, size.width, size.height)];
    [root addSubview:view];
    [_window setTitle:title];
    [root setNeedsDisplay:YES];
}

/* Typed in the search field: the grid keeps the panes whose name has
   the text (the grid shown first); Return opens the only one left. */
- (void)controlTextDidChange:(NSNotification *)note
{
    if ([note object] != _search)
        return;
    if ([[self currentIdentifier] length] > 0)
        [self show:@"" remember:YES];
    _grid.filter = [_search stringValue];
    [_grid setNeedsDisplay:YES];
}

- (void)searched:(id)sender
{
    _grid.filter = [_search stringValue];
    [_grid setNeedsDisplay:YES];
    NSArray *shown = [_grid shownPanes];
    if ([[_search stringValue] length] > 0 && [shown count] == 1)
        [self gridView:_grid choosePane:shown[0]];
}

- (void)updateButtons
{
    [_navigation setEnabled:[_history count] > 0 forSegment:0];
    [_navigation setEnabled:[_future count] > 0 forSegment:1];
}

- (NSString *)currentIdentifier
{
    for (NSString *key in _loaded)
        if (_loaded[key] == _current)
            return key;
    return @"";
}

/* Leaves the pane shown (it may refuse, as NSPreferencePane allows). */
- (BOOL)leaveCurrent
{
    if (_current == nil)
        return YES;
    if ([_current shouldUnselect] == NSUnselectCancel)
        return NO;
    [_current willUnselect];
    [[_current mainView] removeFromSuperview];
    [_current didUnselect];
    _current = nil;
    return YES;
}

- (BOOL)show:(NSString *)identifier remember:(BOOL)remember
{
    NSString *from = [self currentIdentifier];

    if ([identifier isEqualToString:from])
        return YES;
    if ([identifier length] == 0) {
        if (![self leaveCurrent])
            return NO;
        [self showView:_grid title:NSLocalizedString(@"System Preferences", nil)
                  size:NSMakeSize(kGridWidth, [PrefsGridView heightForPanes:_panes])];
    } else {
        PrefsPaneInfo *info = nil;
        for (PrefsPaneInfo *p in _panes)
            if ([p.identifier isEqualToString:identifier])
                info = p;
        if (info == nil)
            return NO;
        NSPreferencePane *pane = _loaded[identifier];
        if (pane == nil) {
            Class cls = [info.bundle principalClass];
            if (cls == Nil || ![cls isSubclassOfClass:[NSPreferencePane class]])
                return NO;
            pane = [[cls alloc] initWithBundle:info.bundle];
            if ([pane loadMainView] == nil)
                return NO;
            _loaded[identifier] = pane;
        }
        if (![self leaveCurrent])
            return NO;
        [pane willSelect];
        NSView *view = [pane mainView];
        [self showView:view title:info.label size:[view frame].size];
        _current = pane;
        [pane didSelect];
        os_log_info(OS_LOG_DEFAULT, "pane %{public}s", [identifier UTF8String]);
    }
    if (remember) {
        [_history addObject:from];
        [_future removeAllObjects];
    }
    [self updateButtons];
    return YES;
}

- (BOOL)showPaneWithIdentifier:(NSString *)identifier
{
    return [self show:identifier remember:YES];
}

- (IBAction)showAll:(id)sender
{
    [self show:@"" remember:YES];
}

- (IBAction)goBack:(id)sender
{
    if ([_history count] == 0)
        return;
    NSString *to = [_history lastObject];
    NSString *from = [self currentIdentifier];
    if ([self show:to remember:NO]) {
        [_history removeLastObject];
        [_future addObject:from];
        [self updateButtons];
    }
}

- (IBAction)goForward:(id)sender
{
    if ([_future count] == 0)
        return;
    NSString *to = [_future lastObject];
    NSString *from = [self currentIdentifier];
    if ([self show:to remember:NO]) {
        [_future removeLastObject];
        [_history addObject:from];
        [self updateButtons];
    }
}

- (void)gridView:(PrefsGridView *)view choosePane:(PrefsPaneInfo *)pane
{
    [self showPaneWithIdentifier:pane.identifier];
}

- (void)windowDidMove:(NSNotification *)note
{
    [_window saveFrameUsingName:@"System Preferences"];
}

- (void)windowWillClose:(NSNotification *)note
{
    [_window saveFrameUsingName:@"System Preferences"];
}

@end
