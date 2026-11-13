/* Copyright (c) 2008 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreText/CTFont.h>
#import "KTFont.h"
#include <stdlib.h>
#include <CoreGraphics/CGContext.h>

CTFontRef CTFontCreateWithGraphicsFont(CGFontRef cgFont,CGFloat size,CGAffineTransform *xform,CFTypeRef attributes) {
   if(cgFont==NULL)
    return NULL;
   return (CTFontRef)[[KTFont alloc] initWithFont:cgFont size:(size>0)?size:12.0];
}

CTFontRef CTFontCreateUIFontForLanguage(CTFontUIFontType uiFontType,CGFloat size,CFStringRef language) {
   return (CTFontRef)[[KTFont alloc] initWithUIFontType:uiFontType size:size language:(NSString *)language];
}

CFStringRef CTFontCopyFullName(CTFontRef font) {
   KTFont *self=(KTFont *)font;
   return [self copyName];
}

CGFloat CTFontGetSize(CTFontRef font) {
   KTFont *self=(KTFont *)font;
   return [self pointSize];
}

size_t CTFontGetGlyphCount(CTFontRef font) {
   KTFont *self=(KTFont *)font;
   return [self numberOfGlyphs];
}

bool CTFontGetGlyphsForCharacters(CTFontRef font,const UniChar *characters,CGGlyph *glyphs,size_t count) {
   KTFont *self=(KTFont *)font;
   [self getGlyphs:glyphs forCharacters:characters length:count];
   // FIXME: change getGlyphs: to return a BOOL
   return YES;
}

CGRect CTFontGetBoundingBox(CTFontRef font) {
   KTFont *self=(KTFont *)font;
   return [self boundingRect];
}

// Minkar: the sum of the advances is returned, and advances may be NULL,
// as on the reference platform (it returned nothing and needed advances).
double CTFontGetAdvancesForGlyphs(CTFontRef font,CTFontOrientation orientation,const CGGlyph *glyphs,CGSize *advances,size_t count) {
   KTFont *self=(KTFont *)font;
   CGSize  buffer[64];
   CGSize *sizes=advances;
   double  total=0;

   if(count==0)
    return 0;
   if(sizes==NULL)
    sizes=(count<=64)?buffer:malloc(count*sizeof(CGSize));
   [self getAdvancements:sizes forGlyphs:glyphs count:count];
   for(size_t i=0;i<count;i++)
    total+=(orientation==kCTFontOrientationVertical)?sizes[i].height:sizes[i].width;
   if(sizes!=advances && sizes!=buffer)
    free(sizes);
   return total;
}

CGFloat CTFontGetUnderlinePosition(CTFontRef font) {
   KTFont *self=(KTFont *)font;
   return [self underlinePosition];
}

CGFloat CTFontGetUnderlineThickness(CTFontRef font) {
   KTFont *self=(KTFont *)font;
   return [self underlineThickness];
}

CGFloat CTFontGetAscent(CTFontRef font) {
   KTFont *self=(KTFont *)font;
   return [self ascender];
}

CGFloat CTFontGetDescent(CTFontRef font) {
   KTFont *self=(KTFont *)font;
   return [self descender];
}

CGFloat CTFontGetLeading(CTFontRef font) {
   KTFont *self=(KTFont *)font;
   return [self leading];
}

CGFloat CTFontGetSlantAngle(CTFontRef font) {
   KTFont *self=(KTFont *)font;
   return [self italicAngle];
}

CGFloat CTFontGetXHeight(CTFontRef font) {
   KTFont *self=(KTFont *)font;
   return [self xHeight];
}

CGFloat CTFontGetCapHeight(CTFontRef font) {
   KTFont *self=(KTFont *)font;
   return [self capHeight];
}

CGPathRef CTFontCreatePathForGlyph(CTFontRef font,CGGlyph glyph,CGAffineTransform *xform) {
   KTFont *self=(KTFont *)font;
   return (CGPathRef)[self createPathForGlyph:glyph transform:xform];
}

/* ---- Names and traits ---- */

/* Fonts are named Family-Style ("Inter-Bold"); a name without a style is
   the family's regular face. */
static NSString *familyOfName(NSString *name) {
   NSRange dash=[name rangeOfString:@"-" options:NSBackwardsSearch];
   return (dash.location==NSNotFound)?name:[name substringToIndex:dash.location];
}

