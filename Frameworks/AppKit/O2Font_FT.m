/* Copyright (c) 2008 Johannes Fortmann
   Copyright (c) 2009 Christopher J. W. Lloyd - <cjwl@objc.net>
   Modifications Copyright (c) 2026 The Minkar contributors

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import "O2Font_FT.h"
#import <Onyx2D/O2Font_freetype.h>
#include FT_TRUETYPE_TABLES_H

O2FontRef O2FontCreateWithFontName_platform(NSString *name) {
    return [[O2Font_FT alloc] initWithFontName:name];
}

/* Minkar: a font made from data is the same kind of font as one made
 * by name, so CoreText and AppKit can lay out and draw it. */
O2FontRef O2FontCreateWithDataProvider_platform(O2DataProviderRef provider) {
    return [[O2Font_FT alloc] initWithDataProvider:provider];
}

@implementation O2Font(FreeType)

+allocWithZone:(NSZone *)zone {
   return NSAllocateObject([O2Font_FT class],0,NULL);
}

@end

@implementation O2Font_FT

FT_Library O2FontSharedFreeTypeLibrary(){
   static FT_Library library=NULL;

   if(library==NULL){
    if(FT_Init_FreeType(&library)!=0)
     NSLog(@"FT_Init_FreeType failed");
   }

   return library;
}

/* Minkar: CoreText's, so the fonts an app registers there are found here. */
extern void *_CTFontManagerSharedFontConfig(void);

FcConfig *O2FontSharedFontConfig() {
   return (FcConfig *)_CTFontManagerSharedFontConfig();
}

/* Minkar: a PostScript name is looked up as one first: a family whose
 * name has spaces ("Monaspace Neon", MonaspaceNeon-Regular) is not the
 * part before the hyphen, which matched no font and gave the first one. */
/* The face's index in its file: a collection (.ttc) holds several faces,
 * and the one asked for is rarely the first. */
static FT_Long faceIndexOf(FcPattern *font) {
   int index=0;

   if(FcPatternGetInteger(font, FC_INDEX, 0, &index)!=FcResultMatch)
    index=0;
   return index;
}

static NSString *filenameAndIndexForPostScriptName(NSString *name,FT_Long *index) {
   FcPattern *pat=FcPatternCreate();
   FcObjectSet *props=FcObjectSetBuild(FC_FILE, FC_INDEX, NULL);
   NSString *ret=nil;

   FcPatternAddString(pat, FC_POSTSCRIPT_NAME, (const FcChar8 *)[name UTF8String]);
   FcFontSet *set=FcFontList(O2FontSharedFontConfig(), pat, props);
   if(set!=NULL) {
      FcChar8 *filename;

      if(set->nfont>0 && FcPatternGetString(set->fonts[0], FC_FILE, 0, &filename)==FcResultMatch) {
         ret=[NSString stringWithUTF8String:(char *)filename];
         if(index!=NULL)
          *index=faceIndexOf(set->fonts[0]);
      }
      FcFontSetDestroy(set);
   }
   FcPatternDestroy(pat);
   FcObjectSetDestroy(props);
   return ret;
}

static NSString *filenameForPostScriptName(NSString *name) {
   return filenameAndIndexForPostScriptName(name,NULL);
}

static NSString *filenameAndIndexForPattern(Class self,NSString *pattern,FT_Long *index) {
   int i;
   if([pattern length]>0 && [pattern rangeOfString:@":"].location==NSNotFound) {
      NSString *file=filenameAndIndexForPostScriptName(pattern,index);

      if(file!=nil)
         return file;
   }
   pattern = [self nativeFontNameForPostscriptName:pattern];
   FcPattern *pat=FcNameParse((unsigned char*)[pattern UTF8String]);

   FcObjectSet *props=FcObjectSetBuild(FC_FILE, FC_INDEX, NULL);

   FcFontSet *set = FcFontList (O2FontSharedFontConfig(), pat, props);
   NSString* ret=NULL;
   for(i = 0; i < set->nfont && !ret; i++) {
      FcChar8 *filename;

      if (FcPatternGetString (set->fonts[i], FC_FILE, 0, &filename) == FcResultMatch) {
         ret=[NSString stringWithUTF8String:(char*)filename];
         if(index!=NULL)
          *index=faceIndexOf(set->fonts[i]);
      }
   }

   FcPatternDestroy(pat);
   FcObjectSetDestroy(props);
   FcFontSetDestroy(set);
   return ret;
}

