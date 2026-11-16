/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSOpenGLPixelFormat.h>
#import <AppKit/NSRaise.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSData.h>
#import <OpenGL/OpenGL.h>

void NSOpenGLSetOption(NSOpenGLGlobalOption pname, GLint param) {
   CGLSetOption((CGLGlobalOption)pname, param);
}

void NSOpenGLGetOption(NSOpenGLGlobalOption pname, GLint *param) {
   CGLGetOption((CGLGlobalOption)pname, param);
}

void NSOpenGLGetVersion(GLint *major, GLint *minor) {
   CGLGetVersion(major, minor);
}

/* Attributes that take a value after them (the others stand alone). */
static BOOL takesValue(NSOpenGLPixelFormatAttribute a) {
   switch(a){
    case NSOpenGLPFAAuxBuffers: case NSOpenGLPFAColorSize: case NSOpenGLPFAAlphaSize:
    case NSOpenGLPFADepthSize: case NSOpenGLPFAStencilSize: case NSOpenGLPFAAccumSize:
    case NSOpenGLPFASampleBuffers: case NSOpenGLPFASamples: case NSOpenGLPFARendererID:
    case NSOpenGLPFAScreenMask: case NSOpenGLPFAOpenGLProfile: case NSOpenGLPFAVirtualScreenCount:
     return YES;
    default:
     return NO;
   }
}

/* The list up to its 0, as data (the attributes property). */
static NSData *attributesData(const NSOpenGLPixelFormatAttribute *attributes) {
   NSUInteger n=0;

   while(attributes[n]!=0)
    n+=takesValue(attributes[n])?2:1;
   return [NSData dataWithBytes:attributes length:(n+1)*sizeof(NSOpenGLPixelFormatAttribute)];
}

@implementation NSOpenGLPixelFormat

-initWithAttributes:(const NSOpenGLPixelFormatAttribute *)attributes {
   if(attributes==NULL){
    [self release];
    return nil;
   }
   CGLChoosePixelFormat((const CGLPixelFormatAttribute *)attributes,(CGLPixelFormatObj *)&_cglPixelFormat,&_numberOfVirtualScreens);
   /* No renderer can satisfy the attributes: no pixel format. */
   if(_cglPixelFormat==NULL){
    [self release];
    return nil;
   }
   _attributes=[attributesData(attributes) retain];
   return self;
}

-initWithCGLPixelFormatObj:(CGLPixelFormatObj)format {
   if(format==NULL){
    [self release];
    return nil;
   }
   _cglPixelFormat=CGLRetainPixelFormat(format);
   GLint count=0;
   CGLDescribePixelFormat(format,0,kCGLPFAVirtualScreenCount,&count);
   _numberOfVirtualScreens=count;
   return self;
}

-initWithData:(NSData *)data {
   NSUInteger count=[data length]/sizeof(NSOpenGLPixelFormatAttribute);
   NSOpenGLPixelFormatAttribute attributes[count+1];

   memcpy(attributes,[data bytes],count*sizeof(NSOpenGLPixelFormatAttribute));
   attributes[count]=0;
   return [self initWithAttributes:attributes];
}

-(void)dealloc {
   CGLReleasePixelFormat(_cglPixelFormat);
   [_attributes release];
   [super dealloc];
}

/* Keyed archives keep the attributes as big-endian 32-bit values. */
-(void)encodeWithCoder:(NSCoder *)coder {
   NSUInteger count=[_attributes length]/sizeof(NSOpenGLPixelFormatAttribute);
   const NSOpenGLPixelFormatAttribute *values=[_attributes bytes];
   NSMutableData *data=[NSMutableData dataWithLength:count*4];
   unsigned char *bytes=[data mutableBytes];

   for(NSUInteger i=0;i<count;i++){
    bytes[i*4]=(unsigned char)(values[i]>>24);
    bytes[i*4+1]=(unsigned char)(values[i]>>16);
    bytes[i*4+2]=(unsigned char)(values[i]>>8);
    bytes[i*4+3]=(unsigned char)values[i];
   }
   if([coder allowsKeyedCoding])
    [coder encodeObject:data forKey:@"NSPixelAttributes"];
   else
    [coder encodeObject:data];
}

-initWithCoder:(NSCoder *)coder {
   NSData *data=[coder allowsKeyedCoding]?[coder decodeObjectForKey:@"NSPixelAttributes"]:[coder decodeObject];
   NSUInteger i,length=[data length];
   const unsigned char *bytes=[data bytes];
   NSOpenGLPixelFormatAttribute attributes[length/4+1];
   NSUInteger a=0;

   if(length%4>0){
    NSLog(@"NSPixelAttributes is not a multiple of 4, length=%lu",(unsigned long)length);
    [self release];
    return nil;
   }
   for(i=0;i<length;i+=4)
    attributes[a++]=((NSOpenGLPixelFormatAttribute)bytes[i]<<24)|((NSOpenGLPixelFormatAttribute)bytes[i+1]<<16)|
                    ((NSOpenGLPixelFormatAttribute)bytes[i+2]<<8)|bytes[i+3];
   attributes[a]=0;
   return [self initWithAttributes:attributes];
}

-(void *)CGLPixelFormatObj {
   return _cglPixelFormat;
}

-(GLint)numberOfVirtualScreens {
   return _numberOfVirtualScreens;
}

-(void)getValues:(GLint *)values forAttribute:(NSOpenGLPixelFormatAttribute)attribute forVirtualScreen:(GLint)screen {
   *values=0;
   CGLDescribePixelFormat(_cglPixelFormat,screen,(CGLPixelFormatAttribute)attribute,values);
}

-(NSData *)attributes {
   return _attributes;
}

/* Another set of attributes: the pixel format chosen again. */
-(void)setAttributes:(NSData *)data {
   NSUInteger count=[data length]/sizeof(NSOpenGLPixelFormatAttribute);
   NSOpenGLPixelFormatAttribute attributes[count+1];
   CGLPixelFormatObj format=NULL;
   GLint screens=0;

   memcpy(attributes,[data bytes],count*sizeof(NSOpenGLPixelFormatAttribute));
   attributes[count]=0;
   CGLChoosePixelFormat((const CGLPixelFormatAttribute *)attributes,&format,&screens);
   if(format==NULL)
    return;
   CGLReleasePixelFormat(_cglPixelFormat);
   _cglPixelFormat=format;
   _numberOfVirtualScreens=screens;
   [_attributes release];
   _attributes=[attributesData(attributes) retain];
}

@end