static NSString *styleOfName(NSString *name) {
   NSRange dash=[name rangeOfString:@"-" options:NSBackwardsSearch];
   return (dash.location==NSNotFound)?@"Regular":[name substringFromIndex:NSMaxRange(dash)];
}

static CTFontSymbolicTraits traitsOfStyle(NSString *style) {
   CTFontSymbolicTraits traits=0;
   NSString *lower=[style lowercaseString];
   if([lower rangeOfString:@"italic"].location!=NSNotFound || [lower rangeOfString:@"oblique"].location!=NSNotFound)
    traits|=kCTFontTraitItalic;
   if([lower rangeOfString:@"bold"].location!=NSNotFound || [lower rangeOfString:@"black"].location!=NSNotFound ||
      [lower rangeOfString:@"heavy"].location!=NSNotFound)
    traits|=kCTFontTraitBold;
   if([lower rangeOfString:@"condensed"].location!=NSNotFound)
    traits|=kCTFontTraitCondensed;
   if([lower rangeOfString:@"expanded"].location!=NSNotFound)
    traits|=kCTFontTraitExpanded;
   return traits;
}

CTFontRef CTFontCreateWithName(CFStringRef name, CGFloat size, const CGAffineTransform *matrix) {
   CGFontRef cgFont=(name!=NULL)?CGFontCreateWithFontName(name):NULL;
   CTFontRef font;

   // A name the system lacks gives its default font, never NULL, as on the
   // reference platform (Helvetica is not one of Minkar's fonts).
   if(cgFont==NULL)
    cgFont=CGFontCreateWithFontName(CFSTR("Inter-Regular"));
   if(cgFont==NULL)
    return NULL;
   font=(CTFontRef)[[KTFont alloc] initWithFont:cgFont size:(size>0)?size:12.0];
   CGFontRelease(cgFont);
   return font;
}

CTFontRef CTFontCreateCopyWithAttributes(CTFontRef font, CGFloat size, const CGAffineTransform *matrix, CFTypeRef attributes) {
   KTFont *self=(KTFont *)font;
   NSString *name=[(NSString *)CTFontCopyFullName(self) autorelease];
   return CTFontCreateWithName((CFStringRef)name,(size>0)?size:[self pointSize],matrix);
}

CFStringRef CTFontCopyPostScriptName(CTFontRef font) {
   KTFont *self=(KTFont *)font;
   return CTFontCopyFullName(self);
}

extern CFStringRef _CGFontCopyNameForID(CGFontRef font,uint16_t nameID);

/* The family in the font's name table, as fontconfig and the font manager
   name it ("Liberation Sans"); a font standing in for another one keeps
   the name it was asked by. */
CFStringRef CTFontCopyFamilyName(CTFontRef font) {
   KTFont   *self=(KTFont *)font;
   NSString *name=[(NSString *)CTFontCopyFullName(self) autorelease];
   NSString *psName=[(NSString *)_CGFontCopyNameForID([self _cgFont],6) autorelease];

   if(psName!=nil && [psName isEqualToString:name]){
    CFStringRef family=_CGFontCopyNameForID([self _cgFont],16);

    if(family==NULL)
     family=_CGFontCopyNameForID([self _cgFont],1);
    if(family!=NULL)
     return family;
   }
   return (CFStringRef)[familyOfName(name) copy];
}

CFStringRef CTFontCopyDisplayName(CTFontRef font) {
   KTFont *self=(KTFont *)font;
   NSString *name=[(NSString *)CTFontCopyFullName(self) autorelease];
   NSString *style=styleOfName(name);
   if([style isEqualToString:@"Regular"] || [name rangeOfString:@"-"].location==NSNotFound)
    return (CFStringRef)[familyOfName(name) copy];
   return (CFStringRef)[[NSString alloc] initWithFormat:@"%@ %@",familyOfName(name),style];
}

