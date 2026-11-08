/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSLevelIndicatorCell.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSRaise.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSView.h>
#import <AppKit/NSWindow.h>
#import <Foundation/NSKeyedArchiver.h>

@implementation NSLevelIndicatorCell

/* Drawn in code in Atlas's style, in the system colors. */

#define TICKLENGTH 4

-initWithLevelIndicatorStyle:(NSLevelIndicatorStyle)style {
   [super initTextCell:@""];
   _style=style;
   _minValue=0;
   _maxValue=(_style==NSLevelIndicatorStyleContinuousCapacity)?100:5;
   _warningValue=0;
   _criticalValue=0;
   _numberOfTickMarks=0;
   _numberOfMajorTickMarks=0;
   _tickMarkPosition=NSTickMarkBelow;
   [self setDoubleValue:0];
   return self;
}

-init {
   return [self initWithLevelIndicatorStyle:NSLevelIndicatorStyleRelevancy];
}

-initTextCell:(NSString *)string {
   return [self initWithLevelIndicatorStyle:NSLevelIndicatorStyleRelevancy];
}

-initWithCoder:(NSCoder *)coder {
   [super initWithCoder:coder];

   if([coder allowsKeyedCoding]){
    _style=[coder decodeIntegerForKey:@"NSIndicatorStyle"];
    _minValue=[coder decodeDoubleForKey:@"NSMinValue"];
    _maxValue=[coder decodeDoubleForKey:@"NSMaxValue"];
    _warningValue=[coder decodeDoubleForKey:@"NSWarningValue"];
    _criticalValue=[coder decodeDoubleForKey:@"NSCriticalValue"];
    _numberOfTickMarks=[coder decodeIntForKey:@"NSNumberOfTickMarks"];
    _numberOfMajorTickMarks=[coder decodeIntForKey:@"NSNumberOfMajorTickMarks"];
    _tickMarkPosition=[coder decodeIntegerForKey:@"NSTickMarkPosition"];
    if([coder containsValueForKey:@"NSValue"])
     [self setDoubleValue:[coder decodeDoubleForKey:@"NSValue"]];
   }
   return self;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   [super encodeWithCoder:coder];
   if([coder allowsKeyedCoding]){
    [coder encodeInteger:_style forKey:@"NSIndicatorStyle"];
    [coder encodeDouble:_minValue forKey:@"NSMinValue"];
    [coder encodeDouble:_maxValue forKey:@"NSMaxValue"];
    [coder encodeDouble:_warningValue forKey:@"NSWarningValue"];
    [coder encodeDouble:_criticalValue forKey:@"NSCriticalValue"];
    [coder encodeInt:_numberOfTickMarks forKey:@"NSNumberOfTickMarks"];
    [coder encodeInt:_numberOfMajorTickMarks forKey:@"NSNumberOfMajorTickMarks"];
    [coder encodeInteger:_tickMarkPosition forKey:@"NSTickMarkPosition"];
    [coder encodeDouble:[self doubleValue] forKey:@"NSValue"];
   }
}

-(NSLevelIndicatorStyle)levelIndicatorStyle {
   return _style;
}

-(double)minValue {
   return _minValue;
}

-(double)maxValue {
   return _maxValue;
}

-(double)warningValue {
   return _warningValue;
}

-(double)criticalValue {
   return _criticalValue;
}

-(int)numberOfTickMarks {
   return _numberOfTickMarks;
}

-(int)numberOfMajorTickMarks {
   return _numberOfMajorTickMarks;
}

-(NSTickMarkPosition)tickMarkPosition {
   return _tickMarkPosition;
}

-(void)setLevelIndicatorStyle:(NSLevelIndicatorStyle)style {
   _style=style;
}

-(void)setMinValue:(double)value {
   _minValue=value;
}

-(void)setMaxValue:(double)value {
   _maxValue=value;
}

-(void)setWarningValue:(double)value {
   _warningValue=value;
}

-(void)setCriticalValue:(double)value {
   _criticalValue=value;
}

-(void)setNumberOfTickMarks:(int)count {
   _numberOfTickMarks=MAX(0,count);
}

-(void)setNumberOfMajorTickMarks:(int)count {
   _numberOfMajorTickMarks=MAX(0,count);
}

-(void)setTickMarkPosition:(NSTickMarkPosition)position {
   _tickMarkPosition=position;
}

-(void)setImage:(NSImage *)image {
}

-(double)tickMarkValueAtIndex:(int)index {
   if(_numberOfTickMarks<=1)
    return _minValue;
   return _minValue+(_maxValue-_minValue)*index/(_numberOfTickMarks-1);
}

/* The level's part of the frame: the frame less the tick marks' room. */
-(NSRect)_levelRectForFrame:(NSRect)frame flipped:(BOOL)flipped {
   if(_numberOfTickMarks>0 && _style!=NSLevelIndicatorStyleRating){
    BOOL below=(_tickMarkPosition==NSTickMarkBelow);

    frame.size.height-=TICKLENGTH+1;
    if(below!=flipped)
     frame.origin.y+=TICKLENGTH+1;
   }
   return frame;
}

