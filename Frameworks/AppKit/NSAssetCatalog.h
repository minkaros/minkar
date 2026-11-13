/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A bundle's asset catalog (Assets.car), read for its images. Private to
 * AppKit: NSImage's imageNamed: and NSBundle's imageForResource: ask it
 * before the bundle's loose files, as on the reference platform.
 */

#import <Foundation/Foundation.h>

@class NSImage;

@interface NSAssetCatalog : NSObject {
    NSData *_data;
    uint32_t _indexOffset;
    uint32_t _blockCount;
    NSDictionary *_variables;
    uint32_t *_keyFormat;
    uint32_t _keyCount;
    NSDictionary *_facets;
    NSDictionary *_renditions;
}

+ (NSAssetCatalog *)catalogForBundle:(NSBundle *)bundle;
- (instancetype)initWithContentsOfFile:(NSString *)path;
- (NSArray *)namesOfImages;
- (NSImage *)imageNamed:(NSString *)name;

@end
