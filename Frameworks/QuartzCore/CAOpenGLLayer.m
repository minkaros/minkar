/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CAOpenGLLayer. -display makes the layer's context (copyCGLContext-
 * ForPixelFormat:, on its pixel format) current on an offscreen drawable
 * of the layer's size in pixels, asks canDrawInCGLContext:..., then
 * drawInCGLContext:...; the frame read back (rows from the bottom) goes
 * upright into the next of three IOSurfaces, which becomes the contents:
 * the window server shows it without this process drawing it again. An
 * asynchronous layer is asked again at every display refresh (60 a
 * second) and displayed when it says it can draw.
 */

#import <QuartzCore/CAOpenGLLayer.h>
#import <QuartzCore/CABase.h>
#import <QuartzCore/CATransaction.h>
#import <IOSurface/IOSurface.h>
#import <OpenGL/gl.h>
#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>
#include <mach/mach_time.h>
#include <stdlib.h>
#include <string.h>

@implementation CAOpenGLLayer

-(void)dealloc {
   [self setAsynchronous:NO];
   if(_glContext!=NULL)
    [self releaseCGLContext:_glContext];
   if(_glPixelFormat!=NULL)
    [self releaseCGLPixelFormat:_glPixelFormat];
   for(int i=0;i<3;i++)
    if(_frames[i]!=NULL)
     CFRelease(_frames[i]);
   free(_pixels);
   CGColorSpaceRelease(_colorspace);
   [super dealloc];
}

-(BOOL)isAsynchronous {
   return _asynchronous;
}

/* While asynchronous: asked at each refresh whether to draw. */
-(void)setAsynchronous:(BOOL)value {
   if(value==_asynchronous)
    return;
   _asynchronous=value;
   if(value){
    dispatch_source_t timer=dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,0,0,dispatch_get_main_queue());
    CAOpenGLLayer *layer=self;   /* not retained: the timer goes before the layer */
    dispatch_source_set_timer(timer,dispatch_time(DISPATCH_TIME_NOW,0),NSEC_PER_SEC/60,NSEC_PER_SEC/600);
    dispatch_source_set_event_handler(timer,^{
     [layer setNeedsDisplay];
     [layer displayIfNeeded];
    });
    dispatch_resume(timer);
    _timer=timer;
   }
   else if(_timer!=NULL){
    dispatch_source_cancel((dispatch_source_t)_timer);
    dispatch_release((dispatch_source_t)_timer);
    _timer=NULL;
   }
}

-(CGColorSpaceRef)colorspace {
   return _colorspace;
}

-(void)setColorspace:(CGColorSpaceRef)space {
   CGColorSpaceRetain(space);
   CGColorSpaceRelease(_colorspace);
   _colorspace=space;
}

-(BOOL)wantsExtendedDynamicRangeContent {
   return _wantsExtendedDynamicRangeContent;
}

-(void)setWantsExtendedDynamicRangeContent:(BOOL)value {
   _wantsExtendedDynamicRangeContent=value;
}

/* ---- What a subclass overrides ---- */

-(BOOL)canDrawInCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf
              forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts {
   return YES;
}

/* The default draws nothing: the frame is flushed as it is. */
-(void)drawInCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf
           forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts {
   glFlush();
}

