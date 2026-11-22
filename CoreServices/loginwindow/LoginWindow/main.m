/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The login window (Atlas.md, M5): loginwindow runs it, as root, when
 * nobody is logged in, and reads its answer on standard output, one line:
 * "user NAME" once a user's password is verified (OpenDirectory), or
 * "restart" or "shutdown". Sleep it does itself.
 *
 * It covers the screen. As on the reference platform, by default it lists
 * the users (those with a UID from 500 that can log in), one tile each:
 * clicking one asks for its password, and with a single user that one is
 * asked at once; "Other…" asks for a name too. With SHOWFULLNAME set in
 * /Library/Preferences/com.apple.loginwindow, it asks for a name and a
 * password. A wrong password shakes the fields and clears the password.
 * Sleep, Restart and Shut Down are at the bottom, unless PowerOffDisabled
 * is set.
 *
 * With -lock NAME it is Lock Screen: above everything, only NAME's tile
 * and password, and "unlock" once it is given.
 */

#import <AppKit/AppKit.h>
#import <OpenDirectory/OpenDirectory.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <os/log.h>
#include <pwd.h>

#define LOGIN_PREFS	@"/Library/Preferences/com.apple.loginwindow.plist"
#define TILE_W		176
#define TILE_H		136
#define AVATAR		88

/* The Atlas desktop's color (Atlas.md, visual language). */
static NSColor *backgroundColor(void)
{
    return [NSColor colorWithCalibratedRed:99 / 255.0 green:130 / 255.0 blue:145 / 255.0 alpha:1];
}

static NSDictionary *loginPreferences(void)
{
    return [NSDictionary dictionaryWithContentsOfFile:LOGIN_PREFS] ?: @{};
}

/* ---- A user's tile ------------------------------------------------------ */

@interface UserTile : NSView
@property (copy) NSString *userName;       /* nil: "Other…" */
@property (copy) NSString *fullName;
@property (assign) BOOL selected;
@property (weak) id target;
@property (assign) SEL action;
@end

@implementation UserTile

- (NSString *)initials
{
    if (_userName == nil)
        return @"…";
    NSMutableString *s = [NSMutableString string];
    for (NSString *word in [_fullName componentsSeparatedByString:@" "])
        if ([word length] > 0 && [s length] < 2)
            [s appendString:[[word substringToIndex:1] uppercaseString]];
    return s;
}

- (void)drawRect:(NSRect)dirty
{
    NSRect bounds = [self bounds];
    NSRect circle = NSMakeRect((bounds.size.width - AVATAR) / 2, bounds.size.height - AVATAR - 4, AVATAR, AVATAR);

    if (_selected) {
        [[NSColor colorWithCalibratedWhite:1 alpha:0.25] setFill];
        [[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(circle, -4, -4)] fill];
    }
    [[NSColor colorWithCalibratedRed:0.22 green:0.31 blue:0.36 alpha:1] setFill];
    [[NSBezierPath bezierPathWithOvalInRect:circle] fill];

    NSDictionary *big = @{ NSFontAttributeName: [NSFont systemFontOfSize:32 weight:NSFontWeightMedium],
                           NSForegroundColorAttributeName: [NSColor whiteColor] };
    NSString *initials = [self initials];
    NSSize size = [initials sizeWithAttributes:big];
    [initials drawAtPoint:NSMakePoint(NSMidX(circle) - size.width / 2, NSMidY(circle) - size.height / 2)
           withAttributes:big];

    NSMutableParagraphStyle *centered = [[NSMutableParagraphStyle alloc] init];
    [centered setAlignment:NSTextAlignmentCenter];
    [centered setLineBreakMode:NSLineBreakByTruncatingTail];
    NSDictionary *label = @{ NSFontAttributeName: [NSFont systemFontOfSize:14 weight:NSFontWeightMedium],
                             NSForegroundColorAttributeName: [NSColor whiteColor],
                             NSParagraphStyleAttributeName: centered };
    NSString *name = _userName == nil ? NSLocalizedString(@"Other…", nil) : _fullName;
    /* Too long for the tile: shortened, with an ellipsis. */
    while ([name length] > 1 && [name sizeWithAttributes:label].width > bounds.size.width - 8)
        name = [[[name substringToIndex:[name length] - 2] stringByTrimmingCharactersInSet:
            [NSCharacterSet whitespaceCharacterSet]] stringByAppendingString:@"…"];
    [name drawInRect:NSMakeRect(0, 8, bounds.size.width, 20) withAttributes:label];
}

