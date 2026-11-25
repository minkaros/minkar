/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * HIShape on pixman's regions (whole pixels): a CF object holding one.
 */

#include <Carbon/HIShape.h>
#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CFRuntime.h>
#include <math.h>
#include <pixman.h>
#include <pthread.h>
#include <stdlib.h>

struct __HIShape {
    CFRuntimeBase base;
    pixman_region32_t region;
};

static CFTypeID shapeTypeID;
static pthread_once_t shapeOnce = PTHREAD_ONCE_INIT;

static void shapeFinalize(CFTypeRef cf) {
    pixman_region32_fini(&((struct __HIShape *)cf)->region);
}

static void shapeRegister(void) {
    static CFRuntimeClass cls;
    cls.className = "HIShape";
    cls.finalize = shapeFinalize;
    shapeTypeID = _CFRuntimeRegisterClass(&cls);
}

CFTypeID HIShapeGetTypeID(void) {
    pthread_once(&shapeOnce, shapeRegister);
    return shapeTypeID;
}

static struct __HIShape *newShape(void) {
    struct __HIShape *s = (struct __HIShape *)_CFRuntimeCreateInstance(NULL, HIShapeGetTypeID(),
        sizeof(struct __HIShape) - sizeof(CFRuntimeBase), NULL);
    if (s != NULL)
        pixman_region32_init(&s->region);
    return s;
}

/* A rectangle rounded out to the pixels it touches. */
static void rectToBox(const HIRect *r, pixman_box32_t *b) {
    CGRect s = CGRectStandardize(*r);
    b->x1 = (int32_t)floor(s.origin.x);
    b->y1 = (int32_t)floor(s.origin.y);
    b->x2 = (int32_t)ceil(s.origin.x + s.size.width);
    b->y2 = (int32_t)ceil(s.origin.y + s.size.height);
}

static void setRect(struct __HIShape *s, const HIRect *r) {
    pixman_box32_t b;
    pixman_region32_fini(&s->region);
    if (r == NULL || CGRectIsEmpty(*r) || CGRectIsNull(*r)) {
        pixman_region32_init(&s->region);
        return;
    }
    rectToBox(r, &b);
    pixman_region32_init_rect(&s->region, b.x1, b.y1, (unsigned)(b.x2 - b.x1), (unsigned)(b.y2 - b.y1));
}

static pixman_region32_t *reg(HIShapeRef s) {
    return &((struct __HIShape *)s)->region;
}

HIShapeRef HIShapeCreateEmpty(void) { return newShape(); }

HIShapeRef HIShapeCreateWithRect(const HIRect *inRect) {
    struct __HIShape *s = newShape();
    if (s != NULL)
        setRect(s, inRect);
    return s;
}

HIShapeRef HIShapeCreateCopy(HIShapeRef inShape) { return HIShapeCreateMutableCopy(inShape); }

HIMutableShapeRef HIShapeCreateMutable(void) { return newShape(); }

HIMutableShapeRef HIShapeCreateMutableCopy(HIShapeRef inOrig) {
    struct __HIShape *s = newShape();
    if (s != NULL && inOrig != NULL)
        pixman_region32_copy(&s->region, reg(inOrig));
    return s;
}

HIMutableShapeRef HIShapeCreateMutableWithRect(const HIRect *inRect) {
    struct __HIShape *s = newShape();
    if (s != NULL)
        setRect(s, inRect);
    return s;
}

typedef pixman_bool_t (*RegionOp)(pixman_region32_t *, const pixman_region32_t *, const pixman_region32_t *);

static pixman_bool_t xorOp(pixman_region32_t *out, const pixman_region32_t *a, const pixman_region32_t *b) {
    pixman_region32_t ab, ba;
    pixman_region32_init(&ab);
    pixman_region32_init(&ba);
    pixman_region32_subtract(&ab, a, b);
    pixman_region32_subtract(&ba, b, a);
    pixman_bool_t ok = pixman_region32_union(out, &ab, &ba);
    pixman_region32_fini(&ab);
    pixman_region32_fini(&ba);
    return ok;
}

static OSStatus apply(RegionOp op, HIShapeRef a, HIShapeRef b, HIMutableShapeRef out) {
    pixman_region32_t result;
    if (a == NULL || b == NULL || out == NULL)
        return paramErr;
    pixman_region32_init(&result);
    if (!op(&result, reg(a), reg(b))) {
        pixman_region32_fini(&result);
        return memFullErr;
    }
    pixman_region32_fini(reg(out));
    pixman_region32_init(reg(out));
    pixman_region32_copy(reg(out), &result);
    pixman_region32_fini(&result);
    return noErr;
}

static HIShapeRef create(RegionOp op, HIShapeRef a, HIShapeRef b) {
    struct __HIShape *s = newShape();
    if (s != NULL && apply(op, a, b, s) != noErr) {
        CFRelease(s);
        return NULL;
    }
    return s;
}

HIShapeRef HIShapeCreateIntersection(HIShapeRef a, HIShapeRef b) { return create(pixman_region32_intersect, a, b); }
HIShapeRef HIShapeCreateDifference(HIShapeRef a, HIShapeRef b) { return create(pixman_region32_subtract, a, b); }
HIShapeRef HIShapeCreateUnion(HIShapeRef a, HIShapeRef b) { return create(pixman_region32_union, a, b); }
HIShapeRef HIShapeCreateXor(HIShapeRef a, HIShapeRef b) { return create(xorOp, a, b); }

