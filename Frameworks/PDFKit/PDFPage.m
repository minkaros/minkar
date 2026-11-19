/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "PDFKitPrivate.h"
#import <Onyx2D/O2PDFTextExtractor.h>

@interface PDFDocument (PDFPageLabels)
- (nullable NSString *)_labelForPage:(PDFPage *)page;
@end

@implementation PDFPage {
    CGPDFPageRef _pageRef;
    NSImage *_image;
    NSMutableDictionary<NSNumber *, NSValue *> *_bounds;  // boxes set in code
    NSNumber *_rotation;                                   // set in code
    NSString *_string;                                     // its text, once read
    NSData *_characterBounds;                              // a PDFRect per character
    BOOL _textRead;
    NSMutableArray<PDFAnnotation *> *_annotations;         // once read
}

/* A blank page, US Letter. */
- (instancetype)init {
    if ((self = [super init])) {
        _displaysAnnotations = YES;
        _bounds = [NSMutableDictionary dictionary];
        _bounds[@(kPDFDisplayBoxMediaBox)] = [NSValue valueWithRect:NSMakeRect(0, 0, 612, 792)];
    }
    return self;
}

- (instancetype)_initWithDocument:(PDFDocument *)document pageRef:(CGPDFPageRef)pageRef {
    if ((self = [self init])) {
        [_bounds removeAllObjects];
        _document = document;
        _pageRef = CGPDFPageRetain(pageRef);
    }
    return self;
}

/* A page the image fills, a point a pixel at 72 dpi (the image's size). */
- (instancetype)initWithImage:(NSImage *)image {
    if (image == nil || image.size.width <= 0 || image.size.height <= 0)
        return nil;
    if ((self = [self init])) {
        _image = image;
        _bounds[@(kPDFDisplayBoxMediaBox)] = [NSValue valueWithRect:NSMakeRect(0, 0, image.size.width, image.size.height)];
    }
    return self;
}

- (void)dealloc {
    if (_pageRef != NULL)
        CGPDFPageRelease(_pageRef);
}

- (id)copyWithZone:(NSZone *)zone {
    PDFPage *copy = [[[self class] allocWithZone:zone] init];

    copy->_pageRef = (_pageRef != NULL) ? CGPDFPageRetain(_pageRef) : NULL;
    copy->_image = _image;
    copy->_bounds = [_bounds mutableCopy];
    copy->_rotation = _rotation;
    return copy;
}

- (void)_setDocument:(PDFDocument *)document {
    _document = document;
}

- (CGPDFPageRef)pageRef {
    return _pageRef;
}

- (NSString *)label {
    return [_document _labelForPage:self];
}

- (PDFRect)boundsForBox:(PDFDisplayBox)box {
    NSValue *set = _bounds[@(box)];

    if (set != nil)
        return set.rectValue;
    if (_pageRef != NULL)
        return CGPDFPageGetBoxRect(_pageRef, (CGPDFBox)box);
    /* A page made in code: its boxes the media box but those set. */
    set = _bounds[@(kPDFDisplayBoxCropBox)];
    if (box != kPDFDisplayBoxMediaBox && set != nil)
        return set.rectValue;
    return [_bounds[@(kPDFDisplayBoxMediaBox)] rectValue];
}

- (void)setBounds:(PDFRect)bounds forBox:(PDFDisplayBox)box {
    _bounds[@(box)] = [NSValue valueWithRect:NSRectFromCGRect(CGRectStandardize(bounds))];
}

/* Between 0 and 270 (a page's -90 is 270). */
- (NSInteger)rotation {
    NSInteger angle = (_rotation != nil) ? _rotation.integerValue : (_pageRef != NULL ? CGPDFPageGetRotationAngle(_pageRef) : 0);

    return ((angle % 360) + 360) % 360;
}

/* The nearest multiple of 90, a half up (45 is 90, -45 is 0), as on the
   reference. */
- (void)setRotation:(NSInteger)rotation {
    _rotation = @((NSInteger)floor(rotation / 90.0 + 0.5) * 90);
}

/* The page's box turned by its rotation, its lower left corner at the
   origin. */
- (CGAffineTransform)transformForBox:(PDFDisplayBox)box {
    CGRect b = [self boundsForBox:box];
    NSInteger angle = ((self.rotation % 360) + 360) % 360;

    switch (angle) {
    case 90:   // (x, y) -> (y - minY, maxX - x)
        return CGAffineTransformMake(0, -1, 1, 0, -CGRectGetMinY(b), CGRectGetMaxX(b));
    case 180:  // (x, y) -> (maxX - x, maxY - y)
        return CGAffineTransformMake(-1, 0, 0, -1, CGRectGetMaxX(b), CGRectGetMaxY(b));
    case 270:  // (x, y) -> (maxY - y, x - minX)
        return CGAffineTransformMake(0, 1, -1, 0, CGRectGetMaxY(b), -CGRectGetMinX(b));
    default:
        return CGAffineTransformMakeTranslation(-CGRectGetMinX(b), -CGRectGetMinY(b));
    }
}

