/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Find panel text views share (-[NSTextView performFindPanelAction:],
 * usesFindPanel): one panel for the app, with the text to find and its
 * replacement, Ignore Case, and Replace All, Replace, Replace & Find,
 * Previous and Next. It acts on the text view in the main window (the
 * first responder there), or on the one that asked. The text to find is
 * the find pasteboard's (NSPasteboardNameFind), so every app shares it,
 * as on the reference. Replacements go through the text view
 * (replaceCharactersInRange:withString:, didChangeText), so they can be
 * undone.
 */

#import <AppKit/NSTextView.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSTextField.h>
#import <AppKit/NSButton.h>
#import <AppKit/NSPasteboard.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSTextStorage.h>
#import <AppKit/NSMenuItem.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import "NSFindPanel.h"

@implementation NSFindPanel {
    NSPanel *_panel;
    NSTextField *_find, *_replace, *_status;
    NSButton *_ignoreCase;
    NSTextView *_asking;        /* weak: the text view that last asked */
}

+ (NSFindPanel *)sharedFindPanel {
    static NSFindPanel *shared;

    if (shared == nil)
        shared = [[NSFindPanel alloc] init];
    return shared;
}

static NSString *text(NSString *english) {
    return [[NSBundle bundleForClass:[NSFindPanel class]] localizedStringForKey:english value:english table:nil];
}

static NSTextField *label(NSString *title, NSRect frame) {
    NSTextField *field = [[[NSTextField alloc] initWithFrame:frame] autorelease];
    [field setStringValue:title];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setBordered:NO];
    [field setDrawsBackground:NO];
    [field setAlignment:NSRightTextAlignment];
    return field;
}

- (NSButton *)button:(NSString *)title frame:(NSRect)frame tag:(NSInteger)tag {
    NSButton *button = [[[NSButton alloc] initWithFrame:frame] autorelease];
    [button setTitle:title];
    [button setBezelStyle:NSRoundedBezelStyle];
    [button setTag:tag];
    [button setTarget:self];
    [button setAction:@selector(buttonPressed:)];
    [[_panel contentView] addSubview:button];
    return button;
}

- (void)makePanel {
    _panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 470, 160)
        styleMask:NSTitledWindowMask | NSClosableWindowMask | NSUtilityWindowMask
          backing:NSBackingStoreBuffered defer:NO];
    [_panel setTitle:text(@"Find")];
    [_panel setFloatingPanel:YES];
    [_panel setHidesOnDeactivate:YES];
    [_panel setReleasedWhenClosed:NO];
    [_panel setBecomesKeyOnlyIfNeeded:NO];
    NSView *content = [_panel contentView];

    [content addSubview:label(text(@"Find:"), NSMakeRect(10, 122, 96, 20))];
    _find = [[NSTextField alloc] initWithFrame:NSMakeRect(110, 120, 344, 24)];
    [_find setTarget:self];
    [_find setAction:@selector(findFieldReturn:)];
    [content addSubview:_find];
    [content addSubview:label(text(@"Replace with:"), NSMakeRect(10, 90, 96, 20))];
    _replace = [[NSTextField alloc] initWithFrame:NSMakeRect(110, 88, 344, 24)];
    [content addSubview:_replace];
    _ignoreCase = [[NSButton alloc] initWithFrame:NSMakeRect(108, 60, 200, 22)];
    [_ignoreCase setButtonType:NSSwitchButton];
    [_ignoreCase setTitle:text(@"Ignore Case")];
    [_ignoreCase setState:NSOnState];
    [content addSubview:_ignoreCase];
    _status = label(@"", NSMakeRect(300, 62, 154, 18));
    [_status setFont:[NSFont systemFontOfSize:11]];
    [content addSubview:_status];

    [self button:text(@"Replace All") frame:NSMakeRect(10, 14, 96, 28) tag:NSFindPanelActionReplaceAll];
    [self button:text(@"Replace") frame:NSMakeRect(110, 14, 80, 28) tag:NSFindPanelActionReplace];
    [self button:text(@"Replace & Find") frame:NSMakeRect(194, 14, 110, 28) tag:NSFindPanelActionReplaceAndFind];
    [self button:text(@"Previous") frame:NSMakeRect(308, 14, 70, 28) tag:NSFindPanelActionPrevious];
    NSButton *next = [self button:text(@"Next") frame:NSMakeRect(382, 14, 72, 28) tag:NSFindPanelActionNext];
    [next setKeyEquivalent:@"\r"];
    /* Tab goes Find, Replace with, Ignore Case and back. */
    [_find setNextKeyView:_replace];
    [_replace setNextKeyView:_ignoreCase];
    [_ignoreCase setNextKeyView:_find];
    [_panel setInitialFirstResponder:_find];
    [_panel center];
}

/* The text to find: the find pasteboard's, which every app shares. */
- (NSString *)findString {
    NSString *shared = [[NSPasteboard pasteboardWithName:NSPasteboardNameFind] stringForType:NSPasteboardTypeString];

    if (_panel != nil && [[_find stringValue] length] > 0)
        return [_find stringValue];
    return shared ?: @"";
}

