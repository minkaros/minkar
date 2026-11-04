/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Alcor, Atlas's text editor. Plain text documents, each in a window;
 * LaunchServices hands it the files to open (it declares
 * public.plain-text), and with none it starts with an untitled one. Its
 * menus have the reference platform's commands and keys: File (New,
 * Open, Close, Save, Save As), Edit (Undo, Redo, Cut, Copy, Paste, Select
 * All) and Window (Minimize). English and Spanish.
 */

#import "AlcorDocumentWindow.h"
#include <os/log.h>

@interface AlcorApplication : NSObject <NSApplicationDelegate, AlcorDocumentWindowDelegate>
@end

@implementation AlcorApplication {
    NSMutableArray<AlcorDocumentWindow *> *_documents;
    NSUInteger _untitled;
}

- (instancetype)init
{
    if ((self = [super init]) != nil)
        _documents = [NSMutableArray array];
    return self;
}

static NSMenuItem *item(NSMenu *menu, NSString *title, id target, SEL action, NSString *key, NSUInteger modifiers)
{
    NSMenuItem *i = [menu addItemWithTitle:title action:action keyEquivalent:key];
    [i setTarget:target];
    [i setKeyEquivalentModifierMask:modifiers];
    return i;
}

static void addMenu(NSString *title, NSMenu *menu)
{
    NSMenuItem *holder = [[NSMenuItem alloc] initWithTitle:title action:NULL keyEquivalent:@""];
    [holder setSubmenu:menu];
    [[NSApp mainMenu] addItem:holder];
}

- (void)buildMenus
{
    NSUInteger cmd = NSCommandKeyMask, shift = NSCommandKeyMask | NSShiftKeyMask;

    NSMenu *file = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"File", nil)];
    item(file, NSLocalizedString(@"New", nil), self, @selector(newDocument:), @"n", cmd);
    item(file, NSLocalizedString(@"Open…", nil), self, @selector(openDocument:), @"o", cmd);
    [file addItem:[NSMenuItem separatorItem]];
    item(file, NSLocalizedString(@"Close", nil), self, @selector(closeDocument:), @"w", cmd);
    item(file, NSLocalizedString(@"Save", nil), self, @selector(saveDocument:), @"s", cmd);
    item(file, NSLocalizedString(@"Save As…", nil), self, @selector(saveDocumentAs:), @"s", shift);
    addMenu(NSLocalizedString(@"File", nil), file);

    /* The text view answers these through the responder chain. */
    NSMenu *edit = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Edit", nil)];
    item(edit, NSLocalizedString(@"Undo", nil), nil, @selector(undo:), @"z", cmd);
    item(edit, NSLocalizedString(@"Redo", nil), nil, @selector(redo:), @"z", shift);
    [edit addItem:[NSMenuItem separatorItem]];
    item(edit, NSLocalizedString(@"Cut", nil), nil, @selector(cut:), @"x", cmd);
    item(edit, NSLocalizedString(@"Copy", nil), nil, @selector(copy:), @"c", cmd);
    item(edit, NSLocalizedString(@"Paste", nil), nil, @selector(paste:), @"v", cmd);
    item(edit, NSLocalizedString(@"Select All", nil), nil, @selector(selectAll:), @"a", cmd);
    addMenu(NSLocalizedString(@"Edit", nil), edit);

    NSMenu *window = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Window", nil)];
    item(window, NSLocalizedString(@"Minimize", nil), nil, @selector(performMiniaturize:), @"m", cmd);
    addMenu(NSLocalizedString(@"Window", nil), window);
}

/* The menus once AppKit has made the menu bar (documents may already
 * be open: they came as it finished launching). */
- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    [self buildMenus];
}

/* Started with no documents: a new one. */
- (BOOL)applicationOpenUntitledFile:(NSApplication *)app
{
    [self newDocument:nil];
    return YES;
}

- (void)application:(NSApplication *)app openURLs:(NSArray<NSURL *> *)urls
{
    for (NSURL *url in urls)
        if ([url isFileURL])
            [self openURL:url];
}

/* A file: its window if it is open already, else a new one. */
- (void)openURL:(NSURL *)url
{
    NSString *path = [[url path] stringByStandardizingPath];

    for (AlcorDocumentWindow *d in _documents)
        if ([[[d.fileURL path] stringByStandardizingPath] isEqualToString:path]) {
            [d show];
            return;
        }
    NSError *error = nil;
    AlcorDocumentWindow *d = [[AlcorDocumentWindow alloc] initWithURL:url error:&error];
    if (d == nil) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithFormat:NSLocalizedString(@"The document “%@” could not be opened.", nil),
                               [path lastPathComponent]]];
        [alert setInformativeText:[error localizedDescription] ?: @""];
        [alert runModal];
        return;
    }
    [self add:d];
}

- (void)add:(AlcorDocumentWindow *)document
{
    document.delegate = self;
    [_documents addObject:document];
    [document show];
}

- (void)documentWindowDidClose:(AlcorDocumentWindow *)document
{
    [_documents removeObject:document];
}

- (AlcorDocumentWindow *)front
{
    for (AlcorDocumentWindow *d in _documents)
        if ([d.window isKeyWindow] || [d.window isMainWindow])
            return d;
    return nil;
}

- (void)newDocument:(id)sender
{
    [self add:[[AlcorDocumentWindow alloc] initUntitled:++_untitled]];
}

- (void)openDocument:(id)sender
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];

    if ([panel runModal] == NSFileHandlingPanelOKButton)
        for (NSURL *url in [panel URLs])
            [self openURL:url];
}

- (void)closeDocument:(id)sender
{
    [[self front] close];
}

- (void)saveDocument:(id)sender
{
    [[self front] save];
}

- (void)saveDocumentAs:(id)sender
{
    [[self front] saveAs];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    SEL action = [menuItem action];

    if (action == @selector(closeDocument:) || action == @selector(saveDocument:) ||
        action == @selector(saveDocumentAs:))
        return [self front] != nil;
    return YES;
}

/* Unsaved documents: each asked about before quitting. */
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)app
{
    for (AlcorDocumentWindow *d in [_documents copy])
        if (d.edited && ![d close])
            return NSTerminateCancel;
    return NSTerminateNow;
}

@end

int main(void)
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        AlcorApplication *delegate = [[AlcorApplication alloc] init];

        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