- (void)transformContext:(CGContextRef)context forBox:(PDFDisplayBox)box {
    CGContextConcatCTM(context, [self transformForBox:box]);
}

/* The page's content in the box, turned by its rotation, into a context
   whose origin is the box's lower left corner. */
- (void)drawWithBox:(PDFDisplayBox)box toContext:(CGContextRef)context {
    CGRect b = [self boundsForBox:box];

    CGContextSaveGState(context);
    [self transformContext:context forBox:box];
    CGContextClipToRect(context, b);
    CGContextSetRGBFillColor(context, 1, 1, 1, 1);
    CGContextFillRect(context, b);
    /* The content starts with the PDF's initial state: black. */
    CGContextSetRGBFillColor(context, 0, 0, 0, 1);
    CGContextSetRGBStrokeColor(context, 0, 0, 0, 1);
    if (_pageRef != NULL)
        CGContextDrawPDFPage(context, _pageRef);
    else if (_image != nil) {
        NSGraphicsContext *saved = [NSGraphicsContext currentContext];

        [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithCGContext:context flipped:NO]];
        [_image drawInRect:[_bounds[@(kPDFDisplayBoxMediaBox)] rectValue] fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1];
        [NSGraphicsContext setCurrentContext:saved];
    }
    [self _drawAnnotationsWithBox:box inContext:context];
    CGContextRestoreGState(context);
}

- (void)drawWithBox:(PDFDisplayBox)box {
    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];

    if (context != NULL)
        [self drawWithBox:box toContext:context];
}

/* The page scaled to fit size, its aspect kept, in the middle. */
- (NSImage *)thumbnailOfSize:(PDFSize)size forBox:(PDFDisplayBox)box {
    CGRect b = [self boundsForBox:box];
    BOOL turned = (self.rotation % 180) != 0;
    CGFloat width = turned ? b.size.height : b.size.width, height = turned ? b.size.width : b.size.height;
    CGFloat scale = (width > 0 && height > 0) ? MIN(size.width / width, size.height / height) : 0;
    NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(size.width, size.height)];

    if (scale <= 0)
        return image;
    [image lockFocus];
    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
    CGContextTranslateCTM(context, floor((size.width - width * scale) / 2), floor((size.height - height * scale) / 2));
    CGContextScaleCTM(context, scale, scale);
    [self drawWithBox:box toContext:context];
    [image unlockFocus];
    return image;
}

/* ---- annotations ---- */

- (NSMutableArray<PDFAnnotation *> *)_annotationList {
    if (_annotations == nil) {
        CGPDFArrayRef annots;

        _annotations = [NSMutableArray array];
        if (_pageRef != NULL && CGPDFDictionaryGetArray(CGPDFPageGetDictionary(_pageRef), "Annots", &annots))
            for (size_t i = 0; i < CGPDFArrayGetCount(annots); i++) {
                CGPDFDictionaryRef dictionary;
                PDFAnnotation *annotation;

                if (CGPDFArrayGetDictionary(annots, i, &dictionary) && (annotation = [PDFAnnotation _annotationWithDictionary:dictionary page:self]) != nil)
                    [_annotations addObject:annotation];
            }
    }
    return _annotations;
}

- (NSArray<PDFAnnotation *> *)annotations {
    return [[self _annotationList] copy];
}

- (void)addAnnotation:(PDFAnnotation *)annotation {
    if (annotation == nil || [[self _annotationList] containsObject:annotation])
        return;
    [annotation.page removeAnnotation:annotation];
    [self willChangeValueForKey:@"annotations"];
    [[self _annotationList] addObject:annotation];
    [self didChangeValueForKey:@"annotations"];
    annotation.page = self;
    [_document _annotationsChanged];
}

/* The annotation keeps its page, as on the reference. */
- (void)removeAnnotation:(PDFAnnotation *)annotation {
    if (annotation == nil || ![[self _annotationList] containsObject:annotation])
        return;
    [self willChangeValueForKey:@"annotations"];
    [[self _annotationList] removeObjectIdenticalTo:annotation];
    [self didChangeValueForKey:@"annotations"];
    [_document _annotationsChanged];
}

- (PDFAnnotation *)annotationAtPoint:(PDFPoint)point {
    for (PDFAnnotation *annotation in [self _annotationList].reverseObjectEnumerator)
        if (![annotation.type isEqual:@"Popup"] && annotation.shouldDisplay && CGRectContainsPoint(annotation.bounds, point))
            return annotation;
    return nil;
}

