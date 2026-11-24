/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Installer (I4; the maintainer's choices, 23 Nov 2026): the installation
 * medium starts it alone, as root (loginwindow, .MinkarInstallMedium). It
 * welcomes; checks the computer (UEFI firmware, a processor with AVX, 4 GB
 * of memory, a disk to install on); lists the disks Minkar may go on (not
 * the medium's, not disk images, 16 GB or more) and, once the user
 * confirms the whole disk is erased, installs: the disk erased into a GUID
 * partition table with an EFI System Partition and an HFS+ volume
 * (diskutil eraseDisk), the medium's own system copied to it compressed
 * (ditto --hfsCompression), less what belongs to the medium or to its
 * running (the medium's mark, devices, caches, logs, swap), and the volume
 * made to start (bless --setBoot). The new system has no account: Setup
 * makes the first administrator at its first start. Each step goes to a
 * log, kept on the new system (/var/log/install.log). Then Restart. The
 * Utilities menu opens Terminal (diskutil and the rest by hand).
 */

#import <AppKit/AppKit.h>
#import <DiskArbitration/DiskArbitration.h>
#import <IOKit/IOKitLib.h>
#include <os/log.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>

extern char **environ;

static NSString *text(NSString *english)
{
    return [[NSBundle mainBundle] localizedStringForKey:english value:english table:nil];
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

static NSString *bytesString(uint64_t bytes)
{
    return [NSByteCountFormatter stringFromByteCount:(long long)bytes countStyle:NSByteCountFormatterCountStyleDecimal];
}

/* ---- The computer ---- */

/* 4 GB, less what the firmware keeps for itself. */
static const uint64_t kMinimumMemory = 3584ULL << 20;
static const uint64_t kMinimumDisk = 16ULL * 1000 * 1000 * 1000;

static BOOL hasUEFI(void)
{
    io_registry_entry_t efi = IORegistryEntryFromPath(kIOMasterPortDefault, "IODeviceTree:/efi");
    if (efi == IO_OBJECT_NULL)
        return NO;
    IOObjectRelease(efi);
    return YES;
}

static BOOL hasAVX(void)
{
    char features[1024] = "";
    size_t size = sizeof(features);
    if (sysctlbyname("machdep.cpu.features", features, &size, NULL, 0) != 0)
        return NO;
    return strstr(features, "AVX") != NULL;
}

static uint64_t memorySize(void)
{
    uint64_t memory = 0;
    size_t size = sizeof(memory);
    sysctlbyname("hw.memsize", &memory, &size, NULL, 0);
    return memory;
}

/* The whole disk the running system is on (the medium). */
static NSString *mediumDisk(void)
{
    struct statfs sf;
    if (statfs("/", &sf) != 0 || strncmp(sf.f_mntfromname, "/dev/disk", 9) != 0)
        return nil;
    NSString *bsd = @(sf.f_mntfromname + 5);
    NSRange slice = [bsd rangeOfString:@"s" options:0 range:NSMakeRange(4, [bsd length] - 4)];
    return slice.location == NSNotFound ? bsd : [bsd substringToIndex:slice.location];
}

/* The disks Minkar may be installed on: whole, writable, not the medium's,
   not a disk image, big enough. */
static NSArray<NSDictionary *> *installableDisks(void)
{
    NSMutableArray *disks = [NSMutableArray array];
    NSString *medium = mediumDisk();
    DASessionRef session = DASessionCreate(kCFAllocatorDefault);
    io_iterator_t it = IO_OBJECT_NULL;
    CFMutableDictionaryRef match = IOServiceMatching("IOMedia");

    CFDictionarySetValue(match, CFSTR("Whole"), kCFBooleanTrue);
    if (session == NULL || IOServiceGetMatchingServices(kIOMasterPortDefault, match, &it) != KERN_SUCCESS)
        return disks;
    io_service_t media;
    while ((media = IOIteratorNext(it)) != IO_OBJECT_NULL) {
        CFTypeRef bsd = IORegistryEntryCreateCFProperty(media, CFSTR("BSD Name"), NULL, 0);
        IOObjectRelease(media);
        if (bsd == NULL)
            continue;
        NSString *name = CFBridgingRelease(bsd);
        DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, [name UTF8String]);
        if (disk == NULL)
            continue;
        NSDictionary *d = CFBridgingRelease(DADiskCopyDescription(disk));
        CFRelease(disk);
        NSString *protocol = d[(__bridge NSString *)kDADiskDescriptionDeviceProtocolKey];
        uint64_t size = [d[(__bridge NSString *)kDADiskDescriptionMediaSizeKey] unsignedLongLongValue];
        if ([name isEqualToString:medium] || [d[(__bridge NSString *)kDADiskDescriptionMediaWritableKey] boolValue] == NO ||
            [protocol isEqualToString:@"Virtual Interface"] || [protocol isEqualToString:@"Disk Image"] ||
            size < kMinimumDisk)
            continue;
        NSString *model = d[(__bridge NSString *)kDADiskDescriptionDeviceModelKey] ?: @"";
        model = [model stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        [disks addObject:@{ @"bsd": name, @"size": @(size), @"model": [model length] ? model : name,
                            @"protocol": protocol ?: @"" }];
    }
    IOObjectRelease(it);
    CFRelease(session);
    [disks sortUsingComparator:^NSComparisonResult(NSDictionary *a, NSDictionary *b) {
        return [a[@"bsd"] compare:b[@"bsd"] options:NSNumericSearch];
    }];
    return disks;
}

/* ---- The window ---- */

@interface Installer : NSObject <NSApplicationDelegate, NSTableViewDataSource, NSTableViewDelegate>
@end

@implementation Installer {
    NSWindow *_window;
    NSView *_page;
    NSButton *_back, *_continue;
    NSInteger _step;
    NSArray<NSDictionary *> *_disks;
    NSTableView *_table;
    NSProgressIndicator *_progress;
    NSTextField *_status;
    NSMutableString *_log;
    BOOL _checksPassed, _installing;
    NSString *_failure;
    NSTimer *_countdown;          /* done: the restart, in _secondsLeft */
    BOOL _restarting;
    int _secondsLeft;
    NSTextField *_countdownLabel;
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    [self makeMenu];
    _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 640, 460)
                                          styleMask:NSTitledWindowMask | NSMiniaturizableWindowMask
                                            backing:NSBackingStoreBuffered defer:NO];
    [_window setTitle:text(@"Install Minkar")];
    NSView *content = [_window contentView];
    _page = [[NSView alloc] initWithFrame:NSMakeRect(0, 60, 640, 400)];
    [content addSubview:_page];
    [content addSubview:[[NSBox alloc] initWithFrame:NSMakeRect(20, 58, 600, 1)]];
    _back = [[NSButton alloc] initWithFrame:NSMakeRect(400, 14, 100, 30)];
    [_back setBezelStyle:NSRoundedBezelStyle];
    [_back setTitle:text(@"Go Back")];
    [_back setTarget:self];
    [_back setAction:@selector(goBack:)];
    [content addSubview:_back];
    _continue = [[NSButton alloc] initWithFrame:NSMakeRect(510, 14, 110, 30)];
    [_continue setBezelStyle:NSRoundedBezelStyle];
    [_continue setKeyEquivalent:@"\r"];
    [_continue setTarget:self];
    [_continue setAction:@selector(goOn:)];
    [content addSubview:_continue];
    _log = [NSMutableString string];
    [self logLine:[NSString stringWithFormat:@"Installer started, from %@", mediumDisk() ?: @"?"]];
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

- (void)makeMenu
{
    NSMenu *bar = [[NSMenu alloc] init];
    NSMenuItem *appItem = [[NSMenuItem alloc] init];
    NSMenu *app = [[NSMenu alloc] initWithTitle:@"Installer"];
    [app addItemWithTitle:text(@"Quit Installer") action:@selector(terminate:) keyEquivalent:@"q"];
    [appItem setSubmenu:app];
    [bar addItem:appItem];
    NSMenuItem *utilitiesItem = [[NSMenuItem alloc] init];
    NSMenu *utilities = [[NSMenu alloc] initWithTitle:text(@"Utilities")];
    [[utilities addItemWithTitle:text(@"Terminal") action:@selector(openTerminal:) keyEquivalent:@""] setTarget:self];
    [utilitiesItem setSubmenu:utilities];
    [bar addItem:utilitiesItem];
    [NSApp setMainMenu:bar];
}

- (void)openTerminal:(id)sender
{
    [[NSWorkspace sharedWorkspace] launchApplication:@"/System/Applications/Utilities/Terminal.app"];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app
{
    return NO;
}

- (void)logLine:(NSString *)line
{
    NSDateFormatter *f = [[NSDateFormatter alloc] init];
    [f setDateFormat:@"yyyy-MM-dd HH:mm:ss"];
    @synchronized(_log) {
        [_log appendFormat:@"%@ %@\n", [f stringFromDate:[NSDate date]], line];
    }
    os_log(OS_LOG_DEFAULT, "installer: %{public}s", [line UTF8String]);
}

/* ---- The steps: 0 welcome, 1 checks, 2 disk, 3 installing, 4 done ---- */

- (void)show:(NSInteger)step
{
    _step = step;
    [[_page subviews] makeObjectsPerformSelector:@selector(removeFromSuperview)];
    [_back setHidden:step == 0 || step >= 3];
    [_continue setHidden:NO];
    [_continue setEnabled:YES];
    [_continue setTitle:text(@"Continue")];
    switch (step) {
    case 0: [self showWelcome]; break;
    case 1: [self showChecks]; break;
    case 2: [self showDisks]; break;
    case 3: [self showInstalling]; break;
    case 4: [self showDone]; break;
    }
}

- (void)showWelcome
{
    [_page addSubview:label(text(@"Welcome to Minkar"), NSMakeRect(40, 320, 560, 34), 24, YES)];
    NSTextField *body = label(text(@"This installs Minkar on a disk of this computer. The disk you choose is erased: "
                                   @"everything on it is lost.\n\nWhen it is done, the computer restarts from that disk "
                                   @"and asks for your language, keyboard, time zone and the first administrator."),
                              NSMakeRect(40, 180, 560, 120), 13, NO);
    [_page addSubview:body];
}

- (void)showChecks
{
    [_page addSubview:label(text(@"Checking this computer"), NSMakeRect(40, 340, 560, 28), 18, YES)];
    struct { NSString *what; BOOL ok; NSString *why; } checks[] = {
        { text(@"UEFI firmware"), hasUEFI(), text(@"Minkar starts only from UEFI firmware.") },
        { text(@"A processor with AVX"), hasAVX(), text(@"Minkar needs a processor with AVX.") },
        { text(@"4 GB of memory or more"),
          memorySize() >= kMinimumMemory, text(@"There is not enough memory.") },
        { [NSString stringWithFormat:text(@"A disk of %@ or more"), bytesString(kMinimumDisk)],
          [installableDisks() count] > 0, text(@"There is no disk to install on.") },
    };
    _checksPassed = YES;
    CGFloat y = 290;
    for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
        NSString *line = [NSString stringWithFormat:@"%@  %@", checks[i].ok ? @"✓" : @"✗", checks[i].what];
        NSTextField *row = label(line, NSMakeRect(60, y, 520, 22), 14, NO);
        [row setTextColor:checks[i].ok ? [NSColor labelColor] : [NSColor systemRedColor]];
        [_page addSubview:row];
        if (!checks[i].ok) {
            NSTextField *why = label(checks[i].why, NSMakeRect(84, y - 20, 500, 18), 12, NO);
            [why setTextColor:[NSColor secondaryLabelColor]];
            [_page addSubview:why];
            y -= 20;
        }
        [self logLine:[NSString stringWithFormat:@"check %@: %@", checks[i].what, checks[i].ok ? @"yes" : @"no"]];
        _checksPassed = _checksPassed && checks[i].ok;
        y -= 34;
    }
    [_continue setEnabled:_checksPassed];
}

- (void)showDisks
{
    [_page addSubview:label(text(@"Choose the disk to install Minkar on"), NSMakeRect(40, 340, 560, 28), 18, YES)];
    NSTextField *warn = label(text(@"The disk is erased: everything on it is lost."), NSMakeRect(40, 314, 560, 20), 13, NO);
    [warn setTextColor:[NSColor systemRedColor]];
    [_page addSubview:warn];
    _disks = installableDisks();
    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(40, 40, 560, 260)];
    [scroll setBorderType:NSBezelBorder];
    [scroll setHasVerticalScroller:YES];
    _table = [[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]];
    for (NSArray *c in @[ @[ @"model", text(@"Disk"), @300 ], @[ @"size", text(@"Size"), @110 ], @[ @"bsd", text(@"Device"), @130 ] ]) {
        NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:c[0]];
        [[column headerCell] setStringValue:c[1]];
        [column setWidth:[c[2] doubleValue]];
        [column setEditable:NO];
        [_table addTableColumn:column];
    }
    [_table setRowHeight:24];
    [_table setDataSource:self];
    [_table setDelegate:self];
    [scroll setDocumentView:_table];
    [_page addSubview:scroll];
    [_table reloadData];
    [_continue setTitle:text(@"Install")];
    [_continue setEnabled:NO];
    for (NSDictionary *d in _disks)
        [self logLine:[NSString stringWithFormat:@"disk %@: %@, %@", d[@"bsd"], d[@"model"], bytesString([d[@"size"] unsignedLongLongValue])]];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)table
{
    return (NSInteger)[_disks count];
}

- (id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    NSDictionary *d = _disks[(NSUInteger)row];
    if ([[column identifier] isEqual:@"size"])
        return bytesString([d[@"size"] unsignedLongLongValue]);
    return d[[column identifier]];
}

- (void)tableViewSelectionDidChange:(NSNotification *)note
{
    [_continue setEnabled:[_table selectedRow] >= 0];
}

- (void)showInstalling
{
    [_page addSubview:label(text(@"Installing Minkar"), NSMakeRect(40, 340, 560, 28), 18, YES)];
    _status = label(@"", NSMakeRect(40, 200, 560, 20), 13, NO);
    [_page addSubview:_status];
    _progress = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(40, 170, 560, 20)];
    [_progress setIndeterminate:NO];
    [_progress setMinValue:0];
    [_progress setMaxValue:100];
    [_page addSubview:_progress];
    [_continue setHidden:YES];
}

