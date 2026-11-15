/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Notifications pane (Atlas.md, 5.6, N1): Do Not Disturb, and for
 * each app that has sent a notification whether it may, how it shows
 * (none, banners, alerts) and whether its sound plays. Written to the
 * notification center's domain (NotificationCenterTypes.h); SystemUIServer
 * reads it again when kNCSettingsChanged is posted (notify(3)). The view
 * is made in code. Atlas's provisional look.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#include <notify.h>
#include <os/log.h>
#include "NotificationCenterTypes.h"

@interface NotificationsPane : NSPreferencePane
@end

#define kDomain CFSTR(kNCSettingsDomain)

static const CGFloat kWidth = 668, kHeight = 300;

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

@implementation NotificationsPane {
    NSButton *_quiet, *_allowed, *_none, *_banner, *_alert, *_sound;
    NSPopUpButton *_apps;
    NSTextField *_empty;
    NSArray<NSString *> *_keys;    /* the apps' keys, in the menu's order */
    int _token;
}

- (NSString *)text:(NSString *)english
{
    return [[self bundle] localizedStringForKey:english value:english table:nil];
}

- (NSView *)loadMainView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];

    _quiet = button([self text:@"Do Not Disturb"], NSMakeRect(232, 250, 360, 22), NSSwitchButton, self,
                    @selector(quietChanged:));
    [view addSubview:_quiet];
    [view addSubview:note([self text:@"No banners, alerts or sounds; notifications are still listed."],
                          NSMakeRect(250, 230, 400, 16))];

    NSBox *line = [[NSBox alloc] initWithFrame:NSMakeRect(20, 212, kWidth - 40, 1)];
    [line setBoxType:NSBoxSeparator];
    [view addSubview:line];

    [view addSubview:label([self text:@"App:"], NSMakeRect(20, 176, 200, 20), NSRightTextAlignment)];
    _apps = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(232, 172, 300, 26) pullsDown:NO];
    [_apps setTarget:self];
    [_apps setAction:@selector(appChosen:)];
    [view addSubview:_apps];
    _empty = label([self text:@"No app has sent a notification yet."], NSMakeRect(232, 176, 400, 20), NSLeftTextAlignment);
    [view addSubview:_empty];

    _allowed = button([self text:@"Allow notifications"], NSMakeRect(232, 136, 360, 22), NSSwitchButton, self,
                      @selector(changed:));
    [view addSubview:_allowed];

    [view addSubview:label([self text:@"Style:"], NSMakeRect(20, 102, 200, 20), NSRightTextAlignment)];
    _none = button([self text:@"None"], NSMakeRect(232, 100, 90, 22), NSRadioButton, self, @selector(chooseStyle:));
    _banner = button([self text:@"Banners"], NSMakeRect(322, 100, 100, 22), NSRadioButton, self, @selector(chooseStyle:));
    _alert = button([self text:@"Alerts"], NSMakeRect(422, 100, 100, 22), NSRadioButton, self, @selector(chooseStyle:));
    for (NSButton *b in @[ _none, _banner, _alert ])
        [view addSubview:b];
    [view addSubview:note([self text:@"Banners go after a few seconds; alerts stay until they are closed."],
                          NSMakeRect(232, 78, 420, 16))];

    _sound = button([self text:@"Play sound for notifications"], NSMakeRect(232, 40, 360, 22), NSSwitchButton, self,
                    @selector(changed:));
    [view addSubview:_sound];

    /* Do Not Disturb from the menu bar, or a new app, while the pane is open. */
    __weak NotificationsPane *weakSelf = self;
    notify_register_dispatch(kNCSettingsChanged, &_token, dispatch_get_main_queue(), ^(int token) {
        [weakSelf reload];
    });
    [self reload];
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
    NSString *chosen = _apps.indexOfSelectedItem >= 0 && (NSUInteger)_apps.indexOfSelectedItem < _keys.count ?
        _keys[_apps.indexOfSelectedItem] : nil;
    NSMutableArray<NSString *> *keys = [NSMutableArray array];
    CFArrayRef all;

    CFPreferencesAppSynchronize(kDomain);
    [_quiet setState:CFPreferencesGetAppBooleanValue(CFSTR(kNCSettingDoNotDisturb), kDomain, NULL) ? NSOnState : NSOffState];
    all = CFPreferencesCopyKeyList(kDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    for (NSString *key in (__bridge NSArray *)all)
        if ([key hasPrefix:@kNCSettingAppPrefix] && [self app:key] != nil)
            [keys addObject:key];
    if (all != NULL)
        CFRelease(all);
    [keys sortUsingComparator:^NSComparisonResult(NSString *a, NSString *b) {
        return [[self nameOf:a] localizedCaseInsensitiveCompare:[self nameOf:b]];
    }];
    _keys = keys;
    [_apps removeAllItems];
    for (NSString *key in keys)
        [_apps addItemWithTitle:[self nameOf:key]];
    if (chosen != nil && [keys containsObject:chosen])
        [_apps selectItemAtIndex:[keys indexOfObject:chosen]];
    [_apps setHidden:keys.count == 0];
    [_empty setHidden:keys.count != 0];
    [self showApp];
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
    NSInteger i = _apps.indexOfSelectedItem;

    return i >= 0 && (NSUInteger)i < _keys.count ? _keys[i] : nil;
}

/* The chosen app's settings in the controls. */
- (void)showApp
{
    NSDictionary *app = self.chosenKey != nil ? [self app:self.chosenKey] : nil;
    BOOL allowed = app[@kNCAppAllowed] == nil || [app[@kNCAppAllowed] boolValue];
    NSString *style = app[@kNCAppStyle] ?: @"banner";

    [_allowed setState:allowed ? NSOnState : NSOffState];
    [_none setState:[style isEqual:@"none"] ? NSOnState : NSOffState];
    [_alert setState:[style isEqual:@"alert"] ? NSOnState : NSOffState];
    [_banner setState:[_none state] == NSOffState && [_alert state] == NSOffState ? NSOnState : NSOffState];
    [_sound setState:app[@kNCAppSound] == nil || [app[@kNCAppSound] boolValue] ? NSOnState : NSOffState];
    for (NSButton *b in @[ _allowed, _none, _banner, _alert, _sound ])
        [b setEnabled:app != nil && (b == _allowed || allowed)];
}

- (void)appChosen:(id)sender
{
    [self showApp];
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

/* The chosen app's settings written, then SystemUIServer told. */
- (void)changed:(id)sender
{
    NSString *key = self.chosenKey;
    NSMutableDictionary *app;
    NSString *style;

    if (key == nil)
        return;
    CFPreferencesAppSynchronize(kDomain);
    app = [[self app:key] mutableCopy] ?: [NSMutableDictionary dictionary];
    style = [_none state] == NSOnState ? @"none" : [_alert state] == NSOnState ? @"alert" : @"banner";
    app[@kNCAppAllowed] = @([_allowed state] == NSOnState);
    app[@kNCAppStyle] = style;
    app[@kNCAppSound] = @([_sound state] == NSOnState);
    CFPreferencesSetAppValue((__bridge CFStringRef)key, (__bridge CFDictionaryRef)app, kDomain);
    CFPreferencesAppSynchronize(kDomain);
    notify_post(kNCSettingsChanged);
    [self showApp];
    os_log_info(OS_LOG_DEFAULT, "notifications %{public}s allowed=%d style=%{public}s sound=%d", key.UTF8String,
                [_allowed state] == NSOnState, style.UTF8String, [_sound state] == NSOnState);
}

@end