CTFontSymbolicTraits CTFontGetSymbolicTraits(CTFontRef font) {
   KTFont *self=(KTFont *)font;
   NSString            *name=[(NSString *)CTFontCopyFullName(self) autorelease];
   CTFontSymbolicTraits traits=traitsOfStyle(styleOfName(name));
   CGGlyph              glyphs[2];
   UniChar              characters[2]={ 'i', 'W' };
   CGSize               advances[2];

   /* Monospaced: an i as wide as a W. */
   [self getGlyphs:glyphs forCharacters:characters length:2];
   if(glyphs[0]!=0 && glyphs[1]!=0){
    [self getAdvancements:advances forGlyphs:glyphs count:2];
    if(advances[0].width>0 && advances[0].width==advances[1].width)
     traits|=kCTFontTraitMonoSpace;
   }
   return traits;
}

CTFontRef CTFontCreateCopyWithSymbolicTraits(CTFontRef font, CGFloat size, const CGAffineTransform *matrix,
                                             CTFontSymbolicTraits value, CTFontSymbolicTraits mask) {
   KTFont              *self=(KTFont *)font;
   NSString            *name=[(NSString *)CTFontCopyFullName(self) autorelease];
   NSString            *family=familyOfName(name);
   CTFontSymbolicTraits wanted=(traitsOfStyle(styleOfName(name))&~mask)|(value&mask);
   NSString            *style;
   CGFontRef            cgFont;
   NSString            *found;
   CTFontRef            result;

   wanted&=(kCTFontTraitBold|kCTFontTraitItalic);
   style=(wanted==(kCTFontTraitBold|kCTFontTraitItalic))?@"BoldItalic":
         (wanted==kCTFontTraitBold)?@"Bold":(wanted==kCTFontTraitItalic)?@"Italic":@"Regular";
   if(size<=0)
    size=[self pointSize];
   cgFont=CGFontCreateWithFontName((CFStringRef)[NSString stringWithFormat:@"%@-%@",family,style]);
   if(cgFont==NULL)
    return NULL;
   /* The font found must be that face, not a stand-in. */
   found=[(NSString *)CGFontCopyFullName(cgFont) autorelease];
   if(!(traitsOfStyle(styleOfName(found))==wanted || ([style isEqualToString:@"Regular"] && traitsOfStyle(styleOfName(found))==0)) ||
      ![familyOfName(found) isEqualToString:family]){
    CGFontRelease(cgFont);
    return NULL;
   }
   result=(CTFontRef)[[KTFont alloc] initWithFont:cgFont size:size];
   CGFontRelease(cgFont);
   return result;
}

CGFontRef CTFontCopyGraphicsFont(CTFontRef font, CFTypeRef *attributes) {
   KTFont *self=(KTFont *)font;
   if(attributes!=NULL)
    *attributes=NULL;
   return CGFontRetain([self _cgFont]);
}

CGFloat CTFontGetUnitsPerEm(CTFontRef font) {
   KTFont *self=(KTFont *)font;
   return CGFontGetUnitsPerEm([self _cgFont]);
}

/* Fonts are KTFont objects with a CF type of their own. */
extern CFTypeID _CFRuntimeRegisterClass(const void *runtimeClass);
static const struct { CFIndex version; const char *cls; void *functions[16]; } fontRuntimeClass={ 0, "CTFont", { NULL } };

CFTypeID CTFontGetTypeID(void) {
   static CFTypeID typeID;
   if(typeID==0)
    typeID=_CFRuntimeRegisterClass(&fontRuntimeClass);
   return typeID;
}

@implementation KTFont (CFType)
-(CFTypeID)_cfTypeID { return CTFontGetTypeID(); }
@end

/* ---- Minkar: the font's tables ---- */

static uint16_t read16(const UInt8 *p) { return (uint16_t)((p[0]<<8)|p[1]); }
static uint32_t read32(const UInt8 *p) { return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }

CFDataRef CTFontCopyTable(CTFontRef font,CTFontTableTag table,CTFontTableOptions options) {
   if(font==NULL)
    return NULL;
   return CGFontCopyTableForTag([(KTFont *)font _cgFont],table);
}

CFArrayRef CTFontCopyAvailableTables(CTFontRef font,CTFontTableOptions options) {
   if(font==NULL)
    return NULL;
   return CGFontCopyTableTags([(KTFont *)font _cgFont]);
}

