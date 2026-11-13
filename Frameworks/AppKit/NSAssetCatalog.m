/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Asset catalogs (Assets.car), as Xcode's actool compiles them: an app's
 * images are often only there. The file is a BOM store (big-endian blocks
 * and named variables) holding the catalog's trees, little-endian: the key
 * format (which attributes a rendition key has, in order), the facets
 * (each name with the attributes that identify it) and the renditions
 * (each key with its image). A rendition is a CSI header and then its
 * data: pixels compressed (none, zlib, LZVN or LZFSE through
 * libcompression, in KCBC chunks), a PDF or other raw data, or a
 * reference to a region of a packed atlas. The layout follows xcbuild's
 * libcar (BSD), which documents the format.
 *
 * Not read yet: compression 11 (Xcode's newer packed format) and palette
 * images; SVG renditions; named colors.
 */

#import "NSAssetCatalog.h"
#import <AppKit/NSImage.h>
#import <AppKit/NSBitmapImageRep.h>
#import <AppKit/NSPDFImageRep.h>
#import <CoreGraphics/CoreGraphics.h>
#include <compression.h>
#include <string.h>
#include <zlib.h>

#define COMPRESSION_LZVN_RAW 0x900

enum {
    AttributeElement = 1, AttributePart = 2, AttributeSize = 3, AttributeDirection = 4,
    AttributeAppearance = 7, AttributeDimension1 = 8, AttributeDimension2 = 9, AttributeState = 10,
    AttributeScale = 12, AttributeIdiom = 15, AttributeIdentifier = 17, AttributeDisplayGamut = 24,
};

enum { LayoutPDF = 9, LayoutInternalReference = 1003, LayoutPackedImage = 1004 };
enum { InfoBytesPerRow = 1007, InfoReference = 1010 };
enum { CompressionNone = 0, CompressionZlib = 2, CompressionLZVN = 3, CompressionLZFSE = 4 };

#define CSI_HEADER_SIZE 184

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0]; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[1] << 8 | p[0]); }

@implementation NSAssetCatalog

/* One catalog for each bundle, read once. */
+ (NSAssetCatalog *)catalogForBundle:(NSBundle *)bundle {
    static NSMutableDictionary *catalogs;
    NSString *path = [bundle pathForResource:@"Assets" ofType:@"car"];
    NSAssetCatalog *catalog;

    if (path == nil)
        return nil;
    @synchronized(self) {
        if (catalogs == nil)
            catalogs = [[NSMutableDictionary alloc] init];
        catalog = [catalogs objectForKey:path];
        if (catalog == nil) {
            catalog = [[[self alloc] initWithContentsOfFile:path] autorelease];
            [catalogs setObject:(catalog != nil ? (id)catalog : (id)[NSNull null]) forKey:path];
        }
    }
    return [catalog isKindOfClass:self] ? catalog : nil;
}

- (instancetype)initWithContentsOfFile:(NSString *)path {
    if ((self = [super init]) == nil)
        return nil;
    _data = [[NSData alloc] initWithContentsOfFile:path options:NSDataReadingMappedIfSafe error:NULL];
    if (_data == nil || ![self _readStore]) {
        [self release];
        return nil;
    }
    return self;
}

- (void)dealloc {
    [_data release];
    [_variables release];
    [_facets release];
    [_renditions release];
    free(_keyFormat);
    [super dealloc];
}

#pragma mark - The BOM store

- (NSData *)_block:(uint32_t)index {
    const uint8_t *bytes = [_data bytes];
    NSUInteger length = [_data length];
    uint32_t address, size;

    if (index >= _blockCount)
        return nil;
    address = be32(bytes + _indexOffset + 4 + 8 * index);
    size = be32(bytes + _indexOffset + 8 + 8 * index);
    if ((NSUInteger)address + size > length)
        return nil;
    return [_data subdataWithRange:NSMakeRange(address, size)];
}