+(NSString*)filenameForPattern:(NSString *)pattern {
   return filenameAndIndexForPattern(self,pattern,NULL);
}

/* Minkar: the fonts applications ask for by name that the system does not
 * have, led to free faces that take the same room (installed by
 * SystemLibrary/Fonts): a name is a family and a style, each face of a
 * family the style it is asked for in. A family not in the table goes by
 * its kind: fixed-pitch to Agave, serif to Liberation Serif, the rest to
 * the system font. A font that is installed is never replaced. */
typedef struct {
   const char *regular,*bold,*italic,*boldItalic;
} substituteFaces;

static const substituteFaces liberationSans={ "LiberationSans","LiberationSans-Bold","LiberationSans-Italic","LiberationSans-BoldItalic" };
static const substituteFaces liberationSerif={ "LiberationSerif","LiberationSerif-Bold","LiberationSerif-Italic","LiberationSerif-BoldItalic" };
static const substituteFaces liberationMono={ "LiberationMono","LiberationMono-Bold","LiberationMono-Italic","LiberationMono-BoldItalic" };
static const substituteFaces dejaVuSans={ "DejaVuSans","DejaVuSans-Bold","DejaVuSans-Oblique","DejaVuSans-BoldOblique" };
static const substituteFaces dejaVuSansMono={ "DejaVuSansMono","DejaVuSansMono-Bold","DejaVuSansMono-Oblique","DejaVuSansMono-BoldOblique" };
static const substituteFaces gelasio={ "Gelasio-Regular","Gelasio_700wght","Gelasio-Italic","GelasioItalic_700wght" };
static const substituteFaces pagella={ "TeXGyrePagella-Regular","TeXGyrePagella-Bold","TeXGyrePagella-Italic","TeXGyrePagella-BoldItalic" };
static const substituteFaces inter={ "Inter-Regular","Inter-Bold","Inter-Italic","Inter-BoldItalic" };
static const substituteFaces agave={ "Agave-Regular","Agave-Bold","Agave-Regular","Agave-Bold" };
static const substituteFaces notoJP={ "NotoSansCJKjp-Regular","NotoSansCJKjp-Bold","NotoSansCJKjp-Regular","NotoSansCJKjp-Bold" };
static const substituteFaces notoSC={ "NotoSansCJKsc-Regular","NotoSansCJKsc-Bold","NotoSansCJKsc-Regular","NotoSansCJKsc-Bold" };
static const substituteFaces notoTC={ "NotoSansCJKtc-Regular","NotoSansCJKtc-Bold","NotoSansCJKtc-Regular","NotoSansCJKtc-Bold" };
static const substituteFaces notoKR={ "NotoSansCJKkr-Regular","NotoSansCJKkr-Bold","NotoSansCJKkr-Regular","NotoSansCJKkr-Bold" };
static const substituteFaces notoArabic={ "NotoSansArabic-Regular","NotoSansArabic-Bold","NotoSansArabic-Regular","NotoSansArabic-Bold" };

