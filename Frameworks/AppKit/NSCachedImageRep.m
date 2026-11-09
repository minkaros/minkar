/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <AppKit/NSCachedImageRep.h>
#import <AppKit/NSGraphicsContextFunctions.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSGraphicsContext.h>
#import <CoreGraphics/CGWindow.h>
#import <AppKit/NSWindow-Private.h>

@implementation NSCachedImageRep

-initWithWindow:(NSWindow *)window rect:(NSRect)rect {
   _size=rect.size;
   _window=[window retain];
   _origin=rect.origin;

// this is a little broken, the windows get resized to larger size when on-screen
	if([[NSUserDefaults standardUserDefaults] boolForKey:@"NSShowAllWindows"])
    [_window orderFront:nil];
   return self;
}

/* An offscreen cache (as the reference platform's image caches are now):
   a bitmap context, with no window in the window server. It held a
   private window, which the server listed and composited. */
-initWithSize:(NSSize)size depth:(NSWindowDepth)windowDepth separate:(BOOL)separateWindow alpha:(BOOL)hasAlpha {
   size_t width=(size_t)MAX(ceil(size.width),1),height=(size_t)MAX(ceil(size.height),1);
   CGColorSpaceRef colorSpace=CGColorSpaceCreateDeviceRGB();

   _bitmapContext=CGBitmapContextCreate(NULL,width,height,8,0,colorSpace,
      kCGImageAlphaPremultipliedFirst|kCGBitmapByteOrder32Little);
   CGColorSpaceRelease(colorSpace);
   if(_bitmapContext==NULL){
      [self release];
      return nil;
   }
   _size=size;
   _origin=NSZeroPoint;
   return self;
}

-(CGContextRef)_graphicsPort {
   return _bitmapContext!=NULL ? _bitmapContext : [_window cgContext];
}

-(void)dealloc {
   if(_bitmapContext!=NULL)
      CGContextRelease(_bitmapContext);
   [_window release];
   [super dealloc];
}

-(NSWindow *)window {
   return _window;
}

-(NSRect)rect {
   return NSMakeRect(_origin.x,_origin.y,_size.width,_size.height);
}

-(BOOL)drawAtPoint:(NSPoint)point {
   NSRect     rect={point,_size};
   CGImageRef imageRef=CGBitmapContextCreateImage([self _graphicsPort]);
   
   CGContextDrawImage(NSCurrentGraphicsPort(),rect,imageRef);
   
   CGImageRelease(imageRef);
   
   return YES;
}

-(BOOL)drawInRect:(NSRect)rect {
   CGImageRef imageRef=CGBitmapContextCreateImage([self _graphicsPort]);
   
   CGContextDrawImage(NSCurrentGraphicsPort(),rect,imageRef);
   
   CGImageRelease(imageRef);
   
   return YES;
}

- (BOOL)draw
{
	return [self drawInRect:[self rect]];
}
@end