/* Each key and value of a tree, from its leftmost leaf along the leaves. */
- (void)_tree:(NSString *)name each:(void (^)(NSData *key, NSData *value))body {
    NSNumber *variable = [_variables objectForKey:name];
    NSData *tree = variable != nil ? [self _block:[variable unsignedIntValue]] : nil;
    NSData *node;
    int guard = 0;

    if ([tree length] < 21 || memcmp([tree bytes], "tree", 4) != 0)
        return;
    node = [self _block:be32((const uint8_t *)[tree bytes] + 8)];
    while (node != nil && [node length] >= 12 && be16([node bytes]) == 0 && guard++ < 64)
        node = [self _block:be32((const uint8_t *)[node bytes] + 12)];
    while (node != nil && [node length] >= 12 && guard++ < 100000) {
        const uint8_t *p = [node bytes];
        uint16_t count = be16(p + 2);
        uint32_t forward = be32(p + 4);

        for (uint16_t i = 0; i < count && 12 + 8 * (NSUInteger)(i + 1) <= [node length]; i++) {
            NSData *value = [self _block:be32(p + 12 + 8 * i)];
            NSData *key = [self _block:be32(p + 16 + 8 * i)];

            if (key != nil && value != nil)
                body(key, value);
        }
        if (forward == 0)
            break;
        node = [self _block:forward];
    }
}

- (BOOL)_readStore {
    const uint8_t *bytes = [_data bytes];
    NSUInteger length = [_data length];
    uint32_t variablesOffset, count, offset;

    if (length < 32 || memcmp(bytes, "BOMStore", 8) != 0)
        return NO;
    _indexOffset = be32(bytes + 16);
    variablesOffset = be32(bytes + 24);
    if (_indexOffset + 4 > length || variablesOffset + 4 > length)
        return NO;
    _blockCount = be32(bytes + _indexOffset);
    if (_indexOffset + 4 + 8 * (NSUInteger)_blockCount > length)
        return NO;

    _variables = [[NSMutableDictionary alloc] init];
    count = be32(bytes + variablesOffset);
    offset = variablesOffset + 4;
    for (uint32_t i = 0; i < count && offset + 5 <= length; i++) {
        uint32_t index = be32(bytes + offset);
        uint8_t nameLength = bytes[offset + 4];
        NSString *name;

        if (offset + 5 + nameLength > length)
            break;
        name = [[[NSString alloc] initWithBytes:bytes + offset + 5 length:nameLength encoding:NSASCIIStringEncoding] autorelease];
        if (name != nil)
            [(NSMutableDictionary *)_variables setObject:[NSNumber numberWithUnsignedInt:index] forKey:name];
        offset += 5 + nameLength;
    }

    // The attributes of a rendition key, in order.
    NSNumber *format = [_variables objectForKey:@"KEYFORMAT"];
    NSData *formatBlock = format != nil ? [self _block:[format unsignedIntValue]] : nil;
    if ([formatBlock length] < 12)
        return NO;
    _keyCount = le32((const uint8_t *)[formatBlock bytes] + 8);
    if ([formatBlock length] < 12 + 4 * (NSUInteger)_keyCount)
        return NO;
    _keyFormat = calloc(_keyCount ? _keyCount : 1, sizeof(uint32_t));
    for (uint32_t i = 0; i < _keyCount; i++)
        _keyFormat[i] = le32((const uint8_t *)[formatBlock bytes] + 12 + 4 * i);

    // Names: each facet's identifier.
    _facets = [[NSMutableDictionary alloc] init];
    [self _tree:@"FACETKEYS" each:^(NSData *key, NSData *value) {
        const uint8_t *p = [value bytes];
        NSString *name = [[[NSString alloc] initWithData:key encoding:NSUTF8StringEncoding] autorelease];
        uint16_t attributes;

        if ([value length] < 6 || name == nil)
            return;
        attributes = le16(p + 4);
        for (uint16_t i = 0; i < attributes && 6 + 4 * (NSUInteger)(i + 1) <= [value length]; i++)
            if (le16(p + 6 + 4 * i) == AttributeIdentifier)
                [(NSMutableDictionary *)_facets setObject:[NSNumber numberWithUnsignedShort:le16(p + 8 + 4 * i)] forKey:name];
    }];

    // Renditions by their key's bytes.
    _renditions = [[NSMutableDictionary alloc] init];
    [self _tree:@"RENDITIONS" each:^(NSData *key, NSData *value) {
        [(NSMutableDictionary *)_renditions setObject:value forKey:key];
    }];
    return [_renditions count] > 0;
}

