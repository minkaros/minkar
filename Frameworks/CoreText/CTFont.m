/* Copyright (c) 2008 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreText/CTFont.h>
#import "KTFont.h"

CTFontRef CTFontCreateWithGraphicsFont(CGFontRef cgFont,CGFloat size,CGAffineTransform *xform,CFTypeRef attributes) {
   return (CTFontRef)[[KTFont alloc] initWithFont:cgFont size:size];
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

void CTFontGetAdvancesForGlyphs(CTFontRef font,int orientation,const CGGlyph *glyphs,CGSize *advances,size_t count) {
   KTFont *self=(KTFont *)font;
   [self getAdvancements:advances forGlyphs:glyphs count:count];
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
   CGFontRef cgFont=CGFontCreateWithFontName((name!=NULL)?name:CFSTR("Helvetica"));
   CTFontRef font;

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

CFStringRef CTFontCopyFamilyName(CTFontRef font) {
   KTFont *self=(KTFont *)font;
   NSString *name=[(NSString *)CTFontCopyFullName(self) autorelease];
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
