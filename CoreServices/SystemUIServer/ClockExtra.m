/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The clock Menu Extra. Its format is the domain's DateFormat key
 * (org.minkar.menuextra.clock), "EEE d MMM  HH:mm" by default.
 */

#import "ClockExtra.h"

static NSString *const kDomain = @"org.minkar.menuextra.clock";
#define kDefaultFormat NSLocalizedString(@"EEE d MMM  HH:mm", @"the clock in the menu bar")

/* Day and month names in the language the texts are shown in. */
static NSLocale *displayLocale(void)
{
    return [NSLocale localeWithLocaleIdentifier:[[[NSBundle mainBundle] preferredLocalizations] firstObject]];
}

static NSString *clockFormat(void)
{
    NSDictionary *prefs = [[NSUserDefaults standardUserDefaults] persistentDomainForName:kDomain];
    NSString *format = prefs[@"DateFormat"];

    return [format isKindOfClass:[NSString class]] && [format length] ? format : kDefaultFormat;
}

@implementation ClockExtra {
    NSStatusItem *_item;
    NSMenuItem *_dateItem;
    NSTimer *_timer;
}

+ (NSString *)titleForDate:(NSDate *)date
{
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];

    [formatter setLocale:displayLocale()];
    [formatter setDateFormat:clockFormat()];
    return [formatter stringFromDate:date];
}

- (instancetype)initWithStatusItem:(NSStatusItem *)item
{
    if ((self = [super init]) == nil)
        return nil;
    _item = item;

    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Clock"];
    _dateItem = [[NSMenuItem alloc] initWithTitle:@"" action:NULL keyEquivalent:@""];
    [_dateItem setEnabled:NO];
    [menu addItem:_dateItem];
    [menu addItem:[NSMenuItem separatorItem]];
    NSMenuItem *prefs = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Open Date & Time Preferences…", nil)
                                                   action:NULL keyEquivalent:@""];
    [prefs setEnabled:NO];   /* with System Preferences (MB3) */
    [menu addItem:prefs];
    [menu setAutoenablesItems:NO];
    [_item setMenu:menu];

    [self tick:nil];
    /* Every minute, on the minute. */
    NSTimeInterval seconds = [[NSDate date] timeIntervalSince1970];
    NSDate *next = [NSDate dateWithTimeIntervalSince1970:(floor(seconds / 60) + 1) * 60];
    _timer = [[NSTimer alloc] initWithFireDate:next interval:60 target:self
                                      selector:@selector(tick:) userInfo:nil repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:_timer forMode:NSDefaultRunLoopMode];
    return self;
}

/* Shows the time. */
- (void)tick:(NSTimer *)timer
{
    NSDate *now = [NSDate date];
    NSDateFormatter *full = [[NSDateFormatter alloc] init];

    [full setLocale:displayLocale()];
    [full setDateFormat:NSLocalizedString(@"EEEE d MMMM yyyy", @"the date in the clock's menu")];
    [_item setTitle:[ClockExtra titleForDate:now]];
    [_dateItem setTitle:[full stringFromDate:now]];
}

@end