- (void)mouseDown:(NSEvent *)event
{
    [NSApp sendAction:_action to:_target from:self];
}

@end

/* ---- The window --------------------------------------------------------- */

/* A borderless window that still takes the keyboard. */
@interface LoginPanel : NSWindow
@end

@implementation LoginPanel
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
@end

@interface LoginWindow : NSObject <NSApplicationDelegate>
@end

@implementation LoginWindow {
    NSWindow *_window;
    NSView *_tiles;
    NSView *_form;
    NSTextField *_nameField;
    NSSecureTextField *_passwordField;
    NSButton *_back;
    NSArray<NSDictionary *> *_users;   /* name, fullName */
    NSString *_chosen;                 /* the user asked for, nil: the name field */
    BOOL _askName;                     /* SHOWFULLNAME */
    NSString *_lockUser;               /* -lock NAME: Lock Screen */
}

/* A user's tile entry, from its record. */
- (NSDictionary *)entryForUser:(NSString *)name
{
    ODNode *node = [ODNode nodeWithSession:[ODSession defaultSession] name:@"/Local/Default" error:NULL];
    ODRecord *r = [node recordWithRecordType:kODRecordTypeUsers name:name attributes:nil error:NULL];
    NSString *full = [[r valuesForAttribute:kODAttributeTypeFullName error:NULL] firstObject];

    return @{ @"name": name, @"fullName": [full length] ? full : name };
}

/* The users who can log in: UID from 500, a shell, not a service account. */
- (NSArray *)loginUsers
{
    NSMutableArray *users = [NSMutableArray array];
    NSError *error = nil;
    ODNode *node = [ODNode nodeWithSession:[ODSession defaultSession] name:@"/Local/Default" error:&error];
    ODQuery *query = [ODQuery queryWithNode:node forRecordTypes:kODRecordTypeUsers attribute:nil
        matchType:kODMatchAny queryValues:nil returnAttributes:kODAttributeTypeStandardOnly
        maximumResults:0 error:&error];

    for (ODRecord *r in [query resultsAllowingPartial:NO error:&error]) {
        NSString *uid = [[r valuesForAttribute:kODAttributeTypeUniqueID error:NULL] firstObject];
        NSString *shell = [[r valuesForAttribute:kODAttributeTypeUserShell error:NULL] firstObject];
        NSString *full = [[r valuesForAttribute:kODAttributeTypeFullName error:NULL] firstObject];
        if ([uid intValue] < 500 || [[r recordName] hasPrefix:@"_"] ||
            [shell hasSuffix:@"/false"] || [shell hasSuffix:@"/nologin"])
            continue;
        [users addObject:@{ @"name": [r recordName], @"fullName": [full length] ? full : [r recordName] }];
    }
    [users sortUsingComparator:^NSComparisonResult(NSDictionary *a, NSDictionary *b) {
        return [a[@"fullName"] localizedCaseInsensitiveCompare:b[@"fullName"]];
    }];
    os_log_info(OS_LOG_DEFAULT, "login window users: %{public}s",
                [[[users valueForKey:@"name"] componentsJoinedByString:@", "] UTF8String]);
    return users;
}

