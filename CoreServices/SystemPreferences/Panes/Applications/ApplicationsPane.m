/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * System Preferences' Applications pane (decided 24 Nov 2026):
 *
 *   - Defaults: the app that opens each kind of thing (the web, mail,
 *     calendars, video, music, images, text, folders, PDF). LaunchServices
 *     keeps a default by exact type, so a kind's choice is set for each of
 *     its types (and URL schemes); the one shown is its first type's.
 *   - Open at Login: the session's login items (LSSharedFileList, which
 *     the Dock's Open at Login also writes and loginwindow opens), with
 *     Add, Remove and Hide.
 *   - Permissions: what each app may reach, once the system keeps those.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#import <CoreServices/CoreServices.h>

static const CGFloat kWidth = 668, kHeight = 470;

/* A kind of thing to open: its name, its URL schemes, its types. */
typedef struct {
    NSString *name;
    NSArray *schemes;
    NSArray *types;
} AppKind;

static NSArray *kinds(void)
{
    static NSArray *list;
    if (list == nil)
        list = @[
            @[ @"Web browser", @[ @"http", @"https" ], @[ @"public.html" ] ],
            @[ @"Email", @[ @"mailto" ], @[] ],
            @[ @"Calendar", @[ @"webcal" ], @[ @"com.apple.ical.ics" ] ],
            @[ @"Videos", @[], @[ @"public.mpeg-4", @"com.apple.quicktime-movie", @"public.avi",
                                  @"org.matroska.mkv", @"org.webmproject.webm", @"public.mpeg" ] ],
            @[ @"Music", @[], @[ @"public.mp3", @"public.mpeg-4-audio", @"com.microsoft.waveform-audio",
                                 @"org.xiph.flac", @"public.aiff-audio", @"org.xiph.ogg-audio" ] ],
            @[ @"Images", @[], @[ @"public.png", @"public.jpeg", @"com.compuserve.gif", @"public.tiff",
                                  @"org.webmproject.webp", @"com.microsoft.bmp", @"public.svg-image" ] ],
            @[ @"Text editor", @[], @[ @"public.plain-text", @"net.daringfireball.markdown", @"public.json",
                                       @"public.xml" ] ],
            @[ @"Files", @[], @[ @"public.folder" ] ],
            @[ @"PDF viewer", @[], @[ @"com.adobe.pdf" ] ],
        ];
    return list;
}

static NSTextField *label(NSString *text, NSRect frame, NSTextAlignment alignment)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
    [field setStringValue:text];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setBordered:NO];
    [field setDrawsBackground:NO];
    [field setAlignment:alignment];
    return field;
}

/* An app's name and small icon, by bundle identifier; nil if not found. */
static NSURL *appURL(NSString *bundleID)
{
    return [[NSWorkspace sharedWorkspace] URLForApplicationWithBundleIdentifier:bundleID];
}

static NSString *appName(NSURL *url)
{
    NSString *name = [[NSFileManager defaultManager] displayNameAtPath:[url path]];
    return [name hasSuffix:@".app"] ? [name stringByDeletingPathExtension] : name;
}

@interface ApplicationsPane : NSPreferencePane <NSTableViewDataSource, NSTableViewDelegate>
@end

@implementation ApplicationsPane {
    NSBundle *_strings;
    NSMutableArray<NSPopUpButton *> *_popups;
    NSTableView *_loginTable;
    NSArray *_loginItems;          /* LSSharedFileListItemRef */
    NSButton *_remove;
}

- (NSString *)text:(NSString *)key
{
    return [[self bundle] localizedStringForKey:key value:key table:nil];
}