static const struct { const char *family; const substituteFaces *faces; } substituteFamilies[]={
   { "helvetica",&liberationSans }, { "helveticaneue",&liberationSans }, { "arial",&liberationSans },
   { "times",&liberationSerif }, { "timesnewroman",&liberationSerif },
   { "courier",&liberationMono }, { "couriernew",&liberationMono },
   { "menlo",&dejaVuSansMono }, { "monaco",&dejaVuSansMono }, { "sfmono",&dejaVuSansMono },
   { ".sfnsmono",&dejaVuSansMono }, { "andalemono",&dejaVuSansMono },
   { "lucidagrande",&dejaVuSans }, { "geneva",&dejaVuSans }, { "verdana",&dejaVuSans },
   { "georgia",&gelasio },
   { "palatino",&pagella }, { "palatinolinotype",&pagella },
   { ".applesystemuifont",&inter }, { ".sfnstext",&inter }, { ".sfnsdisplay",&inter },
   { ".sfns",&inter }, { "sfprotext",&inter }, { "sfprodisplay",&inter }, { "sfpro",&inter },
   { "sanfrancisco",&inter },
   /* Writing systems Inter lacks: the reference platform's families for
      them, to Noto's (a stand-in without their glyphs drew empty boxes). */
   { "hiraginosans",&notoJP }, { "hiraginokakugothicpro",&notoJP }, { "hiraginokakugothicpron",&notoJP },
   { "hiraginokakugothicstd",&notoJP }, { "hiraginomarugothicpro",&notoJP }, { "hiraginomarugothicpron",&notoJP },
   { "hiraginominchopro",&notoJP }, { "hiraginominchopron",&notoJP }, { "osaka",&notoJP }, { "yugothic",&notoJP },
   { "yumincho",&notoJP },
   { "pingfangsc",&notoSC }, { "heitisc",&notoSC }, { "stheitisc",&notoSC }, { "songtisc",&notoSC },
   { "stsong",&notoSC }, { "stheiti",&notoSC }, { "hiraginosansgb",&notoSC },
   { "pingfangtc",&notoTC }, { "pingfanghk",&notoTC }, { "heititc",&notoTC }, { "songtitc",&notoTC },
   { "applesdgothicneo",&notoKR }, { "applegothic",&notoKR }, { "applemyungjo",&notoKR },
   { "nanumgothic",&notoKR }, { "nanummyeongjo",&notoKR },
   { "geezapro",&notoArabic }, { "baghdad",&notoArabic }, { "damascus",&notoArabic }, { "alnile",&notoArabic },
   { "kufistandardgk",&notoArabic }, { "decotypenaskh",&notoArabic },
};

static NSString *stripSuffix(NSString *string,NSArray *suffixes) {
   for(NSString *suffix in suffixes)
    if([string length]>[suffix length] && [string hasSuffix:suffix])
     return [string substringToIndex:[string length]-[suffix length]];
   return string;
}

static NSString *substituteFontName(NSString *name) {
   NSString *lower=[[name lowercaseString] stringByReplacingOccurrencesOfString:@" " withString:@""];
   NSRange   hyphen=[lower rangeOfString:@"-"];
   NSString *family=(hyphen.location==NSNotFound)?lower:[lower substringToIndex:hyphen.location];
   NSString *style=(hyphen.location==NSNotFound)?@"":[lower substringFromIndex:NSMaxRange(hyphen)];
   NSArray  *vendor=[NSArray arrayWithObjects:@"psmt",@"mt",@"ps",nil];
   const substituteFaces *faces=NULL;
   size_t    i;

   family=stripSuffix(family,vendor);
   style=stripSuffix(style,vendor);
   for(i=0;i<sizeof(substituteFamilies)/sizeof(substituteFamilies[0]);i++)
    if([family isEqualToString:[NSString stringWithUTF8String:substituteFamilies[i].family]])
     faces=substituteFamilies[i].faces;
   if(faces==NULL){
    if([family rangeOfString:@"mono"].location!=NSNotFound || [family rangeOfString:@"courier"].location!=NSNotFound ||
       [family rangeOfString:@"code"].location!=NSNotFound || [family rangeOfString:@"console"].location!=NSNotFound ||
       [family rangeOfString:@"typewriter"].location!=NSNotFound)
     faces=&agave;
    else if([family rangeOfString:@"serif"].location!=NSNotFound && [family rangeOfString:@"sans"].location==NSNotFound)
     faces=&liberationSerif;
    else if([family rangeOfString:@"times"].location!=NSNotFound || [family rangeOfString:@"garamond"].location!=NSNotFound ||
            [family rangeOfString:@"baskerville"].location!=NSNotFound || [family rangeOfString:@"roman"].location!=NSNotFound)
     faces=&liberationSerif;
    else
     faces=&inter;
   }

   BOOL bold=[style rangeOfString:@"bold"].location!=NSNotFound || [style rangeOfString:@"black"].location!=NSNotFound ||
             [style rangeOfString:@"heavy"].location!=NSNotFound;
   BOOL italic=[style rangeOfString:@"italic"].location!=NSNotFound || [style rangeOfString:@"oblique"].location!=NSNotFound;

   return [NSString stringWithUTF8String:bold?(italic?faces->boldItalic:faces->bold):(italic?faces->italic:faces->regular)];
}

