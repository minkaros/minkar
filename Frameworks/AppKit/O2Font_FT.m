/* Copyright (c) 2008 Johannes Fortmann
   Copyright (c) 2009 Christopher J. W. Lloyd - <cjwl@objc.net>

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import "O2Font_FT.h"
#import <Onyx2D/O2Font_freetype.h>
#include FT_TRUETYPE_TABLES_H

O2FontRef O2FontCreateWithFontName_platform(NSString *name) {
    return [[O2Font_FT alloc] initWithFontName:name];
}

O2FontRef O2FontCreateWithDataProvider_platform(O2DataProviderRef provider) {
#ifdef FREETYPE_PRESENT
    return [[O2Font_freetype alloc] initWithDataProvider:provider];
#else
    return nil;
#endif

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

FcConfig *O2FontSharedFontConfig() {
   static FcConfig *fontConfig=NULL;

   if(fontConfig==NULL){
    fontConfig=FcInitLoadConfigAndFonts();
   }

   return fontConfig;
}

/* Minkar: a PostScript name is looked up as one first: a family whose
 * name has spaces ("Monaspace Neon", MonaspaceNeon-Regular) is not the
 * part before the hyphen, which matched no font and gave the first one. */
static NSString *filenameForPostScriptName(NSString *name) {
   FcPattern *pat=FcPatternCreate();
   FcObjectSet *props=FcObjectSetBuild(FC_FILE, NULL);
   NSString *ret=nil;

   FcPatternAddString(pat, FC_POSTSCRIPT_NAME, (const FcChar8 *)[name UTF8String]);
   FcFontSet *set=FcFontList(O2FontSharedFontConfig(), pat, props);
   if(set!=NULL) {
      FcChar8 *filename;

      if(set->nfont>0 && FcPatternGetString(set->fonts[0], FC_FILE, 0, &filename)==FcResultMatch)
         ret=[NSString stringWithUTF8String:(char *)filename];
      FcFontSetDestroy(set);
   }
   FcPatternDestroy(pat);
   FcObjectSetDestroy(props);
   return ret;
}

+(NSString*)filenameForPattern:(NSString *)pattern {
   int i;
   if([pattern length]>0 && [pattern rangeOfString:@":"].location==NSNotFound) {
      NSString *file=filenameForPostScriptName(pattern);

      if(file!=nil)
         return file;
   }
   pattern = [self nativeFontNameForPostscriptName:pattern];
   FcPattern *pat=FcNameParse((unsigned char*)[pattern UTF8String]);

   FcObjectSet *props=FcObjectSetBuild(FC_FILE, NULL);

   FcFontSet *set = FcFontList (O2FontSharedFontConfig(), pat, props);
   NSString* ret=NULL;
   for(i = 0; i < set->nfont && !ret; i++) {
      FcChar8 *filename;

      if (FcPatternGetString (set->fonts[i], FC_FILE, 0, &filename) == FcResultMatch) {
         ret=[NSString stringWithUTF8String:(char*)filename];
      }
   }

   FcPatternDestroy(pat);
   FcObjectSetDestroy(props);
   FcFontSetDestroy(set);
   return ret;
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

-initWithFontName:(NSString *)name {
   self = [super initWithFontName:name];

   NSString *filename=[[self class] filenameForPattern:name];
   /* Minkar: a font that is not installed is drawn in the free face that
    * stands for it (substituteFontName), else in the system font, not in
    * whichever font fontconfig lists first. */
   if(filename==nil && [name length]>0)
    filename=filenameForPostScriptName(substituteFontName(name));
   if(filename==nil)
    filename=filenameForPostScriptName(@"Inter-Regular");
   if(filename==nil) {
    filename=[[self class] filenameForPattern:@""];

    if(filename==nil) {
      filename=@"/System/Library/Fonts/TTF/NimbusSans-Regular.ttf";
    }
   }

   FT_Error ret=FT_New_Face(O2FontSharedFreeTypeLibrary(),[filename fileSystemRepresentation],0,&_face);

   if(ret!=0)
    NSLog(@"FT_New_Face returned %d",ret);

   FT_Select_Charmap(_face, FT_ENCODING_UNICODE);
 //  FT_Set_Char_Size(_face,0,2048*64,72,72);

   if(!(_face->face_flags&FT_FACE_FLAG_SCALABLE))
    NSLog(@"FreeType font face is not scalable: %@", name);

   _unitsPerEm=(float)_face->units_per_EM;
   _ascent=_face->ascender;
   _descent=_face->descender;
   _leading=0;
   _capHeight=_face->height;
   _xHeight=_face->height;
   _italicAngle=0;
   _stemV=0;
   _bbox.origin.x=_face->bbox.xMin;
   _bbox.origin.y=_face->bbox.yMin;
   _bbox.size.width=_face->bbox.xMax-_face->bbox.xMin;
   _bbox.size.height=_face->bbox.yMax-_face->bbox.yMin;
   _numberOfGlyphs=_face->num_glyphs;
   _advances=NULL;

   return self;
}

-(void)dealloc {
   FT_Done_Face(_face);
   [super dealloc];
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
-(NSData *)copyTableForTag:(uint32_t)tag {
   FT_ULong length=0;

   if(_face==NULL || FT_Load_Sfnt_Table(_face,tag,0,NULL,&length)!=0 || length==0)
    return nil;
   NSMutableData *data=[[NSMutableData alloc] initWithLength:length];
   if(FT_Load_Sfnt_Table(_face,tag,0,[data mutableBytes],&length)!=0){
    [data release];
    return nil;
   }
   return data;
}

-(O2Glyph)glyphWithGlyphName:(NSString *)name {
// FIXME: implement
   return 0;
}

-(void)fetchAdvances {
   O2Glyph glyph;

   [self setPixelSize:_unitsPerEm];

   _advances=NSZoneMalloc(NULL,sizeof(int)*_numberOfGlyphs);

   for(glyph=0;glyph<_numberOfGlyphs;glyph++){
    FT_Load_Glyph(_face, glyph, FT_LOAD_DEFAULT);

    _advances[glyph]=_face->glyph->advance.x/(float)(2<<5);
   }
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
