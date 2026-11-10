/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSImageRep.h>
#import <AppKit/NSGraphics.h>
#import <ApplicationServices/ApplicationServices.h>

typedef NS_ENUM(NSUInteger, NSBitmapImageFileType) {
    NSBitmapImageFileTypeTIFF,
    NSBitmapImageFileTypeBMP,
    NSBitmapImageFileTypeGIF,
    NSBitmapImageFileTypeJPEG,
    NSBitmapImageFileTypePNG,
    NSBitmapImageFileTypeJPEG2000
};

/* The older names (before API 10.12's). */
static const NSBitmapImageFileType NSTIFFFileType = NSBitmapImageFileTypeTIFF;
static const NSBitmapImageFileType NSBMPFileType = NSBitmapImageFileTypeBMP;
static const NSBitmapImageFileType NSGIFFileType = NSBitmapImageFileTypeGIF;
static const NSBitmapImageFileType NSJPEGFileType = NSBitmapImageFileTypeJPEG;
static const NSBitmapImageFileType NSPNGFileType = NSBitmapImageFileTypePNG;
static const NSBitmapImageFileType NSJPEG2000FileType = NSBitmapImageFileTypeJPEG2000;

typedef enum {
    NSTIFFCompressionNone = 1,
    NSTIFFCompressionCCITTFAX3 = 3,
    NSTIFFCompressionCCITTFAX4 = 4,
    NSTIFFCompressionLZW = 5,
    NSTIFFCompressionJPEG = 6,
    NSTIFFCompressionNEXT = 32766,
    NSTIFFCompressionPackBits = 32773,
    NSTIFFCompressionOldJPEG = 32865,
} NSTIFFCompression;

typedef enum {
    NSAlphaFirstBitmapFormat = 0x01,
    NSAlphaNonpremultipliedBitmapFormat = 0x02,
    NSFloatingPointSamplesBitmapFormat = 0x04,
} NSBitmapFormat;

APPKIT_EXPORT NSString *NSImageCompressionFactor;

@interface NSBitmapImageRep : NSImageRep {
    int _samplesPerPixel;
    int _bitsPerPixel;
    int _bytesPerRow;
    int _bytesPerPlane;
    NSBitmapFormat _bitmapFormat;
    BOOL _freeWhenDone;
    BOOL _isPlanar;
    unsigned char **_bitmapPlanes;
    NSMutableDictionary *_properties;

    CGImageRef _cgImage;
}

+ (void)getTIFFCompressionTypes:(const NSTIFFCompression **)types count:(NSInteger *)count;
+ (NSString *)localizedNameForTIFFCompressionType:(NSTIFFCompression)type;
+ (NSData *)TIFFRepresentationOfImageRepsInArray:(NSArray *)array;
+ (NSData *)TIFFRepresentationOfImageRepsInArray:(NSArray *)array usingCompression:(NSTIFFCompression)compression factor:(CGFloat)factor;

+ (NSData *)representationOfImageRepsInArray:(NSArray *)array usingType:(NSBitmapImageFileType)type properties:(NSDictionary *)properties;

+ (NSArray *)imageRepsWithData:(NSData *)data;
+ imageRepWithData:(NSData *)data;

- initWithBitmapDataPlanes:(unsigned char **)planes pixelsWide:(NSInteger)width pixelsHigh:(NSInteger)height bitsPerSample:(NSInteger)bitsPerSample samplesPerPixel:(NSInteger)samplesPerPixel hasAlpha:(BOOL)hasAlpha isPlanar:(BOOL)isPlanar colorSpaceName:(NSString *)colorSpaceName bitmapFormat:(NSBitmapFormat)bitmapFormat bytesPerRow:(NSInteger)bytesPerRow bitsPerPixel:(NSInteger)bitsPerPixel;

- initWithBitmapDataPlanes:(unsigned char **)planes pixelsWide:(NSInteger)width pixelsHigh:(NSInteger)height bitsPerSample:(NSInteger)bitsPerSample samplesPerPixel:(NSInteger)samplesPerPixel hasAlpha:(BOOL)hasAlpha isPlanar:(BOOL)isPlanar colorSpaceName:(NSString *)colorSpaceName bytesPerRow:(NSInteger)bytesPerRow bitsPerPixel:(NSInteger)bitsPerPixel;

- initForIncrementalLoad;

- initWithFocusedViewRect:(NSRect)rect;

- initWithData:(NSData *)data;
- initWithContentsOfFile:(NSString *)path;
- initWithCGImage:(CGImageRef)cgImage;

- (NSInteger)incrementalLoadFromData:(NSData *)data complete:(BOOL)complete;

- (NSInteger)bitsPerPixel;
- (NSInteger)samplesPerPixel;
- (NSInteger)bytesPerRow;
- (BOOL)isPlanar;
- (NSInteger)numberOfPlanes;
- (NSInteger)bytesPerPlane;

- (NSBitmapFormat)bitmapFormat;
- (unsigned char *)bitmapData;

- (void)getBitmapDataPlanes:(unsigned char **)planes;

- (void)getPixel:(NSUInteger[])pixel atX:(NSInteger)x y:(NSInteger)y;
- (void)setPixel:(NSUInteger[])pixel atX:(NSInteger)x y:(NSInteger)y;

- (NSColor *)colorAtX:(NSInteger)x y:(NSInteger)y;
- (void)setColor:(NSColor *)color atX:(NSInteger)x y:(NSInteger)y;

- valueForProperty:(NSString *)property;
- (void)setProperty:(NSString *)property withValue:value;

- (void)colorizeByMappingGray:(CGFloat)gray toColor:(NSColor *)color blackMapping:(NSColor *)blackMapping whiteMapping:(NSColor *)whiteMapping;

- (void)getCompression:(NSTIFFCompression *)compression factor:(CGFloat *)factor;
- (void)setCompression:(NSTIFFCompression)compression factor:(CGFloat)factor;

- (BOOL)canBeCompressedUsing:(NSTIFFCompression)compression;

- (NSData *)representationUsingType:(NSBitmapImageFileType)type properties:(NSDictionary *)properties;

- (NSData *)TIFFRepresentation;
- (NSData *)TIFFRepresentationUsingCompression:(NSTIFFCompression)compression factor:(CGFloat)factor;

- (CGImageRef)CGImage;

@end