- (void)setFindString:(NSString *)string {
    NSPasteboard *board = [NSPasteboard pasteboardWithName:NSPasteboardNameFind];

    [board declareTypes:@[ NSPasteboardTypeString ] owner:nil];
    [board setString:string ?: @"" forType:NSPasteboardTypeString];
    [_find setStringValue:string ?: @""];
}

- (NSTextView *)target {
    NSResponder *responder = [[NSApp mainWindow] firstResponder];

    if ([responder isKindOfClass:[NSTextView class]] && [(NSTextView *)responder usesFindPanel])
        return (NSTextView *)responder;
    return _asking;
}

- (NSStringCompareOptions)options {
    return _panel == nil || [_ignoreCase state] == NSOnState ? NSCaseInsensitiveSearch : 0;
}

- (void)setStatus:(NSString *)status {
    [_status setStringValue:status ?: @""];
}

/* The next (or previous) match after (before) the selection, wrapping
   round; NSNotFound with none. */
- (NSRange)findIn:(NSTextView *)view backwards:(BOOL)backwards {
    NSString *string = [view string], *find = [self findString];
    NSRange selected = [view selectedRange];
    NSUInteger length = [string length];
    NSStringCompareOptions options = [self options];
    NSRange found;

    if ([find length] == 0 || length == 0)
        return NSMakeRange(NSNotFound, 0);
    if (backwards) {
        found = [string rangeOfString:find options:options | NSBackwardsSearch range:NSMakeRange(0, selected.location)];
        if (found.location == NSNotFound)
            found = [string rangeOfString:find options:options | NSBackwardsSearch range:NSMakeRange(0, length)];
    } else {
        NSUInteger from = NSMaxRange(selected);
        found = [string rangeOfString:find options:options range:NSMakeRange(from, length - from)];
        if (found.location == NSNotFound)
            found = [string rangeOfString:find options:options range:NSMakeRange(0, length)];
    }
    return found;
}

- (BOOL)find:(NSTextView *)view backwards:(BOOL)backwards {
    NSRange found = [self findIn:view backwards:backwards];

    if (found.location == NSNotFound) {
        NSBeep();
        [self setStatus:text(@"Not found")];
        return NO;
    }
    [self setStatus:@""];
    [view setSelectedRange:found];
    [view scrollRangeToVisible:found];
    return YES;
}

/* One replacement, through the text view: its delegate asked, and undone
   by Undo (replaceCharactersInRange:withString: registers it). */
- (BOOL)replace:(NSRange)range in:(NSTextView *)view with:(NSString *)replacement {
    NSUInteger before = [[view string] length];

    if (![view isEditable])
        return NO;
    [view replaceCharactersInRange:range withString:replacement];
    if ([[view string] length] == before - range.length + [replacement length] &&
        [[[view string] substringWithRange:NSMakeRange(range.location, [replacement length])] isEqualToString:replacement]) {
        [view didChangeText];
        return YES;
    }
    return NO;
}

/* The selection, when it is a match, replaced. */
- (BOOL)replaceSelectionIn:(NSTextView *)view {
    NSRange selected = [view selectedRange];
    NSString *find = [self findString];

    if (selected.length == 0 || [[[view string] substringWithRange:selected]
            compare:find options:[self options]] != NSOrderedSame)
        return NO;
    NSString *replacement = [_replace stringValue] ?: @"";
    if (![self replace:selected in:view with:replacement])
        return NO;
    [view setSelectedRange:NSMakeRange(selected.location + [replacement length], 0)];
    return YES;
}

/* Every match in range replaced, the last first so the others stay put;
   one undo step. */
- (NSUInteger)replaceAllIn:(NSTextView *)view range:(NSRange)range {
    NSString *string = [view string], *find = [self findString], *replacement = [_replace stringValue] ?: @"";
    NSMutableArray *matches = [NSMutableArray array];
    NSRange search = range, found;

    if ([find length] == 0 || ![view isEditable])
        return 0;
    while ((found = [string rangeOfString:find options:[self options] range:search]).location != NSNotFound) {
        [matches addObject:[NSValue valueWithRange:found]];
        search = NSMakeRange(NSMaxRange(found), NSMaxRange(range) - NSMaxRange(found));
    }
    if ([matches count] == 0)
        return 0;
    [[view undoManager] beginUndoGrouping];
    NSUInteger done = 0;
    for (NSValue *value in [matches reverseObjectEnumerator])
        if ([self replace:[value rangeValue] in:view with:replacement])
            done++;
    [[view undoManager] endUndoGrouping];
    return done;
}

- (void)selectAllIn:(NSTextView *)view range:(NSRange)range {
    NSString *string = [view string], *find = [self findString];
    NSMutableArray *ranges = [NSMutableArray array];
    NSRange search = range, found;

    while ([find length] > 0 &&
           (found = [string rangeOfString:find options:[self options] range:search]).location != NSNotFound) {
        [ranges addObject:[NSValue valueWithRange:found]];
        search = NSMakeRange(NSMaxRange(found), NSMaxRange(range) - NSMaxRange(found));
    }
    if ([ranges count] == 0) {
        NSBeep();
        [self setStatus:text(@"Not found")];
        return;
    }
    [view setSelectedRanges:ranges];
}

