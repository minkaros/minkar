/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Date & Time pane (ROADMAP Phase 5, item 6): the date and time now;
 * the time zone, by region and city (the zones in
 * /var/db/timezone/zoneinfo, set as /etc/localtime points, as systemsetup
 * -settimezone does); and the clock in the menu bar, in three tabs as on
 * the reference platform. The Clock tab has the reference's settings and
 * keys (the clock Menu Extra's domain, org.minkar.menuextra.clock):
 * whether it is in the menu bar (SystemUIServer's menuExtras list),
 * digital or analog (IsAnalog), and its DateFormat made of the options
 * (day of the week, date, seconds, 24 hours, AM/PM), with
 * FlashDateSeparators. Changes are posted (notify(3): org.minkar.timezone,
 * org.minkar.menuextra.clock, org.minkar.systemuiserver.extras) so the bar
 * follows at once. Setting the time from the network waits for a time
 * daemon; announcing the time, for speech synthesis.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#include <notify.h>
#include <os/log.h>
#include <unistd.h>
#import "../MenuExtraList.h"

#define ZONEINFO	@"/var/db/timezone/zoneinfo"
#define LOCALTIME	"/etc/localtime"
#define kClockDomain	CFSTR("org.minkar.menuextra.clock")


@interface DateTimePane : NSPreferencePane
@end

static const CGFloat kWidth = 668, kHeight = 330;

static NSTextField *label(NSString *text, NSRect frame, CGFloat size, NSTextAlignment alignment)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
    [field setStringValue:text];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setBordered:NO];
    [field setDrawsBackground:NO];
    [field setAlignment:alignment];
    [field setFont:[NSFont systemFontOfSize:size]];
    return field;
}

/* The zones by area: "Area" -> ("Location", "Region/Location", ...). */
static NSDictionary<NSString *, NSArray<NSString *> *> *zonesByRegion(void)
{
    NSMutableDictionary *regions = [NSMutableDictionary dictionary];
    NSFileManager *fm = [NSFileManager defaultManager];

    for (NSString *region in [fm contentsOfDirectoryAtPath:ZONEINFO error:NULL]) {
        BOOL dir;
        if ([region hasPrefix:@"."] || [region rangeOfString:@"."].location != NSNotFound ||
            [region isEqualToString:@"Etc"] || [region isEqualToString:@"SystemV"] ||
            [region isEqualToString:@"US"] || [region characterAtIndex:0] < 'A' || [region characterAtIndex:0] > 'Z')
            continue;
        NSString *path = [ZONEINFO stringByAppendingPathComponent:region];
        if (![fm fileExistsAtPath:path isDirectory:&dir] || !dir)
            continue;
        NSMutableArray *cities = [NSMutableArray array];
        for (NSString *sub in [fm subpathsAtPath:path]) {
            BOOL subdir;
            if ([[sub lastPathComponent] hasPrefix:@"."] ||
                ![fm fileExistsAtPath:[path stringByAppendingPathComponent:sub] isDirectory:&subdir] || subdir)
                continue;
            [cities addObject:sub];
        }
        if ([cities count] > 0)
            regions[region] = [cities sortedArrayUsingSelector:@selector(compare:)];
    }
    /* UTC: a region of its own, without cities. */
    if ([fm fileExistsAtPath:[ZONEINFO stringByAppendingPathComponent:@"UTC"]])
        regions[@"UTC"] = @[];
    return regions;
}

static NSString *currentZone(void)
{
    char link[PATH_MAX];
    ssize_t n = readlink(LOCALTIME, link, sizeof(link) - 1);

    if (n < 0)
        return @"GMT";
    link[n] = '\0';
    NSString *path = @(link);
    NSRange r = [path rangeOfString:@"zoneinfo/"];
    return r.location != NSNotFound ? [path substringFromIndex:NSMaxRange(r)] : path;
}

@implementation DateTimePane {
    NSTextField *_now, *_zoneLabel;
    NSPopUpButton *_region, *_city;
    NSButton *_automatic, *_hours24;
    NSButton *_showClock, *_digital, *_analog, *_seconds, *_flash, *_ampm, *_weekday, *_date;
    NSTimer *_timer;
    NSDictionary<NSString *, NSArray<NSString *> *> *_zones;
    NSArray<NSString *> *_regions;
}

