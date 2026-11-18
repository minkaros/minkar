/* Copyright (c) 2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CGColor.h>
#import <Onyx2D/O2ColorMatching.h>
#import <Onyx2D/O2Color.h>
#import <Onyx2D/O2ColorSpace.h>
#include <dispatch/dispatch.h>

CGColorRef CGColorRetain(CGColorRef self) {
   return O2ColorRetain(self);
}

void CGColorRelease(CGColorRef self) {
   O2ColorRelease(self);
}

CGColorRef CGColorCreate(CGColorSpaceRef colorSpace,const CGFloat *components) {
   return O2ColorCreate(colorSpace,components);
}

CGColorRef CGColorCreateGenericGray(CGFloat gray,CGFloat a) {
   return O2ColorCreateGenericGray(gray,a);
}

CGColorRef CGColorCreateGenericRGB(CGFloat r,CGFloat g,CGFloat b,CGFloat a) {
    return O2ColorCreateGenericRGB(r,g,b,a);
}

CGColorRef CGColorCreateSRGB(CGFloat red,CGFloat green,CGFloat blue,CGFloat alpha) {
   CGColorSpaceRef space=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
   CGFloat         components[4]={ red,green,blue,alpha };
   CGColorRef      color=CGColorCreate(space,components);

   CGColorSpaceRelease(space);
   return color;
}

CGColorRef CGColorCreateGenericCMYK(CGFloat c,CGFloat m,CGFloat y,CGFloat k,CGFloat a) {
   return O2ColorCreateGenericCMYK(c,m,y,k,a);
}

CGColorRef CGColorCreateWithPattern(CGColorSpaceRef colorSpace,CGPatternRef pattern,const CGFloat *components) {
   return O2ColorCreateWithPattern(colorSpace,pattern,components);
}

CGColorRef CGColorCreateCopy(CGColorRef self) {
   return O2ColorCreateCopy(self);
}

NSString *_CGColorSpaceGetMatchingProfile(CGColorSpaceRef space);

/* The color in another space, matched by the spaces' profiles (Little CMS
   in Onyx2D); a space of the same model that is not matched takes the
   components as they are. */
CGColorRef CGColorCreateCopyByMatchingToColorSpace(CGColorSpaceRef space,CGColorRenderingIntent intent,CGColorRef color,CFDictionaryRef options) {
   CGColorSpaceRef from;
   NSString       *fromProfile,*toProfile;
   size_t          inCount,outCount,i;
   const CGFloat  *in;
   double          din[4]={0},dout[4]={0};
   CGFloat         out[5];

   if(space==NULL || color==NULL || CGColorGetPattern(color)!=NULL)
    return NULL;
   from=CGColorGetColorSpace(color);
   inCount=CGColorSpaceGetNumberOfComponents(from);
   outCount=CGColorSpaceGetNumberOfComponents(space);
   in=CGColorGetComponents(color);
   fromProfile=_CGColorSpaceGetMatchingProfile(from);
   toProfile=_CGColorSpaceGetMatchingProfile(space);
   if(fromProfile!=nil && toProfile!=nil && inCount<=3 && outCount<=3){
    for(i=0;i<inCount;i++)
     din[i]=in[i];
    if(!O2ColorMatchComponents(fromProfile,din,toProfile,dout))
     return NULL;
    for(i=0;i<outCount;i++)
     out[i]=dout[i];
   }
   else if(inCount==outCount && CGColorSpaceGetModel(from)==CGColorSpaceGetModel(space)){
    for(i=0;i<outCount;i++)
     out[i]=in[i];
   }
   else
    return NULL;
   out[outCount]=CGColorGetAlpha(color);
   return CGColorCreate(space,out);
}

CGColorRef CGColorCreateCopyWithAlpha(CGColorRef self,CGFloat a) {
   return O2ColorCreateCopyWithAlpha(self,a);
}

bool CGColorEqualToColor(CGColorRef self,CGColorRef other) {
   return O2ColorEqualToColor(self,other);
}

CGColorSpaceRef CGColorGetColorSpace(CGColorRef self) {
   return O2ColorGetColorSpace(self);
}

size_t CGColorGetNumberOfComponents(CGColorRef self) {
   return O2ColorGetNumberOfComponents(self);
}

const CGFloat *CGColorGetComponents(CGColorRef self) {
   return O2ColorGetComponents(self);
}

CGFloat CGColorGetAlpha(CGColorRef self) {
   return O2ColorGetAlpha(self);
}

CGPatternRef CGColorGetPattern(CGColorRef self) {
   return O2ColorGetPattern(self);
}

const CFStringRef kCGColorWhite = CFSTR("kCGColorWhite");
const CFStringRef kCGColorBlack = CFSTR("kCGColorBlack");
const CFStringRef kCGColorClear = CFSTR("kCGColorClear");

CGColorRef CGColorGetConstantColor(CFStringRef colorName) {
    static CGColorRef white, black, clear;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        white = CGColorCreateGenericGray(1, 1);
        black = CGColorCreateGenericGray(0, 1);
        clear = CGColorCreateGenericGray(0, 0);
    });
    if (colorName == NULL)
        return NULL;
    if (CFEqual(colorName, kCGColorWhite))
        return white;
    if (CFEqual(colorName, kCGColorBlack))
        return black;
    if (CFEqual(colorName, kCGColorClear))
        return clear;
    return NULL;
}