- (NSButton *)powerButton:(NSString *)title action:(SEL)action x:(CGFloat)x
{
    NSButton *b = [NSButton buttonWithTitle:NSLocalizedString(title, nil) target:self action:action];
    [b setBordered:NO];
    [b setAttributedTitle:[[NSAttributedString alloc] initWithString:NSLocalizedString(title, nil)
        attributes:@{ NSForegroundColorAttributeName: [NSColor whiteColor],
                      NSFontAttributeName: [NSFont systemFontOfSize:13] }]];
    [b setFrame:NSMakeRect(x, 40, 100, 24)];
    return b;
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    NSDictionary *prefs = loginPreferences();
    NSRect screen = [[NSScreen mainScreen] frame];

    NSArray *args = [[NSProcessInfo processInfo] arguments];
    NSUInteger lock = [args indexOfObject:@"-lock"];
    if (lock != NSNotFound && lock + 1 < [args count])
        _lockUser = args[lock + 1];

    _askName = _lockUser == nil && [prefs[@"SHOWFULLNAME"] boolValue];
    _users = _lockUser != nil ? @[ [self entryForUser:_lockUser] ] : [self loginUsers];

    _window = [[LoginPanel alloc] initWithContentRect:screen styleMask:NSWindowStyleMaskBorderless
        backing:NSBackingStoreBuffered defer:NO];
    [_window setBackgroundColor:backgroundColor()];
    /* Above the session's windows, the Dock and the menu bar when it locks. */
    [_window setLevel:_lockUser != nil ? NSScreenSaverWindowLevel : NSStatusWindowLevel];
    NSView *content = [_window contentView];

    /* Minkar's name, high up. */
    NSTextField *title = [NSTextField labelWithString:@"Minkar"];
    [title setFont:[NSFont systemFontOfSize:28 weight:NSFontWeightLight]];
    [title setTextColor:[NSColor whiteColor]];
    [title sizeToFit];
    [title setFrameOrigin:NSMakePoint(NSMidX(screen) - [title frame].size.width / 2, screen.size.height * 0.78)];
    [content addSubview:title];

    /* The tiles, centered a little above the middle. */
    _tiles = [[NSView alloc] initWithFrame:NSZeroRect];
    [content addSubview:_tiles];

    /* The fields, below. */
    _form = [[NSView alloc] initWithFrame:NSMakeRect(NSMidX(screen) - 120, screen.size.height * 0.5 - 150, 240, 100)];
    _nameField = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 64, 200, 24)];
    [[_nameField cell] setPlaceholderString:NSLocalizedString(@"Name", nil)];
    [_nameField setTarget:self];
    [_nameField setAction:@selector(nameEntered:)];
    _passwordField = [[NSSecureTextField alloc] initWithFrame:NSMakeRect(20, 32, 200, 24)];
    [[_passwordField cell] setPlaceholderString:NSLocalizedString(@"Enter Password", nil)];
    [_passwordField setTarget:self];
    [_passwordField setAction:@selector(logIn:)];
    _back = [NSButton buttonWithTitle:NSLocalizedString(@"Back", nil) target:self action:@selector(back:)];
    [_back setFrame:NSMakeRect(70, 0, 100, 24)];
    [_form addSubview:_nameField];
    [_form addSubview:_passwordField];
    [_form addSubview:_back];
    [content addSubview:_form];

    if (_lockUser == nil && ![prefs[@"PowerOffDisabled"] boolValue]) {
        CGFloat x = NSMidX(screen) - 160;
        [content addSubview:[self powerButton:@"Sleep" action:@selector(sleep:) x:x]];
        [content addSubview:[self powerButton:@"Restart" action:@selector(restart:) x:x + 110]];
        [content addSubview:[self powerButton:@"Shut Down" action:@selector(shutDown:) x:x + 220]];
    }

    if (_askName || [_users count] == 0)
        [self showForm:nil];
    else if ([_users count] == 1)
        [self showForm:_users[0][@"name"]];
    else
        [self showTiles];

    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    /* The field asked for first, once the window is key. */
    if (![_form isHidden])
        [_window makeFirstResponder:[_nameField isHidden] ? _passwordField : _nameField];
    os_log_info(OS_LOG_DEFAULT, "login window shown (%{public}s)",
                _lockUser != nil ? "lock" : _askName ? "name and password" : "list");
}

/* The tiles: every user (or only the one chosen), and "Other…". */
- (void)layoutTiles:(NSArray *)entries
{
    NSRect screen = [[NSScreen mainScreen] frame];
    CGFloat width = TILE_W * [entries count];

    [[_tiles subviews] makeObjectsPerformSelector:@selector(removeFromSuperview)];
    [_tiles setFrame:NSMakeRect(NSMidX(screen) - width / 2, screen.size.height * 0.5 - 40, width, TILE_H)];
    for (NSUInteger i = 0; i < [entries count]; i++) {
        UserTile *tile = [[UserTile alloc] initWithFrame:NSMakeRect(i * TILE_W, 0, TILE_W, TILE_H)];
        NSDictionary *u = entries[i];
        [tile setUserName:[u[@"name"] length] ? u[@"name"] : nil];
        [tile setFullName:u[@"fullName"]];
        [tile setSelected:[entries count] == 1];
        [tile setTarget:self];
        [tile setAction:@selector(tileClicked:)];
        [_tiles addSubview:tile];
    }
}