- (NSString *)text:(NSString *)english
{
    return [[self bundle] localizedStringForKey:english value:english table:nil];
}

static NSButton *check(NSString *title, NSRect frame, id target, SEL action, NSButtonType type)
{
    NSButton *b = [[NSButton alloc] initWithFrame:frame];
    [b setButtonType:type];
    [b setTitle:title];
    [b setTarget:target];
    [b setAction:action];
    return b;
}


- (NSView *)loadMainView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];
    NSTabView *tabs = [[NSTabView alloc] initWithFrame:NSMakeRect(16, 10, kWidth - 32, kHeight - 20)];
    NSRect inside = [tabs contentRect];
    CGFloat top = NSHeight(inside);
    NSTabViewItem *dateTab = [[NSTabViewItem alloc] initWithIdentifier:@"date"];
    NSTabViewItem *zoneTab = [[NSTabViewItem alloc] initWithIdentifier:@"zone"];
    NSTabViewItem *clockTab = [[NSTabViewItem alloc] initWithIdentifier:@"clock"];
    NSView *date = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, NSWidth(inside), top)];
    NSView *zone = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, NSWidth(inside), top)];
    NSView *clock = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, NSWidth(inside), top)];

    [dateTab setLabel:[self text:@"Date & Time"]];
    [zoneTab setLabel:[self text:@"Time Zone"]];
    [clockTab setLabel:[self text:@"Clock"]];

    /* Date & Time. */
    _now = label(@"", NSMakeRect(10, top - 70, NSWidth(inside) - 20, 30), 20, NSCenterTextAlignment);
    [date addSubview:_now];
    _automatic = check([self text:@"Set date and time automatically"], NSMakeRect(150, top - 110, 360, 22),
                       nil, NULL, NSSwitchButton);
    [_automatic setEnabled:NO];    /* with a time daemon */
    [date addSubview:_automatic];

    /* Time Zone. */
    [zone addSubview:label([self text:@"Time zone:"], NSMakeRect(10, top - 60, 140, 20), 13, NSRightTextAlignment)];
    _zones = zonesByRegion();
    _regions = [[_zones allKeys] sortedArrayUsingSelector:@selector(compare:)];
    _region = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(160, top - 64, 170, 26) pullsDown:NO];
    for (NSString *r in _regions)
        [_region addItemWithTitle:[r stringByReplacingOccurrencesOfString:@"_" withString:@" "]];
    [_region setTarget:self];
    [_region setAction:@selector(chooseRegion:)];
    [zone addSubview:_region];
    _city = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(340, top - 64, 240, 26) pullsDown:NO];
    [_city setTarget:self];
    [_city setAction:@selector(chooseCity:)];
    [zone addSubview:_city];
    _zoneLabel = label(@"", NSMakeRect(160, top - 90, 420, 18), 11, NSLeftTextAlignment);
    [zone addSubview:_zoneLabel];

    /* Clock. */
    CGFloat y = top - 44;
    _showClock = check([self text:@"Show date and time in menu bar"], NSMakeRect(40, y, 400, 22),
                       self, @selector(clockChanged:), NSSwitchButton);
    [clock addSubview:_showClock];
    y -= 32;
    [clock addSubview:label([self text:@"Time options:"], NSMakeRect(10, y + 2, 150, 20), 13, NSRightTextAlignment)];
    _digital = check([self text:@"Digital"], NSMakeRect(170, y, 100, 22), self, @selector(chooseStyle:), NSRadioButton);
    _analog = check([self text:@"Analog"], NSMakeRect(280, y, 100, 22), self, @selector(chooseStyle:), NSRadioButton);
    [clock addSubview:_digital];
    [clock addSubview:_analog];
    y -= 26;
    _seconds = check([self text:@"Display the time with seconds"], NSMakeRect(170, y, 300, 22), self, @selector(clockChanged:), NSSwitchButton);
    _flash = check([self text:@"Flash the time separators"], NSMakeRect(170, y - 24, 300, 22), self, @selector(clockChanged:), NSSwitchButton);
    _hours24 = check([self text:@"Use a 24-hour clock"], NSMakeRect(170, y - 48, 300, 22), self, @selector(clockChanged:), NSSwitchButton);
    _ampm = check([self text:@"Show AM/PM"], NSMakeRect(190, y - 72, 280, 22), self, @selector(clockChanged:), NSSwitchButton);
    for (NSButton *b in @[ _seconds, _flash, _hours24, _ampm ])
        [clock addSubview:b];
    y -= 108;
    [clock addSubview:label([self text:@"Date options:"], NSMakeRect(10, y + 2, 150, 20), 13, NSRightTextAlignment)];
    _weekday = check([self text:@"Show the day of the week"], NSMakeRect(170, y, 300, 22), self, @selector(clockChanged:), NSSwitchButton);
    _date = check([self text:@"Show date"], NSMakeRect(170, y - 24, 300, 22), self, @selector(clockChanged:), NSSwitchButton);
    [clock addSubview:_weekday];
    [clock addSubview:_date];

    [dateTab setView:date];
    [zoneTab setView:zone];
    [clockTab setView:clock];
    [tabs addTabViewItem:dateTab];
    [tabs addTabViewItem:zoneTab];
    [tabs addTabViewItem:clockTab];
    [view addSubview:tabs];

    [self showZone:currentZone()];
    [self showClockSettings];
    [self tick:nil];
    [self setMainView:view];
    return view;
}

