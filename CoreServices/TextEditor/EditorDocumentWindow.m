/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The text editor's document window: the text in a scrolling text view, the file's
 * name as the title (with a mark while there are unsaved changes), and a
 * question before unsaved changes are lost. A file keeps its encoding
 * (UTF-8 or UTF-16 by their byte order mark, UTF-8 when it is valid, else
 * Latin-1), its byte order mark and its line endings (LF, CRLF or CR; LF in
 * the window); Save As chooses both (Atlas.md, A2). An RTF file (by its
 * extension, or its first bytes) is rich text, saved as RTF; Make Rich
 * Text and Make Plain Text change a document's kind (plain text loses its
 * formatting, after a question), as on the reference.
 */

#import "EditorDocumentWindow.h"
#include <os/log.h>
#include <string.h>

@interface EditorDocumentWindow () <NSTextViewDelegate, NSWindowDelegate>
@end

@implementation EditorDocumentWindow {
    NSWindow *_window;
    NSTextView *_text;
    NSURL *_fileURL;
    NSString *_untitledName;
    NSStringEncoding _encoding;
    BOOL _byteOrderMark;        /* UTF-8's, kept when the file had it */
    NSString *_lineEnding;      /* @"\n", @"\r\n" or @"\r" */
    NSPopUpButton *_encodingChoice, *_lineEndingChoice;   /* Save As */
    BOOL _rich;                 /* RTF */
    NSString *_pendingDirectory;  /* where Save As starts, after a change of kind */
    BOOL _edited;
}

/* The encodings offered, as Save As lists them. */
static NSArray<NSArray *> *encodings(void)
{
    return @[ @[ @"Unicode (UTF-8)", @(NSUTF8StringEncoding) ],
              @[ @"Unicode (UTF-16)", @(NSUTF16StringEncoding) ],
              @[ @"Western (ISO Latin 1)", @(NSISOLatin1StringEncoding) ],
              @[ @"Western (Windows Latin 1)", @(NSWindowsCP1252StringEncoding) ] ];
}

static NSArray<NSArray *> *lineEndings(void)
{
    return @[ @[ @"LF (Unix)", @"\n" ], @[ @"CRLF (Windows)", @"\r\n" ], @[ @"CR", @"\r" ] ];
}

/* The text of a file's bytes: its encoding by its byte order mark, else
   UTF-8 when valid, else Latin-1 (every byte is one of its characters). */
static NSString *decode(NSData *data, NSStringEncoding *encoding, BOOL *bom)
{
    const unsigned char *b = [data bytes];
    NSUInteger n = [data length];

    *bom = NO;
    if (n >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) {
        *encoding = NSUTF8StringEncoding;
        *bom = YES;
        return [[NSString alloc] initWithData:[data subdataWithRange:NSMakeRange(3, n - 3)]
                                     encoding:NSUTF8StringEncoding];
    }
    if (n >= 2 && ((b[0] == 0xFF && b[1] == 0xFE) || (b[0] == 0xFE && b[1] == 0xFF))) {
        *encoding = NSUTF16StringEncoding;
        return [[NSString alloc] initWithData:data encoding:NSUTF16StringEncoding];
    }
    NSString *text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    if (text != nil) {
        *encoding = NSUTF8StringEncoding;
        return text;
    }
    *encoding = NSISOLatin1StringEncoding;
    return [[NSString alloc] initWithData:data encoding:NSISOLatin1StringEncoding];
}

/* The first line ending's kind (LF with none), and the text with LF. */
static NSString *lineEndingOf(NSString *text)
{
    NSRange r = [text rangeOfCharacterFromSet:[NSCharacterSet characterSetWithCharactersInString:@"\r\n"]];

    if (r.location == NSNotFound || [text characterAtIndex:r.location] == '\n')
        return @"\n";
    if (r.location + 1 < [text length] && [text characterAtIndex:r.location + 1] == '\n')
        return @"\r\n";
    return @"\r";
}

static NSString *withLF(NSString *text)
{
    text = [text stringByReplacingOccurrencesOfString:@"\r\n" withString:@"\n"];
    return [text stringByReplacingOccurrencesOfString:@"\r" withString:@"\n"];
}

static NSPoint nextOrigin;

