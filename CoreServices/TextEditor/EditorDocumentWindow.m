/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The text editor's document window: the text in a scrolling text view, the file's
 * name as the title (with a mark while there are unsaved changes), saving
 * as UTF-8 in place, and a question before unsaved changes are lost.
 * Atlas's provisional look.
 */

#import "EditorDocumentWindow.h"
#include <os/log.h>

@interface EditorDocumentWindow () <NSTextViewDelegate, NSWindowDelegate>
@end

@implementation EditorDocumentWindow {
    NSWindow *_window;
    NSTextView *_text;
    NSURL *_fileURL;
    NSString *_untitledName;
    NSStringEncoding _encoding;
    BOOL _edited;
}

static NSPoint nextOrigin;

- (void)makeWindowWithText:(NSString *)text
{
    NSRect visible = [[NSScreen mainScreen] visibleFrame];
    NSRect frame = NSMakeRect(0, 0, MIN(640, NSWidth(visible) - 80), MIN(480, NSHeight(visible) - 80));

    /* Each new window a step down and right of the last. */
    if (NSEqualPoints(nextOrigin, NSZeroPoint) || nextOrigin.y - NSHeight(frame) < NSMinY(visible) ||
        nextOrigin.x + NSWidth(frame) > NSMaxX(visible))
        nextOrigin = NSMakePoint(NSMinX(visible) + 120, NSMaxY(visible) - 40);
    frame.origin = NSMakePoint(nextOrigin.x, nextOrigin.y - NSHeight(frame));
    nextOrigin = NSMakePoint(nextOrigin.x + 24, nextOrigin.y - 24);

    _window = [[NSWindow alloc] initWithContentRect:frame
        styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask
          backing:NSBackingStoreBuffered defer:NO];
    [_window setReleasedWhenClosed:NO];
    [_window setMinSize:NSMakeSize(240, 160)];
    [_window setDelegate:self];

    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:[[_window contentView] bounds]];
    [scroll setHasVerticalScroller:YES];
    [scroll setBorderType:NSNoBorder];
    [scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    NSSize size = [scroll contentSize];
    _text = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, size.width, size.height)];
    [_text setMinSize:NSMakeSize(0, size.height)];
    [_text setMaxSize:NSMakeSize(FLT_MAX, FLT_MAX)];
    [_text setVerticallyResizable:YES];
    [_text setHorizontallyResizable:NO];
    [_text setAutoresizingMask:NSViewWidthSizable];
    [[_text textContainer] setContainerSize:NSMakeSize(size.width, FLT_MAX)];
    [[_text textContainer] setWidthTracksTextView:YES];
    [_text setRichText:NO];
    [_text setAllowsUndo:YES];
    /* Plain text in the user's fixed-pitch font (NSFixedPitchFont). */
    [_text setFont:[NSFont userFixedPitchFontOfSize:13]];
    [_text setString:text];
    [_text setDelegate:self];
    [scroll setDocumentView:_text];
    [_window setContentView:scroll];
    [_window makeFirstResponder:_text];
    [self updateTitle];
}

- (instancetype)initUntitled:(NSUInteger)number
{
    if ((self = [super init]) == nil)
        return nil;
    _encoding = NSUTF8StringEncoding;
    _untitledName = number <= 1 ? NSLocalizedString(@"Untitled", nil)
        : [NSString stringWithFormat:NSLocalizedString(@"Untitled %lu", nil), (unsigned long)number];
    [self makeWindowWithText:@""];
    return self;
}

