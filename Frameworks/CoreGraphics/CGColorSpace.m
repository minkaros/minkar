/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CGColorSpace.h>
#include <string.h>
#import <Onyx2D/O2ColorSpace.h>

CGColorSpaceRef CGColorSpaceRetain(CGColorSpaceRef colorSpace) {
   return O2ColorSpaceRetain(colorSpace);
}

void CGColorSpaceRelease(CGColorSpaceRef colorSpace) {
   O2ColorSpaceRelease(colorSpace);
}

CGColorSpaceRef CGColorSpaceCreateDeviceRGB() {
   return O2ColorSpaceCreateDeviceRGB();
}

CGColorSpaceRef CGColorSpaceCreateDeviceGray() {
   return O2ColorSpaceCreateDeviceGray();
}

CGColorSpaceRef CGColorSpaceCreateDeviceCMYK() {
   return O2ColorSpaceCreateDeviceCMYK();
}

CGColorSpaceRef CGColorSpaceCreatePattern(CGColorSpaceRef baseSpace) {
   return O2ColorSpaceCreatePattern(baseSpace);
}

CGColorSpaceModel CGColorSpaceGetModel(CGColorSpaceRef self) {
   return O2ColorSpaceGetModel(self);
}

size_t CGColorSpaceGetNumberOfComponents(CGColorSpaceRef self) {
   return O2ColorSpaceGetNumberOfComponents(self);
}

static O2ColorSpace_indexed *indexedSpace(CGColorSpaceRef space) {
    O2ColorSpace *s = (O2ColorSpace *)space;

    return [s isKindOfClass:[O2ColorSpace_indexed class]] ? (O2ColorSpace_indexed *)s : nil;
}

CGColorSpaceRef CGColorSpaceGetBaseColorSpace(CGColorSpaceRef space) {
    O2ColorSpace_indexed *indexed = indexedSpace(space);

    return indexed ? (CGColorSpaceRef)[indexed baseColorSpace] : NULL;
}

size_t CGColorSpaceGetColorTableCount(CGColorSpaceRef space) {
    O2ColorSpace_indexed *indexed = indexedSpace(space);

    return indexed ? (size_t)[indexed hival] + 1 : 0;
}

void CGColorSpaceGetColorTable(CGColorSpaceRef space, uint8_t *table) {
    O2ColorSpace_indexed *indexed = indexedSpace(space);

    if (indexed == nil || table == NULL)
        return;
    size_t perColor = O2ColorSpaceGetNumberOfComponents([indexed baseColorSpace]);
    memcpy(table, [indexed paletteBytes], ((size_t)[indexed hival] + 1) * perColor);
}
