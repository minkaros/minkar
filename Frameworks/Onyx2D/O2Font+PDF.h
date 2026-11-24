#import <Onyx2D/O2Font.h>

@class O2PDFObject, O2PDFContext, NSData;

@interface O2Font (PDF)
- (void)getMacRomanBytes:(unsigned char *)bytes forGlyphs:(const O2Glyph *)glyphs length:(unsigned)length;
- (O2PDFObject *)encodeReferenceWithContext:(O2PDFContext *)context size:(O2Float)size;
/* The font file's bytes, embedded in PDF; nil when there is none. */
- (NSData *)pdfFontFileData;
@end
