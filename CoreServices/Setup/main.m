/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Setup (I5; the maintainer's choices, 23 Nov 2026): a system the Installer
 * put on a disk starts with no account; loginwindow runs this, as root,
 * before anyone logs in (no /var/db/.AppleSetupDone), and reads its answer
 * on standard output as it reads the login window's: "user NAME".
 *
 * It asks, a step at a time: the language (English or Spanish: the
 * system's, .GlobalPreferences' AppleLanguages); the keyboard layout (the
 * login window's and the new user's, com.apple.HIToolbox); the time zone
 * (systemsetup -settimezone); says how the network is (it sets itself up
 * by DHCP; System Preferences changes it); the computer's name (scutil);
 * and the first administrator (sysadminctl -addUser ... -admin: its full
 * name, account name and password, twice). Then it marks the system set up
 * and says whom to log in.
 */

#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#import <SystemConfiguration/SystemConfiguration.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <os/log.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

#define SETUP_DONE	@"/private/var/db/.AppleSetupDone"
#define ZONEINFO	@"/var/db/timezone/zoneinfo"

static NSDictionary *strings;	/* the language chosen's, English's when nil */

static NSString *text(NSString *english)
{
    return strings[english] ?: english;
}

static NSTextField *label(NSString *string, NSRect frame, CGFloat size, BOOL bold)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
    [field setStringValue:string];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setBordered:NO];
    [field setDrawsBackground:NO];
    [field setFont:bold ? [NSFont boldSystemFontOfSize:size] : [NSFont systemFontOfSize:size]];
    return field;
}

static NSTextField *entry(NSRect frame, BOOL secure)
{
    NSTextField *field = secure ? [[NSSecureTextField alloc] initWithFrame:frame] : [[NSTextField alloc] initWithFrame:frame];
    [field setEditable:YES];
    [field setBezeled:YES];
    return field;
}

