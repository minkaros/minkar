/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <QuartzCore/CAShapeLayer.h>

NSString *const kCAFillRuleNonZero=@"non-zero";
NSString *const kCAFillRuleEvenOdd=@"even-odd";
NSString *const kCALineJoinMiter=@"miter";
NSString *const kCALineJoinRound=@"round";
NSString *const kCALineJoinBevel=@"bevel";
NSString *const kCALineCapButt=@"butt";
NSString *const kCALineCapRound=@"round";
NSString *const kCALineCapSquare=@"square";

@implementation CAShapeLayer

-init {
   self=[super init];
   if(self!=nil){
    CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
    CGFloat         black[4]={ 0, 0, 0, 1 };
    _fillColor=CGColorCreate(space,black);
    CGColorSpaceRelease(space);
    _fillRule=[kCAFillRuleNonZero copy];
    _lineWidth=1;
    _miterLimit=10;
    _lineCap=[kCALineCapButt copy];
    _lineJoin=[kCALineJoinMiter copy];
    _strokeEnd=1;
   }
   return self;
}

-(void)dealloc {
   CGPathRelease(_path);
   CGColorRelease(_fillColor);
   CGColorRelease(_strokeColor);
   [_fillRule release];
   [_lineCap release];
   [_lineJoin release];
   [_lineDashPattern release];
   [super dealloc];
}

-(CGPathRef)path { return _path; }
-(void)setPath:(CGPathRef)path {
   path=CGPathRetain(path);
   CGPathRelease(_path);
   _path=path;
   [self setNeedsDisplay];
}

-(CGColorRef)fillColor { return _fillColor; }
-(void)setFillColor:(CGColorRef)color {
   color=CGColorRetain(color);
   CGColorRelease(_fillColor);
   _fillColor=color;
   [self setNeedsDisplay];
}

-(CGColorRef)strokeColor { return _strokeColor; }
-(void)setStrokeColor:(CGColorRef)color {
   color=CGColorRetain(color);
   CGColorRelease(_strokeColor);
   _strokeColor=color;
   [self setNeedsDisplay];
}

#define COPY_PROPERTY(getter,setter,ivar) \
   -(NSString *)getter { return ivar; } \
   -(void)setter:(NSString *)value { value=[value copy]; [ivar release]; ivar=value; [self setNeedsDisplay]; }

COPY_PROPERTY(fillRule,setFillRule,_fillRule)
COPY_PROPERTY(lineCap,setLineCap,_lineCap)
COPY_PROPERTY(lineJoin,setLineJoin,_lineJoin)

#define FLOAT_PROPERTY(getter,setter,ivar) \
   -(CGFloat)getter { return ivar; } \
   -(void)setter:(CGFloat)value { ivar=value; [self setNeedsDisplay]; }

FLOAT_PROPERTY(strokeStart,setStrokeStart,_strokeStart)
FLOAT_PROPERTY(strokeEnd,setStrokeEnd,_strokeEnd)
FLOAT_PROPERTY(lineWidth,setLineWidth,_lineWidth)
FLOAT_PROPERTY(miterLimit,setMiterLimit,_miterLimit)
FLOAT_PROPERTY(lineDashPhase,setLineDashPhase,_lineDashPhase)

-(NSArray *)lineDashPattern { return _lineDashPattern; }
-(void)setLineDashPattern:(NSArray *)pattern {
   pattern=[pattern copy];
   [_lineDashPattern release];
   _lineDashPattern=pattern;
   [self setNeedsDisplay];
}

-(void)drawInContext:(CGContextRef)context {
   if(_path==NULL){
    [super drawInContext:context];
    return;
   }
   CGContextSaveGState(context);
   if(_fillColor!=NULL && CGColorGetAlpha(_fillColor)>0){
    CGContextAddPath(context,_path);
    CGContextSetFillColorWithColor(context,_fillColor);
    if([_fillRule isEqualToString:kCAFillRuleEvenOdd])
     CGContextEOFillPath(context);
    else
     CGContextFillPath(context);
   }
   if(_strokeColor!=NULL && _lineWidth>0 && _strokeEnd>_strokeStart){
    CGContextAddPath(context,_path);
    CGContextSetStrokeColorWithColor(context,_strokeColor);
    CGContextSetLineWidth(context,_lineWidth);
    CGContextSetMiterLimit(context,_miterLimit);
    CGContextSetLineCap(context,[_lineCap isEqualToString:kCALineCapRound]?kCGLineCapRound:
                                [_lineCap isEqualToString:kCALineCapSquare]?kCGLineCapSquare:kCGLineCapButt);
    CGContextSetLineJoin(context,[_lineJoin isEqualToString:kCALineJoinRound]?kCGLineJoinRound:
                                 [_lineJoin isEqualToString:kCALineJoinBevel]?kCGLineJoinBevel:kCGLineJoinMiter);
    if([_lineDashPattern count]>0){
     NSUInteger count=[_lineDashPattern count];
     CGFloat    lengths[count];
     for(NSUInteger i=0;i<count;i++)
      lengths[i]=[[_lineDashPattern objectAtIndex:i] doubleValue];
     CGContextSetLineDash(context,_lineDashPhase,lengths,count);
    }
    CGContextStrokePath(context);
   }
   CGContextRestoreGState(context);
}

@end
