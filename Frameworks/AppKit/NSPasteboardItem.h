/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSPasteboardItem (API 19): one item's data in several types, written
 * with writeObjects: and read back from pasteboardItems.
 */

#import <Foundation/Foundation.h>
#import <AppKit/NSPasteboard.h>

@class NSPasteboardItem;

@protocol NSPasteboardItemDataProvider <NSObject>
@required
- (void)pasteboard:(NSPasteboard *)pasteboard item:(NSPasteboardItem *)item provideDataForType:(NSPasteboardType)type;
@optional
- (void)pasteboardFinishedWithDataProvider:(NSPasteboard *)pasteboard;
@end

@interface NSPasteboardItem : NSObject <NSPasteboardWriting, NSPasteboardReading> {
    NSMutableDictionary *_values;
    NSMutableArray *_types;
    NSMutableDictionary *_providers;
}

@property (readonly, copy) NSArray<NSPasteboardType> *types;
- (NSPasteboardType)availableTypeFromArray:(NSArray<NSPasteboardType> *)types;
/* The provider gives the data for those types when they are read. */
- (BOOL)setDataProvider:(id<NSPasteboardItemDataProvider>)dataProvider forTypes:(NSArray<NSPasteboardType> *)types;
- (BOOL)setData:(NSData *)data forType:(NSPasteboardType)type;
- (BOOL)setString:(NSString *)string forType:(NSPasteboardType)type;
- (BOOL)setPropertyList:(id)propertyList forType:(NSPasteboardType)type;
- (NSData *)dataForType:(NSPasteboardType)type;
- (NSString *)stringForType:(NSPasteboardType)type;
- (id)propertyListForType:(NSPasteboardType)type;

@end

@interface NSPasteboard (NSPasteboardItems)
/* The pasteboard's content as one item with all its types. */
@property (readonly, copy) NSArray<NSPasteboardItem *> *pasteboardItems;
@end
