/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Notifications pane (Atlas.md, 5.6, N1), laid out as the
 * reference's: on the left Do Not Disturb and the apps that have sent a
 * notification, each with what it may do; on the right the chosen one's
 * settings (allowed, alert style, lock screen, preview, kept in the
 * center, badge, sound); below, the center's sort order. Written to the
 * notification center's domain (NotificationCenterTypes.h); SystemUIServer
 * and the Dock read it again when kNCSettingsChanged is posted (notify(3)).
 * The view is made in code.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#include <notify.h>
#include <os/log.h>
#include "NotificationCenterTypes.h"

@interface NotificationsPane : NSPreferencePane <NSTableViewDataSource, NSTableViewDelegate>
@end

#define kDomain CFSTR(kNCSettingsDomain)

static const CGFloat kWidth = 668, kHeight = 420;
static const CGFloat kListWidth = 220, kRowHeight = 44;

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

static NSButton *button(NSString *title, NSRect frame, NSButtonType type, id target, SEL action)
{
    NSButton *b = [[NSButton alloc] initWithFrame:frame];
    [b setButtonType:type];
    [b setTitle:title];
    [b setTarget:target];
    [b setAction:action];
    return b;
}

/* A note under a control: smaller, grey. */
static NSTextField *note(NSString *text, NSRect frame)
{
    NSTextField *field = label(text, frame, NSLeftTextAlignment);
    [field setFont:[NSFont systemFontOfSize:11]];
    [field setTextColor:[NSColor darkGrayColor]];
    return field;
}

/* A row of the list: an icon, the name and, below, what it may do. */
@interface NotificationsRowView : NSView
@property (nonatomic, strong) NSImage *icon;
@property (nonatomic, copy) NSString *name, *detail;
@property (nonatomic) BOOL selected;
@end

@implementation NotificationsRowView
- (BOOL)isFlipped { return YES; }
- (void)drawRect:(NSRect)dirty
{
    NSColor *text = self.selected ? [NSColor selectedMenuItemTextColor] : [NSColor controlTextColor];
    if (self.selected) {
        [[NSColor selectedMenuItemColor] setFill];
        NSRectFill(self.bounds);
    }
    [self.icon drawInRect:NSMakeRect(8, 6, 32, 32) fromRect:NSZeroRect operation:NSCompositeSourceOver
                 fraction:1 respectFlipped:YES hints:nil];
    CGFloat y = self.detail.length > 0 ? 6 : 13;
    [self.name drawAtPoint:NSMakePoint(48, y) withAttributes:@{
        NSFontAttributeName : [NSFont systemFontOfSize:13], NSForegroundColorAttributeName : text }];
    if (self.detail.length > 0)
        [self.detail drawAtPoint:NSMakePoint(48, 23) withAttributes:@{
            NSFontAttributeName : [NSFont systemFontOfSize:11],
            NSForegroundColorAttributeName : self.selected ? text : [NSColor darkGrayColor] }];
}
@end

@implementation NotificationsPane {
    NSTableView *_list;
    NSView *_appView, *_quietView;
    NSTextField *_styleTitle, *_allowedTitle, *_empty;
    NSButton *_allowed, *_none, *_banner, *_alert, *_lockScreen, *_preview, *_inList, *_badge, *_sound;
    NSButton *_quiet;
    NSPopUpButton *_previewWhen, *_sortOrder;
    NSArray<NSString *> *_keys;    /* the apps' keys, in the list's order (row 0 is Do Not Disturb) */
    int _token;
}

- (NSString *)text:(NSString *)english
{
    return [[self bundle] localizedStringForKey:english value:english table:nil];
}

