/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "PDFKitPrivate.h"

@implementation PDFOutline {
    NSMutableArray<PDFOutline *> *_children;
}

- (instancetype)init {
    if ((self = [super init]))
        _children = [NSMutableArray array];
    return self;
}

- (void)_setDocument:(PDFDocument *)document {
    _document = document;
    for (PDFOutline *child in _children)
        [child _setDocument:document];
}

- (NSUInteger)numberOfChildren {
    return _children.count;
}

- (NSUInteger)index {
    NSUInteger index = [_parent->_children indexOfObjectIdenticalTo:self];

    return (index == NSNotFound) ? 0 : index;
}

- (PDFOutline *)childAtIndex:(NSUInteger)index {
    return (index < _children.count) ? _children[index] : nil;
}

- (void)insertChild:(PDFOutline *)child atIndex:(NSUInteger)index {
    [child removeFromParent];
    [_children insertObject:child atIndex:MIN(index, _children.count)];
    child->_parent = self;
    [child _setDocument:_document];
}

- (void)removeFromParent {
    if (_parent != nil) {
        [_parent->_children removeObjectIdenticalTo:self];
        _parent = nil;
    }
}

/* An item with a destination and no action goes to it: its action is a
   GoTo, as on the reference. */
- (PDFAction *)action {
    if (_action == nil && _destination != nil)
        return [[PDFActionGoTo alloc] initWithDestination:_destination];
    return _action;
}

- (PDFDestination *)destination {
    if (_destination == nil && [_action isKindOfClass:[PDFActionGoTo class]])
        return [(PDFActionGoTo *)_action destination];
    return _destination;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p \"%@\" %lu children>", [self class], self, _label, (unsigned long)_children.count];
}

@end