static uint16_t nameIDOfKey(CFStringRef key) {
   static const struct { const CFStringRef *key; uint16_t nameID; } ids[]={
    { &kCTFontCopyrightNameKey,0 }, { &kCTFontFamilyNameKey,1 }, { &kCTFontSubFamilyNameKey,2 },
    { &kCTFontStyleNameKey,2 }, { &kCTFontUniqueNameKey,3 }, { &kCTFontFullNameKey,4 },
    { &kCTFontVersionNameKey,5 }, { &kCTFontPostScriptNameKey,6 },
   };
   for(size_t i=0;key!=NULL && i<sizeof(ids)/sizeof(ids[0]);i++)
    if(CFEqual(key,*ids[i].key))
     return ids[i].nameID;
   return 0xFFFF;
}

CFStringRef CTFontCopyName(CTFontRef font,CFStringRef nameKey) {
   uint16_t nameID=nameIDOfKey(nameKey);

   if(font==NULL || nameID==0xFFFF)
    return NULL;
   if(nameID==1)
    return CTFontCopyFamilyName(font);
   if(nameID==6)
    return CTFontCopyPostScriptName(font);
   return _CGFontCopyNameForID([(KTFont *)font _cgFont],nameID);
}

/* The names are read in English, the language reported. */
CFStringRef CTFontCopyLocalizedName(CTFontRef font,CFStringRef nameKey,CFStringRef *actualLanguage) {
   CFStringRef name=CTFontCopyName(font,nameKey);

   if(actualLanguage!=NULL)
    *actualLanguage=(name!=NULL)?CFSTR("en"):NULL;
   return name;
}

/* The characters of the best Unicode 'cmap' subtable (format 4 or 12). */
CFCharacterSetRef CTFontCopyCharacterSet(CTFontRef font) {
   CFDataRef                table;
   const UInt8             *bytes;
   CFIndex                  length;
   CFMutableCharacterSetRef set;
   uint32_t                 best=0;
   int                      bestRank=0;
   CFCharacterSetRef        result;

   if(font==NULL)
    return NULL;
   set=CFCharacterSetCreateMutable(NULL);
   table=CGFontCopyTableForTag([(KTFont *)font _cgFont],'cmap');
   if(table!=NULL){
    bytes=CFDataGetBytePtr(table);
    length=CFDataGetLength(table);
    for(uint16_t i=0;length>=4 && i<read16(bytes+2) && 4+8*(i+1)<=length;i++){
     uint16_t platform=read16(bytes+4+8*i),encoding=read16(bytes+6+8*i);
     uint32_t offset=read32(bytes+8+8*i);
     int      rank=0;

     if(offset+2>(uint32_t)length)
      continue;
     uint16_t format=read16(bytes+offset);
     if(format==12 && ((platform==3 && encoding==10) || platform==0))
      rank=3;
     else if(format==4 && ((platform==3 && encoding==1) || platform==0))
      rank=2;
     else if(format==4 && platform==3 && encoding==0)
      rank=1;
     if(rank>bestRank){
      bestRank=rank;
      best=offset;
     }
    }
    if(bestRank>0 && read16(bytes+best)==4 && best+14<=(uint32_t)length){
     uint16_t segments=read16(bytes+best+6)/2;
     const UInt8 *ends=bytes+best+14,*starts=ends+2*segments+2;

     if(starts+2*segments<=bytes+length)
      for(uint16_t i=0;i<segments;i++){
       uint16_t start=read16(starts+2*i),end=read16(ends+2*i);

       if(start<=end && start!=0xFFFF)
        CFCharacterSetAddCharactersInRange(set,CFRangeMake(start,end-start+1));
      }
    }
    else if(bestRank>0 && read16(bytes+best)==12 && best+16<=(uint32_t)length){
     uint32_t groups=read32(bytes+best+12);

     for(uint32_t i=0;i<groups && best+16+12*(i+1)<=(uint32_t)length;i++){
      uint32_t start=read32(bytes+best+16+12*i),end=read32(bytes+best+20+12*i);

      if(start<=end && end<=0x10FFFF)
       CFCharacterSetAddCharactersInRange(set,CFRangeMake(start,end-start+1));
     }
    }
    CFRelease(table);
   }
   result=CFCharacterSetCreateCopy(NULL,set);
   CFRelease(set);
   return result;
}