/* A rendition key's attribute, 0 when the format has not it. */
- (uint16_t)_attribute:(uint32_t)identifier ofKey:(NSData *)key {
    for (uint32_t i = 0; i < _keyCount && 2 * (NSUInteger)(i + 1) <= [key length]; i++)
        if (_keyFormat[i] == identifier)
            return le16((const uint8_t *)[key bytes] + 2 * i);
    return 0;
}

#pragma mark - Renditions

/* The pixels of a bitmap rendition, uncompressed. */
- (NSData *)_pixelsOf:(NSData *)rendition length:(NSUInteger)expected {
    const uint8_t *p = [rendition bytes];
    NSUInteger infoLength = le32(p + 168);
    NSUInteger at = CSI_HEADER_SIZE + infoLength;
    uint32_t compression, length;
    const uint8_t *compressed;
    NSMutableData *pixels;
    NSUInteger done = 0;

    if ([rendition length] < at + 16 || memcmp(p + at, "MLEC", 4) != 0)
        return nil;
    compression = le32(p + at + 8);
    length = le32(p + at + 12);
    compressed = p + at + 16;
    if (at + 16 + (NSUInteger)length > [rendition length])
        return nil;
    pixels = [NSMutableData dataWithLength:expected];

    // The data may come in chunks, each with a KCBC header.
    const uint8_t *end = compressed + length;
    while (done < expected && compressed < end) {
        uint32_t chunk = (uint32_t)(end - compressed);

        if (end - compressed >= 20 && memcmp(compressed, "KCBC", 4) == 0) {
            chunk = le32(compressed + 16);
            compressed += 20;
            if (compressed + chunk > end)
                return nil;
        }
        size_t produced = 0;
        uint8_t *out = (uint8_t *)[pixels mutableBytes] + done;

        switch (compression) {
        case CompressionNone:
            produced = MIN((NSUInteger)chunk, expected - done);
            memcpy(out, compressed, produced);
            break;
        case CompressionZlib: {
            z_stream stream;

            memset(&stream, 0, sizeof(stream));
            if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK)
                return nil;
            stream.next_in = (Bytef *)compressed;
            stream.avail_in = chunk;
            stream.next_out = out;
            stream.avail_out = (uInt)(expected - done);
            inflate(&stream, Z_FINISH);
            produced = (expected - done) - stream.avail_out;
            inflateEnd(&stream);
            break;
        }
        case CompressionLZVN:
        case CompressionLZFSE:
            produced = compression_decode_buffer(out, expected - done, compressed, chunk, NULL,
                compression == CompressionLZVN ? (compression_algorithm)COMPRESSION_LZVN_RAW : COMPRESSION_LZFSE);
            break;
        default:
            return nil;
        }
        if (produced == 0)
            return nil;
        done += produced;
        compressed += chunk;
    }
    return done == expected ? pixels : nil;
}

/* Raw data (a PDF, an SVG, a data set), LZFSE-compressed when it starts
   with an LZFSE block ("bvx"). */
static NSData *rawData(NSData *data) {
    if ([data length] < 4 || memcmp([data bytes], "bvx", 3) != 0)
        return data;
    for (size_t capacity = 4 * [data length] + 4096; capacity <= 256 * 1024 * 1024; capacity *= 4) {
        NSMutableData *out = [NSMutableData dataWithLength:capacity];
        size_t length = compression_decode_buffer([out mutableBytes], capacity, [data bytes], [data length], NULL, COMPRESSION_LZFSE);

        if (length == 0)
            return nil;
        if (length < capacity) {
            [out setLength:length];
            return out;
        }
    }
    return nil;
}

