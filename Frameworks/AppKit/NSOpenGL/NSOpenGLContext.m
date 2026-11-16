/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

/*
 * Minkar: a context given a view draws into a surface of the view's size
 * (CGL's window surface, CGLSurface.c); each flushBuffer hands the frame,
 * an IOSurface, to the presenter here, which puts it in the window's
 * server layers (SLSWindowSetLayers) at the view's place, clipped to what
 * of the view is seen: the window server composes it under the window,
 * whose pixels the view leaves clear there. -update tells CGL where the
 * view is; NSOpenGLView calls it when the view moves in its window
 * (NSViewGlobalFrameDidChangeNotification), as the reference platform asks.
 */

#import <AppKit/NSOpenGLContext.h>
#import <AppKit/NSOpenGLPixelFormat.h>
#import <AppKit/NSOpenGLPixelBuffer.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSBitmapImageRep.h>
#import <AppKit/NSRaise.h>
#import <AppKit/NSView.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSWindow-Private.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/CGLPrivate.h>
#import <OpenGL/gl.h>
#import <IOSurface/IOSurface.h>
#import <SkyLight/SkyLight.h>
#import <Foundation/NSThread-Private.h>
#import <Foundation/NSData.h>

/* NSView.m: the window's server layers, and a view left clear for them. */
void _NSWindowSetServerLayers(NSWindow *window,const void *owner,NSData *list);
void _NSViewSetClearsForSurface(NSView *view,BOOL clears);

@interface NSOpenGLContext(private)
-(void)_clearCurrentContext;
-(void)_present:(IOSurfaceRef)surface;
-(void)_takeSurfaceAway;
@end

@implementation NSOpenGLContext

static inline NSOpenGLContext *_currentContext(){
   return (NSOpenGLContext *)NSThreadSharedInstanceDoNotCreate(@"NSOpenGLContext");
}

static void _setCurrentContext(NSOpenGLContext *context){
   [NSCurrentThread() setSharedObject:context forClassName:@"NSOpenGLContext"];
}

static inline void _clearCurrentContext(){
   [_currentContext() _clearCurrentContext];
   _setCurrentContext(nil);
}

static void present(void *info,CGLContextObj ctx,IOSurfaceRef surface) {
   (void)ctx;
   [(NSOpenGLContext *)info _present:surface];
}

+(NSOpenGLContext *)currentContext {
   return _currentContext();
}

+(void)clearCurrentContext {
   _clearCurrentContext();
}

-initWithFormat:(NSOpenGLPixelFormat *)pixelFormat shareContext:(NSOpenGLContext *)shareContext {
   CGLError error;

   if(pixelFormat==nil){
    [self release];
    return nil;
   }
   _pixelFormat=[pixelFormat retain];
   if((error=CGLCreateContext([_pixelFormat CGLPixelFormatObj],[shareContext CGLContextObj],(CGLContextObj *)&_glContext))!=kCGLNoError){
    [self release];
    return nil;
   }
   _CGLSetSurfacePresenter(_glContext,present,self);
   return self;
}

-initWithCGLContextObj:(void *)context {
   if(context==NULL){
    [self release];
    return nil;
   }
   _glContext=CGLRetainContext(context);
   _pixelFormat=[[NSOpenGLPixelFormat alloc] initWithCGLPixelFormatObj:CGLGetPixelFormat(context)];
   _CGLSetSurfacePresenter(_glContext,present,self);
   return self;
}

-(void)dealloc {
   if(_currentContext()==self)
    _clearCurrentContext();
   if(_glContext!=NULL)
    _CGLSetSurfacePresenter(_glContext,NULL,NULL);
   [self _takeSurfaceAway];
   if(_view!=nil)
    _NSViewSetClearsForSurface(_view,NO);
   _view=nil;
   [_pixelFormat release];
   [_pixelBuffer release];
   CGLReleaseContext(_glContext);
   [super dealloc];
}

-(NSOpenGLPixelFormat *)pixelFormat {
   return _pixelFormat;
}

-(NSView *)view {
   return _view;
}

-(void *)CGLContextObj {
   return _glContext;
}

-(void)lock {
   CGLLockContext(_glContext);
}

-(void)unlock {
   CGLUnlockContext(_glContext);
}

/* ---- Parameters ---- */

/* Parameters that are CGL enables (on or off), not values. */
static BOOL isEnable(NSOpenGLContextParameter p) {
   return p==NSOpenGLContextParameterSwapRectangleEnable || p==NSOpenGLContextParameterRasterizationEnable ||
          p==NSOpenGLContextParameterStateValidation;
}