- (NSView *)loadMainView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];
    NSTabView *tabs = [[NSTabView alloc] initWithFrame:NSMakeRect(16, 12, kWidth - 32, kHeight - 24)];

    NSTabViewItem *defaults = [[NSTabViewItem alloc] initWithIdentifier:@"defaults"];
    [defaults setLabel:[self text:@"Defaults"]];
    [defaults setView:[self defaultsView:[tabs contentRect].size]];
    [tabs addTabViewItem:defaults];

    NSTabViewItem *login = [[NSTabViewItem alloc] initWithIdentifier:@"login"];
    [login setLabel:[self text:@"Open at Login"]];
    [login setView:[self loginView:[tabs contentRect].size]];
    [tabs addTabViewItem:login];

    NSTabViewItem *permissions = [[NSTabViewItem alloc] initWithIdentifier:@"permissions"];
    [permissions setLabel:[self text:@"Permissions"]];
    [permissions setView:[self permissionsView:[tabs contentRect].size]];
    [tabs addTabViewItem:permissions];

    [view addSubview:tabs];
    [self setMainView:view];
    return view;
}

- (void)willSelect
{
    [self reloadDefaults];
    [self reloadLoginItems];
}

/* ---- Defaults ---- */

- (NSView *)defaultsView:(NSSize)size
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, size.width, size.height)];
    NSArray *list = kinds();
    CGFloat y = size.height - 40;

    [view addSubview:label([self text:@"The app that opens each kind of item:"],
                           NSMakeRect(20, y + 6, size.width - 40, 20), NSLeftTextAlignment)];
    y -= 30;
    _popups = [NSMutableArray array];
    for (NSUInteger i = 0; i < [list count]; i++) {
        [view addSubview:label([[self text:list[i][0]] stringByAppendingString:@":"],
                               NSMakeRect(20, y, 190, 20), NSRightTextAlignment)];
        NSPopUpButton *popup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(218, y - 4, 280, 26) pullsDown:NO];
        [popup setTag:(NSInteger)i];
        [popup setTarget:self];
        [popup setAction:@selector(chooseDefault:)];
        [view addSubview:popup];
        [_popups addObject:popup];
        y -= 34;
    }
    [self reloadDefaults];
    return view;
}

/* The apps that can open a kind: every handler of its schemes and types,
   once each, by name. */
- (NSArray<NSString *> *)handlersForKind:(NSArray *)kind
{
    NSMutableOrderedSet *ids = [NSMutableOrderedSet orderedSet];
    for (NSString *scheme in kind[1])
        [ids addObjectsFromArray:CFBridgingRelease(LSCopyAllHandlersForURLScheme((__bridge CFStringRef)scheme)) ?: @[]];
    for (NSString *type in kind[2])
        [ids addObjectsFromArray:CFBridgingRelease(LSCopyAllRoleHandlersForContentType((__bridge CFStringRef)type,
                                                                                      kLSRolesAll)) ?: @[]];
    NSMutableArray *found = [NSMutableArray array];
    for (NSString *bundleID in ids)
        if (appURL(bundleID) != nil)
            [found addObject:bundleID];
    return [found sortedArrayUsingComparator:^NSComparisonResult(NSString *a, NSString *b) {
        return [appName(appURL(a)) localizedStandardCompare:appName(appURL(b))];
    }];
}

- (NSString *)defaultForKind:(NSArray *)kind
{
    if ([kind[1] count] > 0)
        return CFBridgingRelease(LSCopyDefaultHandlerForURLScheme((__bridge CFStringRef)kind[1][0]));
    if ([kind[2] count] > 0)
        return CFBridgingRelease(LSCopyDefaultRoleHandlerForContentType((__bridge CFStringRef)kind[2][0], kLSRolesAll));
    return nil;
}

