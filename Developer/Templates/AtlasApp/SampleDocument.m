/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The template's document: text in UTF-8, shown in a text view. It
 * autosaves in place, so it keeps versions (Revert To, Browse All
 * Versions…) and asks nothing when it is closed or the app quits.
 */

#import "SampleDocument.h"

@interface SampleDocument () <NSTextViewDelegate>
@end

@implementation SampleDocument {
    NSTextView *_textView;
}

+ (BOOL)autosavesInPlace
{
    return YES;
}

- (instancetype)init
{
    if ((self = [super init]) != nil)
        _text = @"";
    return self;
}

- (void)makeWindowControllers
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 520, 380)
        styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask
        backing:NSBackingStoreBuffered defer:NO];
    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:[[window contentView] bounds]];

    _textView = [[NSTextView alloc] initWithFrame:[[scroll contentView] bounds]];
    [_textView setString:self.text];
    [_textView setDelegate:self];
    [_textView setAutoresizingMask:NSViewWidthSizable];
    [scroll setDocumentView:_textView];
    [scroll setHasVerticalScroller:YES];
    [scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [[window contentView] addSubview:scroll];
    [window center];

    NSWindowController *controller = [[NSWindowController alloc] initWithWindow:window];
    [self addWindowController:controller];
}

/* An edit makes the document edited, and so autosaved soon. */
- (void)textDidChange:(NSNotification *)note
{
    self.text = [_textView string];
    [self updateChangeCount:NSChangeDone];
}

- (NSData *)dataOfType:(NSString *)type error:(NSError **)error
{
    return [self.text dataUsingEncoding:NSUTF8StringEncoding];
}

- (BOOL)readFromData:(NSData *)data ofType:(NSString *)type error:(NSError **)error
{
    NSString *text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];

    if (text == nil) {
        if (error != NULL)
            *error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadInapplicableStringEncodingError userInfo:nil];
        return NO;
    }
    self.text = text;
    [_textView setString:text];
    return YES;
}

@end