/* The OpenType features of 'GSUB' and 'GPOS', each by its tag. */
CFArrayRef CTFontCopyFeatures(CTFontRef font) {
   CFMutableArrayRef features;
   CFMutableSetRef   seen;
   uint32_t          tables[2]={ 'GSUB','GPOS' };

   if(font==NULL)
    return NULL;
   features=CFArrayCreateMutable(NULL,0,&kCFTypeArrayCallBacks);
   seen=CFSetCreateMutable(NULL,0,&kCFTypeSetCallBacks);
   for(int t=0;t<2;t++){
    CFDataRef table=CGFontCopyTableForTag([(KTFont *)font _cgFont],tables[t]);

    if(table==NULL)
     continue;
    const UInt8 *bytes=CFDataGetBytePtr(table);
    CFIndex      length=CFDataGetLength(table);

    if(length>=10){
     uint16_t list=read16(bytes+6);

     if(list+2<=length){
      uint16_t count=read16(bytes+list);

      for(uint16_t i=0;i<count && list+2+6*(i+1)<=length;i++){
       uint32_t    tag=read32(bytes+list+2+6*i);
       char        name[5]={ (char)(tag>>24),(char)(tag>>16),(char)(tag>>8),(char)tag,0 };
       CFStringRef string=CFStringCreateWithCString(NULL,name,kCFStringEncodingMacRoman);

       if(string!=NULL && !CFSetContainsValue(seen,string)){
        const void *keys[]={ kCTFontOpenTypeFeatureTag };
        const void *values[]={ string };
        CFDictionaryRef feature=CFDictionaryCreate(NULL,keys,values,1,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);

        CFSetAddValue(seen,string);
        CFArrayAppendValue(features,feature);
        CFRelease(feature);
       }
       if(string!=NULL)
        CFRelease(string);
      }
     }
    }
    CFRelease(table);
   }
   CFRelease(seen);
   return features;
}

extern CFArrayRef _CGFontCopyVariationAxesWithTags(CGFontRef font,bool withTags);