/* The Clock tab from the domain: the format read back into its options. */
- (void)showClockSettings
{
    CFPreferencesAppSynchronize(kClockDomain);
    CFPropertyListRef value = CFPreferencesCopyAppValue(CFSTR("DateFormat"), kClockDomain);
    NSString *format = value != NULL && CFGetTypeID(value) == CFStringGetTypeID() ?
        [(__bridge NSString *)value copy] : [self text:@"EEE d MMM  HH:mm"];
    if (value != NULL)
        CFRelease(value);

    [_showClock setState:MenuExtraIsShown(@"Clock") ? NSOnState : NSOffState];
    BOOL analog = CFPreferencesGetAppBooleanValue(CFSTR("IsAnalog"), kClockDomain, NULL);
    [_digital setState:analog ? NSOffState : NSOnState];
    [_analog setState:analog ? NSOnState : NSOffState];
    [_seconds setState:[format containsString:@"ss"] ? NSOnState : NSOffState];
    [_flash setState:CFPreferencesGetAppBooleanValue(CFSTR("FlashDateSeparators"), kClockDomain, NULL) ? NSOnState : NSOffState];
    [_hours24 setState:[format containsString:@"H"] ? NSOnState : NSOffState];
    [_ampm setState:[format containsString:@"a"] || [format containsString:@"H"] ? NSOnState : NSOffState];
    [_weekday setState:[format containsString:@"EEE"] ? NSOnState : NSOffState];
    [_date setState:[format containsString:@"MMM"] ? NSOnState : NSOffState];
    [self enableClockOptions];
}

- (void)enableClockOptions
{
    BOOL shown = [_showClock state] == NSOnState, digital = [_digital state] == NSOnState;

    for (NSButton *b in @[ _digital, _analog ])
        [b setEnabled:shown];
    for (NSButton *b in @[ _seconds, _flash, _hours24, _weekday, _date ])
        [b setEnabled:shown && digital];
    [_ampm setEnabled:shown && digital && [_hours24 state] == NSOffState];
}

- (void)chooseStyle:(NSButton *)sender
{
    [_digital setState:sender == _digital ? NSOnState : NSOffState];
    [_analog setState:sender == _analog ? NSOnState : NSOffState];
    [self clockChanged:sender];
}

/* The options written as the reference writes them, then the bar told. */
- (void)clockChanged:(id)sender
{
    BOOL hours24 = [_hours24 state] == NSOnState;
    NSMutableString *format = [NSMutableString string];

    if ([_weekday state] == NSOnState)
        [format appendString:@"EEE "];
    if ([_date state] == NSOnState)
        [format appendFormat:@"%@  ", [self text:@"MMM d"]];
    [format appendString:hours24 ? @"HH:mm" : @"h:mm"];
    if ([_seconds state] == NSOnState)
        [format appendString:@":ss"];
    if (!hours24 && [_ampm state] == NSOnState)
        [format appendString:@" a"];

    CFPreferencesSetAppValue(CFSTR("DateFormat"), (__bridge CFStringRef)format, kClockDomain);
    CFPreferencesSetAppValue(CFSTR("IsAnalog"), [_analog state] == NSOnState ? kCFBooleanTrue : kCFBooleanFalse, kClockDomain);
    CFPreferencesSetAppValue(CFSTR("FlashDateSeparators"), [_flash state] == NSOnState ? kCFBooleanTrue : kCFBooleanFalse, kClockDomain);
    CFPreferencesAppSynchronize(kClockDomain);
    notify_post("org.minkar.menuextra.clock");

    /* In the menu bar or not: SystemUIServer's list. */
    if (sender == _showClock)
        MenuExtraSetShown(@"Clock", [_showClock state] == NSOnState);
    [self enableClockOptions];
    os_log_info(OS_LOG_DEFAULT, "clock format=%{public}s analog=%d flash=%d shown=%d", [format UTF8String],
                [_analog state] == NSOnState, [_flash state] == NSOnState, [_showClock state] == NSOnState);
    [self tick:nil];
}