-(CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask {
   CGLPixelFormatAttribute attributes[]={
    kCGLPFAColorSize,(CGLPixelFormatAttribute)24,kCGLPFAAlphaSize,(CGLPixelFormatAttribute)8,
    kCGLPFADisplayMask,(CGLPixelFormatAttribute)mask,(CGLPixelFormatAttribute)0 };
   CGLPixelFormatObj format=NULL;
   GLint screens=0;

   CGLChoosePixelFormat(attributes,&format,&screens);
   if(format==NULL){
    /* Any display: Mesa serves them all. */
    attributes[4]=(CGLPixelFormatAttribute)0;
    CGLChoosePixelFormat(attributes,&format,&screens);
   }
   return format;
}

-(void)releaseCGLPixelFormat:(CGLPixelFormatObj)pf {
   CGLReleasePixelFormat(pf);
}

-(CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pf {
   CGLContextObj context=NULL;

   CGLCreateContext(pf,NULL,&context);
   return context;
}

-(void)releaseCGLContext:(CGLContextObj)ctx {
   CGLReleaseContext(ctx);
}

/* ---- Displaying ---- */

static IOSurfaceRef newFrame(size_t width,size_t height) {
   NSDictionary *p=@{ (id)kIOSurfaceWidth: @(width), (id)kIOSurfaceHeight: @(height),
                      (id)kIOSurfaceBytesPerElement: @4, (id)kIOSurfacePixelFormat: @((uint32_t)'BGRA') };
   return IOSurfaceCreate((CFDictionaryRef)p);
}

-(void)display {
   CGRect  bounds=[self bounds];
   CGFloat scale=[self contentsScale];
   size_t  width=(size_t)ceil(bounds.size.width*scale),height=(size_t)ceil(bounds.size.height*scale);

   _needsDisplay=NO;
   if(width==0 || height==0){
    [self setContents:nil];
    return;
   }
   if(_glPixelFormat==NULL && (_glPixelFormat=[self copyCGLPixelFormatForDisplayMask:0xffffffff])==NULL)
    return;
   if(_glContext==NULL && (_glContext=[self copyCGLContextForPixelFormat:_glPixelFormat])==NULL)
    return;

   CGLLockContext(_glContext);
   if(width!=_pixelsWidth || height!=_pixelsHeight){
    free(_pixels);
    _pixels=malloc(width*height*4);
    _pixelsWidth=width;
    _pixelsHeight=height;
    for(int i=0;i<3;i++){
     if(_frames[i]!=NULL)
      CFRelease(_frames[i]);
     _frames[i]=NULL;
    }
    if(_pixels==NULL || CGLSetOffScreen(_glContext,(GLsizei)width,(GLsizei)height,(GLint)width*4,_pixels)!=kCGLNoError){
     free(_pixels);
     _pixels=NULL;
     _pixelsWidth=_pixelsHeight=0;
     CGLUnlockContext(_glContext);
     return;
    }
   }
   CGLContextObj was=CGLGetCurrentContext();
   CGLSetCurrentContext(_glContext);

   CFTimeInterval now=CACurrentMediaTime();
   CVTimeStamp    stamp;
   memset(&stamp,0,sizeof(stamp));
   stamp.version=0;
   stamp.hostTime=mach_absolute_time();
   stamp.videoRefreshPeriod=1;
   stamp.videoTimeScale=60;
   stamp.rateScalar=1.0;
   stamp.flags=kCVTimeStampHostTimeValid|kCVTimeStampVideoRefreshPeriodValid|kCVTimeStampRateScalarValid;

   BOOL drew=NO;
   if([self canDrawInCGLContext:_glContext pixelFormat:_glPixelFormat forLayerTime:now displayTime:&stamp]){
    glViewport(0,0,(GLsizei)width,(GLsizei)height);
    [self drawInCGLContext:_glContext pixelFormat:_glPixelFormat forLayerTime:now displayTime:&stamp];
    CGLFlushDrawable(_glContext);
    drew=YES;
   }
   CGLSetCurrentContext(was);

   if(drew){
    IOSurfaceRef frame=_frames[_nextFrame];
    if(frame==NULL)
     frame=_frames[_nextFrame]=newFrame(width,height);
    if(frame!=NULL){
     BOOL     opaque=[self isOpaque];
     IOSurfaceLock(frame,0,NULL);
     uint8_t *base=IOSurfaceGetBaseAddress(frame);
     size_t   rowBytes=IOSurfaceGetBytesPerRow(frame);
     for(size_t y=0;y<height;y++){
      uint32_t       *out=(uint32_t *)(void *)(base+y*rowBytes);
      const uint32_t *in=(const uint32_t *)_pixels+(height-1-y)*width;
      if(opaque)
       for(size_t x=0;x<width;x++)
        out[x]=in[x]|0xff000000u;
      else
       memcpy(out,in,width*4);
     }
     IOSurfaceUnlock(frame,0,NULL);
     _nextFrame=(_nextFrame+1)%3;
     [CATransaction begin];
     [CATransaction setDisableActions:YES];
     [self setContents:(id)frame];
     [CATransaction commit];
    }
   }
   CGLUnlockContext(_glContext);
}

@end