- (void)makeWindowWithText:(id)text
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
    [_text setRichText:_rich];
    [_text setUsesFontPanel:_rich];
    [_text setUsesRuler:_rich];
    [_text setAllowsUndo:YES];
    if (_rich && [text isKindOfClass:[NSAttributedString class]]) {
        [[_text textStorage] setAttributedString:(NSAttributedString *)text];
        /* Typing at the start takes the text's formatting there. */
        if ([(NSAttributedString *)text length] > 0)
            [_text setTypingAttributes:[(NSAttributedString *)text attributesAtIndex:0 effectiveRange:NULL]];
    } else {
        /* Plain text in the user's fixed-pitch font (NSFixedPitchFont). */
        [_text setFont:_rich ? [NSFont userFontOfSize:13] : [NSFont userFixedPitchFontOfSize:13]];
        [_text setString:(NSString *)text];
    }
    /* The insertion point at the start, the top shown, as the reference
       opens a document. */
    [_text setSelectedRange:NSMakeRange(0, 0)];
    [_text scrollRangeToVisible:NSMakeRange(0, 0)];
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
    _lineEnding = @"\n";
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
    /* RTF: by its extension, or its first bytes. */
    static const char rtfStart[] = "{\\rtf";
    if ([[[url pathExtension] lowercaseString] isEqualToString:@"rtf"] ||
        ([data length] >= 5 && memcmp([data bytes], rtfStart, 5) == 0)) {
        NSAttributedString *rich = [[NSAttributedString alloc] initWithRTF:data documentAttributes:NULL];
        if (rich != nil) {
            _rich = YES;
            _encoding = NSUTF8StringEncoding;
            _lineEnding = @"\n";
            _fileURL = [url copy];
            [self makeWindowWithText:rich];
            os_log_info(OS_LOG_DEFAULT, "opened %{public}s (RTF)", [[url path] fileSystemRepresentation]);
            return self;
        }
    }
    NSString *text = decode(data, &_encoding, &_byteOrderMark);
    if (text == nil)
        return nil;
    _lineEnding = lineEndingOf(text);
    text = withLF(text);
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
    if (_rich) {
        NSData *rtf = [_text RTFFromRange:NSMakeRange(0, [text length])];
        if (rtf == nil || ![rtf writeToURL:url options:NSDataWritingAtomic error:&error]) {
            [self cannotSave:url error:error];
            return NO;
        }
        _fileURL = [url copy];
        os_log_info(OS_LOG_DEFAULT, "saved %{public}s (RTF)", [[url path] fileSystemRepresentation]);
        _edited = YES;
        [self setEdited:NO];
        return YES;
    }
    if (![_lineEnding isEqualToString:@"\n"])
        text = [text stringByReplacingOccurrencesOfString:@"\n" withString:_lineEnding];
    NSMutableData *data = [[text dataUsingEncoding:_encoding] mutableCopy];
    if (data == nil) {
        /* Characters the encoding does not have: UTF-8, said so. */
        os_log_info(OS_LOG_DEFAULT, "the text does not fit its encoding: saved as UTF-8");
        _encoding = NSUTF8StringEncoding;
        _byteOrderMark = NO;
        data = [[text dataUsingEncoding:NSUTF8StringEncoding] mutableCopy];
    }
    if (_encoding == NSUTF8StringEncoding && _byteOrderMark)
        [data replaceBytesInRange:NSMakeRange(0, 0) withBytes:"\xEF\xBB\xBF" length:3];

    if (data == nil || ![data writeToURL:url options:NSDataWritingAtomic error:&error]) {
        [self cannotSave:url error:error];
        return NO;
    }
    _fileURL = [url copy];
    os_log_info(OS_LOG_DEFAULT, "saved %{public}s", [[url path] fileSystemRepresentation]);
    _edited = YES;	/* the title follows the new name too */
    [self setEdited:NO];
    return YES;
}

- (void)cannotSave:(NSURL *)url error:(NSError *)error
{
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:[NSString stringWithFormat:NSLocalizedString(@"The document “%@” could not be saved.", nil),
                           [[url path] lastPathComponent]]];
    [alert setInformativeText:[error localizedDescription] ?: @""];
    [alert runModal];
}

- (BOOL)isRichText
{
    return _rich;
}

/* Make Rich Text / Make Plain Text: the document's kind changed, the
   text kept; to plain text, its formatting goes, after a question. The
   file's name takes the kind's extension when it is saved next. */
