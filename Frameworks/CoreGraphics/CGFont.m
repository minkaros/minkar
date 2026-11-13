/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CGFont.h>
#import <Onyx2D/O2Font.h>

const CFStringRef kCGFontVariationAxisName=CFSTR("kCGFontVariationAxisName");
const CFStringRef kCGFontVariationAxisMinValue=CFSTR("kCGFontVariationAxisMinValue");
const CFStringRef kCGFontVariationAxisMaxValue=CFSTR("kCGFontVariationAxisMaxValue");
const CFStringRef kCGFontVariationAxisDefaultValue=CFSTR("kCGFontVariationAxisDefaultValue");

CGFontRef CGFontCreateWithFontName(CFStringRef name) {
   return O2FontCreateWithFontName((NSString *)name);
}

CGFontRef CGFontRetain(CGFontRef self) {
   return O2FontRetain(self);
}

void      CGFontRelease(CGFontRef self) {
   O2FontRelease(self);
}

CFStringRef CGFontCopyFullName(CGFontRef self) {
   return (CFStringRef)O2FontCopyFullName(self);
}

int       CGFontGetUnitsPerEm(CGFontRef self) {
   return O2FontGetUnitsPerEm(self);
}

int       CGFontGetAscent(CGFontRef self) {
   return O2FontGetAscent(self);
}

int       CGFontGetDescent(CGFontRef self) {
   return O2FontGetDescent(self);
}

int       CGFontGetLeading(CGFontRef self) {
   return O2FontGetLeading(self);
}

int       CGFontGetCapHeight(CGFontRef self) {
   return O2FontGetCapHeight(self);
}

int       CGFontGetXHeight(CGFontRef self) {
   return O2FontGetXHeight(self);
}

CGFloat   CGFontGetItalicAngle(CGFontRef self) {
   return O2FontGetItalicAngle(self);
}

CGFloat   CGFontGetStemV(CGFontRef self) {
   return O2FontGetStemV(self);
}

CGRect    CGFontGetFontBBox(CGFontRef self) {
   return O2FontGetFontBBox(self);
}

size_t    CGFontGetNumberOfGlyphs(CGFontRef self) {
   return O2FontGetNumberOfGlyphs(self);
}

bool      CGFontGetGlyphAdvances(CGFontRef self,const CGGlyph *glyphs,size_t count,int *advances) {
   return O2FontGetGlyphAdvances(self,glyphs,count,advances);
}

CGGlyph   CGFontGetGlyphWithGlyphName(CGFontRef self,CFStringRef name) {
   return O2FontGetGlyphWithGlyphName(self,name);
}

CFStringRef CGFontCopyGlyphNameForGlyph(CGFontRef self,CGGlyph glyph) {
   return (CFStringRef)O2FontCopyGlyphNameForGlyph(self,glyph);
}

CFDataRef CGFontCopyTableForTag(CGFontRef self,uint32_t tag) {
   return (CFDataRef)O2FontCopyTableForTag(self,tag);
}

/* ---- Minkar: what the font's sfnt tables tell ---- */

static uint16_t read16(const UInt8 *p) { return (uint16_t)((p[0]<<8)|p[1]); }
static uint32_t read32(const UInt8 *p) { return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }

CGFontRef CGFontCreateWithDataProvider(CGDataProviderRef provider) {
   return (provider!=NULL)?O2FontCreateWithDataProvider(provider):NULL;
}

/* The table directory of the whole font file (FreeType gives the file
   for the tag 0); a collection's first font. */
CFArrayRef CGFontCopyTableTags(CGFontRef self) {
   CFDataRef          file=CGFontCopyTableForTag(self,0);
   CFMutableArrayRef  tags;
   const UInt8       *bytes;
   CFIndex            length;
   uint32_t           offset=0;

   if(file==NULL)
    return NULL;
   bytes=CFDataGetBytePtr(file);
   length=CFDataGetLength(file);
   if(length>=16 && read32(bytes)=='ttcf')
    offset=read32(bytes+12);
   tags=CFArrayCreateMutable(NULL,0,NULL);
   if((CFIndex)offset+12<=length){
    uint16_t count=read16(bytes+offset+4);

    for(uint16_t i=0;i<count && (CFIndex)offset+12+16*(i+1)<=length;i++)
     CFArrayAppendValue(tags,(const void *)(uintptr_t)read32(bytes+offset+12+16*i));
   }
   CFRelease(file);
   return tags;
}

/* A name of the 'name' table: Windows Unicode in English first, then any
   Windows Unicode, then Mac Roman, then Unicode. */