- (void)showTiles
{
    NSMutableArray *entries = [_users mutableCopy];
    [entries addObject:@{ @"name": @"", @"fullName": @"" }];
    [self layoutTiles:entries];
    [_form setHidden:YES];
    _chosen = nil;
}

/* The password for name, or (nil) a name and a password. */
- (void)showForm:(NSString *)name
{
    NSDictionary *user = nil;
    for (NSDictionary *u in _users)
        if ([u[@"name"] isEqualToString:name])
            user = u;
    _chosen = name;
    [self layoutTiles:@[ user ?: @{ @"name": @"", @"fullName": @"" } ]];
    [_nameField setHidden:name != nil];
    [_nameField setStringValue:@""];
    [_passwordField setStringValue:@""];
    /* Back to the list, when there is one to go back to. */
    [_back setHidden:_askName || [_users count] < 2];
    [_form setHidden:NO];
    [_window makeFirstResponder:name != nil ? _passwordField : _nameField];
}

- (void)tileClicked:(UserTile *)tile
{
    if (_chosen != nil || ![_form isHidden])
        return;
    [self showForm:[tile userName]];
    if ([tile userName] == nil)
        [_window makeFirstResponder:_nameField];
}

- (void)back:(id)sender
{
    [self showTiles];
}

- (void)nameEntered:(id)sender
{
    [_window makeFirstResponder:_passwordField];
}

/* The fields shaken, as the reference does on a wrong password. */
- (void)shake
{
    NSRect frame = [_form frame];
    CGFloat offsets[] = { -12, 12, -9, 9, -5, 5, 0 };

    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        [_form setFrameOrigin:NSMakePoint(frame.origin.x + offsets[i], frame.origin.y)];
        [_window displayIfNeeded];
        [[_window contentView] display];
        usleep(30000);
    }
    [_form setFrame:frame];
}

- (void)logIn:(id)sender
{
    NSString *name = _chosen ?: [[_nameField stringValue] stringByTrimmingCharactersInSet:
        [NSCharacterSet whitespaceCharacterSet]];
    NSString *password = [_passwordField stringValue];
    NSError *error = nil;
    ODNode *node = [ODNode nodeWithSession:[ODSession defaultSession] name:@"/Local/Default" error:&error];
    ODRecord *record = [name length] ? [node recordWithRecordType:kODRecordTypeUsers name:name
        attributes:nil error:&error] : nil;

    if (record != nil && [record verifyPassword:password error:&error]) {
        os_log_info(OS_LOG_DEFAULT, "login window: verified %{public}s", [[record recordName] UTF8String]);
        if (_lockUser != nil)
            printf("unlock\n");
        else
            printf("user %s\n", [[record recordName] UTF8String]);
        fflush(stdout);
        exit(0);
    }
    os_log_info(OS_LOG_DEFAULT, "login window: wrong password for %{public}s (%ld)",
                [name UTF8String], (long)[error code]);
    [_passwordField setStringValue:@""];
    [self shake];
    [_window makeFirstResponder:_passwordField];
}

- (void)sleep:(id)sender
{
    io_connect_t pm = IOPMFindPowerManagement(MACH_PORT_NULL);
    if (pm != MACH_PORT_NULL) {
        IOPMSleepSystem(pm);
        IOServiceClose(pm);
    }
}

- (void)restart:(id)sender
{
    printf("restart\n");
    fflush(stdout);
    exit(0);
}

- (void)shutDown:(id)sender
{
    printf("shutdown\n");
    fflush(stdout);
    exit(0);
}

@end

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        LoginWindow *delegate = [[LoginWindow alloc] init];
        [app setDelegate:delegate];
        [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
        [app run];
    }
    return 0;
}
