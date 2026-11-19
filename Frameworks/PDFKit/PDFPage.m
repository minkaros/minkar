/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "PDFKitPrivate.h"

@interface PDFDocument (PDFPageLabels)
- (nullable NSString *)_labelForPage:(PDFPage *)page;
@end

@implementation PDFPage {
    CGPDFPageRef _pageRef;
    NSImage *_image;
    NSMutableDictionary<NSNumber *, NSValue *> *_bounds;  // boxes set in code
    NSNumber *_rotation;                                   // set in code
}

/* A blank page, US Letter. */
- (instancetype)init {
    if ((self = [super init])) {
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
    if (_pageRef != NULL)
        CGContextDrawPDFPage(context, _pageRef);
    else if (_image != nil) {
        NSGraphicsContext *saved = [NSGraphicsContext currentContext];

        [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithCGContext:context flipped:NO]];
        [_image drawInRect:[_bounds[@(kPDFDisplayBoxMediaBox)] rectValue] fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1];
        [NSGraphicsContext setCurrentContext:saved];
    }
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