CFStringRef _CGFontCopyNameForID(CGFontRef self,uint16_t nameID) {
   CFDataRef    table=CGFontCopyTableForTag(self,'name');
   const UInt8 *bytes;
   CFIndex      length;
   int          best=-1,bestRank=100;
   CFStringRef  result=NULL;

   if(table==NULL)
    return NULL;
   bytes=CFDataGetBytePtr(table);
   length=CFDataGetLength(table);
   if(length>=6){
    uint16_t count=read16(bytes+2);
    uint16_t strings=read16(bytes+4);

    for(int i=0;i<count && 6+12*(i+1)<=length;i++){
     const UInt8 *r=bytes+6+12*i;
     uint16_t platform=read16(r),encoding=read16(r+2),language=read16(r+4);
     int      rank;

     if(read16(r+6)!=nameID)
      continue;
     if(platform==3 && (encoding==1 || encoding==10))
      rank=(language==0x409)?0:1;
     else if(platform==1 && encoding==0)
      rank=(language==0)?2:3;
     else if(platform==0)
      rank=4;
     else
      continue;
     if(rank<bestRank){
      bestRank=rank;
      best=i;
     }
    }
    if(best>=0){
     const UInt8 *r=bytes+6+12*best;
     uint16_t     size=read16(r+8);
     CFIndex      start=strings+read16(r+10);

     if(start+size<=length)
      result=CFStringCreateWithBytes(NULL,bytes+start,size,
       (read16(r)==1)?kCFStringEncodingMacRoman:kCFStringEncodingUTF16BE,false);
    }
   }
   CFRelease(table);
   return result;
}

CFStringRef CGFontCopyPostScriptName(CGFontRef self) {
   CFStringRef name;

   if(self==NULL)
    return NULL;
   name=_CGFontCopyNameForID(self,6);
   return (name!=NULL)?name:CGFontCopyFullName(self);
}

bool CGFontGetGlyphBBoxes(CGFontRef self,const CGGlyph *glyphs,size_t count,CGRect *bboxes) {
   if(self==NULL || (count>0 && (glyphs==NULL || bboxes==NULL)))
    return false;
   return O2FontGetGlyphBBoxes(self,glyphs,count,bboxes);
}

static CFNumberRef newDouble(double value) {
   return CFNumberCreate(NULL,kCFNumberDoubleType,&value);
}

/* The 'fvar' axes, each with its tag beside the four public keys: the
   tag is what CoreText's axes are named by. */
CFArrayRef _CGFontCopyVariationAxesWithTags(CGFontRef self,bool withTags) {
   CFDataRef          table=CGFontCopyTableForTag(self,'fvar');
   CFMutableArrayRef  axes=NULL;
   const UInt8       *bytes;
   CFIndex            length;

   if(table==NULL)
    return NULL;
   bytes=CFDataGetBytePtr(table);
   length=CFDataGetLength(table);
   if(length>=16){
    uint16_t first=read16(bytes+4),count=read16(bytes+8),size=read16(bytes+10);

    if(count>0 && size>=20)
     axes=CFArrayCreateMutable(NULL,count,&kCFTypeArrayCallBacks);
    for(uint16_t i=0;axes!=NULL && i<count && first+(CFIndex)size*(i+1)<=length;i++){
     const UInt8           *a=bytes+first+size*i;
     uint32_t               tag=read32(a);
     CFStringRef            name=_CGFontCopyNameForID(self,read16(a+18));
     CFMutableDictionaryRef axis=CFDictionaryCreateMutable(NULL,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
     CFNumberRef            n;

     if(name==NULL){
      char t[5]={ (char)(tag>>24),(char)(tag>>16),(char)(tag>>8),(char)tag,0 };
      name=CFStringCreateWithCString(NULL,t,kCFStringEncodingMacRoman);
     }
     CFDictionarySetValue(axis,kCGFontVariationAxisName,name);
     CFRelease(name);
     n=newDouble((int32_t)read32(a+4)/65536.0);
     CFDictionarySetValue(axis,kCGFontVariationAxisMinValue,n);
     CFRelease(n);
     n=newDouble((int32_t)read32(a+8)/65536.0);
     CFDictionarySetValue(axis,kCGFontVariationAxisDefaultValue,n);
     CFRelease(n);
     n=newDouble((int32_t)read32(a+12)/65536.0);
     CFDictionarySetValue(axis,kCGFontVariationAxisMaxValue,n);
     CFRelease(n);
     if(withTags){
      int32_t value=(int32_t)tag;

      n=CFNumberCreate(NULL,kCFNumberSInt32Type,&value);
      CFDictionarySetValue(axis,CFSTR("tag"),n);
      CFRelease(n);
     }
     CFArrayAppendValue(axes,axis);
     CFRelease(axis);
    }
   }
   CFRelease(table);
   return axes;
}

CFArrayRef CGFontCopyVariationAxes(CGFontRef self) {
   return (self!=NULL)?_CGFontCopyVariationAxesWithTags(self,false):NULL;
}

CGFontRef CGFontCreateCopyWithVariations(CGFontRef self,CFDictionaryRef variations) {
   if(self==NULL)
    return NULL;
   return O2FontCreateCopyWithVariations(self,(NSDictionary *)variations);
}

CFDictionaryRef CGFontCopyVariations(CGFontRef self) {
   return (self!=NULL)?(CFDictionaryRef)O2FontCopyVariations(self):NULL;
}