- (NSView *)loadMainView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];
    CGFloat top = kHeight - 20;

    /* The list. */
    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 60, kListWidth, top - 60)];
    [scroll setBorderType:NSBezelBorder];
    [scroll setHasVerticalScroller:YES];
    _list = [[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]];
    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:@"app"];
    [column setWidth:kListWidth - 20];
    [_list addTableColumn:column];
    [_list setHeaderView:nil];
    [_list setRowHeight:kRowHeight];
    [_list setDataSource:self];
    [_list setDelegate:self];
    [scroll setDocumentView:_list];
    [view addSubview:scroll];

    /* Below it, the center's order. */
    [view addSubview:label([self text:@"Notification Center sort order:"], NSMakeRect(20, 24, 210, 20),
                           NSLeftTextAlignment)];
    _sortOrder = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(228, 20, 160, 26) pullsDown:NO];
    [_sortOrder addItemsWithTitles:@[ [self text:@"Recents"], [self text:@"Recents by App"] ]];
    [_sortOrder setTarget:self];
    [_sortOrder setAction:@selector(sortOrderChosen:)];
    [view addSubview:_sortOrder];

    CGFloat x = 20 + kListWidth + 20, width = kWidth - x - 20;
    NSRect right = NSMakeRect(x, 60, width, top - 60);

    /* Do Not Disturb's settings. */
    _quietView = [[NSView alloc] initWithFrame:right];
    _quiet = button([self text:@"Turn on Do Not Disturb"], NSMakeRect(0, NSHeight(right) - 24, width, 22),
                    NSSwitchButton, self, @selector(quietChanged:));
    [_quietView addSubview:_quiet];
    [_quietView addSubview:note([self text:@"No banners, alerts or sounds; notifications are still listed."],
                                NSMakeRect(18, NSHeight(right) - 44, width - 18, 16))];
    [view addSubview:_quietView];

    /* An app's. */
    _appView = [[NSView alloc] initWithFrame:right];
    CGFloat y = NSHeight(right) - 24;
    _allowedTitle = label(@"", NSMakeRect(0, y, width - 60, 20), NSLeftTextAlignment);
    [_appView addSubview:_allowedTitle];
    _allowed = button(@"", NSMakeRect(width - 30, y, 24, 22), NSSwitchButton, self, @selector(changed:));
    [_appView addSubview:_allowed];
    y -= 36;
    _styleTitle = label(@"", NSMakeRect(0, y, width, 20), NSLeftTextAlignment);
    [_appView addSubview:_styleTitle];
    y -= 26;
    _none = button([self text:@"None"], NSMakeRect(0, y, 100, 22), NSRadioButton, self, @selector(chooseStyle:));
    _banner = button([self text:@"Banners"], NSMakeRect(110, y, 100, 22), NSRadioButton, self, @selector(chooseStyle:));
    _alert = button([self text:@"Alerts"], NSMakeRect(220, y, 100, 22), NSRadioButton, self, @selector(chooseStyle:));
    for (NSButton *b in @[ _none, _banner, _alert ])
        [_appView addSubview:b];
    y -= 34;
    [_appView addSubview:note([self text:@"Banners appear in the upper-right corner and go away automatically. Alerts stay on screen until dismissed."],
                              NSMakeRect(0, y, width, 30))];
    y -= 34;
    _lockScreen = button([self text:@"Show notifications on lock screen"], NSMakeRect(0, y, width, 22),
                         NSSwitchButton, self, @selector(changed:));
    y -= 26;
    _preview = button([self text:@"Show notification preview:"], NSMakeRect(0, y, 210, 22),
                      NSSwitchButton, self, @selector(changed:));
    _previewWhen = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(210, y - 2, 150, 26) pullsDown:NO];
    [_previewWhen addItemsWithTitles:@[ [self text:@"always"], [self text:@"when unlocked"] ]];
    [_previewWhen setTarget:self];
    [_previewWhen setAction:@selector(changed:)];
    y -= 26;
    _inList = button([self text:@"Show in Notification Center"], NSMakeRect(0, y, width, 22),
                     NSSwitchButton, self, @selector(changed:));
    y -= 26;
    _badge = button([self text:@"Badge app icon"], NSMakeRect(0, y, width, 22), NSSwitchButton, self,
                    @selector(changed:));
    y -= 26;
    _sound = button([self text:@"Play sound for notifications"], NSMakeRect(0, y, width, 22), NSSwitchButton,
                    self, @selector(changed:));
    for (NSView *v in @[ _lockScreen, _preview, _previewWhen, _inList, _badge, _sound ])
        [_appView addSubview:v];
    [view addSubview:_appView];

    _empty = note([self text:@"No app has sent a notification yet."], NSMakeRect(x, top - 60, width, 16));
    [view addSubview:_empty];

    /* Do Not Disturb from the menu bar, or a new app, while the pane is open. */
    __weak NotificationsPane *weakSelf = self;
    notify_register_dispatch(kNCSettingsChanged, &_token, dispatch_get_main_queue(), ^(int token) {
        [weakSelf reload];
    });
    [self reload];
    [_list selectRowIndexes:[NSIndexSet indexSetWithIndex:_keys.count > 0 ? 1 : 0] byExtendingSelection:NO];
    [self showChosen];
    [self setMainView:view];
    return view;
}

- (void)dealloc
{
    if (_token != 0)
        notify_cancel(_token);
}

- (void)willSelect
{
    [self reload];
}

