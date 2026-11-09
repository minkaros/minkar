/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
                 2009 Markus Hitter <mah@jump-ing.de>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

// Original - Christopher Lloyd <cjwl@objc.net>
#import <AppKit/NSStringDrawing.h>
#import <AppKit/NSAttributedString.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSStringDrawer.h>
#import <AppKit/NSRaise.h>

@interface NSStringDrawingContext (Minkar)
-(void)_setTotalBounds:(NSRect)bounds;
@end

/* Text is never scaled down to fit (actualScaleFactor stays 1). */
@implementation NSStringDrawingContext

-init {
   _minimumScaleFactor=0;
   _actualScaleFactor=1;
   _totalBounds=NSZeroRect;
   return self;
}

-(CGFloat)minimumScaleFactor {
   return _minimumScaleFactor;
}

-(void)setMinimumScaleFactor:(CGFloat)value {
   _minimumScaleFactor=value;
}

-(CGFloat)actualScaleFactor {
   return _actualScaleFactor;
}

-(NSRect)totalBounds {
   return _totalBounds;
}

-(void)_setTotalBounds:(NSRect)bounds {
   _totalBounds=bounds;
   _actualScaleFactor=1;
}

@end

@implementation NSString(NSStringDrawing)

-(void)drawAtPoint:(NSPoint)point withAttributes:(NSDictionary *)attributes {
   [[NSStringDrawer sharedStringDrawer] drawString:self withAttributes:attributes atPoint:point inSize:NSZeroSize];
}

-(void)drawInRect:(NSRect)rect withAttributes:(NSDictionary *)attributes {
   [[NSStringDrawer sharedStringDrawer] drawString:self withAttributes:attributes inRect:rect];
}

-(NSSize)sizeWithAttributes:(NSDictionary *)attributes {
   return [[NSStringDrawer sharedStringDrawer] sizeOfString:self withAttributes:attributes inSize:NSZeroSize];
}

- (NSRect)boundingRectWithSize:(NSSize)size options:(NSStringDrawingOptions)options attributes:(NSDictionary *)attributes {
	NSAttributedString *string=[[[NSAttributedString alloc] initWithString:self attributes:attributes] autorelease];

	return [string boundingRectWithSize:size options:options];
}

- (NSRect)boundingRectWithSize:(NSSize)size options:(NSStringDrawingOptions)options attributes:(NSDictionary *)attributes context:(NSStringDrawingContext *)context {
	NSAttributedString *string=[[[NSAttributedString alloc] initWithString:self attributes:attributes] autorelease];

	return [string boundingRectWithSize:size options:options context:context];
}

- (void)drawWithRect:(NSRect)rect options:(NSStringDrawingOptions)options attributes:(NSDictionary *)attributes context:(NSStringDrawingContext *)context {
	NSAttributedString *string=[[[NSAttributedString alloc] initWithString:self attributes:attributes] autorelease];

	[string drawWithRect:rect options:options context:context];
}

- (void)drawWithRect:(NSRect)rect options:(NSStringDrawingOptions)options attributes:(NSDictionary *)attributes {
	NSAttributedString *string=[[[NSAttributedString alloc] initWithString:self attributes:attributes] autorelease];

	[string drawWithRect:rect options:options];
}

@end

@implementation NSAttributedString(NSStringDrawing)

-(void)drawAtPoint:(NSPoint)point {
   [[NSStringDrawer sharedStringDrawer] drawAttributedString:self atPoint:point inSize:NSZeroSize];
}

-(void)drawInRect:(NSRect)rect {
   [[NSStringDrawer sharedStringDrawer] drawAttributedString:self inRect:rect];
}

-(NSSize)size {
   return [[NSStringDrawer sharedStringDrawer] sizeOfAttributedString:self inSize:NSZeroSize];
}

/* With NSStringDrawingUsesLineFragmentOrigin, laid out in rect from its
   first line; without it, on one line, rect's origin being the start of
   the baseline. */
-(void)drawWithRect:(NSRect)rect options:(NSStringDrawingOptions)options {
   if(options&NSStringDrawingUsesLineFragmentOrigin){
    [self drawInRect:rect];
    return;
   }

   NSFont *font=([self length]>0)?[self attribute:NSFontAttributeName atIndex:0 effectiveRange:NULL]:nil;
   BOOL    flipped=[[NSGraphicsContext currentContext] isFlipped];

   if(font==nil)
    font=[NSFont systemFontOfSize:12];
   [self drawAtPoint:NSMakePoint(rect.origin.x,flipped?rect.origin.y-[font ascender]:rect.origin.y+[font descender])];
}

-(NSRect)boundingRectWithSize:(NSSize)size options:(NSStringDrawingOptions)options context:(NSStringDrawingContext *)context {
   NSRect result=[self boundingRectWithSize:size options:options];

   [context _setTotalBounds:result];
   return result;
}

-(void)drawWithRect:(NSRect)rect options:(NSStringDrawingOptions)options context:(NSStringDrawingContext *)context {
   NSRect bounds=[self boundingRectWithSize:rect.size options:options];

   [self drawWithRect:rect options:options];
   [context _setTotalBounds:NSOffsetRect(bounds,rect.origin.x,rect.origin.y)];
}

@end
