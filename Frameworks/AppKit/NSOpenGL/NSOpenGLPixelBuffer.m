/* Copyright (c) 2007 Christopher J. W. Lloyd
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSOpenGLPixelBuffer.h>

@implementation NSOpenGLPixelBuffer

-initWithTextureTarget:(GLenum)target textureInternalFormat:(GLenum)format textureMaxMipMapLevel:(GLint)maxLevel
            pixelsWide:(GLsizei)pixelsWide pixelsHigh:(GLsizei)pixelsHigh {
   if(CGLCreatePBuffer(pixelsWide,pixelsHigh,target,format,maxLevel,&_pbuffer)!=kCGLNoError){
    [self release];
    return nil;
   }
   return self;
}

-initWithCGLPBufferObj:(CGLPBufferObj)pbuffer {
   if(pbuffer==NULL){
    [self release];
    return nil;
   }
   _pbuffer=CGLRetainPBuffer(pbuffer);
   return self;
}

-(void)dealloc {
   CGLReleasePBuffer(_pbuffer);
   [super dealloc];
}

-(CGLPBufferObj)CGLPBufferObj {
   return _pbuffer;
}

/* Everything from CGLDescribePBuffer. */
static void describe(CGLPBufferObj p,GLsizei *w,GLsizei *h,GLenum *target,GLenum *format,GLint *level) {
   GLsizei a=0,b=0;
   GLenum t=0,f=0;
   GLint l=0;

   CGLDescribePBuffer(p,&a,&b,&t,&f,&l);
   if(w) *w=a;
   if(h) *h=b;
   if(target) *target=t;
   if(format) *format=f;
   if(level) *level=l;
}

-(GLsizei)pixelsWide {
   GLsizei w;
   describe(_pbuffer,&w,NULL,NULL,NULL,NULL);
   return w;
}

-(GLsizei)pixelsHigh {
   GLsizei h;
   describe(_pbuffer,NULL,&h,NULL,NULL,NULL);
   return h;
}

-(GLenum)textureTarget {
   GLenum t;
   describe(_pbuffer,NULL,NULL,&t,NULL,NULL);
   return t;
}

-(GLenum)textureInternalFormat {
   GLenum f;
   describe(_pbuffer,NULL,NULL,NULL,&f,NULL);
   return f;
}

-(GLint)textureMaxMipMapLevel {
   GLint l;
   describe(_pbuffer,NULL,NULL,NULL,NULL,&l);
   return l;
}

@end
