/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSCursor.h>
#import <AppKit/NSDisplay.h>
#import <AppKit/NSImage.h>
#import <Foundation/NSNull.h>
#import <AppKit/NSRaise.h>
#import <AppKit/NSGraphicsContext.h>

/* With the new window server: NSCursorImage.m. */
extern int _CGUsesSkyLight(void);
id _NSCursorSkyLightWithName(NSString *name);
id _NSCursorSkyLightWithImage(NSImage *image, NSPoint hotSpot);
void _NSCursorSkyLightSet(id object);

id NSPlatformCreateCursorImpWithName(NSString *name) {
   if(_CGUsesSkyLight())
    return _NSCursorSkyLightWithName(name);
   return [[[NSDisplay currentDisplay] cursorWithName:name] retain];
}

id NSPlatformCreateCursorImpWithImage(NSImage *image,NSPoint hotSpot) {
   if(_CGUsesSkyLight())
    return _NSCursorSkyLightWithImage(image,hotSpot);
   return nil;
}

void NSPlatformReleaseCursorImp(id object){
   [object release];
}


void NSPlatformSetCursorImp(id object) {
   if(_CGUsesSkyLight()) {
    _NSCursorSkyLightSet(object);
    return;
   }
   [[NSDisplay currentDisplay] setCursor:object];
}

@implementation NSCursor

static NSMutableArray *_cursorStack=nil;

+(void)initialize {
   if(self==[NSCursor class]){
    _cursorStack=[[NSMutableArray alloc] init];
   }
}

+(NSCursor *)currentCursor {
   return [[[_cursorStack lastObject] retain] autorelease];
}

+(NSCursor *)currentSystemCursor {
   return [[[_cursorStack lastObject] retain] autorelease];
}

-initWithCoder:(NSCoder *)coder {
   [self dealloc];
   return [NSNull null];
}

-(void)encodeWithCoder:(NSCoder *)coder {
   NSUnimplementedMethod();
}

-initWithName:(NSString *)name {
   _platformCursor=NSPlatformCreateCursorImpWithName(name);
   return self;
}

+(NSCursor *)arrowCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)closedHandCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)contextualMenuCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)crosshairCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)disappearingItemCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)IBeamCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)openHandCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)pointingHandCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)resizeDownCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)resizeLeftCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)resizeLeftRightCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)resizeRightCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)resizeUpCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)resizeUpDownCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)dragCopyCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)dragLinkCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(NSCursor *)operationNotAllowedCursor {
   static NSCursor *shared=nil;

   if(shared==nil)
    shared=[[self alloc] initWithName:NSStringFromSelector(_cmd)];

   return shared;
}

+(void)hide {
   [[NSDisplay currentDisplay] hideCursor];
}

+(void)unhide {
   [[NSDisplay currentDisplay] unhideCursor];
}

+(void)setHiddenUntilMouseMoves:(BOOL)flag {
   if(flag)
    [self hide];
   else
    [self unhide];
}

-initWithImage:(NSImage *)image foregroundColorHint:(NSColor *)foregroundHint backgroundColorHint:(NSColor *)backgroundHint hotSpot:(NSPoint)hotSpot {
// the hints are unused per doc.s
   return [self initWithImage:image hotSpot:hotSpot];
}

-initWithImage:(NSImage *)image hotSpot:(NSPoint)hotSpot {
   _image=[image retain];
   _hotSpot=hotSpot;
   _platformCursor=NSPlatformCreateCursorImpWithImage(image,hotSpot);
   return self;
}

-(void)dealloc {
   [_image release];
   NSPlatformReleaseCursorImp(_platformCursor);
   [super dealloc];
}

-(NSImage *)image {
   return _image;
}

-(NSPoint)hotSpot {
   return _hotSpot;
}

-(BOOL)isSetOnMouseEntered {
   return _isSetOnMouseEntered;
}

-(BOOL)isSetOnMouseExited {
   return _isSetOnMouseExited;
}

-(void)setOnMouseEntered:(BOOL)value {
   _isSetOnMouseEntered=value;
}

-(void)setOnMouseExited:(BOOL)value {
   _isSetOnMouseExited=value;
}

-(void)mouseEntered:(NSEvent *)event {
   NSUnimplementedMethod();
}

-(void)mouseExited:(NSEvent *)event {
   NSUnimplementedMethod();
}

-(void)pop {
   [[self class] pop];
}

-(void)set {
   if([_cursorStack count])
    [_cursorStack removeLastObject];

   [_cursorStack addObject:self];
   NSPlatformSetCursorImp(_platformCursor);
}

-(void)push {
   [_cursorStack addObject:self];
   NSPlatformSetCursorImp(_platformCursor);
}

+(void)pop {
   if([_cursorStack count]<2)
    return;

   [_cursorStack removeLastObject];

   NSCursor *cursor=[_cursorStack lastObject];

   if(cursor==nil)
       cursor=[NSCursor arrowCursor];

   NSPlatformSetCursorImp(cursor->_platformCursor);
}

@end
