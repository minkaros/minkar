/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSControl.h>

@interface NSTextField : NSControl {
    id _delegate;
    SEL _errorAction;
    CGFloat _preferredMaxLayoutWidth;
    NSInteger _maximumNumberOfLines;
    BOOL _allowsDefaultTighteningForTruncation;
}

/* A label: not editable nor selectable, without a bezel or background,
   on one line. A wrapping label wraps its words and can be selected. A
   text field is editable on one line. All in the system font, sized to
   fit their text. */
+ (instancetype)labelWithString:(NSString *)stringValue;
+ (instancetype)wrappingLabelWithString:(NSString *)stringValue;
+ (instancetype)labelWithAttributedString:(NSAttributedString *)attributedStringValue;
+ (instancetype)textFieldWithString:(NSString *)stringValue;

/* The width at which a wrapping field breaks its lines for its intrinsic
   size; 0 for none. */
@property CGFloat preferredMaxLayoutWidth;
/* 0 for no limit. */
@property NSInteger maximumNumberOfLines;
@property BOOL allowsDefaultTighteningForTruncation;
@property BOOL allowsEditingTextAttributes;
@property BOOL importsGraphics;

- delegate;
- (void)setDelegate:delegate;

- (NSColor *)backgroundColor;
- (NSColor *)textColor;
- (BOOL)drawsBackground;

- (void)setBackgroundColor:(NSColor *)color;
- (void)setTextColor:(NSColor *)color;
- (void)setDrawsBackground:(BOOL)flag;

- (void)selectText:sender;

/* Shown, grey, while the field is empty (API 19's, from its cell). */
@property(copy) NSString *placeholderString;
@property(copy) NSAttributedString *placeholderAttributedString;

- previousText;
- nextText;

- (void)setPreviousText:text;
- (void)setNextText:text;

- (void)setTitleWithMnemonic:(NSString *)value;

@end
