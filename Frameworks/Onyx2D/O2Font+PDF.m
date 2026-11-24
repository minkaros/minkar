#import <Onyx2D/O2Font+PDF.h>
#import <Onyx2D/O2PDFArray.h>
#import <Onyx2D/O2PDFDictionary.h>
#import <Onyx2D/O2PDFContext.h>
#import <Foundation/NSArray.h>
#import "O2Encoding.h"
#import <Onyx2D/O2PDFStream.h>
#import <Onyx2D/O2DataProvider.h>
#include <zlib.h>

@implementation O2Font(PDF)

extern NSString *O2MacRomanGlyphNames[256];

uint8_t O2PDFMacRomanPositionOfGlyphName(NSString *name){
   static NSDictionary *map=nil;
   
   if(map==nil){
    NSMutableDictionary *mutmap=[NSMutableDictionary new];
    int i;
    for(i=0;i<256;i++)
     [mutmap setObject:[NSNumber numberWithInt:i] forKey:O2MacRomanGlyphNames[i]];
     map=mutmap;
   }
   
   return [[map objectForKey:name] intValue];
}

O2PDFArray *O2FontCreatePDFWidthsWithEncoding(O2FontRef self,O2Encoding *encoding){
   CGFloat       unitsPerEm=O2FontGetUnitsPerEm(self);
   O2PDFArray   *result=[[O2PDFArray alloc] init];
   O2Glyph       glyphs[256];
   int           widths[256];
   int           i;
   
   [encoding getGlyphs:glyphs];

   O2FontGetGlyphAdvances(self,glyphs,256,widths);

   for(i=32;i<256;i++){
    O2PDFReal width=(widths[i]/unitsPerEm)*1000; // normalized to 1000
    
    [result addNumber:width];
   }
   
    return result;
}

// this is overriden for GDI
-(void)getMacRomanBytes:(unsigned char *)bytes forGlyphs:(const O2Glyph *)glyphs length:(unsigned)length {
   int i;
   
   for(i=0;i<length;i++){
    NSString *name=O2FontCopyGlyphNameForGlyph(self,glyphs[i]);

    bytes[i]=O2PDFMacRomanPositionOfGlyphName(name);

    [name release];
   }
}

-(const char *)pdfFontName {
// possibly wrong encoding
   return [[[self->_name componentsSeparatedByString:@" "] componentsJoinedByString:@","] cStringUsingEncoding:NSISOLatin1StringEncoding];
}

/* The font's program, for a printer or viewer that does not have the font
   (Minkar, 2026-11-23: a printer drew nothing of a font it lacked): a
   TrueType font as FontFile2, an OpenType (CFF) one as FontFile3, flate
   compressed; nil for a collection or a font without its file. */
/* The font file's bytes; a subclass reading its face from a file says. */
-(NSData *)pdfFontFileData {
   if(_provider==NULL || [_provider bytes]==NULL)
    return nil;
   return [NSData dataWithBytes:[_provider bytes] length:[_provider length]];
}

-(O2PDFStream *)_pdfFontFileWithKey:(const char **)key {
   NSData        *data=[self pdfFontFileData];
   const uint8_t *bytes=[data bytes];
   size_t         length=[data length];

   if(bytes==NULL || length<12)
    return nil;
   BOOL trueType=(memcmp(bytes,"\x00\x01\x00\x00",4)==0 || memcmp(bytes,"true",4)==0);
   BOOL cff=(memcmp(bytes,"OTTO",4)==0);
   if(!trueType && !cff)
    return nil;

   uLongf packedLength=compressBound(length);
   NSMutableData *packed=[NSMutableData dataWithLength:packedLength];
   if(compress2([packed mutableBytes],&packedLength,bytes,length,9)!=Z_OK)
    return nil;
   [packed setLength:packedLength];

   O2PDFStream     *stream=[O2PDFStream pdfStreamWithData:packed];
   O2PDFDictionary *dictionary=[stream dictionary];
   [dictionary setNameForKey:"Filter" value:"FlateDecode"];
   if(trueType){
    [dictionary setIntegerForKey:"Length1" value:length];
    *key="FontFile2";
   }
   else {
    [dictionary setNameForKey:"Subtype" value:"OpenType"];
    *key="FontFile3";
   }
   return stream;
}