/* A bitmap of a rendition's pixels (or of a region of them). */
static CGImageRef createImage(NSData *pixels, uint32_t format, uint32_t width, uint32_t height, NSUInteger bytesPerRow,
                              NSRect region) {
    CGColorSpaceRef space;
    CGDataProviderRef provider;
    CGImageRef image, cropped;
    BOOL gray = (format == 'GA8 ');

    space = gray ? CGColorSpaceCreateDeviceGray() : CGColorSpaceCreateDeviceRGB();
    provider = CGDataProviderCreateWithCFData((CFDataRef)pixels);
    image = CGImageCreate(width, height, 8, gray ? 16 : 32, bytesPerRow, space,
                          gray ? kCGImageAlphaPremultipliedLast
                               : (kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little),
                          provider, NULL, false, kCGRenderingIntentDefault);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(space);
    if (image == NULL || NSIsEmptyRect(region))
        return image;
    cropped = CGImageCreateWithImageInRect(image, NSRectToCGRect(region));
    CGImageRelease(image);
    return cropped;
}

/* A rendition as an image representation, or nil. */
- (NSImageRep *)_representationOf:(NSData *)rendition {
    const uint8_t *p = [rendition bytes];
    uint32_t width, height, scale, format, infoLength;
    uint16_t layout;
    NSUInteger bytesPerRow = 0;
    NSData *source = rendition;
    NSRect region = NSZeroRect;

    if ([rendition length] < CSI_HEADER_SIZE || memcmp(p, "ISTC", 4) != 0)
        return nil;
    width = le32(p + 12);
    height = le32(p + 16);
    scale = le32(p + 20);
    format = le32(p + 24);
    layout = le16(p + 36);
    infoLength = le32(p + 168);
    if (CSI_HEADER_SIZE + (NSUInteger)infoLength > [rendition length])
        return nil;

    // The information list: bytes per row, a reference into an atlas.
    for (NSUInteger at = CSI_HEADER_SIZE; at + 8 <= CSI_HEADER_SIZE + infoLength;) {
        uint32_t magic = le32(p + at), length = le32(p + at + 4);

        if (at + 8 + length > CSI_HEADER_SIZE + infoLength)
            break;
        if (magic == InfoBytesPerRow && length >= 4)
            bytesPerRow = le32(p + at + 8);
        else if (magic == InfoReference && length >= 30 && memcmp(p + at + 8, "KLNI", 4) == 0) {
            // The atlas a region is in: by its key's attributes (identifier,
            // value pairs; the others are 0).
            uint16_t pairsLength = le16(p + at + 34);
            uint16_t values[_keyCount ? _keyCount : 1];

            region = NSMakeRect(le32(p + at + 16), le32(p + at + 20), le32(p + at + 24), le32(p + at + 28));
            if (30 + (NSUInteger)pairsLength > length)
                break;
            memset(values, 0, sizeof(values));
            for (NSUInteger pair = 0; pair + 4 <= pairsLength; pair += 4) {
                uint16_t identifier = le16(p + at + 38 + pair), value = le16(p + at + 40 + pair);

                for (uint32_t i = 0; i < _keyCount; i++)
                    if (identifier != 0 && _keyFormat[i] == identifier)
                        values[i] = value;
            }
            NSMutableData *key = [NSMutableData dataWithLength:2 * _keyCount];
            for (uint32_t i = 0; i < _keyCount; i++) {
                uint8_t *k = (uint8_t *)[key mutableBytes] + 2 * i;
                k[0] = values[i] & 0xff;
                k[1] = values[i] >> 8;
            }
            source = [_renditions objectForKey:key];
        }
        at += 8 + length;
    }
    if (source == nil)
        return nil;

    if (layout == LayoutPDF || format == 'PDF ') {
        NSUInteger at = CSI_HEADER_SIZE + infoLength;

        NSData *document;

        if ([rendition length] < at + 12 || memcmp(p + at, "DWAR", 4) != 0)
            return nil;
        document = rawData([rendition subdataWithRange:NSMakeRange(at + 12, MIN((NSUInteger)le32(p + at + 8), [rendition length] - at - 12))]);
        if ([document length] < 5 || memcmp([document bytes], "%PDF-", 5) != 0)
            return nil;
        return [NSPDFImageRep imageRepWithData:document];
    }

    // A bitmap: its own pixels, or the atlas' it refers to.
    const uint8_t *s = [source bytes];
    uint32_t sourceWidth = le32(s + 12), sourceHeight = le32(s + 16), sourceFormat = le32(s + 24);
    if (source != rendition) {
        uint32_t sourceInfo = le32(s + 168);

        bytesPerRow = 0;
        for (NSUInteger at = CSI_HEADER_SIZE; at + 8 <= CSI_HEADER_SIZE + sourceInfo;) {
            uint32_t magic = le32(s + at), length = le32(s + at + 4);

            if (magic == InfoBytesPerRow && length >= 4)
                bytesPerRow = le32(s + at + 8);
            at += 8 + length;
        }
    }
    if (sourceFormat != 'ARGB' && sourceFormat != 'GA8 ')
        return nil;
    if (bytesPerRow == 0)
        bytesPerRow = sourceWidth * (sourceFormat == 'GA8 ' ? 2 : 4);

    NSData *pixels = [self _pixelsOf:source length:bytesPerRow * sourceHeight];
    CGImageRef image = pixels != nil ? createImage(pixels, sourceFormat, sourceWidth, sourceHeight, bytesPerRow, region) : NULL;
    NSBitmapImageRep *rep;

    if (image == NULL)
        return nil;
    rep = [[[NSBitmapImageRep alloc] initWithCGImage:image] autorelease];
    CGImageRelease(image);
    // Its size in points: pixels over its scale (100 is 1x).
    if (scale >= 100) {
        CGFloat factor = scale / 100.0;
        [rep setSize:NSMakeSize([rep pixelsWide] / factor, [rep pixelsHigh] / factor)];
    }
    (void)width;
    (void)height;
    return rep;
}