OSStatus HIShapeUnion(HIShapeRef a, HIShapeRef b, HIMutableShapeRef out) { return apply(pixman_region32_union, a, b, out); }
OSStatus HIShapeIntersect(HIShapeRef a, HIShapeRef b, HIMutableShapeRef out) { return apply(pixman_region32_intersect, a, b, out); }
OSStatus HIShapeDifference(HIShapeRef a, HIShapeRef b, HIMutableShapeRef out) { return apply(pixman_region32_subtract, a, b, out); }
OSStatus HIShapeXor(HIShapeRef a, HIShapeRef b, HIMutableShapeRef out) { return apply(xorOp, a, b, out); }

Boolean HIShapeIsEmpty(HIShapeRef inShape) {
    return inShape == NULL || !pixman_region32_not_empty(reg(inShape));
}

Boolean HIShapeIsRectangular(HIShapeRef inShape) {
    return inShape != NULL && pixman_region32_n_rects(reg(inShape)) <= 1;
}

Boolean HIShapeContainsPoint(HIShapeRef inShape, const HIPoint *inPoint) {
    if (inShape == NULL || inPoint == NULL)
        return false;
    return pixman_region32_contains_point(reg(inShape), (int)floor(inPoint->x), (int)floor(inPoint->y), NULL);
}

Boolean HIShapeIntersectsRect(HIShapeRef inShape, const HIRect *inRect) {
    pixman_box32_t b;
    if (inShape == NULL || inRect == NULL || CGRectIsEmpty(*inRect))
        return false;
    rectToBox(inRect, &b);
    return pixman_region32_contains_rectangle(reg(inShape), &b) != PIXMAN_REGION_OUT;
}

HIRect *HIShapeGetBounds(HIShapeRef inShape, HIRect *outRect) {
    if (outRect == NULL)
        return NULL;
    if (HIShapeIsEmpty(inShape)) {
        *outRect = CGRectZero;
        return outRect;
    }
    pixman_box32_t *b = pixman_region32_extents(reg(inShape));
    *outRect = CGRectMake(b->x1, b->y1, b->x2 - b->x1, b->y2 - b->y1);
    return outRect;
}

OSStatus HIShapeReplacePathInCGContext(HIShapeRef inShape, CGContextRef inContext) {
    int n;
    if (inShape == NULL || inContext == NULL)
        return paramErr;
    CGContextBeginPath(inContext);
    pixman_box32_t *boxes = pixman_region32_rectangles(reg(inShape), &n);
    for (int i = 0; i < n; i++)
        CGContextAddRect(inContext, CGRectMake(boxes[i].x1, boxes[i].y1,
            boxes[i].x2 - boxes[i].x1, boxes[i].y2 - boxes[i].y1));
    return noErr;
}

OSStatus HIShapeUnionWithRect(HIMutableShapeRef inShape, const HIRect *inRect) {
    pixman_box32_t b;
    if (inShape == NULL || inRect == NULL)
        return paramErr;
    if (CGRectIsEmpty(*inRect))
        return noErr;
    rectToBox(inRect, &b);
    return pixman_region32_union_rect(reg(inShape), reg(inShape), b.x1, b.y1,
        (unsigned)(b.x2 - b.x1), (unsigned)(b.y2 - b.y1)) ? noErr : memFullErr;
}

OSStatus HIShapeOffset(HIMutableShapeRef inShape, CGFloat inDX, CGFloat inDY) {
    if (inShape == NULL)
        return paramErr;
    pixman_region32_translate(reg(inShape), (int)lround(inDX), (int)lround(inDY));
    return noErr;
}

OSStatus HIShapeSetEmpty(HIMutableShapeRef inShape) {
    if (inShape == NULL)
        return paramErr;
    pixman_region32_clear(reg(inShape));
    return noErr;
}

OSStatus HIShapeSetWithShape(HIMutableShapeRef inDestShape, HIShapeRef inSrcShape) {
    if (inDestShape == NULL || inSrcShape == NULL)
        return paramErr;
    if (inDestShape != inSrcShape)
        pixman_region32_copy(reg(inDestShape), reg(inSrcShape));
    return noErr;
}

OSStatus HIShapeEnumerate(HIShapeRef inShape, OptionBits inOptions, HIShapeEnumerateProcPtr inProc, void *inRefcon) {
    int n;
    OSStatus status;
    if (inShape == NULL || inProc == NULL)
        return paramErr;
    pixman_box32_t *boxes = pixman_region32_rectangles(reg(inShape), &n);
    Boolean fromBottom = (inOptions & kHIShapeParseFromBottom) != 0;
    Boolean fromRight = (inOptions & kHIShapeParseFromRight) != 0;
    status = inProc(kHIShapeEnumerateInit, inShape, NULL, inRefcon);
    for (int k = 0; status == noErr && k < n; k++) {
        /* pixman keeps them top to bottom, left to right within a band. */
        int i = fromBottom ? n - 1 - k : k;
        if (fromRight && !fromBottom)
            i = n - 1 - k;
        CGRect r = CGRectMake(boxes[i].x1, boxes[i].y1, boxes[i].x2 - boxes[i].x1, boxes[i].y2 - boxes[i].y1);
        status = inProc(kHIShapeEnumerateRect, inShape, &r, inRefcon);
    }
    (void)inProc(kHIShapeEnumerateTerminate, inShape, NULL, inRefcon);
    return status;
}