/* The metrics and character map of the face just opened. */
static void readFace(O2Font_FT *self,NSString *name) {
   FT_Face face=self->_face;

   FT_Select_Charmap(face, FT_ENCODING_UNICODE);

   if(!(face->face_flags&FT_FACE_FLAG_SCALABLE))
    NSLog(@"FreeType font face is not scalable: %@", name);

   self->_unitsPerEm=(float)face->units_per_EM;
   self->_ascent=face->ascender;
   self->_descent=face->descender;
   self->_leading=0;
   self->_capHeight=face->height;
   self->_xHeight=face->height;
   self->_italicAngle=0;
   self->_stemV=0;
   self->_bbox.origin.x=face->bbox.xMin;
   self->_bbox.origin.y=face->bbox.yMin;
   self->_bbox.size.width=face->bbox.xMax-face->bbox.xMin;
   self->_bbox.size.height=face->bbox.yMax-face->bbox.yMin;
   self->_numberOfGlyphs=face->num_glyphs;
   self->_advances=NULL;
}

/* Minkar: a font read from memory (a web font, a font in a document);
 * its name is the face's PostScript name. */
-initWithDataProvider:(O2DataProviderRef)provider {
   CFDataRef   data;
   const char *psName;

   self=[super initWithDataProvider:provider];
   data=O2DataProviderCopyData(provider);
   if(data==NULL || CFDataGetLength(data)==0){
    if(data!=NULL)
     CFRelease(data);
    [self release];
    return nil;
   }
   _data=(NSData *)data;
   if(FT_New_Memory_Face(O2FontSharedFreeTypeLibrary(),[_data bytes],[_data length],0,&_face)!=0){
    _face=NULL;
    [self release];
    return nil;
   }
   psName=FT_Get_Postscript_Name(_face);
   _name=[[NSString alloc] initWithUTF8String:(psName!=NULL)?psName:""];
   readFace(self,_name);
   return self;
}

-initWithFontName:(NSString *)name {
   self = [super initWithFontName:name];

   FT_Long   index=0;
   NSString *filename=filenameAndIndexForPattern([self class],name,&index);
   /* Minkar: a font that is not installed is drawn in the free face that
    * stands for it (substituteFontName), else in the system font, not in
    * whichever font fontconfig lists first. */
   if(filename==nil && [name length]>0)
    filename=filenameAndIndexForPostScriptName(substituteFontName(name),&index);
   if(filename==nil)
    filename=filenameAndIndexForPostScriptName(@"Inter-Regular",&index);
   if(filename==nil) {
    filename=filenameAndIndexForPattern([self class],@"",&index);

    if(filename==nil) {
      filename=@"/System/Library/Fonts/TTF/NimbusSans-Regular.ttf";
      index=0;
    }
   }

   FT_Error ret=FT_New_Face(O2FontSharedFreeTypeLibrary(),[filename fileSystemRepresentation],index,&_face);

   if(ret!=0){
    NSLog(@"FT_New_Face returned %d",ret);
    [self release];
    return nil;
   }
   _path=[filename copy];
   _faceIndex=index;
   readFace(self,name);
   return self;
}

-(void)dealloc {
   if(_face!=NULL)
    FT_Done_Face(_face);
   [_path release];
   [_data release];
   [_variations release];
   [super dealloc];
}

/* Minkar: each glyph's box in font units, from its unscaled outline. */
-(BOOL)getGlyphBBoxes:(const O2Glyph *)glyphs count:(size_t)count rects:(O2Rect *)rects {
   if(_face==NULL)
    return NO;
   for(size_t i=0;i<count;i++){
    if(FT_Load_Glyph(_face,glyphs[i],FT_LOAD_NO_SCALE|FT_LOAD_NO_HINTING|FT_LOAD_NO_BITMAP)!=0){
     rects[i]=O2RectZero;
     continue;
    }
    FT_Glyph_Metrics *m=&_face->glyph->metrics;
    rects[i]=O2RectMake(m->horiBearingX,m->horiBearingY-m->height,m->width,m->height);
   }
   /* The face's size is set again before the next scaled load. */
   _pixelSize=0;
   return YES;
}

