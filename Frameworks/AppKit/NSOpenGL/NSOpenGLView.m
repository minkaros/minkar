/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSOpenGLView.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSOpenGLContext.h>
#import <AppKit/NSOpenGLPixelFormat.h>
#import <AppKit/NSRaise.h>
#import <Foundation/NSKeyedArchiver.h>
#import <Foundation/NSNotification.h>

@interface NSView(NSOpenGLViewFocus)
-(void)_lockFocusInContext:(NSGraphicsContext *)context;
@end

@implementation NSOpenGLView

+(NSOpenGLPixelFormat *)defaultPixelFormat {
   NSOpenGLPixelFormatAttribute attributes[]={ 0 };

   return [[[NSOpenGLPixelFormat alloc] initWithAttributes:attributes] autorelease];
}

/* The view follows its place in the window (NSViewGlobalFrameDidChange):
   the context is updated and the view reshaped before it draws again. */
-(void)_observeGlobalFrame {
   [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_globalFrameDidChange:)
      name:NSViewGlobalFrameDidChangeNotification object:self];
}

-(void)_globalFrameDidChange:(NSNotification *)note {
   [self update];
   _needsReshape=YES;
   [self setNeedsDisplay:YES];
}

-initWithFrame:(NSRect)frame pixelFormat:(NSOpenGLPixelFormat *)pixelFormat {
   if((self=[super initWithFrame:frame])==nil)
    return nil;
   _pixelFormat=[pixelFormat retain];
   _needsReshape=YES;
   [self _observeGlobalFrame];
   return self;
}

-initWithFrame:(NSRect)frame {
   return [self initWithFrame:frame pixelFormat:[[self class] defaultPixelFormat]];
}

-initWithCoder:(NSCoder *)coder {
   if((self=[super initWithCoder:coder])==nil)
    return nil;
   if([coder allowsKeyedCoding])
    _pixelFormat=[[coder decodeObjectForKey:@"NSPixelFormat"] retain];
   else
    _pixelFormat=[[coder decodeObject] retain];
   if(_pixelFormat==nil)
    _pixelFormat=[[[self class] defaultPixelFormat] retain];
   _needsReshape=YES;
   [self _observeGlobalFrame];
   return self;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   [super encodeWithCoder:coder];
   if([coder allowsKeyedCoding])
    [coder encodeObject:_pixelFormat forKey:@"NSPixelFormat"];
   else
    [coder encodeObject:_pixelFormat];
}

-(void)dealloc {
   [[NSNotificationCenter defaultCenter] removeObserver:self name:NSViewGlobalFrameDidChangeNotification object:self];
   [_context clearDrawable];
   [_context release];
   [_pixelFormat release];
   [super dealloc];
}

-(NSOpenGLPixelFormat *)pixelFormat {
   return _pixelFormat;
}

-(void)setPixelFormat:(NSOpenGLPixelFormat *)pixelFormat {
   pixelFormat=[pixelFormat retain];
   [_pixelFormat release];
   _pixelFormat=pixelFormat;
}

-(NSOpenGLContext *)openGLContext {
   if(_context==nil){
    _context=[[NSOpenGLContext alloc] initWithFormat:_pixelFormat shareContext:nil];
    if([self window]!=nil)
     [_context setView:self];
    _needsReshape=YES;
   }
   return _context;
}

-(void)setOpenGLContext:(NSOpenGLContext *)context {
   if(context==_context)
    return;
   [_context clearDrawable];
   [context retain];
   [_context release];
   _context=context;
   if([self window]!=nil)
    [_context setView:self];
   _needsReshape=YES;
}

-(void)_setWindow:(NSWindow *)window {
   [super _setWindow:window];
   if(_context!=nil){
    if(window!=nil)
     [_context setView:self];
    else
     [_context update];
   }
   _needsReshape=YES;
}

-(BOOL)wantsBestResolutionOpenGLSurface {
   return _wantsBestResolution;
}

/* Minkar draws at one pixel a point: the surface is the view's size. */
-(void)setWantsBestResolutionOpenGLSurface:(BOOL)value {
   _wantsBestResolution=value;
   [self update];
}

-(BOOL)wantsExtendedDynamicRangeOpenGLSurface {
   return _wantsExtendedDynamicRange;
}

-(void)setWantsExtendedDynamicRangeOpenGLSurface:(BOOL)value {
   _wantsExtendedDynamicRange=value;
}

/* The context is not made here when there is none yet. */
-(void)update {
   [_context update];
}

-(void)reshape {
}

-(void)prepareOpenGL {
}

-(BOOL)isOpaque {
   return YES;
}

-(void)viewDidHide {
   [self update];
}

-(void)viewDidUnhide {
   [self update];
}

/* Every drawing of the view (display, lockFocus) comes through here: the
   context is made current, and the view reshaped first when it must. */
-(void)_lockFocusInContext:(NSGraphicsContext *)graphicsContext {
   [super _lockFocusInContext:graphicsContext];
   NSOpenGLContext *context=[self openGLContext];

   if([context view]!=self && [self window]!=nil)
    [context setView:self];
   [context lock];
   [context makeCurrentContext];
   if(_needsReshape){
    _needsReshape=NO;
    [self reshape];
   }
}

/* The app flushes (flushBuffer, or glFlush for a single buffer). */
-(void)unlockFocus {
   [_context unlock];
   [super unlockFocus];
}

-(void)clearGLContext {
   [_context clearDrawable];
   [_context release];
   _context=nil;
}

@end
