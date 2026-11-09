/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CGColorSpace.h>
#include <string.h>
#import <Onyx2D/O2ColorSpace.h>

CGColorSpaceRef CGColorSpaceRetain(CGColorSpaceRef colorSpace) {
   return O2ColorSpaceRetain(colorSpace);
}

void CGColorSpaceRelease(CGColorSpaceRef colorSpace) {
   O2ColorSpaceRelease(colorSpace);
}

CGColorSpaceRef CGColorSpaceCreateDeviceRGB() {
   return O2ColorSpaceCreateDeviceRGB();
}

CGColorSpaceRef CGColorSpaceCreateDeviceGray() {
   return O2ColorSpaceCreateDeviceGray();
}

CGColorSpaceRef CGColorSpaceCreateDeviceCMYK() {
   return O2ColorSpaceCreateDeviceCMYK();
}

CGColorSpaceRef CGColorSpaceCreatePattern(CGColorSpaceRef baseSpace) {
   return O2ColorSpaceCreatePattern(baseSpace);
}

CGColorSpaceModel CGColorSpaceGetModel(CGColorSpaceRef self) {
   return O2ColorSpaceGetModel(self);
}

size_t CGColorSpaceGetNumberOfComponents(CGColorSpaceRef self) {
   return O2ColorSpaceGetNumberOfComponents(self);
}

static O2ColorSpace_indexed *indexedSpace(CGColorSpaceRef space) {
    O2ColorSpace *s = (O2ColorSpace *)space;

    return [s isKindOfClass:[O2ColorSpace_indexed class]] ? (O2ColorSpace_indexed *)s : nil;
}

CGColorSpaceRef CGColorSpaceGetBaseColorSpace(CGColorSpaceRef space) {
    O2ColorSpace_indexed *indexed = indexedSpace(space);

    return indexed ? (CGColorSpaceRef)[indexed baseColorSpace] : NULL;
}

size_t CGColorSpaceGetColorTableCount(CGColorSpaceRef space) {
    O2ColorSpace_indexed *indexed = indexedSpace(space);

    return indexed ? (size_t)[indexed hival] + 1 : 0;
}

void CGColorSpaceGetColorTable(CGColorSpaceRef space, uint8_t *table) {
    O2ColorSpace_indexed *indexed = indexedSpace(space);

    if (indexed == nil || table == NULL)
        return;
    size_t perColor = O2ColorSpaceGetNumberOfComponents([indexed baseColorSpace]);
    memcpy(table, [indexed paletteBytes], ((size_t)[indexed hival] + 1) * perColor);
}

const CFStringRef kCGColorSpaceSRGB=CFSTR("kCGColorSpaceSRGB");
const CFStringRef kCGColorSpaceGenericRGB=CFSTR("kCGColorSpaceGenericRGB");
const CFStringRef kCGColorSpaceGenericRGBLinear=CFSTR("kCGColorSpaceGenericRGBLinear");
const CFStringRef kCGColorSpaceAdobeRGB1998=CFSTR("kCGColorSpaceAdobeRGB1998");
const CFStringRef kCGColorSpaceDisplayP3=CFSTR("kCGColorSpaceDisplayP3");
const CFStringRef kCGColorSpaceExtendedSRGB=CFSTR("kCGColorSpaceExtendedSRGB");
const CFStringRef kCGColorSpaceLinearSRGB=CFSTR("kCGColorSpaceLinearSRGB");
const CFStringRef kCGColorSpaceExtendedLinearSRGB=CFSTR("kCGColorSpaceExtendedLinearSRGB");
const CFStringRef kCGColorSpaceITUR_709=CFSTR("kCGColorSpaceITUR_709");
const CFStringRef kCGColorSpaceITUR_2020=CFSTR("kCGColorSpaceITUR_2020");
const CFStringRef kCGColorSpaceROMMRGB=CFSTR("kCGColorSpaceROMMRGB");
const CFStringRef kCGColorSpaceDCIP3=CFSTR("kCGColorSpaceDCIP3");
const CFStringRef kCGColorSpaceACESCGLinear=CFSTR("kCGColorSpaceACESCGLinear");
const CFStringRef kCGColorSpaceGenericXYZ=CFSTR("kCGColorSpaceGenericXYZ");
const CFStringRef kCGColorSpaceDisplayP3_HLG=CFSTR("kCGColorSpaceDisplayP3_HLG");
const CFStringRef kCGColorSpaceDisplayP3_PQ_EOTF=CFSTR("kCGColorSpaceDisplayP3_PQ_EOTF");
const CFStringRef kCGColorSpaceITUR_2020_HLG=CFSTR("kCGColorSpaceITUR_2020_HLG");
const CFStringRef kCGColorSpaceITUR_2020_PQ_EOTF=CFSTR("kCGColorSpaceITUR_2020_PQ_EOTF");
const CFStringRef kCGColorSpaceExtendedLinearDisplayP3=CFSTR("kCGColorSpaceExtendedLinearDisplayP3");
const CFStringRef kCGColorSpaceExtendedLinearITUR_2020=CFSTR("kCGColorSpaceExtendedLinearITUR_2020");
const CFStringRef kCGColorSpaceGenericGray=CFSTR("kCGColorSpaceGenericGray");
const CFStringRef kCGColorSpaceGenericGrayGamma2_2=CFSTR("kCGColorSpaceGenericGrayGamma2_2");
const CFStringRef kCGColorSpaceExtendedGray=CFSTR("kCGColorSpaceExtendedGray");
const CFStringRef kCGColorSpaceLinearGray=CFSTR("kCGColorSpaceLinearGray");
const CFStringRef kCGColorSpaceExtendedLinearGray=CFSTR("kCGColorSpaceExtendedLinearGray");
const CFStringRef kCGColorSpaceGenericCMYK=CFSTR("kCGColorSpaceGenericCMYK");

