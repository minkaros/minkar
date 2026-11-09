/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <Foundation/Foundation.h>
#import <AppKit/AppKitExport.h>

@class NSEvent, NSAttributedString;

typedef NSString *NSTextInputSourceIdentifier;

APPKIT_EXPORT NSString *const NSTextInputContextKeyboardSelectionDidChangeNotification;

@protocol NSTextInputClient
- (void)insertText:(id)string replacementRange:(NSRange)replacementRange;
- (void)doCommandBySelector:(SEL)selector;
- (void)setMarkedText:(id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange;
- (void)unmarkText;
- (NSRange)selectedRange;
- (NSRange)markedRange;
- (BOOL)hasMarkedText;
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actualRange;
- (NSArray *)validAttributesForMarkedText;
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange;
- (NSUInteger)characterIndexForPoint:(NSPoint)point;
@end

/* A text input client's link to the input methods. There are no input
   methods yet (keys go to the client as they come): the context keeps the
   client and its keyboard input source, and discarding marked text
   unmarks the client's. */
@interface NSTextInputContext : NSObject {
    id _client;
    BOOL _acceptsGlyphInfo;
    NSArray *_allowedInputSourceLocales;
}

+ (NSTextInputContext *)currentInputContext;
- (instancetype)initWithClient:(id<NSTextInputClient>)client;

@property (readonly) id<NSTextInputClient> client;
@property BOOL acceptsGlyphInfo;
@property (copy) NSArray<NSString *> *allowedInputSourceLocales;
@property (readonly) NSArray<NSTextInputSourceIdentifier> *keyboardInputSources;
@property (copy) NSTextInputSourceIdentifier selectedKeyboardInputSource;

+ (NSString *)localizedNameForInputSource:(NSTextInputSourceIdentifier)inputSourceIdentifier;
- (void)activate;
- (void)deactivate;
- (BOOL)handleEvent:(NSEvent *)event;
- (void)discardMarkedText;
- (void)invalidateCharacterCoordinates;

@end