/* A tool, quietly; its status. */
static int run(NSString *path, NSArray<NSString *> *arguments)
{
    NSMutableArray *all = [NSMutableArray arrayWithObject:path];
    [all addObjectsFromArray:arguments];
    char **argv = calloc([all count] + 1, sizeof(char *));
    for (NSUInteger i = 0; i < [all count]; i++)
        argv[i] = strdup([all[i] UTF8String]);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    pid_t pid;
    int status = -1, err = posix_spawn(&pid, argv[0], &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    for (NSUInteger i = 0; i < [all count]; i++)
        free(argv[i]);
    free(argv);
    if (err != 0)
        return -1;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
        ;
    os_log(OS_LOG_DEFAULT, "setup: %{public}s %{public}s: %d", [path UTF8String],
           [[arguments firstObject] UTF8String], WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* The zones by area, as the Date & Time pane lists them. */
static NSDictionary<NSString *, NSArray<NSString *> *> *zonesByRegion(void)
{
    NSMutableDictionary *regions = [NSMutableDictionary dictionary];
    NSFileManager *fm = [NSFileManager defaultManager];
    for (NSString *region in [fm contentsOfDirectoryAtPath:ZONEINFO error:NULL]) {
        BOOL dir;
        if ([region hasPrefix:@"."] || [region rangeOfString:@"."].location != NSNotFound ||
            [region isEqualToString:@"Etc"] || [region isEqualToString:@"SystemV"] || [region isEqualToString:@"US"] ||
            [region characterAtIndex:0] < 'A' || [region characterAtIndex:0] > 'Z')
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
        if ([cities count])
            regions[region] = [cities sortedArrayUsingSelector:@selector(compare:)];
    }
    if ([fm fileExistsAtPath:[ZONEINFO stringByAppendingPathComponent:@"UTC"]])
        regions[@"UTC"] = @[];
    return regions;
}

/* An account name from a full name: lowercase letters and digits. */
static NSString *accountName(NSString *full)
{
    NSString *plain = [full stringByApplyingTransform:NSStringTransformStripDiacritics reverse:NO] ?: full;
    NSMutableString *name = [NSMutableString string];
    for (NSUInteger i = 0; i < [plain length] && [name length] < 20; i++) {
        unichar c = [plain characterAtIndex:i];
        if (c < 128 && isalnum(c))
            [name appendFormat:@"%c", (char)tolower(c)];
    }
    return name;
}

/* The network now: the first interface up with an IPv4 address. */
static NSString *networkState(void)
{
    struct ifaddrs *list, *a;
    NSString *state = nil;
    if (getifaddrs(&list) != 0)
        return nil;
    for (a = list; a != NULL && state == nil; a = a->ifa_next) {
        if (a->ifa_addr == NULL || a->ifa_addr->sa_family != AF_INET || (a->ifa_flags & IFF_LOOPBACK) ||
            !(a->ifa_flags & IFF_UP))
            continue;
        char addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &((struct sockaddr_in *)a->ifa_addr)->sin_addr, addr, sizeof(addr));
        state = [NSString stringWithFormat:@"%s, %s", a->ifa_name, addr];
    }
    freeifaddrs(list);
    return state;
}

@interface Setup : NSObject <NSApplicationDelegate, NSTextFieldDelegate>
@end

@implementation Setup {
    NSWindow *_window;
    NSView *_page;
    NSButton *_back, *_continue;
    NSInteger _step;
    NSString *_language, *_layout, *_zone, *_computer;
    NSPopUpButton *_popup, *_cityPopup;
    NSArray *_layouts;
    NSDictionary<NSString *, NSArray<NSString *> *> *_zones;
    NSArray<NSString *> *_regions;
    NSTextField *_fullName, *_account, *_password, *_verify, *_computerField, *_problem;
    BOOL _accountEdited;
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    _language = @"en";
    _zones = zonesByRegion();
    _regions = [[_zones allKeys] sortedArrayUsingSelector:@selector(compare:)];
    _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 640, 460) styleMask:NSTitledWindowMask
                                            backing:NSBackingStoreBuffered defer:NO];
    NSView *content = [_window contentView];
    _page = [[NSView alloc] initWithFrame:NSMakeRect(0, 60, 640, 400)];
    [content addSubview:_page];
    [content addSubview:[[NSBox alloc] initWithFrame:NSMakeRect(20, 58, 600, 1)]];
    _back = [[NSButton alloc] initWithFrame:NSMakeRect(400, 14, 100, 30)];
    [_back setBezelStyle:NSRoundedBezelStyle];
    [_back setTarget:self];
    [_back setAction:@selector(goBack:)];
    [content addSubview:_back];
    _continue = [[NSButton alloc] initWithFrame:NSMakeRect(510, 14, 110, 30)];
    [_continue setBezelStyle:NSRoundedBezelStyle];
    [_continue setKeyEquivalent:@"\r"];
    [_continue setTarget:self];
    [_continue setAction:@selector(goOn:)];
    [content addSubview:_continue];
    [self show:0];
    [_window center];
    /* Always in the middle of the screen, whatever its mode: also when
       the display changes after the window is shown (the display driver
       moves from the firmware's mode to the display's own). */
    [[NSNotificationCenter defaultCenter] addObserver:_window selector:@selector(center)
        name:NSApplicationDidChangeScreenParametersNotification object:nil];
    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

/* ---- Steps: 0 language, 1 keyboard, 2 time zone, 3 network, 4 name, 5 administrator ---- */

- (void)show:(NSInteger)step
{
    _step = step;
    [[_page subviews] makeObjectsPerformSelector:@selector(removeFromSuperview)];
    [_window setTitle:text(@"Set Up Minkar")];
    [_back setTitle:text(@"Go Back")];
    [_back setHidden:step == 0];
    [_continue setTitle:step == 5 ? text(@"Finish") : text(@"Continue")];
    [_continue setEnabled:YES];
    switch (step) {
    case 0: [self showLanguage]; break;
    case 1: [self showKeyboard]; break;
    case 2: [self showZone]; break;
    case 3: [self showNetwork]; break;
    case 4: [self showComputer]; break;
    case 5: [self showAdministrator]; break;
    }
}

- (void)title:(NSString *)title body:(NSString *)body
{
    [_page addSubview:label(title, NSMakeRect(40, 330, 560, 30), 20, YES)];
    if (body != nil)
        [_page addSubview:label(body, NSMakeRect(40, 270, 560, 52), 13, NO)];
}

- (void)showLanguage
{
    [self title:text(@"Welcome") body:text(@"Choose the language Minkar uses.")];
    _popup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(40, 220, 260, 26) pullsDown:NO];
    [_popup addItemsWithTitles:@[ @"English", @"Español" ]];
    [_popup selectItemAtIndex:[_language isEqual:@"es"] ? 1 : 0];
    [_popup setTarget:self];
    [_popup setAction:@selector(chooseLanguage:)];
    [_page addSubview:_popup];
}