/* A device space that keeps the name it was made with; it equals the
   device space of its model, as nothing converts between them yet. */
@interface CGNamedColorSpace : O2ColorSpace {
    CFStringRef _name;
}
@end

@implementation CGNamedColorSpace

-initWithModel:(O2ColorSpaceModel)model name:(CFStringRef)name {
   _type=model;
   _isPlatformRGB=NO;
   _name=CFStringCreateCopy(NULL,name);
   return self;
}

-(void)dealloc {
   CFRelease(_name);
   [super dealloc];
}

-(CFStringRef)_name {
   return _name;
}

-copyWithZone:(NSZone *)zone {
   return [self retain];
}

@end

static O2ColorSpaceModel modelOfName(CFStringRef name) {
   static const CFStringRef *gray[]={&kCGColorSpaceGenericGray,&kCGColorSpaceGenericGrayGamma2_2,&kCGColorSpaceExtendedGray,&kCGColorSpaceLinearGray,&kCGColorSpaceExtendedLinearGray};
   static const CFStringRef *rgb[]={&kCGColorSpaceSRGB,&kCGColorSpaceGenericRGB,&kCGColorSpaceGenericRGBLinear,&kCGColorSpaceAdobeRGB1998,&kCGColorSpaceDisplayP3,&kCGColorSpaceExtendedSRGB,&kCGColorSpaceLinearSRGB,&kCGColorSpaceExtendedLinearSRGB,&kCGColorSpaceITUR_709,&kCGColorSpaceITUR_2020,&kCGColorSpaceROMMRGB,&kCGColorSpaceDCIP3,&kCGColorSpaceACESCGLinear,&kCGColorSpaceGenericXYZ,&kCGColorSpaceDisplayP3_HLG,&kCGColorSpaceDisplayP3_PQ_EOTF,&kCGColorSpaceITUR_2020_HLG,&kCGColorSpaceITUR_2020_PQ_EOTF,&kCGColorSpaceExtendedLinearDisplayP3,&kCGColorSpaceExtendedLinearITUR_2020};
   size_t i;

   if(name==NULL)
    return (O2ColorSpaceModel)-1;
   for(i=0;i<sizeof(gray)/sizeof(gray[0]);i++)
    if(CFEqual(name,*gray[i]))
     return kO2ColorSpaceModelMonochrome;
   for(i=0;i<sizeof(rgb)/sizeof(rgb[0]);i++)
    if(CFEqual(name,*rgb[i]))
     return kO2ColorSpaceModelRGB;
   if(CFEqual(name,kCGColorSpaceGenericCMYK))
    return kO2ColorSpaceModelCMYK;
   return (O2ColorSpaceModel)-1;
}

CGColorSpaceRef CGColorSpaceCreateWithName(CFStringRef name) {
   O2ColorSpaceModel model=modelOfName(name);

   if((int)model<0)
    return NULL;
   return (CGColorSpaceRef)[[CGNamedColorSpace alloc] initWithModel:model name:name];
}

CFStringRef CGColorSpaceGetName(CGColorSpaceRef space) {
   O2ColorSpace *s=(O2ColorSpace *)space;

   if(s==nil)
    return NULL;
   if([s isKindOfClass:[CGNamedColorSpace class]])
    return [(CGNamedColorSpace *)s _name];
   switch([s type]){
    case kO2ColorSpaceModelMonochrome: return CFSTR("kCGColorSpaceDeviceGray");
    case kO2ColorSpaceModelRGB:        return CFSTR("kCGColorSpaceDeviceRGB");
    case kO2ColorSpaceModelCMYK:       return CFSTR("kCGColorSpaceDeviceCMYK");
    default:                           return NULL;
   }
}

CFStringRef CGColorSpaceCopyName(CGColorSpaceRef space) {
   CFStringRef name=CGColorSpaceGetName(space);

   return (name!=NULL)?CFRetain(name):NULL;
}