/* The apps the notification center has seen, by name. */
- (void)reload
{
    NSString *chosen = self.chosenKey;
    NSMutableArray<NSString *> *keys = [NSMutableArray array];
    CFArrayRef all;

    CFPreferencesAppSynchronize(kDomain);
    [_quiet setState:CFPreferencesGetAppBooleanValue(CFSTR(kNCSettingDoNotDisturb), kDomain, NULL) ? NSOnState : NSOffState];
    CFPropertyListRef order = CFPreferencesCopyAppValue(CFSTR(kNCSettingSortOrder), kDomain);
    [_sortOrder selectItemAtIndex:order != NULL && CFEqual(order, CFSTR("recentsByApp")) ? 1 : 0];
    if (order != NULL)
        CFRelease(order);
    all = CFPreferencesCopyKeyList(kDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    for (NSString *key in (__bridge NSArray *)all)
        if ([key hasPrefix:@kNCSettingAppPrefix] && [self app:key] != nil)
            [keys addObject:key];
    if (all != NULL)
        CFRelease(all);
    [keys sortUsingComparator:^NSComparisonResult(NSString *a, NSString *b) {
        return [[self nameOf:a] localizedCaseInsensitiveCompare:[self nameOf:b]];
    }];
    BOOL quietChosen = _list.selectedRow == 0;
    _keys = keys;
    [_list reloadData];
    if (chosen != nil && [keys containsObject:chosen])
        [_list selectRowIndexes:[NSIndexSet indexSetWithIndex:[keys indexOfObject:chosen] + 1] byExtendingSelection:NO];
    else if (!quietChosen)
        [_list selectRowIndexes:[NSIndexSet indexSetWithIndex:keys.count > 0 ? 1 : 0] byExtendingSelection:NO];
    [self showChosen];
}

- (NSDictionary *)app:(NSString *)key
{
    CFPropertyListRef value = CFPreferencesCopyAppValue((__bridge CFStringRef)key, kDomain);
    NSDictionary *app = nil;

    if (value != NULL && CFGetTypeID(value) == CFDictionaryGetTypeID())
        app = (__bridge NSDictionary *)value;
    if (value != NULL)
        CFRelease(value);
    return app;
}

- (NSString *)nameOf:(NSString *)key
{
    NSString *name = [self app:key][@kNCAppName];

    return name.length > 0 ? name : [key substringFromIndex:strlen(kNCSettingAppPrefix)];
}

- (NSString *)chosenKey
{
    NSInteger row = _list.selectedRow;

    return row >= 1 && (NSUInteger)row <= _keys.count ? _keys[row - 1] : nil;
}

static BOOL flag(NSDictionary *app, NSString *key)
{
    return app[key] == nil || [app[key] boolValue];
}

/* What an app may do, under its name: as the reference's list says it. */
- (NSString *)detailOf:(NSDictionary *)app
{
    if (!flag(app, @kNCAppAllowed))
        return [self text:@"Off"];
    NSMutableArray *parts = [NSMutableArray array];
    if (flag(app, @kNCAppBadge))
        [parts addObject:[self text:@"Badges"]];
    if (flag(app, @kNCAppSound))
        [parts addObject:[self text:@"Sounds"]];
    NSString *style = app[@kNCAppStyle] ?: @"banner";
    if ([style isEqual:@"banner"])
        [parts addObject:[self text:@"Banners"]];
    else if ([style isEqual:@"alert"])
        [parts addObject:[self text:@"Alerts"]];
    return [parts componentsJoinedByString:@", "];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)table
{
    return 1 + _keys.count;
}

- (NSView *)tableView:(NSTableView *)table viewForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    NotificationsRowView *v = [[NotificationsRowView alloc] initWithFrame:NSMakeRect(0, 0, kListWidth - 20, kRowHeight)];
    if (row == 0) {
        v.name = [self text:@"Do Not Disturb"];
        v.icon = [NSImage imageNamed:NSImageNameStatusUnavailable];
    } else {
        NSString *key = _keys[row - 1];
        NSDictionary *app = [self app:key];
        v.name = [self nameOf:key];
        v.detail = [self detailOf:app];
        NSString *path = app[@kNCAppPath];
        v.icon = path.length > 0 ? [[NSWorkspace sharedWorkspace] iconForFile:path]
                                 : [NSImage imageNamed:NSImageNameApplicationIcon];
    }
    v.selected = [table selectedRow] == row;
    return v;
}

- (void)tableViewSelectionDidChange:(NSNotification *)note
{
    [_list reloadData];
    [self showChosen];
}

