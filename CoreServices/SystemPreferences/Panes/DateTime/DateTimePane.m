/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Date & Time pane (ROADMAP Phase 5, item 6): the date and time now;
 * the time zone, by region and city (the zones in
 * /var/db/timezone/zoneinfo, set as /etc/localtime points, as systemsetup
 * -settimezone does); and the clock's 24-hour format (the clock Menu
 * Extra's domain, org.minkar.menuextra.clock). Both are posted (notify(3):
 * org.minkar.timezone, org.minkar.menuextra.clock) so the clock follows at
 * once. Setting the time from the network waits for a time daemon.
 * Atlas's provisional look.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#include <notify.h>
#include <os/log.h>
#include <unistd.h>

#define ZONEINFO	@"/var/db/timezone/zoneinfo"
#define LOCALTIME	"/etc/localtime"
#define kClockDomain	CFSTR("org.minkar.menuextra.clock")

@interface DateTimePane : NSPreferencePane
@end

static const CGFloat kWidth = 668, kHeight = 280;

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

/* The zones by region: "America" -> ("Argentina/Buenos_Aires", ...). */
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
    NSTimer *_timer;
    NSDictionary<NSString *, NSArray<NSString *> *> *_zones;
    NSArray<NSString *> *_regions;
}

- (NSString *)text:(NSString *)english
{
    return [[self bundle] localizedStringForKey:english value:english table:nil];
}

- (NSView *)loadMainView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];

    _now = label(@"", NSMakeRect(20, 220, kWidth - 40, 30), 20, NSCenterTextAlignment);
    [view addSubview:_now];

    _automatic = [[NSButton alloc] initWithFrame:NSMakeRect(170, 186, 360, 22)];
    [_automatic setButtonType:NSSwitchButton];
    [_automatic setTitle:[self text:@"Set date and time automatically"]];
    [_automatic setEnabled:NO];    /* with a time daemon */
    [view addSubview:_automatic];

    [view addSubview:label([self text:@"Time zone:"], NSMakeRect(20, 140, 140, 20), 13, NSRightTextAlignment)];
    _zones = zonesByRegion();
    _regions = [[_zones allKeys] sortedArrayUsingSelector:@selector(compare:)];
    _region = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(170, 136, 170, 26) pullsDown:NO];
    for (NSString *r in _regions)
        [_region addItemWithTitle:[r stringByReplacingOccurrencesOfString:@"_" withString:@" "]];
    [_region setTarget:self];
    [_region setAction:@selector(chooseRegion:)];
    [view addSubview:_region];
    _city = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(350, 136, 240, 26) pullsDown:NO];
    [_city setTarget:self];
    [_city setAction:@selector(chooseCity:)];
    [view addSubview:_city];
    _zoneLabel = label(@"", NSMakeRect(170, 110, 420, 18), 11, NSLeftTextAlignment);
    [view addSubview:_zoneLabel];

    _hours24 = [[NSButton alloc] initWithFrame:NSMakeRect(170, 60, 360, 22)];
    [_hours24 setButtonType:NSSwitchButton];
    [_hours24 setTitle:[self text:@"Use a 24-hour clock"]];
    [_hours24 setTarget:self];
    [_hours24 setAction:@selector(chooseHours:)];
    [view addSubview:_hours24];

    [self showZone:currentZone()];
    CFPreferencesAppSynchronize(kClockDomain);
    CFStringRef format = CFPreferencesCopyAppValue(CFSTR("DateFormat"), kClockDomain);
    BOOL twelve = format != NULL && CFGetTypeID(format) == CFStringGetTypeID() &&
        [(__bridge NSString *)format rangeOfString:@"h"].location != NSNotFound;
    [_hours24 setState:twelve ? NSOffState : NSOnState];
    if (format != NULL)
        CFRelease(format);

    [self tick:nil];
    [self setMainView:view];
    return view;
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

/* The menus on a zone ("America/Argentina/Buenos_Aires"). */
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
}

- (void)chooseRegion:(id)sender
{
    [self fillCities];
    [self chooseCity:sender];
}

- (void)chooseCity:(id)sender
{
    NSInteger r = [_region indexOfSelectedItem], c = [_city indexOfSelectedItem];

    if (r < 0 || c < 0)
        return;
    NSString *region = _regions[(NSUInteger)r];
    NSString *zone = [region stringByAppendingPathComponent:_zones[region][(NSUInteger)c]];
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

- (void)chooseHours:(id)sender
{
    CFStringRef format = [_hours24 state] == NSOnState ? CFSTR("EEE d MMM  HH:mm") : CFSTR("EEE d MMM  h:mm a");

    CFPreferencesSetAppValue(CFSTR("DateFormat"), format, kClockDomain);
    CFPreferencesAppSynchronize(kClockDomain);
    notify_post("org.minkar.menuextra.clock");
    os_log_info(OS_LOG_DEFAULT, "clock 24-hour=%d", [_hours24 state] == NSOnState);
    [self tick:nil];
}

@end
