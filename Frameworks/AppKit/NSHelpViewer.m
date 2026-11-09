/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * An app's help book, shown by AppKit itself (-[NSApplication showHelp:])
 * until Atlas has its Help app (19.6.x): one window for the app with the
 * book's page, Back and Home. A link inside the book opens its page in
 * the window; any other goes to the app that opens it.
 */

#import <AppKit/NSApplication.h>
#import <AppKit/NSAttributedString.h>
#import <AppKit/NSButton.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSScrollView.h>
#import <AppKit/NSTextStorage.h>
#import <AppKit/NSTextView.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSWorkspace.h>
#import <Foundation/Foundation.h>
#import "NSHelpViewer.h"
#import "NSHTMLReader.h"

@interface NSHelpViewer () <NSTextViewDelegate>
@end

@implementation NSHelpViewer {
    NSWindow       *_window;
    NSTextView     *_textView;
    NSButton       *_back;
    NSURL          *_home;
    NSURL          *_page;
    NSMutableArray *_history;
}

+ (NSHelpViewer *)sharedViewer
{
    static NSHelpViewer *shared;

    if (shared == nil)
        shared = [[NSHelpViewer alloc] init];
    return shared;
}

- (void)dealloc
{
    [_window release];
    [_home release];
    [_page release];
    [_history release];
    [super dealloc];
}

- (NSButton *)button:(NSString *)imageName action:(SEL)action x:(CGFloat)x
{
    NSButton *button = [[[NSButton alloc] initWithFrame:NSMakeRect(x, 0, 32, 24)] autorelease];

    [button setBezelStyle:NSRoundedBezelStyle];
    [button setImage:[NSImage imageNamed:imageName]];
    [button setImagePosition:NSImageOnly];
    [button setTarget:self];
    [button setAction:action];
    [button setAutoresizingMask:NSViewMinYMargin];
    return button;
}

- (void)makeWindow
{
    NSRect frame = NSMakeRect(0, 0, 600, 480);
    NSScrollView *scroll;
    NSView *content;

    _window = [[NSWindow alloc] initWithContentRect:frame
        styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask
        backing:NSBackingStoreBuffered defer:NO];
    [_window setReleasedWhenClosed:NO];
    [_window setDelegate:(id)self];
    content = [_window contentView];

    _back = [self button:NSImageNameGoBackTemplate action:@selector(goBack:) x:12];
    [_back setFrameOrigin:NSMakePoint(12, NSHeight(frame) - 34)];
    [content addSubview:_back];
    NSButton *home = [self button:NSImageNameHomeTemplate action:@selector(goHome:) x:48];
    [home setFrameOrigin:NSMakePoint(48, NSHeight(frame) - 34)];
    [content addSubview:home];

    scroll = [[[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, NSWidth(frame), NSHeight(frame) - 44)] autorelease];
    [scroll setHasVerticalScroller:YES];
    [scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    _textView = [[[NSTextView alloc] initWithFrame:[[scroll contentView] bounds]] autorelease];
    [_textView setEditable:NO];
    [_textView setSelectable:YES];
    [_textView setTextContainerInset:NSMakeSize(16, 12)];
    [_textView setAutoresizingMask:NSViewWidthSizable];
    [_textView setDelegate:self];
    [scroll setDocumentView:_textView];
    [content addSubview:scroll];
    [_window center];
}

- (BOOL)loadPage:(NSURL *)url
{
    NSData *data = [NSData dataWithContentsOfURL:url];
    NSAttributedString *page;

    if (data == nil)
        return NO;
    page = [[[NSAttributedString alloc] initWithHTML:data baseURL:url documentAttributes:NULL] autorelease];
    if (page == nil)
        return NO;
    [[_textView textStorage] setAttributedString:page];
    [_textView scrollRangeToVisible:NSMakeRange(0, 0)];
    [_page release];
    _page = [url copy];
    [_back setEnabled:[_history count] > 0];
    return YES;
}

/* Scrolls to where the page's anchor is (an id, or <a name>). */
- (void)scrollToAnchor:(NSString *)anchor
{
    NSTextStorage *text = [_textView textStorage];
    NSUInteger i, length = [text length];

    for (i = 0; i < length; i++) {
        NSRange range;
        id value = [text attribute:NSHTMLAnchorAttributeName atIndex:i effectiveRange:&range];

        if ([value isEqual:anchor]) {
            [_textView scrollRangeToVisible:NSMakeRange(range.location, MIN(length - range.location, (NSUInteger)200))];
            [_textView setSelectedRange:NSMakeRange(range.location, 0)];
            return;
        }
        i = NSMaxRange(range) - 1;
    }
}

- (void)showPage:(NSURL *)page anchor:(NSString *)anchor home:(NSURL *)home title:(NSString *)title
{
    BOOL sameBook = (_window != nil && [_home isEqual:home]);

    if (_window == nil)
        [self makeWindow];
    [_window setTitle:title];
    if (!sameBook) {
        [_home release];
        _home = [home copy];
        [_history release];
        _history = [[NSMutableArray alloc] init];
    } else if (_page != nil && ![_page isEqual:page])
        [_history addObject:_page];
    [self loadPage:page];
    if ([anchor length])
        [self scrollToAnchor:anchor];
    [_window makeKeyAndOrderFront:nil];
}

- (void)showBookAtURL:(NSURL *)index title:(NSString *)title
{
    [self showPage:index anchor:nil home:index title:title];
}

- (void)goBack:(id)sender
{
    NSURL *previous = [[[_history lastObject] retain] autorelease];

    if (previous == nil)
        return;
    [_history removeLastObject];
    [self loadPage:previous];
}

- (void)goHome:(id)sender
{
    if (_home != nil && ![_home isEqual:_page]) {
        [_history addObject:_page];
        [self loadPage:_home];
    }
}

/* A page of the book: in the window, with Back to the one before. */
- (BOOL)textView:(NSTextView *)textView clickedOnLink:(id)link atIndex:(NSUInteger)charIndex
{
    NSURL *url = [link isKindOfClass:[NSURL class]] ? link : [NSURL URLWithString:[link description]];
    NSString *folder = [[[_home path] stringByDeletingLastPathComponent] stringByStandardizingPath];

    if (url == nil || ![url isFileURL] || folder == nil ||
            ![[[url path] stringByStandardizingPath] hasPrefix:folder])
        return NO;
    NSURL *previous = [[_page retain] autorelease];
    NSURL *page = [NSURL fileURLWithPath:[url path]];
    if ([self loadPage:page] && previous != nil) {
        [_history addObject:previous];
        [_back setEnabled:YES];
    }
    if ([[url fragment] length])
        [self scrollToAnchor:[url fragment]];
    return YES;
}

/* A closed window is not shown again: the next book or page makes one. */
- (void)windowWillClose:(NSNotification *)note
{
    if ([note object] != _window)
        return;
    [_window setDelegate:nil];
    [_window autorelease];
    _window = nil;
    _textView = nil;
    _back = nil;
    [_page release];
    _page = nil;
    [_home release];
    _home = nil;
}

- (NSWindow *)window
{
    return _window;
}

- (NSString *)pageText
{
    return [_textView string];
}

@end