- (void)toggleRichText
{
    if (_rich) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:NSLocalizedString(@"Convert this document to plain text?", nil)];
        [alert setInformativeText:NSLocalizedString(@"Making a rich text document plain will lose all text styles (such as fonts and colors) and attachments.", nil)];
        [alert addButtonWithTitle:NSLocalizedString(@"OK", nil)];
        [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
        if ([alert runModal] != NSAlertFirstButtonReturn)
            return;
    }
    NSString *plain = [[_text string] copy];
    _rich = !_rich;
    [_text setRichText:_rich];
    [_text setUsesFontPanel:_rich];
    [_text setUsesRuler:_rich];
    if (!_rich) {
        [_text setRulerVisible:NO];
        [_text setFont:[NSFont userFixedPitchFontOfSize:13]];
        [_text setString:plain];
    } else
        [_text setFont:[NSFont userFontOfSize:13]];
    if (_fileURL != nil) {
        NSString *path = [[[_fileURL path] stringByDeletingPathExtension]
                          stringByAppendingPathExtension:_rich ? @"rtf" : @"txt"];
        _fileURL = nil;
        _untitledName = [[path lastPathComponent] stringByDeletingPathExtension];
        _pendingDirectory = [path stringByDeletingLastPathComponent];
    }
    _edited = NO;
    [self setEdited:YES];
}

/* Print…: the text view printed (AppKit's panel: a printer, or PDF),
   the document's name as the job's. */
- (void)print
{
    NSPrintInfo *info = [[NSPrintInfo sharedPrintInfo] copy];
    NSPrintOperation *operation = [NSPrintOperation printOperationWithView:_text printInfo:info];
    NSString *name = _fileURL ? [[_fileURL path] lastPathComponent] : _untitledName;

    [operation setJobTitle:[name stringByDeletingPathExtension]];
    [operation runOperation];
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
    NSString *extension = _rich ? @"rtf" : @"txt";
    [panel setAllowedFileTypes:@[ extension ]];
    [panel setAllowsOtherFileTypes:!_rich];

    [panel setNameFieldStringValue:_fileURL ? [[_fileURL path] lastPathComponent]
                                            : [_untitledName stringByAppendingPathExtension:extension]];
    if (_fileURL != nil)
        [panel setDirectoryURL:[_fileURL URLByDeletingLastPathComponent]];
    else if (_pendingDirectory != nil)
        [panel setDirectoryURL:[NSURL fileURLWithPath:_pendingDirectory isDirectory:YES]];
    /* The encoding and the line endings are plain text's. */
    if (!_rich)
        [panel setAccessoryView:[self saveOptionsView]];
    if ([panel runModal] != NSFileHandlingPanelOKButton || [panel URL] == nil)
        return NO;
    if (_rich)
        return [self writeTo:[panel URL]];
    NSStringEncoding chosen = [encodings()[(NSUInteger)[_encodingChoice indexOfSelectedItem]][1] unsignedIntegerValue];
    if (chosen != _encoding)
        _byteOrderMark = NO;
    _encoding = chosen;
    _lineEnding = lineEndings()[(NSUInteger)[_lineEndingChoice indexOfSelectedItem]][1];
    return [self writeTo:[panel URL]];
}

/* Save As's options: the encoding and the line endings, the document's
   own chosen. */
- (NSView *)saveOptionsView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 420, 70)];
    NSArray *rows = @[ @[ NSLocalizedString(@"Plain Text Encoding:", nil), encodings() ],
                       @[ NSLocalizedString(@"Line Endings:", nil), lineEndings() ] ];

    for (NSUInteger r = 0; r < [rows count]; r++) {
        NSTextField *label = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 40 - r * 32, 170, 20)];
        [label setStringValue:rows[r][0]];
        [label setEditable:NO];
        [label setBordered:NO];
        [label setDrawsBackground:NO];
        [label setAlignment:NSRightTextAlignment];
        [view addSubview:label];
        NSPopUpButton *popup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(176, 36 - r * 32, 230, 26) pullsDown:NO];
        NSInteger selected = 0;
        NSArray *choices = rows[r][1];
        for (NSUInteger i = 0; i < [choices count]; i++) {
            [popup addItemWithTitle:NSLocalizedString(choices[i][0], nil)];
            if (r == 0 ? [choices[i][1] unsignedIntegerValue] == _encoding : [choices[i][1] isEqual:_lineEnding])
                selected = (NSInteger)i;
        }
        [popup selectItemAtIndex:selected];
        [view addSubview:popup];
        if (r == 0)
            _encodingChoice = popup;
        else
            _lineEndingChoice = popup;
    }
    return view;
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