/* The axes of a variable font, named by their tags as numbers. */
CFArrayRef CTFontCopyVariationAxes(CTFontRef font) {
   CFArrayRef        axes;
   CFMutableArrayRef result;

   if(font==NULL || (axes=_CGFontCopyVariationAxesWithTags([(KTFont *)font _cgFont],true))==NULL)
    return NULL;
   result=CFArrayCreateMutable(NULL,0,&kCFTypeArrayCallBacks);
   for(CFIndex i=0;i<CFArrayGetCount(axes);i++){
    CFDictionaryRef        axis=CFArrayGetValueAtIndex(axes,i);
    CFMutableDictionaryRef d=CFDictionaryCreateMutable(NULL,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(d,kCTFontVariationAxisIdentifierKey,CFDictionaryGetValue(axis,CFSTR("tag")));
    CFDictionarySetValue(d,kCTFontVariationAxisMinimumValueKey,CFDictionaryGetValue(axis,kCGFontVariationAxisMinValue));
    CFDictionarySetValue(d,kCTFontVariationAxisMaximumValueKey,CFDictionaryGetValue(axis,kCGFontVariationAxisMaxValue));
    CFDictionarySetValue(d,kCTFontVariationAxisDefaultValueKey,CFDictionaryGetValue(axis,kCGFontVariationAxisDefaultValue));
    CFDictionarySetValue(d,kCTFontVariationAxisNameKey,CFDictionaryGetValue(axis,kCGFontVariationAxisName));
    CFArrayAppendValue(result,d);
    CFRelease(d);
   }
   CFRelease(axes);
   return result;
}

/* An instance's values, by axis tag; NULL for a font at its defaults. */
CFDictionaryRef CTFontCopyVariation(CTFontRef font) {
   CFDictionaryRef        byName;
   CFArrayRef             axes;
   CFMutableDictionaryRef result;

   if(font==NULL || (byName=CGFontCopyVariations([(KTFont *)font _cgFont]))==NULL)
    return NULL;
   axes=_CGFontCopyVariationAxesWithTags([(KTFont *)font _cgFont],true);
   result=CFDictionaryCreateMutable(NULL,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
   for(CFIndex i=0;axes!=NULL && i<CFArrayGetCount(axes);i++){
    CFDictionaryRef axis=CFArrayGetValueAtIndex(axes,i);
    CFTypeRef       value=CFDictionaryGetValue(byName,CFDictionaryGetValue(axis,kCGFontVariationAxisName));

    if(value!=NULL)
     CFDictionarySetValue(result,CFDictionaryGetValue(axis,CFSTR("tag")),value);
   }
   if(axes!=NULL)
    CFRelease(axes);
   CFRelease(byName);
   if(CFDictionaryGetCount(result)==0){
    CFRelease(result);
    return NULL;
   }
   return result;
}

extern CFStringRef _CTFontCopyPostScriptNameForCharacter(UTF32Char character,CFStringRef language);

/* The font itself when it has the characters, else the face fontconfig
   finds for the first one it lacks, at the same size. */
CTFontRef CTFontCreateForString(CTFontRef font,CFStringRef string,CFRange range) {
   KTFont *self=(KTFont *)font;

   if(font==NULL)
    return NULL;
   if(string==NULL || range.length<=0)
    return CFRetain(font);
   for(CFIndex i=range.location;i<range.location+range.length && i<CFStringGetLength(string);i++){
    UniChar   pair[2]={ CFStringGetCharacterAtIndex(string,i),0 };
    UTF32Char c=pair[0];
    CGGlyph   glyph=0;

    if(CFStringIsSurrogateHighCharacter(pair[0]) && i+1<CFStringGetLength(string)){
     pair[1]=CFStringGetCharacterAtIndex(string,i+1);
     c=CFStringGetLongCharacterForSurrogatePair(pair[0],pair[1]);
     i++;
    }
    /* Controls and spaces never decide the font. */
    if(c<0x20 || c==0x200B || c==0x200C || c==0x200D || c==0xFEFF)
     continue;
    if(c<=0xFFFF){
     UniChar u=(UniChar)c;

     [self getGlyphs:&glyph forCharacters:&u length:1];
    }
    else {
     CFCharacterSetRef set=CTFontCopyCharacterSet(font);

     glyph=CFCharacterSetIsLongCharacterMember(set,c)?1:0;
     CFRelease(set);
    }
    if(glyph!=0)
     continue;

    CFStringRef name=_CTFontCopyPostScriptNameForCharacter(c,NULL);
    CTFontRef   result=NULL;

    if(name!=NULL){
     result=CTFontCreateWithName(name,[self pointSize],NULL);
     CFRelease(name);
    }
    return (result!=NULL)?result:CFRetain(font);
   }
   return CFRetain(font);
}

void CTFontDrawGlyphs(CTFontRef font,const CGGlyph glyphs[],const CGPoint positions[],size_t count,CGContextRef context) {
   if(font==NULL || context==NULL || count==0)
    return;
   CGContextSetFont(context,[(KTFont *)font _cgFont]);
   CGContextSetFontSize(context,[(KTFont *)font pointSize]);
   CGContextShowGlyphsAtPositions(context,glyphs,positions,count);
}

/* Each glyph's box at the font's size, and the box of them all. */
CGRect CTFontGetBoundingRectsForGlyphs(CTFontRef font,CTFontOrientation orientation,const CGGlyph glyphs[],CGRect *boundingRects,CFIndex count) {
   KTFont *self=(KTFont *)font;
   CGRect  all=CGRectNull;
   CGRect  buffer[64];
   CGRect *rects;
   CGFloat scale;

   if(font==NULL)
    return CGRectZero;
   if(glyphs==NULL || count<=0)
    return [self boundingRect];
   scale=[self pointSize]/CGFontGetUnitsPerEm([self _cgFont]);
   rects=(boundingRects!=NULL)?boundingRects:(count<=64)?buffer:malloc(count*sizeof(CGRect));
   if(!CGFontGetGlyphBBoxes([self _cgFont],glyphs,count,rects)){
    for(CFIndex i=0;i<count;i++){
     CGPathRef path=CTFontCreatePathForGlyph(font,glyphs[i],NULL);

     rects[i]=(path!=NULL)?CGPathGetBoundingBox(path):CGRectZero;
     if(path!=NULL)
      CGPathRelease(path);
    }
   }
   else
    for(CFIndex i=0;i<count;i++)
     rects[i]=CGRectMake(rects[i].origin.x*scale,rects[i].origin.y*scale,rects[i].size.width*scale,rects[i].size.height*scale);
   for(CFIndex i=0;i<count;i++)
    if(!CGRectIsEmpty(rects[i]))
     all=CGRectUnion(all,rects[i]);
   if(rects!=boundingRects && rects!=buffer)
    free(rects);
   return CGRectIsNull(all)?CGRectZero:all;
}
