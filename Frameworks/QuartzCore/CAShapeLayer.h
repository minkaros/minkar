/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <QuartzCore/CALayer.h>

CA_EXPORT NSString *const kCAFillRuleNonZero;
CA_EXPORT NSString *const kCAFillRuleEvenOdd;
CA_EXPORT NSString *const kCALineJoinMiter;
CA_EXPORT NSString *const kCALineJoinRound;
CA_EXPORT NSString *const kCALineJoinBevel;
CA_EXPORT NSString *const kCALineCapButt;
CA_EXPORT NSString *const kCALineCapRound;
CA_EXPORT NSString *const kCALineCapSquare;

/* A layer that draws a path: filled, then stroked. strokeStart and
   strokeEnd are kept; the whole path is stroked while strokeEnd is past
   strokeStart. */
@interface CAShapeLayer : CALayer {
    CGPathRef _path;
    CGColorRef _fillColor;
    CGColorRef _strokeColor;
    NSString *_fillRule;
    CGFloat _lineWidth;
    CGFloat _miterLimit;
    NSString *_lineCap;
    NSString *_lineJoin;
    CGFloat _lineDashPhase;
    NSArray *_lineDashPattern;
    CGFloat _strokeStart;
    CGFloat _strokeEnd;
}

@property CGPathRef path;
@property CGColorRef fillColor;
@property(copy) NSString *fillRule;
@property CGColorRef strokeColor;
@property CGFloat strokeStart;
@property CGFloat strokeEnd;
@property CGFloat lineWidth;
@property CGFloat miterLimit;
@property(copy) NSString *lineCap;
@property(copy) NSString *lineJoin;
@property CGFloat lineDashPhase;
@property(copy) NSArray *lineDashPattern;

@end