/* The descriptor, in the glyph space's thousandths as PDF wants it, with
   the font's program when there is one. Not symbolic: its characters are
   the standard Latin set, encoded as MacRoman. */
-(O2PDFDictionary *)_pdfFontDescriptorWithContext:(O2PDFContext *)context {
   O2PDFDictionary *result=[O2PDFDictionary pdfDictionary];
   CGFloat          unitsPerEm=O2FontGetUnitsPerEm(self);
   CGFloat          scale=(unitsPerEm>0)?1000.0/unitsPerEm:1;
   O2Rect           bRect=O2FontGetFontBBox(self);
   O2PDFReal        bbox[4];

   [result setNameForKey:"Type" value:"FontDescriptor"];
   [result setNameForKey:"FontName" value:[self pdfFontName]];
   [result setIntegerForKey:"Flags" value:32];
   bbox[0]=bRect.origin.x*scale;
   bbox[1]=bRect.origin.y*scale;
   bbox[2]=(bRect.origin.x+bRect.size.width)*scale;
   bbox[3]=(bRect.origin.y+bRect.size.height)*scale;
   [result setObjectForKey:"FontBBox" value:[O2PDFArray pdfArrayWithNumbers:bbox count:4]];
   [result setIntegerForKey:"ItalicAngle" value:O2FontGetItalicAngle(self)];
   [result setIntegerForKey:"Ascent" value:O2FontGetAscent(self)*scale];
   [result setIntegerForKey:"Descent" value:O2FontGetDescent(self)*scale];
   [result setIntegerForKey:"CapHeight" value:O2FontGetCapHeight(self)*scale];
   [result setIntegerForKey:"StemV" value:80];

   const char  *key=NULL;
   O2PDFStream *file=[self _pdfFontFileWithKey:&key];
   if(file!=nil)
    [result setObjectForKey:key value:[context encodeIndirectPDFObject:file]];

   return result;
}

-(O2PDFObject *)encodeReferenceWithContext:(O2PDFContext *)context size:(CGFloat)size {
   O2PDFObject *reference=[context referenceForFontWithName:self->_name size:size];
   
   if(reference==nil){
    O2PDFDictionary *result=[O2PDFDictionary pdfDictionary];
    O2Encoding      *encoding=[self createEncodingForTextEncoding:kO2EncodingMacRoman];
     
    [result setNameForKey:"Type" value:"Font"];
    [result setNameForKey:"Subtype" value:"TrueType"];
    [result setNameForKey:"BaseFont" value:[self pdfFontName]];
    [result setIntegerForKey:"FirstChar" value:32];
    [result setIntegerForKey:"LastChar" value:255];
    O2PDFArray *widths=O2FontCreatePDFWidthsWithEncoding(self,encoding);
    [encoding release];
    [result setObjectForKey:"Widths" value:[context encodeIndirectPDFObject:widths]];
    [widths release];
    
    /* One descriptor (and font program) per font, whatever its sizes. */
    O2PDFObject *descriptor=[context referenceForFontWithName:self->_name size:-1];
    if(descriptor==nil){
     descriptor=[context encodeIndirectPDFObject:[self _pdfFontDescriptorWithContext:context]];
     [context setReference:descriptor forFontWithName:self->_name size:-1];
    }
    [result setObjectForKey:"FontDescriptor" value:descriptor];

    [result setNameForKey:"Encoding" value:"MacRomanEncoding"];

    reference=[context encodeIndirectPDFObject:result];
    [context setReference:reference forFontWithName:self->_name size:size];
   }
   
   return reference;
}

@end
