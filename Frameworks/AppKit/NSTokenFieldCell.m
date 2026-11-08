/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSTokenFieldCell.h>
#import <Foundation/NSCharacterSet.h>
#import <Foundation/NSArray.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSStringDrawing.h>
#import <AppKit/NSAttributedString.h>
#import <AppKit/NSGraphicsContext.h>

@implementation NSTokenFieldCell

+(NSTimeInterval)defaultCompletionDelay {
   return 0;
}

+(NSCharacterSet *)defaultTokenizingCharacterSet {
   return [NSCharacterSet characterSetWithCharactersInString:@","];
}

-(NSTokenStyle)tokenStyle {
   return _style;
}

-(NSCharacterSet *)tokenizingCharacterSet {
   return _set;
}

-(NSTimeInterval)completionDelay {
   return _completionDelay;
}

-delegate {
   return _delegate;
}

-(void)setTokenStyle:(NSTokenStyle)style {
   _style=style;
}

-(void)setTokenizingCharacterSet:(NSCharacterSet *)set {
   set=[set copy];
   [_set release];
   _set=set;
}

-(void)setCompletionDelay:(NSTimeInterval)delay {
   _completionDelay=delay;
}

-(void)setDelegate:delegate {
   _delegate=delegate;
}

/* The text a token shows: the delegate's, or the object's description. */
-(NSString *)_displayStringForToken:(id)token {
   SEL display=@selector(tokenFieldCell:displayStringForRepresentedObject:);

   if([_delegate respondsToSelector:display]){
    NSString *string=[_delegate performSelector:display withObject:self withObject:token];
    if(string!=nil)
     return string;
   }
   return [token description];
}

/* An array value is drawn as tokens: light blue rounded pills, each with
   its text, side by side (as the reference platform shows them); any
   other value as text. */
-(void)drawInteriorWithFrame:(NSRect)frame inView:(NSView *)view {
   id value=[self objectValue];

   if(![value isKindOfClass:[NSArray class]] || [value count]==0){
    [super drawInteriorWithFrame:frame inView:view];
    return;
   }

   NSFont       *font=[self font]?:[NSFont systemFontOfSize:12];
   NSDictionary *attributes=[NSDictionary dictionaryWithObjectsAndKeys:font,NSFontAttributeName,
      [self isEnabled]?[NSColor controlTextColor]:[NSColor disabledControlTextColor],NSForegroundColorAttributeName,nil];
   NSRect        inner=NSInsetRect(frame,4,2);
   CGFloat       x=NSMinX(inner);
   NSColor      *fill=[NSColor colorWithCalibratedRed:175/255.0 green:205/255.0 blue:1.0 alpha:1];

   [NSGraphicsContext saveGraphicsState];
   NSRectClip(inner);
   for(id token in value){
    NSString *text=[self _displayStringForToken:token];
    NSSize    size=[text sizeWithAttributes:attributes];
    CGFloat   height=MIN(NSHeight(inner),ceil(size.height)+2);
    NSRect    pill=NSMakeRect(x,floor(NSMidY(inner)-height/2),ceil(size.width)+16,height);

    if(x>=NSMaxX(inner))
     break;
    if(_style!=NSPlainTextTokenStyle){
     [fill setFill];
     [[NSBezierPath bezierPathWithRoundedRect:pill xRadius:4 yRadius:4] fill];
    }
    [text drawAtPoint:NSMakePoint(NSMinX(pill)+8,floor(NSMidY(pill)-size.height/2)) withAttributes:attributes];
    x=NSMaxX(pill)+6;
   }
   [NSGraphicsContext restoreGraphicsState];
}

@end