/* Minkar: an instance of a variable font. The keys are the axes' names
 * (CoreGraphics) or their tags as numbers (CoreText); axes not given
 * keep their default. */
-(O2FontRef)copyWithVariations:(NSDictionary *)variations {
   FT_MM_Var *mm=NULL;
   O2Font_FT *copy;
   FT_Face    face=NULL;
   FT_Error   error;

   if(_face==NULL || !FT_HAS_MULTIPLE_MASTERS(_face) || [variations count]==0)
    return [self retain];
   if(_path!=nil)
    error=FT_New_Face(O2FontSharedFreeTypeLibrary(),[_path fileSystemRepresentation],_faceIndex,&face);
   else
    error=FT_New_Memory_Face(O2FontSharedFreeTypeLibrary(),[_data bytes],[_data length],0,&face);
   if(error!=0)
    return [self retain];
   if(FT_Get_MM_Var(face,&mm)!=0){
    FT_Done_Face(face);
    return [self retain];
   }

   FT_Fixed            coords[mm->num_axis>0?mm->num_axis:1];
   NSMutableDictionary *applied=[NSMutableDictionary dictionary];

   for(FT_UInt i=0;i<mm->num_axis;i++){
    FT_Var_Axis *axis=&mm->axis[i];
    NSString    *axisName=[NSString stringWithUTF8String:(axis->name!=NULL)?axis->name:""];
    id           value=[variations objectForKey:axisName];

    if(value==nil)
     value=[variations objectForKey:[NSNumber numberWithUnsignedInt:(unsigned)axis->tag]];
    coords[i]=axis->def;
    if([value respondsToSelector:@selector(doubleValue)]){
     double v=[value doubleValue];
     double lo=axis->minimum/65536.0,hi=axis->maximum/65536.0;

     v=(v<lo)?lo:(v>hi)?hi:v;
     coords[i]=(FT_Fixed)(v*65536.0);
    }
    [applied setObject:[NSNumber numberWithDouble:coords[i]/65536.0] forKey:axisName];
   }
   FT_Set_Var_Design_Coordinates(face,mm->num_axis,coords);
   FT_Done_MM_Var(O2FontSharedFreeTypeLibrary(),mm);

   copy=NSAllocateObject([O2Font_FT class],0,NULL);
   copy->_name=[_name copy];
   copy->_provider=(_provider!=NULL)?O2DataProviderRetain(_provider):NULL;
   copy->_face=face;
   copy->_path=[_path copy];
   copy->_faceIndex=_faceIndex;
   copy->_data=[_data retain];
   copy->_variations=[applied copy];
   readFace(copy,_name);
   return copy;
}

-(NSDictionary *)variations {
   return _variations;
}

-(FT_Face)face {
   return _face;
}

-(void)setPixelSize:(CGFloat)size {
   if(size!=_pixelSize){
    FT_Set_Char_Size(_face,0,(FT_F26Dot6)(size*64),72,72);
    _pixelSize=size;
   }
}

/* An sfnt table's bytes ('post', 'OS/2', ...); nil when the font has none. */
/* A table's bytes: read into memory not cleared first (the table fills
   it all; a CJK font's tables are megabytes). */
-(NSData *)copyTableForTag:(uint32_t)tag {
   FT_ULong length=0;

   if(_face==NULL || FT_Load_Sfnt_Table(_face,tag,0,NULL,&length)!=0 || length==0)
    return nil;
   void *bytes=malloc(length);
   if(bytes==NULL)
    return nil;
   if(FT_Load_Sfnt_Table(_face,tag,0,bytes,&length)!=0){
    free(bytes);
    return nil;
   }
   return [[NSData alloc] initWithBytesNoCopy:bytes length:length freeWhenDone:YES];
}

/* The tables' tags from the face's table directory, without reading the
   file (CGFontCopyTableTags read the whole of it to find them). */