- (void)chooseLanguage:(id)sender
{
    _language = [_popup indexOfSelectedItem] == 1 ? @"es" : @"en";
    NSString *path = [[[NSBundle mainBundle] resourcePath]
        stringByAppendingPathComponent:[_language stringByAppendingString:@".lproj/Localizable.strings"]];
    NSString *contents = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:NULL];
    strings = [_language isEqual:@"en"] || contents == nil ? nil : [contents propertyListFromStringsFileFormat];
    [self show:0];
}

- (void)showKeyboard
{
    [self title:text(@"Keyboard") body:text(@"Choose the layout of your keyboard.")];
    NSMutableArray *layouts = [NSMutableArray array];
    NSArray *all = CFBridgingRelease(TISCreateInputSourceList(NULL, true));
    for (id s in all) {
        CFStringRef type = TISGetInputSourceProperty((__bridge TISInputSourceRef)s, kTISPropertyInputSourceType);
        if (type && CFEqual(type, kTISTypeKeyboardLayout))
            [layouts addObject:s];
    }
    [layouts sortUsingComparator:^NSComparisonResult(id a, id b) {
        NSString *x = (__bridge NSString *)TISGetInputSourceProperty((__bridge TISInputSourceRef)a, kTISPropertyLocalizedName);
        NSString *y = (__bridge NSString *)TISGetInputSourceProperty((__bridge TISInputSourceRef)b, kTISPropertyLocalizedName);
        return [x localizedCaseInsensitiveCompare:y];
    }];
    _layouts = layouts;
    _popup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(40, 220, 320, 26) pullsDown:NO];
    NSInteger chosen = 0, preferred = -1;
    for (NSUInteger i = 0; i < [layouts count]; i++) {
        TISInputSourceRef s = (__bridge TISInputSourceRef)layouts[i];
        NSString *ident = (__bridge NSString *)TISGetInputSourceProperty(s, kTISPropertyInputSourceID);
        [_popup addItemWithTitle:(__bridge NSString *)TISGetInputSourceProperty(s, kTISPropertyLocalizedName)];
        /* The one chosen before, else the language's usual one. */
        if ([ident isEqual:_layout])
            chosen = (NSInteger)i;
        else if (_layout == nil && preferred < 0 &&
                 [(__bridge NSString *)TISGetInputSourceProperty(s, kTISPropertyLocalizedName)
                     hasPrefix:[_language isEqual:@"es"] ? @"Spanish" : @"U.S."])
            chosen = preferred = (NSInteger)i;
    }
    if ([layouts count])
        [_popup selectItemAtIndex:chosen];
    [_page addSubview:_popup];
    NSTextField *try = entry(NSMakeRect(40, 170, 320, 24), NO);
    [[try cell] setPlaceholderString:text(@"Type here to try it")];
    [_page addSubview:try];
}

- (void)showZone
{
    [self title:text(@"Time Zone") body:text(@"Choose the region and the city nearest you.")];
    _popup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(40, 220, 200, 26) pullsDown:NO];
    for (NSString *r in _regions)
        [_popup addItemWithTitle:[r stringByReplacingOccurrencesOfString:@"_" withString:@" "]];
    [_popup setTarget:self];
    [_popup setAction:@selector(chooseRegion:)];
    [_page addSubview:_popup];
    _cityPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(250, 220, 300, 26) pullsDown:NO];
    [_page addSubview:_cityPopup];
    NSString *region = _zone ? [[_zone pathComponents] firstObject] : @"UTC";
    NSUInteger r = [_regions indexOfObject:region];
    [_popup selectItemAtIndex:r == NSNotFound ? 0 : (NSInteger)r];
    [self chooseRegion:nil];
    if (_zone != nil && [[_zone pathComponents] count] > 1) {
        NSString *city = [_zone substringFromIndex:[region length] + 1];
        NSUInteger c = [_zones[region] indexOfObject:city];
        if (c != NSNotFound)
            [_cityPopup selectItemAtIndex:(NSInteger)c];
    }
}

