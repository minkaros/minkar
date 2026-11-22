/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The notification center's Menu Extra (N1). Its panel is a window of its
 * own, as the volume's is, and goes when it loses the keyboard. The
 * menu bar's icon is drawn here: a card with lines; hollow when Do Not
 * Disturb is on.
 */

#import "NotificationCenterExtra.h"
#import "NotificationSettings.h"
#include "NotificationCenterTypes.h"

static const CGFloat kPanelWidth = 340, kRowHeight = 64, kHeader = 66, kFooter = 34;
static const NSUInteger kRowsShown = 6;

@interface NotificationCenterPanel : NSPanel
/* The extra's button in the menu bar: highlighted while the panel is open,
   as a menu's title is while its menu is. */
@property (nonatomic, weak) NSStatusBarButton *statusButton;
@end

@implementation NotificationCenterPanel
- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (void)makeKeyAndOrderFront:(id)sender
{
    [super makeKeyAndOrderFront:sender];
    [self.statusButton highlight:YES];
}

- (void)orderOut:(id)sender
{
    [super orderOut:sender];
    [self.statusButton highlight:NO];
}

- (void)resignKeyWindow
{
    [super resignKeyWindow];
    [self orderOut:nil];
}
@end

@class NotificationCenterExtra;

@interface NotificationCenterExtra ()
- (void)rowClicked:(NSDictionary *)notification;
@end

/* The list: one row a notification, the app's icon, its name and time,
   the title and the text. */
@interface NotificationListView : NSView
@property (copy) NSArray<NSDictionary *> *notifications;
@property (weak) NotificationCenterExtra *extra;
@end

@implementation NotificationListView

- (BOOL)isFlipped
{
    return YES;
}

static void drawText(NSString *text, NSFont *font, NSColor *color, NSRect rect, NSTextAlignment alignment)
{
    NSMutableParagraphStyle *style = [[NSMutableParagraphStyle alloc] init];

    style.lineBreakMode = NSLineBreakByTruncatingTail;
    style.alignment = alignment;
    [text drawInRect:rect withAttributes:@{
        NSFontAttributeName : font,
        NSForegroundColorAttributeName : color,
        NSParagraphStyleAttributeName : style,
    }];
}

- (void)drawRect:(NSRect)dirty
{
    NSDateFormatter *time = [[NSDateFormatter alloc] init];
    CGFloat width = NSWidth(self.bounds);

    time.dateFormat = @"HH:mm";
    /* In the appearance's colors (General in System Preferences). */
    BOOL dark = [[[NSApp effectiveAppearance] name] isEqualToString:NSAppearanceNameDarkAqua];
    [[NSColor colorWithGenericGamma22White:dark ? 0.19 : 0.97 alpha:1] setFill];
    NSRectFill(self.bounds);
    if (self.notifications.count == 0) {
        drawText(NSLocalizedString(@"No Notifications", @"the notification center, empty"),
                 [NSFont systemFontOfSize:13], [NSColor grayColor], NSMakeRect(0, 22, width, 18), NSTextAlignmentCenter);
        return;
    }
    [self.notifications enumerateObjectsUsingBlock:^(NSDictionary *n, NSUInteger i, BOOL *stop) {
        CGFloat top = i * kRowHeight, x = 56, textWidth = width - x - 12;
        NSString *path = n[@kNCKeyAppPath];
        NSImage *icon = path != nil ? [[NSWorkspace sharedWorkspace] iconForFile:path] : nil;
        NSString *subtitle = n[@kNCKeySubtitle], *text = n[@kNCKeyText];
        NSString *second = subtitle.length > 0 && text.length > 0 ? [NSString stringWithFormat:@"%@ — %@", subtitle, text]
                         : subtitle.length > 0 ? subtitle : text ?: @"";

        if (icon == nil)
            icon = [[NSWorkspace sharedWorkspace] iconForFileType:@"app"];
        [icon drawInRect:NSMakeRect(12, top + 14, 32, 32) fromRect:NSZeroRect
               operation:NSCompositingOperationSourceOver fraction:1 respectFlipped:YES hints:nil];
        drawText(n[@kNCKeyAppName] ?: @"", [NSFont systemFontOfSize:11], [NSColor grayColor],
                 NSMakeRect(x, top + 6, textWidth - 48, 14), NSTextAlignmentLeft);
        if ([n[@kNCKeyDate] isKindOfClass:[NSDate class]])
            drawText([time stringFromDate:n[@kNCKeyDate]], [NSFont systemFontOfSize:11], [NSColor grayColor],
                     NSMakeRect(width - 60, top + 6, 48, 14), NSTextAlignmentRight);
        drawText(n[@kNCKeyTitle] ?: @"", [NSFont boldSystemFontOfSize:13], [NSColor labelColor],
                 NSMakeRect(x, top + 22, textWidth, 17), NSTextAlignmentLeft);
        drawText(second, [NSFont systemFontOfSize:12], [NSColor secondaryLabelColor],
                 NSMakeRect(x, top + 40, textWidth, 16), NSTextAlignmentLeft);
        if (i + 1 < self.notifications.count) {
            [[NSColor colorWithGenericGamma22White:dark ? 0.32 : 0.82 alpha:1] setFill];
            NSRectFill(NSMakeRect(12, top + kRowHeight - 1, width - 24, 1));
        }
    }];
}