- (void)showDone
{
    BOOL ok = _failure == nil;
    [_page addSubview:label(ok ? text(@"Minkar is installed") : text(@"The installation failed"),
                            NSMakeRect(40, 340, 560, 28), 18, YES)];
    NSString *body = ok ? text(@"Restart the computer and start it from the disk Minkar is on. The installation's "
                               @"log is kept there, in /var/log/install.log.")
                        : [NSString stringWithFormat:text(@"%@\n\nThe disk may be left unusable. The log is in "
                                                          @"/var/log/install.log on the installation medium."), _failure];
    [_page addSubview:label(body, NSMakeRect(40, 200, 560, 120), 13, NO)];
    [_continue setTitle:text(@"Restart")];
    /* Installed: the computer restarts by itself after ten seconds, or
       at once with Restart. */
    if (ok) {
        _secondsLeft = 10;
        _countdownLabel = label(@"", NSMakeRect(40, 150, 560, 20), 13, NO);
        [_page addSubview:_countdownLabel];
        [self tick:nil];
        _countdown = [NSTimer scheduledTimerWithTimeInterval:1 target:self selector:@selector(tick:)
                                                    userInfo:nil repeats:YES];
    }
}

- (void)tick:(NSTimer *)timer
{
    if (timer != nil)
        _secondsLeft--;
    if (_secondsLeft <= 0) {
        [self restart];
        return;
    }
    [_countdownLabel setStringValue:[NSString stringWithFormat:
        _secondsLeft == 1 ? text(@"The computer restarts in 1 second.")
                          : text(@"The computer restarts in %d seconds."), _secondsLeft]];
}