- (instancetype)initWithURL:(NSURL *)url error:(NSError **)error
{
    if ((self = [super init]) == nil)
        return nil;
    NSData *data = [NSData dataWithContentsOfURL:url options:0 error:error];
    if (data == nil)
        return nil;
    /* UTF-8, else Latin-1 (every byte is one of its characters). */
    _encoding = NSUTF8StringEncoding;
    NSString *text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    if (text == nil) {
        _encoding = NSISOLatin1StringEncoding;
        text = [[NSString alloc] initWithData:data encoding:NSISOLatin1StringEncoding];
    }
    if (text == nil)
        return nil;
    _fileURL = [url copy];
    [self makeWindowWithText:text];
    os_log_info(OS_LOG_DEFAULT, "opened %{public}s", [[url path] fileSystemRepresentation]);
    return self;
}

- (NSWindow *)window
{
    return _window;
}

- (NSURL *)fileURL
{
    return _fileURL;
}

- (BOOL)isEdited
{
    return _edited;
}

- (void)show
{
    [_window makeKeyAndOrderFront:nil];
}

- (void)updateTitle
{
    NSString *name = _fileURL ? [[_fileURL path] lastPathComponent] : _untitledName;

    [_window setTitle:_edited ? [NSString stringWithFormat:NSLocalizedString(@"%@ — Edited", nil), name] : name];
    [_window setDocumentEdited:_edited];
}

- (void)setEdited:(BOOL)edited
{
    if (edited == _edited)
        return;
    _edited = edited;
    [self updateTitle];
}

- (void)textDidChange:(NSNotification *)note
{
    [self setEdited:YES];
}

/* -- Saving -- */

- (BOOL)writeTo:(NSURL *)url
{
    NSError *error = nil;
    NSString *text = [_text string];
    NSData *data = [text dataUsingEncoding:_encoding] ?: [text dataUsingEncoding:NSUTF8StringEncoding];

    if (data == nil || ![data writeToURL:url options:NSDataWritingAtomic error:&error]) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithFormat:NSLocalizedString(@"The document “%@” could not be saved.", nil),
                               [[url path] lastPathComponent]]];
        [alert setInformativeText:[error localizedDescription] ?: @""];
        [alert runModal];
        return NO;
    }
    _fileURL = [url copy];
    os_log_info(OS_LOG_DEFAULT, "saved %{public}s", [[url path] fileSystemRepresentation]);
    _edited = YES;	/* the title follows the new name too */
    [self setEdited:NO];
    return YES;
}

- (BOOL)save
{
    if (_fileURL == nil)
        return [self saveAs];
    return [self writeTo:_fileURL];
}

- (BOOL)saveAs
{
    NSSavePanel *panel = [NSSavePanel savePanel];
    [panel setAllowedFileTypes:@[ @"txt" ]];
    [panel setAllowsOtherFileTypes:YES];

    [panel setNameFieldStringValue:_fileURL ? [[_fileURL path] lastPathComponent]
                                            : [_untitledName stringByAppendingPathExtension:@"txt"]];
    if (_fileURL != nil)
        [panel setDirectoryURL:[_fileURL URLByDeletingLastPathComponent]];
    if ([panel runModal] != NSFileHandlingPanelOKButton || [panel URL] == nil)
        return NO;
    return [self writeTo:[panel URL]];
}

/* -- Closing -- */

- (BOOL)windowShouldClose:(id)sender
{
    if (!_edited)
        return YES;
    NSString *name = _fileURL ? [[_fileURL path] lastPathComponent] : _untitledName;
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:[NSString stringWithFormat:
        NSLocalizedString(@"Do you want to save the changes made to the document “%@”?", nil), name]];
    [alert setInformativeText:NSLocalizedString(@"Your changes will be lost if you don't save them.", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Save", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Don't Save", nil)];
    switch ([alert runModal]) {
    case NSAlertFirstButtonReturn:
        return [self save];
    case NSAlertThirdButtonReturn:
        return YES;
    default:
        return NO;
    }
}

- (void)windowWillClose:(NSNotification *)note
{
    [self.delegate documentWindowDidClose:self];
}

- (BOOL)close
{
    if (![self windowShouldClose:_window])
        return NO;
    [_window close];
    return YES;
}

@end