- (void)reloadDefaults
{
    NSArray *list = kinds();

    for (NSPopUpButton *popup in _popups) {
        NSArray *kind = list[(NSUInteger)[popup tag]];
        NSArray *handlers = [self handlersForKind:kind];
        NSString *chosen = [self defaultForKind:kind];

        [popup removeAllItems];
        if ([handlers count] == 0) {
            [popup addItemWithTitle:[self text:@"None"]];
            [popup setEnabled:NO];
            continue;
        }
        [popup setEnabled:YES];
        for (NSString *bundleID in handlers) {
            NSURL *url = appURL(bundleID);
            [popup addItemWithTitle:appName(url)];
            NSMenuItem *item = [popup lastItem];
            [item setRepresentedObject:bundleID];
            NSImage *icon = [[NSWorkspace sharedWorkspace] iconForFile:[url path]];
            [icon setSize:NSMakeSize(16, 16)];
            [item setImage:icon];
        }
        NSInteger index = chosen != nil ? [popup indexOfItemWithRepresentedObject:chosen] : -1;
        [popup selectItemAtIndex:index >= 0 ? index : 0];
    }
}

/* The kind's choice for each of its schemes and types. */
- (void)chooseDefault:(NSPopUpButton *)sender
{
    NSArray *kind = kinds()[(NSUInteger)[sender tag]];
    NSString *bundleID = [[sender selectedItem] representedObject];

    if (bundleID == nil)
        return;
    for (NSString *scheme in kind[1])
        LSSetDefaultHandlerForURLScheme((__bridge CFStringRef)scheme, (__bridge CFStringRef)bundleID);
    for (NSString *type in kind[2])
        LSSetDefaultRoleHandlerForContentType((__bridge CFStringRef)type, kLSRolesAll, (__bridge CFStringRef)bundleID);
}

/* ---- Open at Login ---- */

- (NSView *)loginView:(NSSize)size
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, size.width, size.height)];

    [view addSubview:label([self text:@"These items open when you log in:"],
                           NSMakeRect(20, size.height - 34, size.width - 40, 20), NSLeftTextAlignment)];
    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 56, size.width - 40, size.height - 100)];
    _loginTable = [[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]];
    NSTableColumn *hide = [[NSTableColumn alloc] initWithIdentifier:@"hide"];
    [[hide headerCell] setStringValue:[self text:@"Hide"]];
    [hide setWidth:60];
    NSButtonCell *check = [[NSButtonCell alloc] init];
    [check setButtonType:NSSwitchButton];
    [check setTitle:@""];
    [hide setDataCell:check];
    NSTableColumn *name = [[NSTableColumn alloc] initWithIdentifier:@"name"];
    [[name headerCell] setStringValue:[self text:@"Item"]];
    [name setWidth:size.width - 140];
    [name setEditable:NO];
    [_loginTable addTableColumn:hide];
    [_loginTable addTableColumn:name];
    [_loginTable setDataSource:self];
    [_loginTable setDelegate:self];
    [scroll setDocumentView:_loginTable];
    [scroll setHasVerticalScroller:YES];
    [scroll setBorderType:NSBezelBorder];
    [view addSubview:scroll];

    NSButton *add = [[NSButton alloc] initWithFrame:NSMakeRect(20, 22, 28, 26)];
    [add setTitle:@"+"];
    [add setBezelStyle:NSSmallSquareBezelStyle];
    [add setTarget:self];
    [add setAction:@selector(addLoginItem:)];
    [view addSubview:add];
    _remove = [[NSButton alloc] initWithFrame:NSMakeRect(48, 22, 28, 26)];
    [_remove setTitle:@"−"];
    [_remove setBezelStyle:NSSmallSquareBezelStyle];
    [_remove setTarget:self];
    [_remove setAction:@selector(removeLoginItem:)];
    [view addSubview:_remove];
    [view addSubview:label([self text:@"To hide an app when you log in, select Hide next to it."],
                           NSMakeRect(90, 26, size.width - 110, 18), NSLeftTextAlignment)];
    [self reloadLoginItems];
    return view;
}

- (LSSharedFileListRef)loginList
{
    return LSSharedFileListCreate(NULL, kLSSharedFileListSessionLoginItems, NULL);
}

- (void)reloadLoginItems
{
    LSSharedFileListRef list = [self loginList];
    _loginItems = list != NULL ? CFBridgingRelease(LSSharedFileListCopySnapshot(list, NULL)) : @[];
    if (list != NULL)
        CFRelease(list);
    [_loginTable reloadData];
    [_remove setEnabled:[_loginTable selectedRow] >= 0];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)table
{
    return (NSInteger)[_loginItems count];
}