- (void)mouseUp:(NSEvent *)event
{
    NSPoint where = [self convertPoint:event.locationInWindow fromView:nil];
    NSInteger row = (NSInteger)floor(where.y / kRowHeight);

    if (NSPointInRect(where, self.bounds) && row >= 0 && row < (NSInteger)self.notifications.count)
        [self.extra rowClicked:self.notifications[row]];
}

@end

@implementation NotificationCenterExtra {
    NSStatusItem *_item;
    NotificationPresenter *_presenter;
    NSRunningApplication *_previous;   /* in front before the panel */
    NotificationCenterPanel *_panel;
    NSButton *_quiet, *_clear;
    NSScrollView *_scroll;
    NotificationListView *_list;
}

- (instancetype)initWithStatusItem:(NSStatusItem *)item
{
    return nil;
}

- (instancetype)initWithStatusItem:(NSStatusItem *)item presenter:(NotificationPresenter *)presenter
{
    if ((self = [super init]) == nil)
        return nil;
    _item = item;
    _presenter = presenter;
    [_item setTarget:self];
    [_item setAction:@selector(toggle:)];
    [_item sendActionOn:NSEventMaskLeftMouseDown];
    [self buildPanel];
    [self showIcon];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(deactivated:)
                                                 name:NSApplicationDidResignActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(settingsChanged:)
                                                 name:NotificationSettingsDidChange object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(listChanged:)
                                                 name:NotificationPresenterListDidChange object:nil];
    return self;
}

/* A card with three lines of text; hollow when Do Not Disturb is on. */
- (void)showIcon
{
    BOOL quiet = [NotificationSettings shared].doNotDisturb;
    NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(18, 18)];

    [image lockFocus];
    NSBezierPath *card = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(2.5, 3.5, 13, 11) xRadius:2 yRadius:2];
    [[NSColor blackColor] set];
    [card setLineWidth:1.2];
    [card stroke];
    if (!quiet)
        for (int i = 0; i < 3; i++)
            NSRectFill(NSMakeRect(5, 11 - i * 2.6, i == 2 ? 5 : 8, 1.2));
    [image unlockFocus];
    [image setTemplate:YES];
    [_item setImage:image];
    [[_item button] display];
}

static NSButton *makeButton(NSString *title, NSRect frame, id target, SEL action)
{
    NSButton *b = [[NSButton alloc] initWithFrame:frame];

    [b setTitle:title];
    [b setTarget:target];
    [b setAction:action];
    return b;
}

- (void)buildPanel
{
    NSRect frame = NSMakeRect(0, 0, kPanelWidth, kHeader + kRowHeight + kFooter);
    NSTextField *title;

    _panel = [[NotificationCenterPanel alloc] initWithContentRect:frame styleMask:NSWindowStyleMaskBorderless
                                                          backing:NSBackingStoreBuffered defer:NO];
    [_panel setLevel:NSPopUpMenuWindowLevel];
    [_panel setHidesOnDeactivate:NO];
    ((NotificationCenterPanel *)_panel).statusButton = [_item button];
    [_panel setReleasedWhenClosed:NO];

    title = [[NSTextField alloc] initWithFrame:NSZeroRect];
    [title setStringValue:NSLocalizedString(@"Notifications", @"the notification center's title")];
    [title setFont:[NSFont boldSystemFontOfSize:13]];
    [title setEditable:NO];
    [title setBordered:NO];
    [title setDrawsBackground:NO];
    [title setTag:1];
    [[_panel contentView] addSubview:title];

    _clear = makeButton(NSLocalizedString(@"Clear", @"the notification center: remove them all"), NSZeroRect, self,
                        @selector(clear:));
    [_clear setBezelStyle:NSBezelStyleRounded];
    [[_panel contentView] addSubview:_clear];

    _quiet = makeButton(NSLocalizedString(@"Do Not Disturb", nil), NSZeroRect, self, @selector(quietChanged:));
    [_quiet setButtonType:NSButtonTypeSwitch];
    [[_panel contentView] addSubview:_quiet];

    _list = [[NotificationListView alloc] initWithFrame:NSMakeRect(0, 0, kPanelWidth, kRowHeight)];
    _list.extra = self;
    _scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, kFooter, kPanelWidth, kRowHeight)];
    [_scroll setHasVerticalScroller:YES];
    [_scroll setAutohidesScrollers:YES];
    [_scroll setBorderType:NSNoBorder];
    [_scroll setDocumentView:_list];
    [[_panel contentView] addSubview:_scroll];

    NSButton *prefs = makeButton(NSLocalizedString(@"Notification Preferences…", nil), NSMakeRect(10, 5, 260, 24),
                                 self, @selector(openPreferences:));
    [prefs setBordered:NO];
    [prefs setAlignment:NSTextAlignmentLeft];
    [[_panel contentView] addSubview:prefs];
}