- (void)didSelect
{
    _timer = [NSTimer scheduledTimerWithTimeInterval:1 target:self selector:@selector(tick:) userInfo:nil repeats:YES];
}

- (void)didUnselect
{
    [_timer invalidate];
    _timer = nil;
}

- (void)tick:(NSTimer *)timer
{
    NSDateFormatter *f = [[NSDateFormatter alloc] init];
    [f setLocale:[NSLocale localeWithLocaleIdentifier:[[[self bundle] preferredLocalizations] firstObject]]];
    [f setDateFormat:[_hours24 state] == NSOnState ? [self text:@"EEEE d MMMM yyyy, HH:mm:ss"]
                                                   : [self text:@"EEEE d MMMM yyyy, h:mm:ss a"]];
    [_now setStringValue:[f stringFromDate:[NSDate date]]];
}

/* The menus on a zone (an area, then a location, which may have its own
   part: "Area/Location" or "Area/Region/Location"). */
- (void)showZone:(NSString *)zone
{
    NSRange slash = [zone rangeOfString:@"/"];
    NSString *region = slash.location != NSNotFound ? [zone substringToIndex:slash.location] : zone;
    NSString *city = slash.location != NSNotFound ? [zone substringFromIndex:NSMaxRange(slash)] : nil;
    NSUInteger index = [_regions indexOfObject:region];

    if (index != NSNotFound)
        [_region selectItemAtIndex:(NSInteger)index];
    [self fillCities];
    NSUInteger c = city ? [_zones[region] indexOfObject:city] : NSNotFound;
    if (c != NSNotFound)
        [_city selectItemAtIndex:(NSInteger)c];
    [_zoneLabel setStringValue:[NSString stringWithFormat:@"%@ (%@)", zone,
                                [[NSTimeZone timeZoneWithName:zone] abbreviation] ?: @""]];
}

- (void)fillCities
{
    NSInteger r = [_region indexOfSelectedItem];

    [_city removeAllItems];
    if (r < 0 || (NSUInteger)r >= [_regions count])
        return;
    for (NSString *c in _zones[_regions[(NSUInteger)r]])
        [_city addItemWithTitle:[c stringByReplacingOccurrencesOfString:@"_" withString:@" "]];
    [_city setHidden:[_city numberOfItems] == 0];
}

- (void)chooseRegion:(id)sender
{
    [self fillCities];
    [self chooseCity:sender];
}

- (void)chooseCity:(id)sender
{
    NSInteger r = [_region indexOfSelectedItem], c = [_city indexOfSelectedItem];

    if (r < 0)
        return;
    NSString *region = _regions[(NSUInteger)r];
    NSArray<NSString *> *cities = _zones[region];
    if (c < 0 && [cities count] > 0)
        return;
    /* A region without cities (UTC) is the zone itself. */
    NSString *zone = [cities count] > 0 ? [region stringByAppendingPathComponent:cities[(NSUInteger)c]] : region;
    NSString *target = [ZONEINFO stringByAppendingPathComponent:zone];

    /* As systemsetup -settimezone: /etc/localtime points at the zone. */
    unlink(LOCALTIME ".new");
    if (symlink([target fileSystemRepresentation], LOCALTIME ".new") != 0 ||
        rename(LOCALTIME ".new", LOCALTIME) != 0) {
        NSBeep();
        [self showZone:currentZone()];
        return;
    }
    [NSTimeZone resetSystemTimeZone];
    notify_post("org.minkar.timezone");
    os_log_info(OS_LOG_DEFAULT, "timezone %{public}s", [zone UTF8String]);
    [self showZone:zone];
    [self tick:nil];
}

@end
