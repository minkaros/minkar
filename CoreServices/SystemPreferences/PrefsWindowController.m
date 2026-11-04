/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * System Preferences' window (ROADMAP Phase 5, item 6): a bar with Back,
 * Forward and Show All over either the grid of the panes found on disk
 * (their NSPrefPaneIconLabel under their icon) or one pane's view, loaded
 * from its bundle through NSPreferencePane. The window takes the pane's
 * size; the panes' order is the reference platform's for those it knows.
 *
 * Atlas's provisional look: a pane with no icon file gets a drawn one, a
 * rounded square in its MinkarPrefPaneIconColor with its label's initial.
 */

#import "PrefsWindowController.h"
#import <PreferencePanes/PreferencePanes.h>
#include <os/log.h>

static const CGFloat kBarHeight = 38, kCellWidth = 96, kCellHeight = 92, kIcon = 48;
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
        [NSColor colorWithCalibratedRed:[rgb[0] doubleValue] green:[rgb[1] doubleValue] blue:[rgb[2] doubleValue] alpha:1] :
        [NSColor colorWithCalibratedWhite:0.5 alpha:1];
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
    return @[ @"org.minkar.preference.dock", @"org.minkar.preference.users",
              @"org.minkar.preference.displays", @"org.minkar.preference.keyboard",
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

@class PrefsGridView;

@protocol PrefsGridViewDelegate <NSObject>
- (void)gridView:(PrefsGridView *)view choosePane:(PrefsPaneInfo *)pane;
@end

@interface PrefsGridView : NSView
@property (nonatomic, copy) NSArray<PrefsPaneInfo *> *panes;
@property (nonatomic, weak) id<PrefsGridViewDelegate> delegate;
+ (CGFloat)heightForCount:(NSUInteger)count;
@end

@implementation PrefsGridView {
    NSInteger _pressed;
}

+ (NSUInteger)columns
{
    return (NSUInteger)floor((kGridWidth - 16) / kCellWidth);
}

+ (CGFloat)heightForCount:(NSUInteger)count
{
    return 24 + ceil((double)MAX(count, 1) / [self columns]) * kCellHeight;
}

- (BOOL)isFlipped
{
    return YES;
}

- (NSRect)rectForIndex:(NSUInteger)i
{
    NSUInteger columns = [PrefsGridView columns];
    CGFloat margin = floor((NSWidth([self bounds]) - columns * kCellWidth) / 2);
    return NSMakeRect(margin + (i % columns) * kCellWidth, 12 + (i / columns) * kCellHeight, kCellWidth, kCellHeight);
}

- (void)drawRect:(NSRect)dirty
{
    NSMutableParagraphStyle *center = [[NSMutableParagraphStyle alloc] init];
    [center setAlignment:NSCenterTextAlignment];
    NSDictionary *attributes = @{ NSFontAttributeName: [NSFont systemFontOfSize:11],
                                  NSForegroundColorAttributeName: [NSColor blackColor],
                                  NSParagraphStyleAttributeName: center };

    [[NSColor colorWithCalibratedWhite:0.93 alpha:1] set];
    NSRectFill(dirty);
    for (NSUInteger i = 0; i < [self.panes count]; i++) {
        NSRect cell = [self rectForIndex:i];
        PrefsPaneInfo *pane = self.panes[i];
        NSRect icon = NSMakeRect(NSMidX(cell) - kIcon / 2, NSMinY(cell) + 6, kIcon, kIcon);
        [pane.icon drawInRect:icon fromRect:NSZeroRect operation:NSCompositeSourceOver
                     fraction:(NSInteger)i == _pressed ? 0.6 : 1 respectFlipped:YES hints:nil];
        [pane.label drawInRect:NSMakeRect(NSMinX(cell) + 4, NSMaxY(icon) + 4, kCellWidth - 8, 30)
                withAttributes:attributes];
    }
}

- (NSInteger)indexAtPoint:(NSPoint)p
{
    for (NSUInteger i = 0; i < [self.panes count]; i++)
        if (NSPointInRect(p, [self rectForIndex:i]))
            return (NSInteger)i;
    return -1;
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

/* ---- The window ---- */

@interface PrefsWindowController () <PrefsGridViewDelegate, NSWindowDelegate>
@end

@implementation PrefsWindowController {
    NSWindow *_window;
    NSView *_content;          /* below the bar */
    NSView *_bar;
    NSButton *_back, *_forward, *_all;
    NSTextField *_title;
    PrefsGridView *_grid;
    NSArray<PrefsPaneInfo *> *_panes;
    NSMutableDictionary<NSString *, NSPreferencePane *> *_loaded;
    NSPreferencePane *_current;
    NSMutableArray<NSString *> *_history, *_future;   /* pane identifiers; @"" the grid */
}

static NSButton *barButton(NSString *title, id target, SEL action, CGFloat width)
{
    NSButton *button = [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, width, 24)];
    [button setTitle:title];
    [button setBezelStyle:NSRoundedBezelStyle];
    [button setTarget:target];
    [button setAction:action];
    return button;
}

- (instancetype)init
{
    if ((self = [super init]) == nil)
        return nil;
    _panes = findPanes();
    _loaded = [NSMutableDictionary dictionary];
    _history = [NSMutableArray array];
    _future = [NSMutableArray array];

    CGFloat gridHeight = [PrefsGridView heightForCount:[_panes count]];
    _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(180, 160, kGridWidth, gridHeight + kBarHeight)
        styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask
          backing:NSBackingStoreBuffered defer:NO];
    [_window setReleasedWhenClosed:NO];
    [_window setDelegate:self];
    NSView *root = [[NSView alloc] initWithFrame:[[_window contentView] frame]];
    [_window setContentView:root];

    _bar = [[NSView alloc] initWithFrame:NSZeroRect];
    _back = barButton(@"‹", self, @selector(goBack:), 30);
    _forward = barButton(@"›", self, @selector(goForward:), 30);
    _all = barButton(NSLocalizedString(@"Show All", nil), self, @selector(showAll:), 84);
    _title = [[NSTextField alloc] initWithFrame:NSZeroRect];
    [_title setEditable:NO];
    [_title setBordered:NO];
    [_title setDrawsBackground:NO];
    [_title setFont:[NSFont boldSystemFontOfSize:13]];
    for (NSView *v in @[ _back, _forward, _all, _title ])
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
    CGFloat top = NSMaxY(content);
    NSRect newContent = NSMakeRect(NSMinX(content), top - size.height - kBarHeight, size.width, size.height + kBarHeight);

    [_content removeFromSuperview];
    _content = view;
    [_window setFrame:[_window frameRectForContentRect:newContent] display:NO];
    NSView *root = [_window contentView];
    [_bar setFrame:NSMakeRect(0, size.height, size.width, kBarHeight)];
    [_back setFrame:NSMakeRect(10, 7, 30, 24)];
    [_forward setFrame:NSMakeRect(42, 7, 30, 24)];
    [_all setFrame:NSMakeRect(80, 7, 84, 24)];
    [_title setFrame:NSMakeRect(176, 9, size.width - 190, 20)];
    [_title setStringValue:title];
    [view setFrame:NSMakeRect(0, 0, size.width, size.height)];
    [root addSubview:view];
    [_window setTitle:title];
    [root setNeedsDisplay:YES];
}

- (void)updateButtons
{
    [_back setEnabled:[_history count] > 0];
    [_forward setEnabled:[_future count] > 0];
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
                  size:NSMakeSize(kGridWidth, [PrefsGridView heightForCount:[_panes count]])];
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

@end