/* The panel's size for the list: up to kRowsShown rows, the rest scrolled. */
- (void)layOutPanel
{
    NSArray *delivered = _presenter.delivered;
    NSUInteger shown = MAX(1u, MIN(kRowsShown, delivered.count));
    CGFloat listHeight = shown * kRowHeight, height = kHeader + listHeight + kFooter;
    NSRect frame = [_panel frame];

    frame.origin.y += frame.size.height - height;
    frame.size = NSMakeSize(kPanelWidth, height);
    [_panel setFrame:frame display:NO];
    [[[_panel contentView] viewWithTag:1] setFrame:NSMakeRect(16, height - 30, 200, 18)];
    [_clear setFrame:NSMakeRect(kPanelWidth - 92, height - 34, 80, 26)];
    [_clear setEnabled:delivered.count > 0];
    [_quiet setFrame:NSMakeRect(14, height - 58, 260, 20)];
    [_quiet setState:[NotificationSettings shared].doNotDisturb ? NSControlStateValueOn : NSControlStateValueOff];
    [_scroll setFrame:NSMakeRect(0, kFooter, kPanelWidth, listHeight)];
    _list.notifications = delivered;
    [_list setFrame:NSMakeRect(0, 0, [_scroll contentSize].width, MAX(1u, delivered.count) * kRowHeight)];
    [_list setNeedsDisplay:YES];
    [[_panel contentView] setNeedsDisplay:YES];
}

- (void)deactivated:(NSNotification *)note
{
    _previous = nil;
    [_panel orderOut:nil];
}

- (void)close
{
    [_panel orderOut:nil];
    [_previous activateWithOptions:0];
    _previous = nil;
}

- (void)toggle:(id)sender
{
    if ([_panel isVisible]) {
        [self close];
        return;
    }
    NSRunningApplication *front = [[NSWorkspace sharedWorkspace] frontmostApplication];
    if (![front isEqual:[NSRunningApplication currentApplication]])
        _previous = front;
    [self layOutPanel];
    NSRect itemFrame = [[[_item button] window] frame];
    NSRect visible = [[NSScreen mainScreen] visibleFrame];
    NSRect frame = [_panel frame];
    frame.origin.x = MIN(NSMinX(itemFrame), NSMaxX(visible) - frame.size.width);
    frame.origin.y = NSMinY(itemFrame) - frame.size.height;
    [_panel setFrame:frame display:NO];
    [NSApp activateIgnoringOtherApps:YES];
    [_panel makeKeyAndOrderFront:nil];
    fprintf(stderr, "SystemUIServer: notification center open, %lu listed\n", (unsigned long)_presenter.delivered.count);
}

- (void)settingsChanged:(NSNotification *)note
{
    [self showIcon];
    if ([_panel isVisible])
        [self layOutPanel];
}

- (void)listChanged:(NSNotification *)note
{
    if ([_panel isVisible])
        [self layOutPanel];
}

- (void)quietChanged:(id)sender
{
    [NotificationSettings shared].doNotDisturb = [_quiet state] == NSControlStateValueOn;
    [self showIcon];
}

- (void)clear:(id)sender
{
    [_presenter clearDelivered];
}

- (void)rowClicked:(NSDictionary *)notification
{
    /* The app comes to the front: the one before does not. */
    _previous = nil;
    [_panel orderOut:nil];
    [_presenter activateDelivered:notification];
}

- (void)openPreferences:(id)sender
{
    _previous = nil;
    [_panel orderOut:nil];
    /* System Preferences, at the Notifications pane. */
    [[NSWorkspace sharedWorkspace]
        launchApplicationAtURL:[NSURL fileURLWithPath:@"/System/Applications/System Preferences.app"]
                       options:NSWorkspaceLaunchDefault
                 configuration:@{ NSWorkspaceLaunchConfigurationArguments : @[ @"-pane", @"org.minkar.preference.notifications" ] }
                         error:NULL];
}

@end