#pragma mark - Images by name

- (NSArray *)namesOfImages {
    return [_facets allKeys];
}

/* The image of this name: a representation for each scale, the light
   appearance's (Minkar draws in light). */
- (NSImage *)imageNamed:(NSString *)name {
    NSNumber *identifier = [_facets objectForKey:name];
    NSMutableDictionary *byScale = [NSMutableDictionary dictionary];
    NSImage *image;

    if (identifier == nil)
        return nil;
    for (NSData *key in _renditions) {
        uint16_t scale;

        if ([self _attribute:AttributeIdentifier ofKey:key] != [identifier unsignedShortValue])
            continue;
        if ([self _attribute:AttributeAppearance ofKey:key] != 0)
            continue;
        scale = [self _attribute:AttributeScale ofKey:key];
        if ([byScale objectForKey:[NSNumber numberWithUnsignedShort:scale]] != nil)
            continue;
        NSImageRep *rep = [self _representationOf:[_renditions objectForKey:key]];

        if (rep != nil)
            [byScale setObject:rep forKey:[NSNumber numberWithUnsignedShort:scale]];
    }
    if ([byScale count] == 0)
        return nil;
    image = [[[NSImage alloc] initWithSize:NSZeroSize] autorelease];
    for (NSNumber *scale in [[byScale allKeys] sortedArrayUsingSelector:@selector(compare:)])
        [image addRepresentation:[byScale objectForKey:scale]];
    [image setSize:[[[image representations] objectAtIndex:0] size]];
    return image;
}

@end