-(void)getValues:(GLint *)vals forParameter:(NSOpenGLContextParameter)parameter {
   if(isEnable(parameter))
    CGLIsEnabled(_glContext,(CGLContextEnable)parameter,vals);
   else
    CGLGetParameter(_glContext,(CGLContextParameter)parameter,vals);
}

-(void)setValues:(const GLint *)vals forParameter:(NSOpenGLContextParameter)parameter {
   if(isEnable(parameter)){
    if(vals[0])
     CGLEnable(_glContext,(CGLContextEnable)parameter);
    else
     CGLDisable(_glContext,(CGLContextEnable)parameter);
   }
   else
    CGLSetParameter(_glContext,(CGLContextParameter)parameter,vals);
}

-(void)copyAttributesFromContext:(NSOpenGLContext *)context withMask:(GLbitfield)mask {
   CGLCopyContext([context CGLContextObj],_glContext,mask);
}

-(GLint)currentVirtualScreen {
   GLint screen=0;
   CGLGetVirtualScreen(_glContext,&screen);
   return screen;
}

-(void)setCurrentVirtualScreen:(GLint)screen {
   CGLSetVirtualScreen(_glContext,screen);
}

/* ---- The view ---- */

-(void)setView:(NSView *)view {
   if(_view!=view){
    _hasPrepared=NO;
    if(_view!=nil)
     _NSViewSetClearsForSurface(_view,NO);
    [self _takeSurfaceAway];
   }
   _view=view;
   if(view!=nil){
    /* A view draws only once it is in a window. */
    if([view window]==nil)
     NSLog(@"NSOpenGLContext: the view %@ is not in a window: no drawable",view);
    _NSViewSetClearsForSurface(view,YES);
   }
   [self update];
}

/* Where the view is in its window: told to CGL (the surface's size) and
   kept for the presenter (the surface's place and what is seen of it). */
-(void)update {
   NSWindow *window=[_view window];
   GLint     number[1]={ 0 };
   GLint     frame[4]={ 0,0,0,0 };
   GLint     hidden[1]={ 0 };

   if(_view!=nil && window!=nil && [window respondsToSelector:@selector(_skyLightWindowID)]){
    NSRect  r=[_view convertRect:[_view bounds] toView:nil];
    NSRect  seen=[_view convertRect:[_view visibleRect] toView:nil];
    CGFloat height=[window frame].size.height;

    number[0]=(GLint)[window _skyLightWindowID];
    frame[0]=(GLint)floor(r.origin.x);
    frame[1]=(GLint)floor(height-(r.origin.y+r.size.height));
    frame[2]=(GLint)r.size.width;
    frame[3]=(GLint)r.size.height;
    hidden[0]=[_view isHiddenOrHasHiddenAncestor]?1:0;
    @synchronized(self){
     if(_surfaceWindow!=window){
      [self _takeSurfaceAway];
      [_surfaceWindow release];
      _surfaceWindow=[window retain];
     }
     _surfaceX=frame[0];
     _surfaceY=frame[1];
     _clipX=(int32_t)floor(seen.origin.x);
     _clipY=(int32_t)floor(height-(seen.origin.y+seen.size.height));
     _clipWidth=(int32_t)seen.size.width;
     _clipHeight=(int32_t)seen.size.height;
    }
   }
   else
    [self _takeSurfaceAway];
   CGLLockContext(_glContext);
   CGLSetParameter(_glContext,kCGLCPSurfaceHidden,hidden);
   CGLSetParameter(_glContext,kCGLCPSurfaceFrame,frame);
   CGLSetParameter(_glContext,kCGLCPSurfaceWindowNumber,number);
   CGLUnlockContext(_glContext);
   if(hidden[0])
    [self _takeSurfaceAway];
}

/* From CGL, on the thread that flushed: the frame at the view's place. */
-(void)_present:(IOSurfaceRef)surface {
   @synchronized(self){
    if(_surfaceWindow==nil)
     return;
    SLSWindowLayer l={ IOSurfaceGetID(surface),(uint32_t)IOSurfaceGetWidth(surface),(uint32_t)IOSurfaceGetHeight(surface),
                       _surfaceX,_surfaceY,_clipX,_clipY,_clipWidth,_clipHeight };
    _NSWindowSetServerLayers(_surfaceWindow,self,[NSData dataWithBytes:&l length:sizeof(l)]);
   }
}

-(void)_takeSurfaceAway {
   @synchronized(self){
    if(_surfaceWindow!=nil){
     _NSWindowSetServerLayers(_surfaceWindow,self,nil);
     [_surfaceWindow release];
     _surfaceWindow=nil;
    }
   }
}

