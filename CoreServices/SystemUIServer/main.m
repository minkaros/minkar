/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * SystemUIServer (Atlas.md, MB1): an agent app (LSUIElement) whose status
 * items are the Menu Extras, laid out by the window server at the right of
 * the menu bar. It also shows the notification center's banners (5.6, N0)
 * and its list, the rightmost extra (N1).
 */

#include <notify.h>
#include <unistd.h>
#import "MenuExtra.h"
#import "NotificationBanner.h"
#import "NotificationCenterExtra.h"
#import "SystemHotKeys.h"

@interface SystemUIServer : NSObject <NSApplicationDelegate>
@end

/* The Menu Extras SystemUIServer loads by default, right to left after
   the notification center: com.apple.systemuiserver's menuExtras keeps
   the user's list, as on the reference platform. */
static NSString *const kExtrasDirectory = @"/System/Library/CoreServices/Menu Extras";

static NSArray<NSString *> *defaultExtras(void)
{
    NSMutableArray *paths = [NSMutableArray array];
    for (NSString *name in @[ @"Clock", @"Displays", @"Network", @"Volume" ])
        [paths addObject:[kExtrasDirectory stringByAppendingPathComponent:[name stringByAppendingPathExtension:@"menu"]]];
    return paths;
}

@implementation SystemUIServer {
    NSMutableArray *_extras;
    NSMutableDictionary<NSString *, NSStatusItem *> *_items;    /* by bundle path */
    NotificationPresenter *_notifications;
    SystemHotKeys *_hotKeys;
}

/* A .menu bundle: its principal class fills a status item named after the
   bundle's identifier, so its place and visibility are kept; Command-drag
   takes it off the bar. */
- (BOOL)loadExtraAtPath:(NSString *)path
{
    NSBundle *bundle = [NSBundle bundleWithPath:path];
    Class cls = [bundle principalClass];

    if (cls == Nil || ![cls instancesRespondToSelector:@selector(initWithStatusItem:)]) {
        NSLog(@"SystemUIServer: %@ is not a Menu Extra", path);
        return NO;
    }
    NSStatusItem *item = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    [item setAutosaveName:[bundle bundleIdentifier] ?: [path lastPathComponent]];
    [item setBehavior:NSStatusItemBehaviorRemovalAllowed];
    id extra = [(id<MenuExtra>)[cls alloc] initWithStatusItem:item];
    if (extra == nil) {
        [[NSStatusBar systemStatusBar] removeStatusItem:item];
        return NO;
    }
    [_extras addObject:extra];
    _items[path] = item;
    NSLog(@"SystemUIServer: loaded %@", [path lastPathComponent]);
    return YES;
}

/* A .menu opened (a double click, open(1)): added to the bar, or shown
   again if it was taken off. */
- (void)application:(NSApplication *)app openURLs:(NSArray<NSURL *> *)urls
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    for (NSURL *url in urls) {
        NSString *path = [[url path] stringByStandardizingPath];
        if (![[path pathExtension] isEqualToString:@"menu"])
            continue;
        NSStatusItem *item = _items[path];
        if (item != nil) {
            [item setVisible:YES];
            continue;
        }
        if ([self loadExtraAtPath:path]) {
            NSArray *list = [defaults arrayForKey:@"menuExtras"] ?: defaultExtras();
            [defaults setObject:[list arrayByAddingObject:path] forKey:@"menuExtras"];
            [defaults synchronize];
        }
    }
}

/* The list changed elsewhere (the Date & Time pane's "Show date and time
   in menu bar"): what left it is hidden, what came into it is shown, and
   loaded if it was not. */
- (void)extrasChanged
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSArray *list;

    [defaults synchronize];
    list = [defaults arrayForKey:@"menuExtras"] ?: defaultExtras();
    for (NSString *path in [_items allKeys])
        if (![list containsObject:path])
            [_items[path] setVisible:NO];
    for (id path in list) {
        if (![path isKindOfClass:[NSString class]])
            continue;
        NSString *p = [path stringByStandardizingPath];
        if (_items[p] != nil)
            [_items[p] setVisible:YES];
        else
            [self loadExtraAtPath:p];
    }
    NSLog(@"SystemUIServer: menu extras %lu", (unsigned long)[list count]);
}

- (void)watchExtras
{
    int fd = -1, token;

    if (notify_register_file_descriptor("org.minkar.systemuiserver.extras", &fd, 0, &token) != NOTIFY_STATUS_OK)
        return;
    [NSThread detachNewThreadWithBlock:^{
        int value;
        while (read(fd, &value, sizeof(value)) == sizeof(value))
            [self performSelectorOnMainThread:@selector(extrasChanged) withObject:nil waitUntilDone:NO];
    }];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    NSStatusBar *bar = [NSStatusBar systemStatusBar];

    _extras = [NSMutableArray array];
    _notifications = [[NotificationPresenter alloc] init];
    /* Right to left: the notification center, then the Menu Extras. */
    [_extras addObject:[[NotificationCenterExtra alloc]
        initWithStatusItem:[bar statusItemWithLength:NSVariableStatusItemLength] presenter:_notifications]];
    _items = [NSMutableDictionary dictionary];
    NSArray *list = [[NSUserDefaults standardUserDefaults] arrayForKey:@"menuExtras"] ?: defaultExtras();
    for (id path in list)
        if ([path isKindOfClass:[NSString class]])
            [self loadExtraAtPath:[path stringByStandardizingPath]];
    [_notifications start];
    [self watchExtras];
    _hotKeys = [[SystemHotKeys alloc] init];
    [_hotKeys start];
}

@end

int main(void)
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        SystemUIServer *server = [[SystemUIServer alloc] init];

        [app setDelegate:server];
        [app run];
    }
    return 0;
}