- (void)perform:(NSFindPanelAction)action for:(NSTextView *)view {
    _asking = view;
    switch (action) {
    case NSFindPanelActionShowFindPanel:
        if (_panel == nil)
            [self makePanel];
        [_find setStringValue:[[NSPasteboard pasteboardWithName:NSPasteboardNameFind]
                                  stringForType:NSPasteboardTypeString] ?: @""];
        [self setStatus:@""];
        [_panel makeKeyAndOrderFront:nil];
        [_find selectText:nil];
        break;
    case NSFindPanelActionNext:
    case NSFindPanelActionPrevious:
        if (view != nil)
            [self find:view backwards:action == NSFindPanelActionPrevious];
        break;
    case NSFindPanelActionReplace:
        if (view != nil && ![self replaceSelectionIn:view])
            NSBeep();
        break;
    case NSFindPanelActionReplaceAndFind:
        if (view != nil) {
            [self replaceSelectionIn:view];
            [self find:view backwards:NO];
        }
        break;
    case NSFindPanelActionReplaceAll:
    case NSFindPanelActionReplaceAllInSelection: {
        if (view == nil)
            break;
        NSRange range = action == NSFindPanelActionReplaceAll ? NSMakeRange(0, [[view string] length])
                                                              : [view selectedRange];
        NSUInteger done = [self replaceAllIn:view range:range];
        if (done == 0)
            NSBeep();
        [self setStatus:done == 0 ? text(@"Not found")
                                  : [NSString stringWithFormat:text(@"%lu replaced"), (unsigned long)done]];
        break;
    }
    case NSFindPanelActionSetFindString:
        if (view != nil && [view selectedRange].length > 0)
            [self setFindString:[[view string] substringWithRange:[view selectedRange]]];
        break;
    case NSFindPanelActionSelectAll:
        if (view != nil)
            [self selectAllIn:view range:NSMakeRange(0, [[view string] length])];
        break;
    case NSFindPanelActionSelectAllInSelection:
        if (view != nil)
            [self selectAllIn:view range:[view selectedRange]];
        break;
    }
}

/* The panel's own buttons, and Return in the Find field (Next). */
- (void)buttonPressed:(NSButton *)sender {
    [self setFindString:[_find stringValue]];
    [self perform:(NSFindPanelAction)[sender tag] for:[self target]];
}

- (void)findFieldReturn:(id)sender {
    [self setFindString:[_find stringValue]];
    [self perform:NSFindPanelActionNext for:[self target]];
}

- (BOOL)validateAction:(NSFindPanelAction)action for:(NSTextView *)view {
    BOOL haveFind = [[self findString] length] > 0;

    switch (action) {
    case NSFindPanelActionShowFindPanel:
        return YES;
    case NSFindPanelActionSetFindString:
        return [view selectedRange].length > 0;
    case NSFindPanelActionReplace:
    case NSFindPanelActionReplaceAndFind:
    case NSFindPanelActionReplaceAll:
    case NSFindPanelActionReplaceAllInSelection:
        return haveFind && [view isEditable];
    default:
        return haveFind;
    }
}

@end

/* usesFindPanel (YES unless set) and usesFindBar, kept with the view. */
static char kUsesFindPanelKey, kUsesFindBarKey;

@implementation NSTextView (NSFindPanel)

-(BOOL)usesFindPanel {
    NSNumber *value = objc_getAssociatedObject(self, &kUsesFindPanelKey);
    return value != nil ? [value boolValue] : YES;
}

-(void)setUsesFindPanel:(BOOL)flag {
    objc_setAssociatedObject(self, &kUsesFindPanelKey, [NSNumber numberWithBool:flag], OBJC_ASSOCIATION_RETAIN);
}

/* The find bar is not drawn yet: a view that asks for it gets the panel. */
-(BOOL)usesFindBar {
    return [objc_getAssociatedObject(self, &kUsesFindBarKey) boolValue];
}

-(void)setUsesFindBar:(BOOL)flag {
    objc_setAssociatedObject(self, &kUsesFindBarKey, [NSNumber numberWithBool:flag], OBJC_ASSOCIATION_RETAIN);
}

-(void)performFindPanelAction:(id)sender {
    if (![self usesFindPanel] && ![self usesFindBar]) {
        NSBeep();
        return;
    }
    [[NSFindPanel sharedFindPanel] perform:(NSFindPanelAction)[sender tag] for:self];
}

-(BOOL)_validateFindPanelAction:(id)sender {
    if (![self usesFindPanel] && ![self usesFindBar])
        return NO;
    return [[NSFindPanel sharedFindPanel] validateAction:(NSFindPanelAction)[sender tag] for:self];
}

@end