-(void)clearDrawable {
   [self _takeSurfaceAway];
   if(_view!=nil)
    _NSViewSetClearsForSurface(_view,NO);
   _view=nil;
   CGLClearDrawable(_glContext);
   [_pixelBuffer release];
   _pixelBuffer=nil;
}

/* Full-screen contexts are no more: a full-screen window instead. */
-(void)setFullScreen {
   CGLError error=CGLSetFullScreen(_glContext);
   if(error!=kCGLNoError)
    NSLog(@"NSOpenGLContext: no full-screen contexts (%d): use a full-screen window",error);
}

-(void)setOffScreen:(void *)baseaddr width:(GLsizei)width height:(GLsizei)height rowbytes:(GLint)rowbytes {
   [self _takeSurfaceAway];
   CGLSetOffScreen(_glContext,width,height,rowbytes,baseaddr);
}

/* ---- Drawing ---- */

-(void)makeCurrentContext {
   CGLError error;

   if((error=CGLSetCurrentContext(_glContext))!=kCGLNoError)
    NSLog(@"CGLSetCurrentContext failed with %d",error);
   _setCurrentContext(self);
   if(!_hasPrepared){
    _hasPrepared=YES;
    /* Any view, not only an NSOpenGLView, is asked to prepare. */
    if([_view respondsToSelector:@selector(prepareOpenGL)])
     [_view performSelector:@selector(prepareOpenGL)];
   }
}

-(void)_clearCurrentContext {
   CGLError error;

   if((error=CGLSetCurrentContext(NULL))!=kCGLNoError)
    NSLog(@"CGLSetCurrentContext failed with %d",error);
}

-(void)flushBuffer {
   CGLFlushDrawable(_glContext);
}

/* The texture bound to target in this context takes the view's picture. */
-(void)createTexture:(GLenum)target fromView:(NSView *)view internalFormat:(GLenum)format {
   NSRect bounds=[view bounds];
   NSBitmapImageRep *rep=[view bitmapImageRepForCachingDisplayInRect:bounds];

   if(rep==nil)
    return;
   [view cacheDisplayInRect:bounds toBitmapImageRep:rep];
   NSInteger w=[rep pixelsWide],h=[rep pixelsHigh];
   NSMutableData *rgba=[NSMutableData dataWithLength:w*h*4];
   unsigned char *out=[rgba mutableBytes];
   /* Rows from the bottom, as GL takes them. */
   for(NSInteger y=0;y<h;y++)
    for(NSInteger x=0;x<w;x++){
     NSUInteger p[4]={ 0,0,0,255 };
     [rep getPixel:p atX:x y:h-1-y];
     unsigned char *o=out+(y*w+x)*4;
     o[0]=(unsigned char)p[0]; o[1]=(unsigned char)p[1]; o[2]=(unsigned char)p[2];
     o[3]=[rep samplesPerPixel]>3?(unsigned char)p[3]:255;
    }
   CGLContextObj was=CGLGetCurrentContext();
   CGLSetCurrentContext(_glContext);
   glTexImage2D(target,0,(GLint)format,(GLsizei)w,(GLsizei)h,0,GL_RGBA,GL_UNSIGNED_BYTE,out);
   CGLSetCurrentContext(was);
}

/* ---- Pixel buffers ---- */

-(void)setPixelBuffer:(NSOpenGLPixelBuffer *)pixelBuffer cubeMapFace:(GLenum)face mipMapLevel:(GLint)level
 currentVirtualScreen:(GLint)screen {
   [self _takeSurfaceAway];
   if(CGLSetPBuffer(_glContext,[pixelBuffer CGLPBufferObj],face,level,screen)!=kCGLNoError)
    return;
   [pixelBuffer retain];
   [_pixelBuffer release];
   _pixelBuffer=pixelBuffer;
   _pixelBufferFace=face;
   _pixelBufferLevel=level;
}

-(NSOpenGLPixelBuffer *)pixelBuffer {
   return _pixelBuffer;
}

-(GLenum)pixelBufferCubeMapFace {
   return (GLenum)_pixelBufferFace;
}

-(GLint)pixelBufferMipMapLevel {
   return (GLint)_pixelBufferLevel;
}

-(void)setTextureImageToPixelBuffer:(NSOpenGLPixelBuffer *)pixelBuffer colorBuffer:(GLenum)source {
   CGLTexImagePBuffer(_glContext,[pixelBuffer CGLPBufferObj],source);
}

@end