-(NSArray *)copyTableTags {
   FT_ULong tag,length;
   NSMutableArray *tags;

   if(_face==NULL || !FT_IS_SFNT(_face))
    return nil;
   tags=[[NSMutableArray alloc] init];
   for(FT_UInt i=0;FT_Sfnt_Table_Info(_face,i,&tag,&length)==0;i++)
    [tags addObject:[NSNumber numberWithUnsignedInt:(uint32_t)tag]];
   return tags;
}

-(O2Glyph)glyphWithGlyphName:(NSString *)name {
// FIXME: implement
   return 0;
}

/* Minkar: every glyph's advance in font units, read from the font's
 * metrics (FT_Get_Advances) without loading the glyphs: loading each one
 * took seconds for a font of tens of thousands of glyphs (CJK), on every
 * font made. */
-(void)fetchAdvances {
   FT_Fixed *units=NSZoneMalloc(NULL,sizeof(FT_Fixed)*(_numberOfGlyphs>0?_numberOfGlyphs:1));
   O2Glyph   glyph;

   _advances=NSZoneMalloc(NULL,sizeof(int)*(_numberOfGlyphs>0?_numberOfGlyphs:1));
   if(_numberOfGlyphs>0 && FT_Get_Advances(_face,0,(FT_UInt)_numberOfGlyphs,FT_LOAD_NO_SCALE,units)==0){
    for(glyph=0;glyph<_numberOfGlyphs;glyph++)
     _advances[glyph]=(int)units[glyph];
   }
   else {
    [self setPixelSize:_unitsPerEm];
    for(glyph=0;glyph<_numberOfGlyphs;glyph++){
     FT_Load_Glyph(_face, glyph, FT_LOAD_DEFAULT);

     _advances[glyph]=_face->glyph->advance.x/(float)(2<<5);
    }
   }
   NSZoneFree(NULL,units);
}

-(NSCharacterSet *)coveredCharacterSet {
    if(_coveredCharSet == nil) {
        NSMutableCharacterSet *set = [[NSMutableCharacterSet alloc] init];
        uint32_t code, first, last, index;

        /* Minkar: the ranges are first..last inclusive. They were one
         * short, so the last character of each run (U+21E7, Shift's
         * arrow, in Inter) seemed missing and was drawn from another
         * font. */
        code = first = last = FT_Get_First_Char(_face, &index);
        while(index != 0) {
            last = code;
            code = FT_Get_Next_Char(_face, code, &index);
            if(index == 0 || code > (last+1)) {
                [set addCharactersInRange:NSMakeRange(first, last - first + 1)];
                first = code;
            }
        }
        _coveredCharSet = set;
    }
    return _coveredCharSet;
}

@end

@implementation O2Font(FT)

+ (NSString *)nativeFontNameForPostscriptName:(NSString *)name
{
    if([name rangeOfString:@"-"].length == NSNotFound)
        return [NSString stringWithFormat:@"%@:style=Regular", name];
    return [name stringByReplacingOccurrencesOfString:@"-" withString:@":style="];
}

+ (NSString *)postscriptNameForNativeName:(NSString *)name
{
   return [[name stringByReplacingOccurrencesOfString:@":style="
        withString:@"-"] stringByReplacingOccurrencesOfString:@"-Regular"
        withString:@""];
}

+ (NSString *)postscriptNameForDisplayName:(NSString *)name
{
   return name;
}

+ (NSString *)displayNameForPostscriptName:(NSString *)name
{
   return name;
}

+ (NSString *)postscriptNameForFontName:(NSString *)name
{
   // The name of the face drawn: the font's own when it is installed, else
   // that of the face that stands for it (NSFont's fontName is that face's).
   static NSMutableDictionary *names;
   NSString *result;

   if([name length]==0)
    return name;
   @synchronized(self){
    if(names==nil)
     names=[[NSMutableDictionary alloc] init];
    result=[names objectForKey:name];
   }
   if(result!=nil)
    return result;
   if([O2Font_FT filenameForPattern:name]!=nil)
    result=name;
   else {
    NSString *substitute=substituteFontName(name);

    result=(filenameForPostScriptName(substitute)!=nil)?substitute:name;
   }
   @synchronized(self){
    [names setObject:result forKey:name];
   }
   return result;
}

@end