-(NSRect)rectOfTickMarkAtIndex:(int)index {
   NSRect  frame=_lastFrame;
   BOOL    flipped=[[self controlView] isFlipped];
   NSRect  level=[self _levelRectForFrame:frame flipped:flipped];
   CGFloat x=(_numberOfTickMarks<=1)?NSMidX(level):NSMinX(level)+2+(NSWidth(level)-5)*index/(_numberOfTickMarks-1);
   BOOL    major=(_numberOfMajorTickMarks>0 && _numberOfTickMarks>1 &&
      index%MAX(1,(_numberOfTickMarks-1)/MAX(1,_numberOfMajorTickMarks-1))==0);
   CGFloat length=major?TICKLENGTH:TICKLENGTH-1;
   BOOL    atBottom=((_tickMarkPosition==NSTickMarkBelow)!=flipped);

   return NSMakeRect(floor(x),atBottom?NSMinY(frame):NSMaxY(frame)-length,1,length);
}

/* The fraction of the range the value is at, 0 to 1. */
-(double)_fraction {
   if(_maxValue<=_minValue)
    return 0;
   return MAX(0,MIN(1,([self doubleValue]-_minValue)/(_maxValue-_minValue)));
}

/* Green, then the warning's yellow, then the critical red, as the value
   passes them; when the warning is above the critical value, lower is worse. */
-(NSColor *)_levelColor {
   double value=[self doubleValue];

   if(_warningValue==0 && _criticalValue==0)
    return [NSColor systemGreenColor];
   if(_warningValue<=_criticalValue){
    if(value>=_criticalValue)
     return [NSColor systemRedColor];
    if(value>=_warningValue)
     return [NSColor systemYellowColor];
   }
   else {
    if(value<=_criticalValue)
     return [NSColor systemRedColor];
    if(value<=_warningValue)
     return [NSColor systemYellowColor];
   }
   return [NSColor systemGreenColor];
}

static void fillRounded(NSRect rect,CGFloat radius,NSColor *color){
   if(NSWidth(rect)<=0 || NSHeight(rect)<=0)
    return;
   radius=MIN(radius,MIN(NSWidth(rect),NSHeight(rect))/2);
   [color setFill];
   [[NSBezierPath bezierPathWithRoundedRect:rect xRadius:radius yRadius:radius] fill];
}

static void strokeRounded(NSRect rect,CGFloat radius,NSColor *color){
   NSBezierPath *path;

   rect=NSInsetRect(rect,0.5,0.5);
   if(NSWidth(rect)<=0 || NSHeight(rect)<=0)
    return;
   radius=MIN(radius,MIN(NSWidth(rect),NSHeight(rect))/2);
   path=[NSBezierPath bezierPathWithRoundedRect:rect xRadius:radius yRadius:radius];
   [color setStroke];
   [path setLineWidth:1];
   [path stroke];
}

/* A five-pointed star in rect. */
static NSBezierPath *starPath(NSRect rect){
   NSBezierPath *path=[NSBezierPath bezierPath];
   CGFloat       outer=MIN(NSWidth(rect),NSHeight(rect))/2,inner=outer*0.42;
   NSPoint       c=NSMakePoint(NSMidX(rect),NSMidY(rect));
   int           i;

   for(i=0;i<10;i++){
    CGFloat r=(i%2)?inner:outer,angle=M_PI_2+i*M_PI/5;
    NSPoint p=NSMakePoint(c.x+r*cos(angle),c.y+r*sin(angle));

    if(i==0)
     [path moveToPoint:p];
    else
     [path lineToPoint:p];
   }
   [path closePath];
   return path;
}