- (void)goBack:(id)sender
{
    if (_step > 0 && _step < 3)
        [self show:_step - 1];
}

- (void)goOn:(id)sender
{
    switch (_step) {
    case 0: [self show:1]; break;
    case 1: if (_checksPassed) [self show:2]; break;
    case 2: [self confirm]; break;
    case 4: [self restart]; break;
    }
}

- (void)confirm
{
    NSInteger row = [_table selectedRow];
    if (row < 0)
        return;
    NSDictionary *disk = _disks[(NSUInteger)row];
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setAlertStyle:NSCriticalAlertStyle];
    [alert setMessageText:[NSString stringWithFormat:text(@"Erase “%@” (%@) and install Minkar on it?"), disk[@"model"],
                                                     bytesString([disk[@"size"] unsignedLongLongValue])]];
    [alert setInformativeText:text(@"Everything on the disk is lost. This cannot be undone.")];
    [alert addButtonWithTitle:text(@"Erase and Install")];
    [alert addButtonWithTitle:text(@"Cancel")];
    if ([alert runModal] != NSAlertFirstButtonReturn)
        return;
    [self show:3];
    _installing = YES;
    NSString *bsd = disk[@"bsd"];
    [NSThread detachNewThreadWithBlock:^{
        [self installOn:bsd];
    }];
}

