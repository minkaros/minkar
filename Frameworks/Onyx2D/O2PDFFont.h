#import <Onyx2D/O2PDFObject.h>
#import <Onyx2D/O2Font.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSData.h>

@class O2PDFDictionary, O2Encoding, O2PDFCharWidths, O2Font;

@interface O2PDFFont : O2PDFObject {
    O2PDFDictionary *_info; // If we get rid of this, make sure to copy/retain any values in it
    const char *_baseFont;
    O2Font *_resourceFont;
    O2Font *_graphicsFont;
    O2Encoding *_encoding;
    O2PDFCharWidths *_pdfCharWidths;
    /* Minkar: a composite font (Type0): two-byte codes (Identity-H or V)
       that are CIDs, its descendant's widths and CID-to-glyph map, and
       its ToUnicode's text per code. */
    BOOL _composite;
    BOOL _vertical;
    O2Float _defaultWidth;
    NSMutableDictionary *_cidWidths;
    NSData *_cidToGID;
    BOOL _embedded;
    NSMutableDictionary *_toUnicode;
}

+ (O2PDFFont *)createWithPDFDictionary:(O2PDFDictionary *)info;

- (O2Encoding *)encoding;
- (O2PDFCharWidths *)pdfCharWidths;

- (O2Font *)graphicsFont;

/* Minkar: whether its codes are two bytes (a composite font). */
- (BOOL)isComposite;
/* A composite font's codes in bytes: each code, its glyph in the
   graphics font and its width (thousandths of the size); the count. */
- (unsigned)decodeBytes:(const uint8_t *)bytes length:(unsigned)length codes:(unsigned *)codes glyphs:(O2Glyph *)glyphs widths:(O2Float *)widths;
/* A code's text (its ToUnicode's, else the encoding's); nil if none. */
- (NSString *)unicodeForCode:(unsigned)code;

@end
