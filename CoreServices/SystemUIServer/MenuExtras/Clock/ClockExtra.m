/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The clock Menu Extra, with the reference platform's keys in its domain
 * (org.minkar.menuextra.clock), which the Date & Time pane's Clock tab
 * writes: DateFormat ("EEE d MMM  HH:mm" by default; with seconds it
 * ticks every second), FlashDateSeparators (the time's colons blink each
 * second) and IsAnalog (a clock face instead of the text).
 */

#import "ClockExtra.h"

/* A .menu bundle's texts are its own (Resources/es.lproj). */
#undef NSLocalizedString
#define NSLocalizedString(key, comment) \
    [[NSBundle bundleForClass:[ClockExtra class]] localizedStringForKey:(key) value:@"" table:nil]
#include <notify.h>
#include <unistd.h>

static NSString *const kDomain = @"org.minkar.menuextra.clock";
#define kDefaultFormat NSLocalizedString(@"EEE d MMM  HH:mm", @"the clock in the menu bar")

/* Day and month names in the language the texts are shown in. */
static NSLocale *displayLocale(void)
{
    return [NSLocale localeWithLocaleIdentifier:[[[NSBundle mainBundle] preferredLocalizations] firstObject]];
}

static NSDictionary *clockPreferences(void)
{
    [[NSUserDefaults standardUserDefaults] synchronize];
    return [[NSUserDefaults standardUserDefaults] persistentDomainForName:kDomain];
}

static NSString *clockFormat(void)
{
    NSString *format = clockPreferences()[@"DateFormat"];

    return [format isKindOfClass:[NSString class]] && [format length] ? format : kDefaultFormat;
}

/* A clock face for the menu bar: a ring and the hands, in the text's
   colour (a template image). */
static NSImage *clockFace(NSDate *date)
{
    NSDateComponents *c = [[NSCalendar currentCalendar] components:NSCalendarUnitHour | NSCalendarUnitMinute fromDate:date];
    NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(16, 16)];
    double minute = [c minute], hour = fmod([c hour], 12) + minute / 60;

    [image lockFocus];
    [[NSColor blackColor] set];
    NSBezierPath *ring = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(1.5, 1.5, 13, 13)];
    [ring setLineWidth:1.2];
    [ring stroke];
    for (int hand = 0; hand < 2; hand++) {
        double angle = M_PI / 2 - 2 * M_PI * (hand == 0 ? hour / 12 : minute / 60);
        double length = hand == 0 ? 3.6 : 5.2;
        NSBezierPath *line = [NSBezierPath bezierPath];
        [line moveToPoint:NSMakePoint(8, 8)];
        [line lineToPoint:NSMakePoint(8 + length * cos(angle), 8 + length * sin(angle))];
        [line setLineWidth:hand == 0 ? 1.5 : 1.0];
        [line setLineCapStyle:NSRoundLineCapStyle];
        [line stroke];
    }
    [image unlockFocus];
    [image setTemplate:YES];
    return image;
}

@implementation ClockExtra {
    NSStatusItem *_item;
    NSMenuItem *_dateItem;
    NSTimer *_timer;
    NSTimeInterval _period;     /* 60, or 1 with seconds or blinking */
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
                                                   action:@selector(openPreferences:) keyEquivalent:@""];
    [prefs setTarget:self];
    [menu addItem:prefs];
    [menu setAutoenablesItems:NO];
    [_item setMenu:menu];

    [self schedule];
    [self watch:"org.minkar.menuextra.clock"];   /* its format changed */
    [self watch:"org.minkar.timezone"];          /* the time zone changed */
    return self;
}

/* A notify(3) name, read on a thread of its own: the clock again. */
- (void)watch:(const char *)name
{
    int fd = -1, token;

    if (notify_register_file_descriptor(name, &fd, 0, &token) != NOTIFY_STATUS_OK)
        return;
    [NSThread detachNewThreadWithBlock:^{
        int value;
        while (read(fd, &value, sizeof(value)) == sizeof(value))
            [self performSelectorOnMainThread:@selector(changed) withObject:nil waitUntilDone:NO];
    }];
}

- (void)changed
{
    [NSTimeZone resetSystemTimeZone];
    [self schedule];
}

/* Every minute on the minute; every second when it shows the seconds or
   blinks its separators. */
- (void)schedule
{
    NSDictionary *prefs = clockPreferences();
    BOOL everySecond = ![prefs[@"IsAnalog"] boolValue] &&
        ([clockFormat() containsString:@"ss"] || [prefs[@"FlashDateSeparators"] boolValue]);
    NSTimeInterval period = everySecond ? 1 : 60;

    [self tick:nil];
    if (_timer != nil && period == _period)
        return;
    [_timer invalidate];
    _period = period;
    NSTimeInterval now = [[NSDate date] timeIntervalSince1970];
    NSDate *next = [NSDate dateWithTimeIntervalSince1970:(floor(now / period) + 1) * period];
    _timer = [[NSTimer alloc] initWithFireDate:next interval:period target:self
                                      selector:@selector(tick:) userInfo:nil repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:_timer forMode:NSRunLoopCommonModes];
}

/* Shows the time. */
- (void)tick:(NSTimer *)timer
{
    NSDate *now = [NSDate date];
    NSDateFormatter *full = [[NSDateFormatter alloc] init];

    [full setLocale:displayLocale()];
    NSDictionary *prefs = clockPreferences();

    [full setDateFormat:NSLocalizedString(@"EEEE d MMMM yyyy", @"the date in the clock's menu")];
    if ([prefs[@"IsAnalog"] boolValue]) {
        /* The face, and the time in words in the menu. */
        NSDateFormatter *time = [[NSDateFormatter alloc] init];
        [time setLocale:displayLocale()];
        [time setDateFormat:clockFormat()];
        [_item setTitle:@""];
        [_item setImage:clockFace(now)];
        [_dateItem setTitle:[NSString stringWithFormat:@"%@, %@", [full stringFromDate:now], [time stringFromDate:now]]];
        return;
    }
    NSString *title = [ClockExtra titleForDate:now];
    /* Blinking: the colons off every other second. */
    if ([prefs[@"FlashDateSeparators"] boolValue] && ((long)[now timeIntervalSince1970]) % 2 == 1)
        title = [title stringByReplacingOccurrencesOfString:@":" withString:@" "];
    [_item setImage:nil];
    [_item setTitle:title];
    [_dateItem setTitle:[full stringFromDate:now]];
}

/* System Preferences on its Date & Time pane. */
- (void)openPreferences:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:
        [NSURL fileURLWithPath:@"/System/Library/PreferencePanes/DateTime.prefPane"]];
}

@end