- (void)chooseRegion:(id)sender
{
    [_cityPopup removeAllItems];
    for (NSString *c in _zones[_regions[(NSUInteger)[_popup indexOfSelectedItem]]])
        [_cityPopup addItemWithTitle:[c stringByReplacingOccurrencesOfString:@"_" withString:@" "]];
    [_cityPopup setEnabled:[_cityPopup numberOfItems] > 0];
}

- (void)showNetwork
{
    NSString *state = networkState();
    [self title:text(@"Network")
           body:state ? [NSString stringWithFormat:text(@"Connected (%@)."), state]
                      : text(@"Not connected. Minkar sets up a cabled network by itself; you can set it up later in "
                             @"System Preferences.")];
}

- (void)showComputer
{
    [self title:text(@"Computer Name") body:text(@"The name other computers on the network see this one by.")];
    _computerField = entry(NSMakeRect(40, 220, 320, 24), NO);
    [_computerField setStringValue:_computer ?: @"Minkar"];
    [_page addSubview:_computerField];
}

- (void)showAdministrator
{
    [self title:text(@"Create an Administrator")
           body:text(@"The first account of this computer. It administers it: it installs software and changes the "
                     @"system's settings.")];
    NSArray *rows = @[ text(@"Full name:"), text(@"Account name:"), text(@"Password:"), text(@"Verify:") ];
    NSMutableArray *fields = [NSMutableArray array];
    for (NSUInteger i = 0; i < [rows count]; i++) {
        NSTextField *l = label(rows[i], NSMakeRect(40, 226 - i * 34, 140, 20), 13, NO);
        [l setAlignment:NSRightTextAlignment];
        [_page addSubview:l];
        NSTextField *f = entry(NSMakeRect(190, 224 - i * 34, 300, 24), i >= 2);
        [f setDelegate:self];
        [_page addSubview:f];
        [fields addObject:f];
    }
    _fullName = fields[0];
    _account = fields[1];
    _password = fields[2];
    _verify = fields[3];
    _problem = label(@"", NSMakeRect(190, 60, 400, 36), 12, NO);
    [_problem setTextColor:[NSColor systemRedColor]];
    [_page addSubview:_problem];
    [_window makeFirstResponder:_fullName];
}

/* The account name follows the full name until it is typed in itself. */
- (void)controlTextDidChange:(NSNotification *)note
{
    if ([note object] == _account)
        _accountEdited = [[_account stringValue] length] > 0;
    else if ([note object] == _fullName && !_accountEdited)
        [_account setStringValue:accountName([_fullName stringValue])];
}

- (void)goBack:(id)sender
{
    if (_step > 0)
        [self show:_step - 1];
}