/* The chosen row's settings in the controls. */
- (void)showChosen
{
    BOOL quiet = _list.selectedRow <= 0;
    NSString *key = self.chosenKey;
    NSDictionary *app = key != nil ? [self app:key] : nil;

    [_quietView setHidden:!quiet];
    [_appView setHidden:quiet || app == nil];
    [_empty setHidden:_keys.count > 0 || !quiet];
    if (app == nil)
        return;

    NSString *name = [self nameOf:key];
    BOOL allowed = flag(app, @kNCAppAllowed);
    NSString *style = app[@kNCAppStyle] ?: @"banner";
    NSString *preview = app[@kNCAppPreview] ?: @"always";

    [_allowedTitle setStringValue:[NSString stringWithFormat:[self text:@"Allow Notifications from %@"], name]];
    [_styleTitle setStringValue:[NSString stringWithFormat:[self text:@"%@ alert style:"], name]];
    [_allowed setState:allowed ? NSOnState : NSOffState];
    [_none setState:[style isEqual:@"none"] ? NSOnState : NSOffState];
    [_alert setState:[style isEqual:@"alert"] ? NSOnState : NSOffState];
    [_banner setState:[_none state] == NSOffState && [_alert state] == NSOffState ? NSOnState : NSOffState];
    [_lockScreen setState:flag(app, @kNCAppLockScreen) ? NSOnState : NSOffState];
    [_preview setState:[preview isEqual:@"never"] ? NSOffState : NSOnState];
    [_previewWhen selectItemAtIndex:[preview isEqual:@"unlocked"] ? 1 : 0];
    [_inList setState:flag(app, @kNCAppShowInList) ? NSOnState : NSOffState];
    [_badge setState:flag(app, @kNCAppBadge) ? NSOnState : NSOffState];
    [_sound setState:flag(app, @kNCAppSound) ? NSOnState : NSOffState];
    for (NSControl *c in @[ _none, _banner, _alert, _lockScreen, _preview, _inList, _badge, _sound ])
        [c setEnabled:allowed];
    [_previewWhen setEnabled:allowed && [_preview state] == NSOnState];
}

- (void)chooseStyle:(NSButton *)sender
{
    for (NSButton *b in @[ _none, _banner, _alert ])
        [b setState:b == sender ? NSOnState : NSOffState];
    [self changed:sender];
}

- (void)quietChanged:(id)sender
{
    CFPreferencesSetAppValue(CFSTR(kNCSettingDoNotDisturb), [_quiet state] == NSOnState ? kCFBooleanTrue : kCFBooleanFalse,
                             kDomain);
    CFPreferencesAppSynchronize(kDomain);
    notify_post(kNCSettingsChanged);
    os_log_info(OS_LOG_DEFAULT, "notifications doNotDisturb=%d", [_quiet state] == NSOnState);
}

- (void)sortOrderChosen:(id)sender
{
    CFPreferencesSetAppValue(CFSTR(kNCSettingSortOrder),
                             [_sortOrder indexOfSelectedItem] == 1 ? CFSTR("recentsByApp") : CFSTR("recents"), kDomain);
    CFPreferencesAppSynchronize(kDomain);
    notify_post(kNCSettingsChanged);
}

/* The chosen app's settings written, then SystemUIServer and the Dock told. */
- (void)changed:(id)sender
{
    NSString *key = self.chosenKey;
    NSMutableDictionary *app;
    NSString *style, *preview;

    if (key == nil)
        return;
    CFPreferencesAppSynchronize(kDomain);
    app = [[self app:key] mutableCopy] ?: [NSMutableDictionary dictionary];
    style = [_none state] == NSOnState ? @"none" : [_alert state] == NSOnState ? @"alert" : @"banner";
    preview = [_preview state] == NSOffState ? @"never" : [_previewWhen indexOfSelectedItem] == 1 ? @"unlocked" : @"always";
    app[@kNCAppAllowed] = @([_allowed state] == NSOnState);
    app[@kNCAppStyle] = style;
    app[@kNCAppSound] = @([_sound state] == NSOnState);
    app[@kNCAppLockScreen] = @([_lockScreen state] == NSOnState);
    app[@kNCAppPreview] = preview;
    app[@kNCAppShowInList] = @([_inList state] == NSOnState);
    app[@kNCAppBadge] = @([_badge state] == NSOnState);
    CFPreferencesSetAppValue((__bridge CFStringRef)key, (__bridge CFDictionaryRef)app, kDomain);
    CFPreferencesAppSynchronize(kDomain);
    notify_post(kNCSettingsChanged);
    [_list reloadData];
    [self showChosen];
    os_log_info(OS_LOG_DEFAULT, "notifications %{public}s allowed=%d style=%{public}s sound=%d badge=%d list=%d lock=%d preview=%{public}s",
                key.UTF8String, [_allowed state] == NSOnState, style.UTF8String, [_sound state] == NSOnState,
                [_badge state] == NSOnState, [_inList state] == NSOnState, [_lockScreen state] == NSOnState,
                preview.UTF8String);
}

@end
