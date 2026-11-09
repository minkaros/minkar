/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Dragging items and sessions (API 19): what beginDraggingSessionWithItems:
 * event:source: drags, and the source's protocol.
 */

#import <Foundation/Foundation.h>
#import <AppKit/AppKitExport.h>
#import <AppKit/NSDragging.h>
#import <AppKit/NSPasteboard.h>

@class NSImage, NSView, NSEvent, NSDraggingSession;

typedef NSString *NSDraggingImageComponentKey;
APPKIT_EXPORT NSDraggingImageComponentKey const NSDraggingImageComponentIconKey;
APPKIT_EXPORT NSDraggingImageComponentKey const NSDraggingImageComponentLabelKey;

typedef NS_ENUM(NSInteger, NSDraggingFormation) {
    NSDraggingFormationDefault = 0,
    NSDraggingFormationNone,
    NSDraggingFormationPile,
    NSDraggingFormationList,
    NSDraggingFormationStack,
};

typedef NS_ENUM(NSInteger, NSDraggingContext) {
    NSDraggingContextOutsideApplication = 0,
    NSDraggingContextWithinApplication,
};

typedef NS_OPTIONS(NSUInteger, NSDraggingItemEnumerationOptions) {
    NSDraggingItemEnumerationConcurrent = NSEnumerationConcurrent,
    NSDraggingItemEnumerationClearNonenumeratedImages = (1UL << 16),
};

typedef NS_ENUM(NSInteger, NSSpringLoadingHighlight) {
    NSSpringLoadingHighlightNone = 0,
    NSSpringLoadingHighlightStandard,
    NSSpringLoadingHighlightEmphasized,
};

@interface NSDraggingImageComponent : NSObject {
    NSDraggingImageComponentKey _key;
    id _contents;
    NSRect _frame;
}
+ (instancetype)draggingImageComponentWithKey:(NSDraggingImageComponentKey)key;
- (instancetype)initWithKey:(NSDraggingImageComponentKey)key;
@property (copy) NSDraggingImageComponentKey key;
@property (strong) id contents;
@property NSRect frame;
@end

@interface NSDraggingItem : NSObject {
    id _item;
    NSRect _draggingFrame;
    NSArray *(^_imageComponentsProvider)(void);
}
/* The item a pasteboard writer (a string, a URL, an NSPasteboardItem...). */
- (instancetype)initWithPasteboardWriter:(id<NSPasteboardWriting>)pasteboardWriter;
@property (readonly, strong) id item;
@property NSRect draggingFrame;
@property (copy) NSArray<NSDraggingImageComponent *> *(^imageComponentsProvider)(void);
- (void)setDraggingFrame:(NSRect)frame contents:(id)contents;
@property (readonly, copy) NSArray<NSDraggingImageComponent *> *imageComponents;
@end

@protocol NSDraggingSource <NSObject>
@required
- (NSDragOperation)draggingSession:(NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context;
@optional
- (void)draggingSession:(NSDraggingSession *)session willBeginAtPoint:(NSPoint)screenPoint;
- (void)draggingSession:(NSDraggingSession *)session movedToPoint:(NSPoint)screenPoint;
- (void)draggingSession:(NSDraggingSession *)session endedAtPoint:(NSPoint)screenPoint operation:(NSDragOperation)operation;
- (BOOL)ignoreModifierKeysForDraggingSession:(NSDraggingSession *)session;
@end

@interface NSDraggingSession : NSObject {
    NSDraggingFormation _draggingFormation;
    BOOL _animatesToStartingPositionsOnCancelOrFail;
    NSInteger _draggingLeaderIndex;
    NSPasteboard *_draggingPasteboard;
    NSInteger _draggingSequenceNumber;
    NSPoint _draggingLocation;
    NSArray *_items;
    id _source;
}
@property NSDraggingFormation draggingFormation;
@property BOOL animatesToStartingPositionsOnCancelOrFail;
@property NSInteger draggingLeaderIndex;
@property (readonly, strong) NSPasteboard *draggingPasteboard;
@property (readonly) NSInteger draggingSequenceNumber;
@property (readonly) NSPoint draggingLocation;
- (void)enumerateDraggingItemsWithOptions:(NSDraggingItemEnumerationOptions)enumOpts forView:(NSView *)view classes:(NSArray<Class> *)classArray searchOptions:(NSDictionary *)searchOptions usingBlock:(void (^)(NSDraggingItem *draggingItem, NSInteger idx, BOOL *stop))block;
- (void)_setPasteboard:(NSPasteboard *)pasteboard items:(NSArray *)items;
@end
