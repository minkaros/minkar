/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
// MRC, as the rest of AppKit.

#import <AppKit/NSTreeNode.h>
#import <Foundation/Foundation.h>

@interface NSTreeNode ()
- (void)_setParentNode:(NSTreeNode *)parent;
- (NSMutableArray *)_children;
@end

/* A node's children as a mutable array: a node added has this node as its
   parent, a node taken out none. */
@interface _NSTreeNodeChildren : NSMutableArray {
    NSTreeNode *_owner;   /* not retained: the owner holds this array */
}
- (instancetype)initWithOwner:(NSTreeNode *)owner;
@end

@implementation _NSTreeNodeChildren

- (instancetype)initWithOwner:(NSTreeNode *)owner {
    if((self = [super init]) != nil)
        _owner = owner;
    return self;
}

- (NSUInteger)count {
    return [[_owner _children] count];
}

- (id)objectAtIndex:(NSUInteger)index {
    return [[_owner _children] objectAtIndex:index];
}

- (void)insertObject:(id)node atIndex:(NSUInteger)index {
    NSTreeNode *previous = [node parentNode];

    /* A node has one parent: it leaves the one it had. */
    if(previous != nil)
        [[previous mutableChildNodes] removeObjectIdenticalTo:node];
    [[_owner _children] insertObject:node atIndex:MIN(index, [[_owner _children] count])];
    [node _setParentNode:_owner];
}

- (void)removeObjectAtIndex:(NSUInteger)index {
    NSTreeNode *node = [[[[_owner _children] objectAtIndex:index] retain] autorelease];

    [[_owner _children] removeObjectAtIndex:index];
    [node _setParentNode:nil];
}

- (void)addObject:(id)node {
    [self insertObject:node atIndex:[self count]];
}

- (void)removeLastObject {
    if([self count] > 0)
        [self removeObjectAtIndex:[self count] - 1];
}

- (void)replaceObjectAtIndex:(NSUInteger)index withObject:(id)node {
    [self removeObjectAtIndex:index];
    [self insertObject:node atIndex:index];
}

@end

@implementation NSTreeNode {
    id _representedObject;
    NSTreeNode *_parentNode;          /* not retained */
    NSMutableArray *_childNodes;
    _NSTreeNodeChildren *_mutableChildNodes;
}

+ (instancetype)treeNodeWithRepresentedObject:(id)modelObject {
    return [[[self alloc] initWithRepresentedObject:modelObject] autorelease];
}

- (instancetype)initWithRepresentedObject:(id)modelObject {
    if((self = [super init]) != nil) {
        _representedObject = [modelObject retain];
        _childNodes = [[NSMutableArray alloc] init];
    }
    return self;
}

- (instancetype)init {
    return [self initWithRepresentedObject:nil];
}

- (void)dealloc {
    for(NSTreeNode *child in _childNodes)
        [child _setParentNode:nil];
    [_representedObject release];
    [_childNodes release];
    [_mutableChildNodes release];
    [super dealloc];
}

- (id)representedObject {
    return _representedObject;
}

- (NSTreeNode *)parentNode {
    return _parentNode;
}

- (void)_setParentNode:(NSTreeNode *)parent {
    _parentNode = parent;
}

- (NSMutableArray *)_children {
    return _childNodes;
}

- (NSArray *)childNodes {
    return [[[self _children] copy] autorelease];
}

- (NSMutableArray *)mutableChildNodes {
    if(_mutableChildNodes == nil)
        _mutableChildNodes = [[_NSTreeNodeChildren alloc] initWithOwner:self];
    return _mutableChildNodes;
}

/* From the root: its own is empty, a child's adds its place. */
- (NSIndexPath *)indexPath {
    NSTreeNode *parent = [self parentNode];

    if(parent == nil)
        return [[[NSIndexPath alloc] init] autorelease];
    return [[parent indexPath] indexPathByAddingIndex:[[parent _children] indexOfObjectIdenticalTo:self]];
}

- (BOOL)isLeaf {
    return [[self _children] count] == 0;
}

- (NSTreeNode *)descendantNodeAtIndexPath:(NSIndexPath *)indexPath {
    NSTreeNode *node = self;

    for(NSUInteger i = 0; i < [indexPath length]; i++) {
        NSUInteger index = [indexPath indexAtPosition:i];
        NSArray *children = [node _children];

        if(index >= [children count])
            return nil;
        node = [children objectAtIndex:index];
    }
    return node;
}

/* Each node by its represented object's values. */
- (void)sortWithSortDescriptors:(NSArray *)sortDescriptors recursively:(BOOL)recursively {
    if([sortDescriptors count] > 0)
        [[self _children] sortUsingComparator:^NSComparisonResult(NSTreeNode *a, NSTreeNode *b) {
            for(NSSortDescriptor *descriptor in sortDescriptors) {
                NSComparisonResult result = [descriptor compareObject:[a representedObject] toObject:[b representedObject]];
                if(result != NSOrderedSame)
                    return result;
            }
            return NSOrderedSame;
        }];
    if(recursively)
        for(NSTreeNode *child in [self _children])
            [child sortWithSortDescriptors:sortDescriptors recursively:YES];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@: %p, %@>", [self class], self, [self representedObject]];
}

@end
