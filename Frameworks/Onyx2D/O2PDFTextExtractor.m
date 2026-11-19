/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A context that draws nothing: it is given the page's content as any
 * context is, and keeps the text it is shown.
 */

#import <Onyx2D/O2PDFTextExtractor.h>
#import <Onyx2D/O2Context.h>
#import <Onyx2D/O2GraphicsState.h>
#import <Onyx2D/O2Encoding.h>
#import <Onyx2D/O2PDFCharWidths.h>
#import <Onyx2D/O2Font.h>
#import <Onyx2D/O2PDFPage.h>
#import <math.h>

@interface O2Context_textExtractor : O2Context {
@public
    NSMutableData *_characters;
}
@end

/* One string shown: each byte's character, placed by the text rendering
   matrix (PDF 1.7, 9.4.4), and the text matrix moved past it. */
static void extractShowText(O2ContextRef self, const char *text, unsigned length) {
    O2Context_textExtractor *extractor = (O2Context_textExtractor *)self;
    O2GState *gState = O2ContextCurrentGState(self);
    O2Encoding *encoding = O2GStateEncoding(gState);
    O2PDFCharWidths *widths = O2GStateCharWidths(gState);
    O2FontRef font = O2GStateFont(gState);
    O2Float size = O2GStatePointSize(gState);
    O2Float spacing = O2GStateCharacterSpacing(gState), wordSpacing = O2GStateWordSpacing(gState);
    O2Float scaling = O2GStateTextHorizontalScaling(gState), rise = O2GStateTextRise(gState);
    uint16_t unicode[length];
    O2Glyph glyphs[length];
    int advances[length];
    O2Size pdfAdvances[length];
    int unitsPerEm = (font != NULL) ? O2FontGetUnitsPerEm(font) : 1000;
    O2Float ascent = (font != NULL && unitsPerEm > 0) ? (O2Float)O2FontGetCapHeight(font) / unitsPerEm : 0.7;
    O2Float descent = (font != NULL && unitsPerEm > 0) ? (O2Float)O2FontGetDescent(font) / unitsPerEm : -0.2;

    if (scaling == 0)
        scaling = 1;
    else if (scaling > 10)        // given as a percentage
        scaling /= 100;
    if (ascent <= 0)
        ascent = 0.7;
    if (descent > 0)
        descent = -descent;
    if (unitsPerEm <= 0)
        unitsPerEm = 1000;

    /* No font set (its Tf failed): the bytes as Latin-1, no glyphs. */
    if (encoding != nil) {
        O2EncodingGetUnicodeForBytes(encoding, unicode, (const uint8_t *)text, length);
        O2EncodingGetGlyphsForBytes(encoding, glyphs, (const uint8_t *)text, length);
    } else {
        for (unsigned i = 0; i < length; i++) {
            unicode[i] = (uint8_t)text[i];
            glyphs[i] = 0;
        }
    }
    if (widths != nil)
        O2PDFCharWidthsGetAdvances(widths, pdfAdvances, (const uint8_t *)text, length);
    else if (font == NULL || !O2FontGetGlyphAdvances(font, glyphs, length, advances))
        memset(advances, 0, sizeof(advances));

    for (unsigned i = 0; i < length; i++) {
        O2AffineTransform tm = O2ContextGetTextMatrix(self);
        O2AffineTransform ctm = O2ContextGetCTM(self);
        O2AffineTransform trm = O2AffineTransformConcat(O2AffineTransformMake(size * scaling, 0, 0, size, 0, rise), O2AffineTransformConcat(tm, ctm));
        O2Float w0 = (widths != nil) ? pdfAdvances[i].width : (O2Float)advances[i] / unitsPerEm;
        O2Float tx = (w0 * size + spacing + (((uint8_t)text[i] == 32) ? wordSpacing : 0)) * scaling;
        O2Point origin = O2PointApplyAffineTransform(O2PointMake(0, 0), trm);
        O2Point end = O2PointApplyAffineTransform(O2PointMake(w0, 0), trm);
        O2Point top = O2PointApplyAffineTransform(O2PointMake(0, 1), trm);
        O2Float pageSize = hypot(top.x - origin.x, top.y - origin.y);
        O2PDFTextCharacter character;

        character.unicode = unicode[i] ? unicode[i] : (unichar)(uint8_t)text[i];
        character.origin = origin;
        character.advance = hypot(end.x - origin.x, end.y - origin.y);
        character.fontSize = pageSize;
        character.ascent = ascent * pageSize;
        character.descent = descent * pageSize;
        [extractor->_characters appendBytes:&character length:sizeof(character)];
        O2ContextSetTextMatrix(self, O2AffineTransformConcat(O2AffineTransformMakeTranslation(tx, 0), tm));
    }
}

@implementation O2Context_textExtractor

- (instancetype)init {
    if ((self = [super init])) {
        _characters = [[NSMutableData alloc] init];
        _showTextFunction = extractShowText;
    }
    return self;
}

- (void)dealloc {
    [_characters release];
    [super dealloc];
}

/* Nothing is drawn. */
- (void)drawPath:(O2PathDrawingMode)pathMode { O2ContextBeginPath(self); }
- (void)drawShading:(O2Shading *)shading {}
- (void)drawImage:(O2Image *)image inRect:(O2Rect)rect {}
- (void)drawLayer:(O2LayerRef)layer inRect:(O2Rect)rect {}
- (void)clipToState:(O2ClipState *)clipState {}
- (void)showGlyphs:(const O2Glyph *)glyphs advances:(const O2Size *)advances count:(unsigned)count {}

@end

NSData *O2PDFPageCopyTextCharacters(O2PDFPage *page) {
    O2Context_textExtractor *extractor = [[O2Context_textExtractor alloc] init];
    NSData *result;

    if (page != nil)
        [page drawInContext:extractor];
    result = [extractor->_characters copy];
    [extractor release];
    return result;
}