- (id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    LSSharedFileListItemRef item = (__bridge LSSharedFileListItemRef)_loginItems[(NSUInteger)row];
    if ([[column identifier] isEqual:@"hide"]) {
        id hidden = CFBridgingRelease(LSSharedFileListItemCopyProperty(item, kLSSharedFileListLoginItemHidden));
        return @([hidden boolValue]);
    }
    NSString *name = CFBridgingRelease(LSSharedFileListItemCopyDisplayName(item));
    return [name hasSuffix:@".app"] ? [name stringByDeletingPathExtension] : name;
}

- (void)tableView:(NSTableView *)table setObjectValue:(id)value forTableColumn:(NSTableColumn *)column
              row:(NSInteger)row
{
    if (![[column identifier] isEqual:@"hide"])
        return;
    LSSharedFileListItemRef item = (__bridge LSSharedFileListItemRef)_loginItems[(NSUInteger)row];
    CFURLRef url = LSSharedFileListItemCopyResolvedURL(item, 0, NULL);
    if (url == NULL)
        return;
    /* Hidden is the item's own: it is put again with the new value, at
       its place. */
    LSSharedFileListRef list = [self loginList];
    LSSharedFileListItemRef before = row > 0 ? (__bridge LSSharedFileListItemRef)_loginItems[(NSUInteger)row - 1]
                                             : kLSSharedFileListItemBeforeFirst;
    NSDictionary *properties = @{ (__bridge NSString *)kLSSharedFileListLoginItemHidden: @([value boolValue]) };
    LSSharedFileListItemRemove(list, item);
    LSSharedFileListItemRef added = LSSharedFileListInsertItemURL(list, before, NULL, NULL, url,
                                                                  (__bridge CFDictionaryRef)properties, NULL);
    if (added != NULL)
        CFRelease(added);
    CFRelease(url);
    CFRelease(list);
    [self reloadLoginItems];
}

- (void)tableViewSelectionDidChange:(NSNotification *)note
{
    [_remove setEnabled:[_loginTable selectedRow] >= 0];
}

- (void)addLoginItem:(id)sender
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setCanChooseFiles:YES];
    [panel setCanChooseDirectories:NO];
    [panel setAllowsMultipleSelection:YES];
    [panel setDirectoryURL:[NSURL fileURLWithPath:@"/Applications"]];
    [panel setPrompt:[self text:@"Add"]];
    if ([panel runModal] != NSModalResponseOK)
        return;
    LSSharedFileListRef list = [self loginList];
    for (NSURL *url in [panel URLs]) {
        LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(list, kLSSharedFileListItemLast, NULL, NULL,
                                                                     (__bridge CFURLRef)url, NULL, NULL);
        if (item != NULL)
            CFRelease(item);
    }
    if (list != NULL)
        CFRelease(list);
    [self reloadLoginItems];
}

- (void)removeLoginItem:(id)sender
{
    NSInteger row = [_loginTable selectedRow];
    if (row < 0 || row >= (NSInteger)[_loginItems count])
        return;
    LSSharedFileListRef list = [self loginList];
    LSSharedFileListItemRemove(list, (__bridge LSSharedFileListItemRef)_loginItems[(NSUInteger)row]);
    if (list != NULL)
        CFRelease(list);
    [self reloadLoginItems];
}

/* ---- Permissions ---- */

- (NSView *)permissionsView:(NSSize)size
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, size.width, size.height)];
    NSTextField *note = label([self text:@"What each app may reach (the network, your folders, devices, "
                                          @"notifications, the screen…) will be chosen here."],
                              NSMakeRect(40, size.height / 2 - 20, size.width - 80, 40), NSCenterTextAlignment);
    [[note cell] setWraps:YES];
    [note setTextColor:[NSColor secondaryLabelColor]];
    [view addSubview:note];
    return view;
}

@end
