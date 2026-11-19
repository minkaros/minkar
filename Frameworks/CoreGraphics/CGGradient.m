/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <CoreGraphics/CGGradient.h>
#import <CoreGraphics/CGColor.h>
#import <CoreGraphics/CGContext.h>
#import <CoreGraphics/CGFunction.h>
#import <CoreGraphics/CGShading.h>
#import <Foundation/NSObject.h>
#include <stdlib.h>
#include <string.h>

/* A gradient: colors (each its space's components and alpha) at locations
   from 0 to 1, drawn through an axial or radial shading whose function
   interpolates them linearly. */
@interface _CGGradient : NSObject {
@public
    CGColorSpaceRef _space;
    size_t _count;        /* colors */
    size_t _components;   /* per color, alpha included */
    CGFloat *_colors;
    CGFloat *_locations;
}
@end

@implementation _CGGradient
- (void)dealloc {
    CGColorSpaceRelease(_space);
    free(_colors);
    free(_locations);
    [super dealloc];
}
@end

CFTypeID CGGradientGetTypeID(void) {
    return (CFTypeID)(uintptr_t)[_CGGradient class];
}

CGGradientRef CGGradientCreateWithColorComponents(CGColorSpaceRef space, const CGFloat *components, const CGFloat *locations, size_t count) {
    if (space == NULL || components == NULL || count == 0)
        return NULL;

    _CGGradient *self = [_CGGradient new];
    size_t i;

    self->_space = CGColorSpaceRetain(space);
    self->_count = count;
    self->_components = CGColorSpaceGetNumberOfComponents(space) + 1;
    self->_colors = malloc(sizeof(CGFloat) * count * self->_components);
    memcpy(self->_colors, components, sizeof(CGFloat) * count * self->_components);
    self->_locations = malloc(sizeof(CGFloat) * count);
    for (i = 0; i < count; i++)
        /* Without locations the colors are spread evenly (one is at 0). */
        self->_locations[i] = (locations != NULL) ? locations[i] : (count > 1 ? (CGFloat)i / (CGFloat)(count - 1) : 0);
    return (CGGradientRef)self;
}

/* A color's components in the gradient's space: as they are when it is
   that space's (by count), gray made RGB, or else gray from its first. */
static void colorInSpace(CGColorRef color, size_t components, CGFloat *out) {
    size_t have = CGColorGetNumberOfComponents(color), j;
    const CGFloat *in = CGColorGetComponents(color);

    if (have == components)
        memcpy(out, in, sizeof(CGFloat) * components);
    else if (have == 2 && components == 4) {
        out[0] = out[1] = out[2] = in[0];
        out[3] = in[1];
    }
    else if (have == 4 && components == 2) {
        out[0] = 0.3 * in[0] + 0.59 * in[1] + 0.11 * in[2];
        out[1] = in[3];
    }
    else {
        for (j = 0; j + 1 < components; j++)
            out[j] = (have > 0) ? in[0] : 0;
        out[components - 1] = (have > 0) ? in[have - 1] : 1;
    }
}

CGGradientRef CGGradientCreateWithColors(CGColorSpaceRef space, CFArrayRef colors, const CGFloat *locations) {
    CFIndex i, count = (colors != NULL) ? CFArrayGetCount(colors) : 0;
    CGColorSpaceRef own = NULL;

    if (count == 0)
        return NULL;
    /* No space: the first color's. */
    if (space == NULL)
        space = own = CGColorSpaceRetain(CGColorGetColorSpace((CGColorRef)CFArrayGetValueAtIndex(colors, 0)));

    size_t components = CGColorSpaceGetNumberOfComponents(space) + 1;
    CGFloat *values = malloc(sizeof(CGFloat) * count * components);

    for (i = 0; i < count; i++)
        colorInSpace((CGColorRef)CFArrayGetValueAtIndex(colors, i), components, values + i * components);

    CGGradientRef result = CGGradientCreateWithColorComponents(space, values, locations, count);
    free(values);
    CGColorSpaceRelease(own);
    return result;
}

CGGradientRef CGGradientRetain(CGGradientRef gradient) {
    return gradient ? (CGGradientRef)CFRetain(gradient) : NULL;
}

void CGGradientRelease(CGGradientRef gradient) {
    if (gradient)
        CFRelease(gradient);
}

/* The color at t: the first color before the first location, the last
   after the last, linear between two. */
static void evaluate(void *info, const CGFloat *in, CGFloat *out) {
    _CGGradient *self = (_CGGradient *)info;
    CGFloat t = in[0];
    size_t n = self->_components, i, j;

    if (self->_count == 1 || t <= self->_locations[0]) {
        memcpy(out, self->_colors, sizeof(CGFloat) * n);
        return;
    }
    for (i = 1; i < self->_count; i++)
        if (t <= self->_locations[i])
            break;
    if (i == self->_count) {
        memcpy(out, self->_colors + (self->_count - 1) * n, sizeof(CGFloat) * n);
        return;
    }

    CGFloat span = self->_locations[i] - self->_locations[i - 1];
    CGFloat f = (span > 0) ? (t - self->_locations[i - 1]) / span : 1;
    for (j = 0; j < n; j++)
        out[j] = self->_colors[(i - 1) * n + j] * (1 - f) + self->_colors[i * n + j] * f;
}

static void releaseInfo(void *info) {
    [(_CGGradient *)info release];
}

static CGFunctionRef functionOf(_CGGradient *self) {
    static const CGFloat domain[2] = { 0, 1 };
    static const CGFunctionCallbacks callbacks = { 0, evaluate, releaseInfo };
    CGFloat range[2 * 5];
    size_t j;

    for (j = 0; j < self->_components && j < 5; j++) {
        range[2 * j] = 0;
        range[2 * j + 1] = 1;
    }
    return CGFunctionCreate([self retain], 1, domain, self->_components, range, &callbacks);
}

void CGContextDrawLinearGradient(CGContextRef context, CGGradientRef gradient, CGPoint startPoint, CGPoint endPoint, CGGradientDrawingOptions options) {
    _CGGradient *self = (_CGGradient *)gradient;

    if (context == NULL || self == nil)
        return;
    CGFunctionRef function = functionOf(self);
    CGShadingRef shading = CGShadingCreateAxial(self->_space, startPoint, endPoint, function,
        (options & kCGGradientDrawsBeforeStartLocation) != 0, (options & kCGGradientDrawsAfterEndLocation) != 0);
    CGContextDrawShading(context, shading);
    CGShadingRelease(shading);
    CGFunctionRelease(function);
}

void CGContextDrawRadialGradient(CGContextRef context, CGGradientRef gradient, CGPoint startCenter, CGFloat startRadius, CGPoint endCenter, CGFloat endRadius, CGGradientDrawingOptions options) {
    _CGGradient *self = (_CGGradient *)gradient;

    if (context == NULL || self == nil)
        return;
    CGFunctionRef function = functionOf(self);
    CGShadingRef shading = CGShadingCreateRadial(self->_space, startCenter, startRadius, endCenter, endRadius, function,
        (options & kCGGradientDrawsBeforeStartLocation) != 0, (options & kCGGradientDrawsAfterEndLocation) != 0);
    CGContextDrawShading(context, shading);
    CGShadingRelease(shading);
    CGFunctionRelease(function);
}
