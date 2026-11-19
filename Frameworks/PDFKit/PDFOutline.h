/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <PDFKit/PDFKitPlatform.h>

@class PDFDocument, PDFDestination, PDFAction;

NS_ASSUME_NONNULL_BEGIN

/* A document's outline: the root (no label of its own) and its items,
   each with a destination or an action. */
PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFOutline : NSObject
- (instancetype)init NS_DESIGNATED_INITIALIZER;
@property (nonatomic, readonly, weak, nullable) PDFDocument *document;
@property (nonatomic, readonly, weak, nullable) PDFOutline *parent;
@property (nonatomic, readonly) NSUInteger numberOfChildren;
@property (nonatomic, readonly) NSUInteger index;
- (nullable PDFOutline *)childAtIndex:(NSUInteger)index;
- (void)insertChild:(PDFOutline *)child atIndex:(NSUInteger)index;
- (void)removeFromParent;
@property (nonatomic, copy, nullable) NSString *label;
@property (nonatomic) BOOL isOpen;
@property (nonatomic, strong, nullable) PDFDestination *destination;
@property (nonatomic, strong, nullable) PDFAction *action;
@end

NS_ASSUME_NONNULL_END