- (void)_drawAnnotationsWithBox:(PDFDisplayBox)box inContext:(CGContextRef)context {
    if (!_displaysAnnotations)
        return;
    for (PDFAnnotation *annotation in [self _annotationList])
        if (![annotation.type isEqual:@"Popup"])
            [annotation drawWithBox:box inContext:context];
}

/* ---- text ---- */

/* The text from the content's characters (Onyx2D's text extractor). */
- (void)_readText {
    NSMutableString *string = [NSMutableString string];
    NSMutableData *bounds = [NSMutableData data];
    NSData *data;
    const O2PDFTextCharacter *characters;
    NSUInteger count;
    BOOL started = NO;
    O2PDFTextCharacter previous = {0};
    CGFloat lineSize = 0;

    _textRead = YES;
    if (_pageRef == NULL)
        return;
    data = O2PDFPageCopyTextCharacters((__bridge O2PDFPage *)(void *)_pageRef);
    characters = data.bytes;
    count = data.length / sizeof(O2PDFTextCharacter);

    void (^append)(unichar, CGRect) = ^(unichar c, CGRect r) {
        [string appendFormat:@"%C", c];
        [bounds appendBytes:&r length:sizeof(r)];
    };
    CGRect (^boxOf)(O2PDFTextCharacter, CGFloat, CGFloat) = ^CGRect(O2PDFTextCharacter c, CGFloat x, CGFloat width) {
        return CGRectMake(x, c.origin.y + c.descent, width, c.ascent - c.descent);
    };

    for (NSUInteger i = 0; i < count; i++) {
        O2PDFTextCharacter c = characters[i];
        CGFloat previousEnd = previous.origin.x + previous.advance;

        if (c.unicode < 0x20 || c.unicode == 0x7F)
            continue;
        if (started) {
            CGFloat size = MAX(c.fontSize, previous.fontSize);
            BOOL sameLine = fabs(c.origin.y - previous.origin.y) <= size * 0.5 && c.origin.x >= previousEnd - size;

            if (!sameLine) {
                /* The next line close under this one goes on with a space;
                   any other ends this one. */
                CGFloat drop = previous.origin.y - c.origin.y;
                unichar separator = (drop > 0 && drop <= 2 * lineSize) ? ' ' : '\n';

                if (!(separator == ' ' && previous.unicode == ' '))
                    append(separator, boxOf(previous, previousEnd, 0));
                lineSize = c.fontSize;
            } else if (c.unicode != ' ' && previous.unicode != ' ' && c.origin.x - previousEnd > size * 0.15) {
                /* A gap in the line is a space, as wide as the gap. */
                append(' ', boxOf(previous, previousEnd, c.origin.x - previousEnd));
            }
        } else
            lineSize = c.fontSize;
        /* Its compatibility decomposition, as the reference gives the text
           (an accented letter and its mark, a ligature's letters); each part
           a share of its width. */
        NSString *parts = [[NSString stringWithCharacters:&c.unicode length:1] decomposedStringWithCompatibilityMapping];
        NSCharacterSet *marks = [NSCharacterSet nonBaseCharacterSet];
        NSUInteger spacing = 0, k;

        if (parts.length == 0)
            parts = [NSString stringWithCharacters:&c.unicode length:1];
        for (k = 0; k < parts.length; k++)
            if (![marks characterIsMember:[parts characterAtIndex:k]])
                spacing++;
        /* Letters share the width; a combining mark is as wide as nothing,
           at the end of its letter. */
        CGFloat x = c.origin.x, share = c.advance / MAX(spacing, (NSUInteger)1);
        for (k = 0; k < parts.length; k++) {
            unichar part = [parts characterAtIndex:k];
            BOOL mark = [marks characterIsMember:part] && spacing > 0;

            append(part, boxOf(c, x, mark ? 0 : share));
            if (!mark)
                x += share;
        }
        lineSize = MAX(lineSize, c.fontSize);
        previous = c;
        started = YES;
    }
    if (started)
        append('\n', boxOf(previous, previous.origin.x + previous.advance, 0));
    _string = (string.length > 0) ? [string copy] : nil;
    _characterBounds = bounds;
}

- (NSString *)string {
    if (!_textRead)
        [self _readText];
    return _string;
}

- (NSAttributedString *)attributedString {
    NSString *string = self.string;

    return (string != nil) ? [[NSAttributedString alloc] initWithString:string] : nil;
}

- (NSUInteger)numberOfCharacters {
    return self.string.length;
}

- (PDFRect)characterBoundsAtIndex:(NSInteger)index {
    if (!_textRead)
        [self _readText];
    if (index < 0 || (NSUInteger)index >= _characterBounds.length / sizeof(CGRect))
        return CGRectZero;
    return ((const CGRect *)_characterBounds.bytes)[index];
}

