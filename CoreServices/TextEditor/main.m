/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Text Editor, Atlas's text editor. Plain text documents, each in a window;
 * LaunchServices hands it the files to open (it declares
 * public.plain-text), and with none it starts with an untitled one. Its
 * menus have the reference platform's commands and keys: File (New,
 * Open, Close, Save, Save As), Edit (Undo, Redo, Cut, Copy, Paste, Select
 * All) and Window (Minimize). English and Spanish.
 */

#import "EditorDocumentWindow.h"
#include <os/log.h>

@interface EditorApplication : NSObject <NSApplicationDelegate, EditorDocumentWindowDelegate>
@end

@implementation EditorApplication {
    NSMutableArray<EditorDocumentWindow *> *_documents;
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
    [edit addItem:[NSMenuItem separatorItem]];
    /* Find, as the reference's: AppKit's Find panel, through the text
       view's performFindPanelAction: with each item's NSFindPanelAction. */
    NSMenu *find = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Find", nil)];
    [item(find, NSLocalizedString(@"Find…", nil), nil, @selector(performFindPanelAction:), @"f", cmd)
        setTag:NSFindPanelActionShowFindPanel];
    [item(find, NSLocalizedString(@"Find Next", nil), nil, @selector(performFindPanelAction:), @"g", cmd)
        setTag:NSFindPanelActionNext];
    [item(find, NSLocalizedString(@"Find Previous", nil), nil, @selector(performFindPanelAction:), @"g", shift)
        setTag:NSFindPanelActionPrevious];
    [item(find, NSLocalizedString(@"Use Selection for Find", nil), nil, @selector(performFindPanelAction:), @"e", cmd)
        setTag:NSFindPanelActionSetFindString];
    NSMenuItem *findHolder = [edit addItemWithTitle:NSLocalizedString(@"Find", nil) action:NULL keyEquivalent:@""];
    [findHolder setSubmenu:find];
    addMenu(NSLocalizedString(@"Edit", nil), edit);

    /* Format, as the reference's: the font panel, bold, italic and
       underline through the text view and NSFontManager, the ruler, and
       the document's kind. */
    NSMenu *format = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Format", nil)];
    NSMenu *font = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Font", nil)];
    NSFontManager *fonts = [NSFontManager sharedFontManager];
    item(font, NSLocalizedString(@"Show Fonts", nil), fonts, @selector(orderFrontFontPanel:), @"t", cmd);
    [item(font, NSLocalizedString(@"Bold", nil), fonts, @selector(addFontTrait:), @"b", cmd) setTag:NSBoldFontMask];
    [item(font, NSLocalizedString(@"Italic", nil), fonts, @selector(addFontTrait:), @"i", cmd) setTag:NSItalicFontMask];
    item(font, NSLocalizedString(@"Underline", nil), nil, @selector(underline:), @"u", cmd);
    [[format addItemWithTitle:NSLocalizedString(@"Font", nil) action:NULL keyEquivalent:@""] setSubmenu:font];
    NSMenu *textMenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Text", nil)];
    item(textMenu, NSLocalizedString(@"Show Ruler", nil), nil, @selector(toggleRuler:), @"", 0);
    [[format addItemWithTitle:NSLocalizedString(@"Text", nil) action:NULL keyEquivalent:@""] setSubmenu:textMenu];
    [format addItem:[NSMenuItem separatorItem]];
    item(format, NSLocalizedString(@"Make Rich Text", nil), self, @selector(toggleRichText:), @"t", shift);
    addMenu(NSLocalizedString(@"Format", nil), format);

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
    /* Started to open files (they came first): no new one besides. */
    if ([_documents count] > 0)
        return YES;
    [self newDocument:nil];
    return YES;
}

- (void)application:(NSApplication *)app openURLs:(NSArray<NSURL *> *)urls
{
    /* A new document left as it was made (the one made at launch, before
       the files came) gives way to them, as on the reference. */
    EditorDocumentWindow *blank = [_documents count] == 1 && ![_documents[0] fileURL] &&
        ![_documents[0] isEdited] ? _documents[0] : nil;
    for (NSURL *url in urls)
        if ([url isFileURL])
            [self openURL:url];
    if (blank != nil && [_documents count] > 1)
        [blank close];
}

/* A file: its window if it is open already, else a new one. */
- (void)openURL:(NSURL *)url
{
    NSString *path = [[url path] stringByStandardizingPath];

    for (EditorDocumentWindow *d in _documents)
        if ([[[d.fileURL path] stringByStandardizingPath] isEqualToString:path]) {
            [d show];
            return;
        }
    NSError *error = nil;
    EditorDocumentWindow *d = [[EditorDocumentWindow alloc] initWithURL:url error:&error];
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

- (void)add:(EditorDocumentWindow *)document
{
    document.delegate = self;
    [_documents addObject:document];
    [document show];
}

- (void)documentWindowDidClose:(EditorDocumentWindow *)document
{
    [_documents removeObject:document];
}

/* The document the menus act on: the key window's, else the main
   window's (a panel of the app's may be key). */
- (EditorDocumentWindow *)front
{
    for (NSWindow *window in @[ [NSApp keyWindow] ?: (id)[NSNull null], [NSApp mainWindow] ?: (id)[NSNull null] ])
        for (EditorDocumentWindow *d in _documents)
            if (d.window == window)
                return d;
    return nil;
}

- (void)newDocument:(id)sender
{
    [self add:[[EditorDocumentWindow alloc] initUntitled:++_untitled]];
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

- (void)toggleRichText:(id)sender
{
    [[self front] toggleRichText];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    SEL action = [menuItem action];

    if (action == @selector(toggleRichText:)) {
        EditorDocumentWindow *front = [self front];
        [menuItem setTitle:front != nil && [front isRichText] ? NSLocalizedString(@"Make Plain Text", nil)
                                                               : NSLocalizedString(@"Make Rich Text", nil)];
        return front != nil;
    }
    if (action == @selector(closeDocument:) || action == @selector(saveDocument:) ||
        action == @selector(saveDocumentAs:))
        return [self front] != nil;
    return YES;
}

/* Unsaved documents: each asked about before quitting. */
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)app
{
    for (EditorDocumentWindow *d in [_documents copy])
        if (d.edited && ![d close])
            return NSTerminateCancel;
    return NSTerminateNow;
}

@end

int main(void)
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        EditorApplication *delegate = [[EditorApplication alloc] init];

        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