-(void)drawWithFrame:(NSRect)frame inView:(NSView *)controlView {
   BOOL    flipped=[controlView isFlipped];
   NSRect  level;
   double  fraction=[self _fraction];
   NSColor *track=[NSColor colorWithCalibratedWhite:0.88 alpha:1];
   NSColor *border=[NSColor colorWithCalibratedWhite:0.70 alpha:1];
   BOOL    enabled=[self isEnabled];
   int     i;

   _lastFrame=frame;
   [self setControlView:controlView];
   level=[self _levelRectForFrame:frame flipped:flipped];

   /* As the reference platform draws them: the empty part light grey, the
      level in its color; relevancy dark grey stripes on a grey track;
      rating small stars, dark for the value and light for the rest. */
   NSColor *empty=[NSColor colorWithCalibratedWhite:0.80 alpha:1];
   NSColor *edge=[NSColor colorWithCalibratedWhite:0.72 alpha:1];

   switch(_style){

    case NSLevelIndicatorStyleContinuousCapacity:{
      NSRect bar=NSInsetRect(level,0,MAX(0,floor((NSHeight(level)-14)/2)));
      NSRect fill=bar;

      fillRounded(bar,3,empty);
      fill.size.width=floor(NSWidth(bar)*fraction);
      if(NSWidth(fill)>0){
       [NSGraphicsContext saveGraphicsState];
       NSRectClip(fill);
       fillRounded(bar,3,enabled?[self _levelColor]:track);
       [NSGraphicsContext restoreGraphicsState];
      }
      strokeRounded(bar,3,edge);
     }
     break;

    case NSLevelIndicatorStyleDiscreteCapacity:{
      NSRect  bar=NSInsetRect(level,0,MAX(0,floor((NSHeight(level)-14)/2)));
      int     count=MAX(1,(int)floor(_maxValue-_minValue));
      CGFloat gap=1,width=(NSWidth(bar)-gap*(count-1))/count;
      int     lit=(int)floor([self doubleValue]-_minValue+0.5);
      NSColor *color=[self _levelColor];

      for(i=0;i<count;i++){
       NSRect segment=NSMakeRect(floor(NSMinX(bar)+i*(width+gap)),NSMinY(bar),floor(width),NSHeight(bar));

       fillRounded(segment,2,(i<lit && enabled)?color:empty);
       strokeRounded(segment,2,edge);
      }
     }
     break;

    case NSLevelIndicatorStyleRelevancy:{
      NSRect bar=NSInsetRect(level,0,MAX(0,floor((NSHeight(level)-10)/2)));
      int    width=(int)floor(NSWidth(bar)*fraction);

      [[NSColor colorWithCalibratedWhite:0.77 alpha:1] setFill];
      NSRectFill(bar);
      for(i=0;i<width;i+=2){
       [[NSColor colorWithCalibratedWhite:(enabled?0.45:0.65) alpha:1] setFill];
       NSRectFill(NSMakeRect(NSMinX(bar)+i,NSMinY(bar),1,NSHeight(bar)));
       [[NSColor colorWithCalibratedWhite:(enabled?0.62:0.72) alpha:1] setFill];
       NSRectFill(NSMakeRect(NSMinX(bar)+i+1,NSMinY(bar),1,NSHeight(bar)));
      }
     }
     break;

    case NSLevelIndicatorStyleRating:{
      int     count=MAX(1,(int)floor(_maxValue-_minValue));
      CGFloat side=MIN(NSHeight(level),12);
      double  value=[self doubleValue]-_minValue;
      NSColor *filled=enabled?[NSColor colorWithCalibratedWhite:0.40 alpha:1]:[NSColor tertiaryLabelColor];
      NSColor *unfilled=[NSColor colorWithCalibratedWhite:0.77 alpha:1];

      for(i=0;i<count;i++){
       NSRect star=NSMakeRect(NSMinX(level)+i*(side+1),floor(NSMidY(level)-side/2),side,side);

       [(value>=i+0.5?filled:unfilled) setFill];
       [starPath(star) fill];
      }
     }
     break;
   }

   if(_style!=NSLevelIndicatorStyleRating){
    [[NSColor secondaryLabelColor] setFill];
    for(i=0;i<_numberOfTickMarks;i++)
     NSRectFill([self rectOfTickMarkAtIndex:i]);
   }
}

/* An editable indicator takes the value under the mouse: whole stars and
   segments, any value for the continuous ones. */
-(void)_setValueFromPoint:(NSPoint)point inView:(NSView *)view {
   NSRect level=[self _levelRectForFrame:_lastFrame flipped:[view isFlipped]];
   double value;

   if(_style==NSLevelIndicatorStyleRating){
    CGFloat side=MIN(NSHeight(level),15);

    value=_minValue+ceil((point.x-NSMinX(level))/(side+2));
   }
   else {
    double fraction=(NSWidth(level)>0)?(point.x-NSMinX(level))/NSWidth(level):0;

    value=_minValue+MAX(0,MIN(1,fraction))*(_maxValue-_minValue);
    if(_style==NSLevelIndicatorStyleDiscreteCapacity)
     value=ceil(value);
   }
   [self setDoubleValue:MAX(_minValue,MIN(_maxValue,value))];
   [view setNeedsDisplay:YES];
}

-(BOOL)startTrackingAt:(NSPoint)startPoint inView:(NSView *)controlView {
   if(![self isEditable])
    return NO;
   [self _setValueFromPoint:[controlView convertPoint:startPoint fromView:nil] inView:controlView];
   return YES;
}

-(BOOL)continueTracking:(NSPoint)lastPoint at:(NSPoint)currentPoint inView:(NSView *)controlView {
   if(![self isEditable])
    return NO;
   [self _setValueFromPoint:[controlView convertPoint:currentPoint fromView:nil] inView:controlView];
   return YES;
}

-(NSSize)cellSize {
   if(_style==NSLevelIndicatorStyleRating){
    int count=MAX(1,(int)floor(_maxValue-_minValue));

    return NSMakeSize(count*17-2,15);
   }
   return NSMakeSize(100,(_numberOfTickMarks>0)?19:14);
}

@end