- (NSInteger)characterIndexAtPoint:(PDFPoint)point {
    if (!_textRead)
        [self _readText];
    const CGRect *rects = _characterBounds.bytes;
    NSUInteger count = _characterBounds.length / sizeof(CGRect);

    for (NSUInteger i = 0; i < count; i++)
        if (rects[i].size.width > 0 && CGRectContainsPoint(rects[i], point))
            return (NSInteger)i;
    return NSNotFound;
}

/* ---- selections ---- */

- (PDFSelection *)_selectionWithRange:(NSRange)range {
    PDFSelection *selection;

    if (range.length == 0 || range.location >= self.numberOfCharacters)
        return nil;
    selection = [[PDFSelection alloc] initWithDocument:(PDFDocument *_Nonnull)_document];
    [selection _addRange:range onPage:self];
    return (selection.pages.count > 0) ? selection : nil;
}

- (PDFSelection *)selectionForRange:(NSRange)range {
    return [self _selectionWithRange:range];
}

- (PDFSelection *)selectionForRect:(PDFRect)rect {
    NSUInteger count = self.numberOfCharacters;
    PDFSelection *selection = [[PDFSelection alloc] initWithDocument:(PDFDocument *_Nonnull)_document];

    rect = CGRectStandardize(rect);
    for (NSUInteger i = 0; i < count; i++)
        if (CGRectContainsRect(rect, [self characterBoundsAtIndex:i]))
            [selection _addRange:NSMakeRange(i, 1) onPage:self];
    return (selection.pages.count > 0) ? selection : nil;
}

- (PDFSelection *)selectionForWordAtPoint:(PDFPoint)point {
    NSInteger index = [self characterIndexAtPoint:point];
    NSString *text = self.string;
    NSCharacterSet *word = [NSCharacterSet alphanumericCharacterSet];
    NSUInteger start, end;

    if (index == NSNotFound || ![word characterIsMember:[text characterAtIndex:index]])
        return nil;
    for (start = index; start > 0 && [word characterIsMember:[text characterAtIndex:start - 1]]; start--)
        ;
    for (end = index + 1; end < text.length && [word characterIsMember:[text characterAtIndex:end]]; end++)
        ;
    return [self _selectionWithRange:NSMakeRange(start, end - start)];
}

/* The characters on the line of index: those with its baseline, around it,
   without the separators. */
- (NSRange)_lineRangeAtIndex:(NSUInteger)index {
    NSString *text = self.string;
    CGRect box = [self characterBoundsAtIndex:index];
    NSUInteger start = index, end = index + 1;

    BOOL (^sameLine)(NSUInteger) = ^BOOL(NSUInteger i) {
        CGRect other = [self characterBoundsAtIndex:i];
        unichar c = [text characterAtIndex:i];

        if (c == '\n' || (c == ' ' && other.size.width == 0))
            return NO;
        return fabs(CGRectGetMinY(other) - CGRectGetMinY(box)) <= MAX(box.size.height, other.size.height) / 2;
    };
    while (start > 0 && sameLine(start - 1))
        start--;
    while (end < text.length && sameLine(end))
        end++;
    return NSMakeRange(start, end - start);
}

- (PDFSelection *)selectionForLineAtPoint:(PDFPoint)point {
    NSInteger index = [self characterIndexAtPoint:point];

    if (index == NSNotFound)
        return nil;
    return [self _selectionWithRange:[self _lineRangeAtIndex:index]];
}

/* Where a point falls between characters: before the one it is on, or
   after it past its middle; NSNotFound off the text. */
- (NSUInteger)_boundaryAtPoint:(PDFPoint)point {
    NSInteger index = [self characterIndexAtPoint:point];
    CGRect box;

    if (index == NSNotFound)
        return NSNotFound;
    box = [self characterBoundsAtIndex:index];
    return (point.x > CGRectGetMidX(box)) ? index + 1 : index;
}

- (PDFSelection *)selectionFromPoint:(PDFPoint)startPoint toPoint:(PDFPoint)endPoint {
    NSUInteger a = [self _boundaryAtPoint:startPoint], b = [self _boundaryAtPoint:endPoint];

    if (a == NSNotFound || b == NSNotFound)
        return nil;
    return [self _selectionWithRange:NSMakeRange(MIN(a, b), MAX(a, b) - MIN(a, b))];
}

/* The page alone, as a one-page PDF. */
- (NSData *)dataRepresentation {
    PDFDocument *document = [[PDFDocument alloc] init];

    [document insertPage:[self copy] atIndex:0];
    return [document dataRepresentation];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p label %@>", [self class], self, self.label];
}

@end
