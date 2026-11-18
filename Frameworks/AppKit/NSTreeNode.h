/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSTreeNode (API 10.5): a node of a tree, the object it represents, its
 * parent and its children; NSTreeController's arranged objects are nodes.
 */

#import <Foundation/NSObject.h>

@class NSArray, NSMutableArray, NSIndexPath, NSSortDescriptor;

@interface NSTreeNode : NSObject

+ (instancetype)treeNodeWithRepresentedObject:(id)modelObject;
- (instancetype)initWithRepresentedObject:(id)modelObject;

@property (readonly, strong) id representedObject;
@property (readonly, weak) NSTreeNode *parentNode;
@property (readonly, copy) NSArray *childNodes;
@property (readonly, strong) NSMutableArray *mutableChildNodes;
@property (readonly, strong) NSIndexPath *indexPath;
@property (readonly, getter=isLeaf) BOOL leaf;

- (NSTreeNode *)descendantNodeAtIndexPath:(NSIndexPath *)indexPath;
- (void)sortWithSortDescriptors:(NSArray *)sortDescriptors recursively:(BOOL)recursively;

@end
