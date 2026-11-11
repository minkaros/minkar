/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSControl.h>
#import <AppKit/NSButtonCell.h>

@interface NSButton : NSControl

/* A push button (rounded bezel), with a title, an image, or both; a
   checkbox; a radio button. Sized to fit. */
+ (instancetype)buttonWithTitle:(NSString *)title image:(NSImage *)image target:(id)target action:(SEL)action;
+ (instancetype)buttonWithTitle:(NSString *)title target:(id)target action:(SEL)action;
+ (instancetype)buttonWithImage:(NSImage *)image target:(id)target action:(SEL)action;
+ (instancetype)checkboxWithTitle:(NSString *)title target:(id)target action:(SEL)action;
+ (instancetype)radioButtonWithTitle:(NSString *)title target:(id)target action:(SEL)action;

- (BOOL)isTransparent;
- (NSString *)keyEquivalent;
- (NSUInteger)keyEquivalentModifierMask;
- (NSImage *)image;
- (NSCellImagePosition)imagePosition;
- (NSString *)title;
- (NSControlStateValue)state;
- (BOOL)allowsMixedState;
- (NSSound *)sound;

- (NSBezelStyle)bezelStyle;
- (NSString *)alternateTitle;
- (NSImage *)alternateImage;
- (NSAttributedString *)attributedTitle;
- (NSAttributedString *)attributedAlternateTitle;
- (BOOL)showsBorderOnlyWhileMouseInside;
- (void)getPeriodicDelay:(float *)delay interval:(float *)interval;

- (void)setTransparent:(BOOL)value;
- (void)setKeyEquivalent:(NSString *)value;
- (void)setKeyEquivalentModifierMask:(NSUInteger)value;
- (void)setImage:(NSImage *)value;
- (void)setImagePosition:(NSCellImagePosition)value;
- (void)setTitle:(NSString *)value;
- (void)setState:(NSControlStateValue)value;
- (void)setNextState;
- (void)setAllowsMixedState:(BOOL)value;
- (void)setSound:(NSSound *)value;

- (void)setBezelStyle:(NSBezelStyle)value;
- (void)setAlternateTitle:(NSString *)value;
- (void)setAlternateImage:(NSImage *)value;
- (void)setAttributedTitle:(NSAttributedString *)value;
- (void)setAttributedAlternateTitle:(NSAttributedString *)value;
- (void)setShowsBorderOnlyWhileMouseInside:(BOOL)value;
- (void)setPeriodicDelay:(float)delay interval:(float)interval;

- (void)setButtonType:(NSButtonType)value;
- (void)setTitleWithMnemonic:(NSString *)value;

- (void)highlight:(BOOL)value;
- (BOOL)performKeyEquivalent:(NSEvent *)event;
- (void)performClick:sender;

/* Minkar: as properties too, for dot syntax. */
@property (getter=isTransparent) BOOL transparent;
@end

/* Minkar: the image's scaling, the cell's (API 10.5). */
@interface NSButton (MinkarImageScaling)
@property NSImageScaling imageScaling;
@end