/* ---- Installing (off the main thread; the window told on it) ---- */

- (void)say:(NSString *)status at:(double)percent
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (status != nil)
            [self->_status setStringValue:status];
        if (percent >= 0)
            [self->_progress setDoubleValue:percent];
    });
}

/* A tool, its output in the log; its status. posix_spawn and waitpid:
   this runs off the main thread, where NSTask's wait has no run loop. */
- (int)run:(NSString *)path arguments:(NSArray<NSString *> *)arguments
{
    int fds[2];
    pid_t pid;
    int status = -1;
    posix_spawn_file_actions_t actions;
    NSMutableArray<NSString *> *all = [NSMutableArray arrayWithObject:path];
    [all addObjectsFromArray:arguments];
    char **argv = calloc([all count] + 1, sizeof(char *));

    [self logLine:[NSString stringWithFormat:@"run %@ %@", path, [arguments componentsJoinedByString:@" "]]];
    for (NSUInteger i = 0; i < [all count]; i++)
        argv[i] = strdup([all[i] fileSystemRepresentation]);
    if (pipe(fds) != 0) {
        free(argv);
        return -1;
    }
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, fds[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, fds[0]);
    int err = posix_spawn(&pid, argv[0], &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(fds[1]);
    for (NSUInteger i = 0; i < [all count]; i++)
        free(argv[i]);
    free(argv);
    if (err != 0) {
        close(fds[0]);
        [self logLine:[NSString stringWithFormat:@"cannot run %@: %s", path, strerror(err)]];
        return -1;
    }
    NSMutableData *output = [NSMutableData data];
    char buf[8192];
    ssize_t n;
    while ((n = read(fds[0], buf, sizeof(buf))) > 0 || (n < 0 && errno == EINTR))
        if (n > 0)
            [output appendBytes:buf length:(NSUInteger)n];
    close(fds[0]);
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
        ;
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    NSString *s = [[NSString alloc] initWithData:output encoding:NSUTF8StringEncoding];
    if ([s length]) {
        @synchronized(_log) {
            [_log appendString:s];
        }
    }
    [self logLine:[NSString stringWithFormat:@"status %d", code]];
    return code;
}

static NSString *mountPointOf(NSString *bsd)
{
    struct statfs *mnt;
    int n = getmntinfo(&mnt, MNT_NOWAIT);
    NSString *dev = [@"/dev/" stringByAppendingString:bsd];
    for (int i = 0; i < n; i++)
        if ([dev isEqualToString:@(mnt[i].f_mntfromname)])
            return @(mnt[i].f_mntonname);
    return nil;
}

static uint64_t usedBytes(NSString *path)
{
    struct statfs sf;
    if (path == nil || statfs([path fileSystemRepresentation], &sf) != 0)
        return 0;
    return (uint64_t)(sf.f_blocks - sf.f_bfree) * sf.f_bsize;
}

- (void)fail:(NSString *)why
{
    [self logLine:[@"failed: " stringByAppendingString:why]];
    [_log writeToFile:@"/var/log/install.log" atomically:YES encoding:NSUTF8StringEncoding error:NULL];
    dispatch_async(dispatch_get_main_queue(), ^{
        self->_failure = why;
        self->_installing = NO;
        [self show:4];
    });
}

- (void)installOn:(NSString *)bsd
{
    [self logLine:[NSString stringWithFormat:@"installing on %@", bsd]];

    /* 1. The disk: a GUID partition table, EFI and an HFS+ volume. */
    [self say:text(@"Erasing the disk…") at:2];
    if ([self run:@"/usr/sbin/diskutil" arguments:@[ @"eraseDisk", @"JHFS+", @"Minkar", @"GPT", bsd ]] != 0)
        return [self fail:text(@"The disk could not be erased.")];
    NSString *target = nil;
    for (int i = 0; i < 50 && target == nil; i++) {
        target = mountPointOf([bsd stringByAppendingString:@"s2"]);
        if (target == nil)
            usleep(200000);
    }
    if (target == nil)
        return [self fail:text(@"The new volume did not mount.")];
    [self logLine:[@"new volume at " stringByAppendingString:target]];

    /* 2. The system: the medium's own, compressed. */
    uint64_t total = usedBytes(@"/"), base = usedBytes(target);
    NSFileManager *fm = [NSFileManager defaultManager];
    NSArray *empty = @[ @"dev", @"Volumes", @"cores" ];
    __block BOOL copying = YES;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        while (copying) {
            uint64_t now = usedBytes(target) - base;
            /* Compressed, the copy is about half the medium's room. */
            double share = total ? MIN(1.0, (double)now / (double)(total / 2)) : 0;
            [self say:nil at:5 + 85 * share];
            sleep(1);
        }
    });
    [self say:text(@"Copying the system…") at:5];
    for (NSString *name in [fm contentsOfDirectoryAtPath:@"/" error:NULL]) {
        NSString *from = [@"/" stringByAppendingString:name], *to = [target stringByAppendingPathComponent:name];
        NSDictionary *a = [fm attributesOfItemAtPath:from error:NULL];
        if ([name hasPrefix:@".Spotlight"] || [name hasPrefix:@".Trashes"] || [name hasPrefix:@".fseventsd"])
            continue;
        if ([a[NSFileType] isEqual:NSFileTypeSymbolicLink]) {
            [fm createSymbolicLinkAtPath:to withDestinationPath:[fm destinationOfSymbolicLinkAtPath:from error:NULL] error:NULL];
            continue;
        }
        if ([empty containsObject:name] || ![a[NSFileType] isEqual:NSFileTypeDirectory]) {
            if ([a[NSFileType] isEqual:NSFileTypeDirectory])
                [fm createDirectoryAtPath:to withIntermediateDirectories:NO
                               attributes:@{ NSFilePosixPermissions: a[NSFilePosixPermissions] } error:NULL];
            else
                [self run:@"/usr/bin/ditto" arguments:@[ @"--hfsCompression", from, to ]];
            continue;
        }
        if ([self run:@"/usr/bin/ditto" arguments:@[ @"--hfsCompression", from, to ]] != 0) {
            copying = NO;
            return [self fail:[NSString stringWithFormat:text(@"%@ could not be copied."), from]];
        }
    }
    copying = NO;

    /* What belongs to the medium or to its running: emptied. */
    [self say:text(@"Preparing the new system…") at:92];
    for (NSString *dir in @[ @"private/var/vm", @"private/var/run", @"private/var/tmp", @"private/var/folders",
                             @"private/var/log", @"private/tmp", @"Users" ]) {
        NSString *path = [target stringByAppendingPathComponent:dir];
        for (NSString *item in [fm contentsOfDirectoryAtPath:path error:NULL])
            if (![dir isEqualToString:@"Users"] || ![item isEqualToString:@"Shared"])
                [fm removeItemAtPath:[path stringByAppendingPathComponent:item] error:NULL];
    }
    for (NSString *file in @[ @"private/var/db/.MinkarInstallMedium", @"private/var/db/.AppleSetupDone",
                              @"private/var/db/dhcpclient", @"private/var/db/auth.db" ])
        [fm removeItemAtPath:[target stringByAppendingPathComponent:file] error:NULL];

    /* 3. It starts: the loader, the kernel and its boot configuration. */
    [self say:text(@"Making the disk start…") at:96];
    if ([self run:@"/usr/sbin/bless" arguments:@[ @"--mount", target, @"--setBoot", @"--verbose" ]] != 0)
        return [self fail:text(@"The disk could not be made to start.")];

    [self logLine:@"installed"];
    [_log writeToFile:[target stringByAppendingPathComponent:@"private/var/log/install.log"] atomically:YES
             encoding:NSUTF8StringEncoding error:NULL];
    /* The new disk's volumes unmounted now: all they hold is written, so
       the restart need not wait for the disk (it waited over a minute for
       buffers the compressed copy leaves busy). */
    [self say:text(@"Finishing…") at:98];
    sync();
    [self run:@"/usr/sbin/diskutil" arguments:@[ @"unmountDisk", bsd ]];
    [self say:text(@"Done.") at:100];
    dispatch_async(dispatch_get_main_queue(), ^{
        self->_installing = NO;
        [self show:4];
    });
}