- (void)goOn:(id)sender
{
    switch (_step) {
    case 0:
        [self applyLanguage];
        break;
    case 1:
        if ([_layouts count]) {
            TISInputSourceRef s = (__bridge TISInputSourceRef)_layouts[(NSUInteger)[_popup indexOfSelectedItem]];
            _layout = (__bridge NSString *)TISGetInputSourceProperty(s, kTISPropertyInputSourceID);
            TISEnableInputSource(s);
            TISSelectInputSource(s);
        }
        break;
    case 2: {
        NSString *region = _regions[(NSUInteger)[_popup indexOfSelectedItem]];
        NSArray *cities = _zones[region];
        _zone = [cities count] ? [region stringByAppendingPathComponent:cities[(NSUInteger)[_cityPopup indexOfSelectedItem]]] : region;
        run(@"/usr/sbin/systemsetup", @[ @"-settimezone", _zone ]);
        break;
    }
    case 4:
        _computer = [[_computerField stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        if ([_computer length] == 0)
            return;
        break;
    case 5:
        [self finish];
        return;
    }
    [self show:_step + 1];
}

- (void)applyLanguage
{
    NSArray *languages = [_language isEqual:@"es"] ? @[ @"es", @"en" ] : @[ @"en", @"es" ];
    CFPreferencesSetValue(CFSTR("AppleLanguages"), (__bridge CFArrayRef)languages, kCFPreferencesAnyApplication,
                          kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    CFPreferencesSetValue(CFSTR("AppleLocale"), (__bridge CFStringRef)_language, kCFPreferencesAnyApplication,
                          kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    CFPreferencesSynchronize(kCFPreferencesAnyApplication, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
}

- (void)finish
{
    NSString *full = [[_fullName stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    NSString *name = [_account stringValue], *password = [_password stringValue];
    if ([full length] == 0 || [name length] == 0) {
        [_problem setStringValue:text(@"Enter a full name and an account name.")];
        return;
    }
    if (![accountName(name) isEqualToString:name] || getpwnam([name UTF8String]) != NULL) {
        [_problem setStringValue:text(@"The account name must be new, in lowercase letters and digits.")];
        return;
    }
    if ([password length] < 4 || ![password isEqualToString:[_verify stringValue]]) {
        [_problem setStringValue:text(@"The passwords must match and have 4 characters or more.")];
        [_verify setStringValue:@""];
        return;
    }
    [_continue setEnabled:NO];
    [_back setEnabled:NO];

    /* The computer's name, for people and for the network. */
    NSString *host = [[accountName(_computer) length] ? accountName(_computer) : @"minkar" copy];
    run(@"/usr/sbin/scutil", @[ @"--set", @"ComputerName", _computer ]);
    run(@"/usr/sbin/scutil", @[ @"--set", @"LocalHostName", host ]);

    /* The administrator. */
    if (run(@"/usr/sbin/sysadminctl", @[ @"-addUser", name, @"-fullName", full, @"-password", password, @"-admin" ]) != 0) {
        [_problem setStringValue:text(@"The account could not be made.")];
        [_continue setEnabled:YES];
        [_back setEnabled:YES];
        return;
    }
    /* The language and the keyboard chosen: the new user's too (its own
       preferences, which its apps read first), and the login window's. */
    CFPreferencesAppSynchronize(CFSTR("com.apple.HIToolbox"));
    NSString *mine = @"/var/root/Library/Preferences/com.apple.HIToolbox.plist";
    struct passwd *pw = getpwnam([name UTF8String]);
    NSFileManager *fm = [NSFileManager defaultManager];
    if (pw != NULL) {
        uid_t uid = pw->pw_uid;
        gid_t gid = pw->pw_gid;
        NSString *library = [@(pw->pw_dir) stringByAppendingPathComponent:@"Library"];
        NSString *dir = [library stringByAppendingPathComponent:@"Preferences"];
        for (NSString *d in @[ library, dir ]) {
            [fm createDirectoryAtPath:d withIntermediateDirectories:YES attributes:nil error:NULL];
            chown([d fileSystemRepresentation], uid, gid);
        }
        NSArray *languages = [_language isEqual:@"es"] ? @[ @"es", @"en" ] : @[ @"en", @"es" ];
        NSString *global = [dir stringByAppendingPathComponent:@".GlobalPreferences.plist"];
        NSMutableDictionary *g = [NSMutableDictionary dictionaryWithContentsOfFile:global] ?: [NSMutableDictionary dictionary];
        g[@"AppleLanguages"] = languages;
        g[@"AppleLocale"] = _language;
        [g writeToFile:global atomically:YES];
        chown([global fileSystemRepresentation], uid, gid);
        if ([fm fileExistsAtPath:mine]) {
            NSString *theirs = [dir stringByAppendingPathComponent:@"com.apple.HIToolbox.plist"];
            [fm removeItemAtPath:theirs error:NULL];
            [fm copyItemAtPath:mine toPath:theirs error:NULL];
            chown([theirs fileSystemRepresentation], uid, gid);
            [fm removeItemAtPath:@"/Library/Preferences/com.apple.HIToolbox.plist" error:NULL];
            [fm copyItemAtPath:mine toPath:@"/Library/Preferences/com.apple.HIToolbox.plist" error:NULL];
        }
    }

    /* Set up: not again; the administrator logs in. */
    [[NSData data] writeToFile:SETUP_DONE atomically:YES];
    os_log(OS_LOG_DEFAULT, "setup: done, %{public}s logs in", [name UTF8String]);
    printf("user %s\n", [name UTF8String]);
    fflush(stdout);
    exit(0);
}

@end

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        Setup *setup = [[Setup alloc] init];
        [app setDelegate:setup];
        [app run];
    }
    return 0;
}
