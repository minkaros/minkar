/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSView.h>
#import <AppKit/NSCell.h>

@class NSTimer;

/* Minkar: the names of API 10.14 beside the older ones. */
typedef NS_ENUM(NSUInteger, NSProgressIndicatorStyle) {
    NSProgressIndicatorStyleBar = 0,
    NSProgressIndicatorStyleSpinning = 1,
    NSProgressIndicatorBarStyle = NSProgressIndicatorStyleBar,
    NSProgressIndicatorSpinningStyle = NSProgressIndicatorStyleSpinning,
};

@interface NSProgressIndicator : NSView {
    double _minValue;
    double _maxValue;
    double _value;
    NSTimeInterval _animationDelay;
    NSTimer *_animationTimer;
    double _animationValue;
    NSProgressIndicatorStyle _style;
    NSControlSize _size;
    NSControlTint _tint;
    BOOL _displayWhenStopped;
    BOOL _isBezeled;
    BOOL _isIndeterminate;
    BOOL _usesThreadedAnimation;
    BOOL _endThreadedAnimation;
}

- (NSProgressIndicatorStyle)style;
- (NSControlSize)controlSize;
- (NSControlTint)controlTint;
- (BOOL)isDisplayedWhenStopped;
- (BOOL)usesThreadedAnimation;

- (double)minValue;
- (double)maxValue;
- (double)doubleValue;

- (NSTimeInterval)animationDelay;
- (BOOL)isIndeterminate;
- (BOOL)isBezeled;

- (void)setStyle:(NSProgressIndicatorStyle)value;
- (void)setControlSize:(NSControlSize)value;
- (void)setControlTint:(NSControlTint)value;
- (void)setDisplayedWhenStopped:(BOOL)value;
- (void)setUsesThreadedAnimation:(BOOL)value;

- (void)setMinValue:(double)value;
- (void)setMaxValue:(double)value;
- (void)setDoubleValue:(double)value;
- (void)setAnimationDelay:(double)value;
- (void)setIndeterminate:(BOOL)value;
- (void)setBezeled:(BOOL)value;

- (void)incrementBy:(double)value;

- (void)sizeToFit;

- (void)startAnimation:sender;
- (void)stopAnimation:sender;
- (void)animate:sender;

/* Minkar: as properties too, for dot syntax. */
@property (getter=isBezeled) BOOL bezeled;
@property (getter=isDisplayedWhenStopped) BOOL displayedWhenStopped;
@property (getter=isIndeterminate) BOOL indeterminate;
@end