/* Each step of the restart, with its time, in the medium's
 * /var/log/install-restart.log, made to reach the medium before the next. */
static void restartLog(NSString *line)
{
    static int fd = -1;
    struct timeval tv;
    struct tm tm;
    char when[32];

    if (fd < 0)
        fd = open("/private/var/log/install-restart.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm);
    strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", &tm);
    NSString *text = [NSString stringWithFormat:@"%s.%03d %@\n", when, (int)(tv.tv_usec / 1000), line];
    const char *bytes = [text UTF8String];
    if (fd >= 0) {
        (void)write(fd, bytes, strlen(bytes));
        (void)fcntl(fd, F_FULLFSYNC);
    }
    os_log(OS_LOG_DEFAULT, "installer: restart: %{public}s", [line UTF8String]);
}

/* The new system is written and its disk unmounted (installOn:): the
 * medium is synchronized and the computer restarts without the kernel's
 * wait for the buffer cache (reboot -n). */
- (void)restart
{
    if (_restarting)
        return;
    _restarting = YES;
    [_countdown invalidate];
    _countdown = nil;
    restartLog(_secondsLeft > 0 ? @"Restart pressed" : @"the countdown ended");
    /* Every other local volume unmounted (the computer's other disks the
       system mounted), each written as it goes. */
    struct statfs *mounts = NULL;
    int n = getmntinfo(&mounts, MNT_NOWAIT);
    for (int i = 0; i < n; i++) {
        if (!(mounts[i].f_flags & MNT_LOCAL) || strcmp(mounts[i].f_mntonname, "/") == 0 ||
            strcmp(mounts[i].f_fstypename, "devfs") == 0)
            continue;
        int rc = unmount(mounts[i].f_mntonname, 0);
        restartLog([NSString stringWithFormat:@"unmount %s (%s, %s): %s", mounts[i].f_mntonname,
                    mounts[i].f_mntfromname, mounts[i].f_fstypename, rc == 0 ? "done" : strerror(errno)]);
    }
    restartLog(@"sync");
    sync();
    restartLog(@"synchronized; reboot -n");
    int status = [self run:@"/sbin/reboot" arguments:@[ @"-n" ]];
    restartLog([NSString stringWithFormat:@"reboot -n returned %d; shutdown -r now", status]);
    [self run:@"/sbin/shutdown" arguments:@[ @"-r", @"now" ]];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)app
{
    return _installing ? NSTerminateCancel : NSTerminateNow;
}

@end

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        Installer *installer = [[Installer alloc] init];
        [app setDelegate:installer];
        [app run];
    }
    return 0;
}
