/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSTreeController: a tree of objects, each with its children at a key
 * path, arranged as a tree of NSTreeNode (the root has no object, the
 * content's objects are its children), selected by index paths; in Core
 * Data's entity mode the content is fetched, as NSObjectController does.
 * Values follow what the reference does, measured on it.
 */
// MRC, as the rest of AppKit.

#import <AppKit/NSTreeController.h>
#import <AppKit/NSTreeNode.h>
#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@interface NSObjectController (NSTreeControllerPrivate)
- (void)_selectionWillChange;
- (void)_selectionDidChange;
- (BOOL)_isEntityMode;
- (id)_defaultNewObject;
- (void)_setContentWithoutKVO:(id)value;
@end

@interface NSTreeNode (NSTreeControllerPrivate)
- (NSMutableArray *)_children;
@end

/* A node of the arranged tree: its object's children are its children; a
   leaf by the leaf key path, or when it has none. */
@interface NSTreeControllerTreeNode : NSTreeNode {
    BOOL _leafKnown, _leaf;
}
@end

@implementation NSTreeControllerTreeNode

- (BOOL)isLeaf {
    if(_leafKnown)
        return _leaf;
    return [super isLeaf];
}

- (void)_setLeaf:(BOOL)leaf {
    _leafKnown = YES;
    _leaf = leaf;
}

@end

/* The root: no object, the content's objects as its children; it answers
   key paths through its children, as an array would (a table's column
   bound to arrangedObjects.title). */
@interface _NSControllerTreeProxy : NSTreeNode
@end

@implementation _NSControllerTreeProxy

- (id)valueForKey:(NSString *)key {
    if([key isEqualToString:@"childNodes"] || [key isEqualToString:@"mutableChildNodes"] ||
       [key isEqualToString:@"indexPath"] || [key isEqualToString:@"representedObject"])
        return [super valueForKey:key];
    return [[[self childNodes] valueForKey:@"representedObject"] valueForKey:key];
}

- (NSUInteger)count {
    return [[self childNodes] count];
}

/* Observed as an array is: a column's value binding watches the top
   objects. */
- (NSArray *)_topObjects {
    return [[self childNodes] valueForKey:@"representedObject"];
}

- (void)addObserver:(NSObject *)observer toObjectsAtIndexes:(NSIndexSet *)indexes forKeyPath:(NSString *)keyPath
            options:(NSKeyValueObservingOptions)options context:(void *)context {
    [[self _topObjects] addObserver:observer toObjectsAtIndexes:indexes forKeyPath:keyPath options:options context:context];
}

- (void)removeObserver:(NSObject *)observer fromObjectsAtIndexes:(NSIndexSet *)indexes forKeyPath:(NSString *)keyPath {
    [[self _topObjects] removeObserver:observer fromObjectsAtIndexes:indexes forKeyPath:keyPath];
}

- (id)objectAtIndex:(NSUInteger)index {
    return [[[self childNodes] objectAtIndex:index] representedObject];
}

@end

@implementation NSTreeController {
    NSString *_childrenKeyPath, *_countKeyPath, *_leafKeyPath;
    NSArray *_sortDescriptors;
    _NSControllerTreeProxy *_root;
    NSArray *_selectionIndexPaths;
    BOOL _avoidsEmptySelection, _selectsInsertedObjects, _preservesSelection, _alwaysUsesMultipleValuesMarker;
}

static void setTreeDefaults(NSTreeController *self_, BOOL *avoids, BOOL *selects, BOOL *preserves, BOOL *multiple) {
    *avoids = YES;
    *selects = YES;
    *preserves = YES;
    *multiple = NO;
}

- (instancetype)initWithContent:(id)content {
    if((self = [super initWithContent:nil]) != nil) {
        setTreeDefaults(self, &_avoidsEmptySelection, &_selectsInsertedObjects, &_preservesSelection, &_alwaysUsesMultipleValuesMarker);
        _selectionIndexPaths = [[NSArray alloc] init];
        [self setContent:content];
    }
    return self;
}

- (instancetype)init {
    return [self initWithContent:nil];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if((self = [super initWithCoder:coder]) != nil) {
        setTreeDefaults(self, &_avoidsEmptySelection, &_selectsInsertedObjects, &_preservesSelection, &_alwaysUsesMultipleValuesMarker);
        _selectionIndexPaths = [[NSArray alloc] init];
        if([coder allowsKeyedCoding]) {
            _childrenKeyPath = [[coder decodeObjectForKey:@"NSTreeContentChildrenKey"] copy];
            _countKeyPath = [[coder decodeObjectForKey:@"NSTreeContentCountKey"] copy];
            _leafKeyPath = [[coder decodeObjectForKey:@"NSTreeContentLeafKey"] copy];
            if([coder containsValueForKey:@"NSAvoidsEmptySelection"])
                _avoidsEmptySelection = [coder decodeBoolForKey:@"NSAvoidsEmptySelection"];
            if([coder containsValueForKey:@"NSPreservesSelection"])
                _preservesSelection = [coder decodeBoolForKey:@"NSPreservesSelection"];
            if([coder containsValueForKey:@"NSSelectsInsertedObjects"])
                _selectsInsertedObjects = [coder decodeBoolForKey:@"NSSelectsInsertedObjects"];
            _alwaysUsesMultipleValuesMarker = [coder decodeBoolForKey:@"NSAlwaysUsesMultipleValuesMarker"];
        }
        if([self content] == nil)
            [self _setContentWithoutKVO:[NSMutableArray array]];
        [self _rebuildTree];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_childrenKeyPath forKey:@"NSTreeContentChildrenKey"];
    [coder encodeObject:_countKeyPath forKey:@"NSTreeContentCountKey"];
    [coder encodeObject:_leafKeyPath forKey:@"NSTreeContentLeafKey"];
    [coder encodeBool:_avoidsEmptySelection forKey:@"NSAvoidsEmptySelection"];
    [coder encodeBool:_preservesSelection forKey:@"NSPreservesSelection"];
    [coder encodeBool:_selectsInsertedObjects forKey:@"NSSelectsInsertedObjects"];
    [coder encodeBool:_alwaysUsesMultipleValuesMarker forKey:@"NSAlwaysUsesMultipleValuesMarker"];
}

/* Nil once released: the superclass asks for the selection as it goes. */
- (void)dealloc {
    [_childrenKeyPath release];
    [_countKeyPath release];
    [_leafKeyPath release];
    [_sortDescriptors release];
    [_selectionIndexPaths release];
    _selectionIndexPaths = nil;
    [_root release];
    _root = nil;
    [super dealloc];
}

#pragma mark - Key paths and settings

- (NSString *)childrenKeyPath { return _childrenKeyPath; }
- (void)setChildrenKeyPath:(NSString *)path { [_childrenKeyPath release]; _childrenKeyPath = [path copy]; [self rearrangeObjects]; }
- (NSString *)countKeyPath { return _countKeyPath; }
- (void)setCountKeyPath:(NSString *)path { [_countKeyPath release]; _countKeyPath = [path copy]; [self rearrangeObjects]; }
- (NSString *)leafKeyPath { return _leafKeyPath; }
- (void)setLeafKeyPath:(NSString *)path { [_leafKeyPath release]; _leafKeyPath = [path copy]; [self rearrangeObjects]; }
- (void)setLeafPathKey:(NSString *)key { [self setLeafKeyPath:key]; }
- (NSString *)childrenKeyPathForNode:(NSTreeNode *)node { return _childrenKeyPath; }
- (NSString *)countKeyPathForNode:(NSTreeNode *)node { return _countKeyPath; }
- (NSString *)leafKeyPathForNode:(NSTreeNode *)node { return _leafKeyPath; }
- (NSArray *)sortDescriptors { return _sortDescriptors; }
- (void)setSortDescriptors:(NSArray *)descriptors { [_sortDescriptors release]; _sortDescriptors = [descriptors copy]; [self rearrangeObjects]; }
- (BOOL)alwaysUsesMultipleValuesMarker { return _alwaysUsesMultipleValuesMarker; }
- (void)setAlwaysUsesMultipleValuesMarker:(BOOL)flag { _alwaysUsesMultipleValuesMarker = flag; }
- (BOOL)avoidsEmptySelection { return _avoidsEmptySelection; }
- (void)setAvoidsEmptySelection:(BOOL)flag { _avoidsEmptySelection = flag; }
- (BOOL)selectsInsertedObjects { return _selectsInsertedObjects; }
- (void)setSelectsInsertedObjects:(BOOL)flag { _selectsInsertedObjects = flag; }
- (BOOL)preservesSelection { return _preservesSelection; }
- (void)setPreservesSelection:(BOOL)flag { _preservesSelection = flag; }

#pragma mark - The arranged tree

/* The content's top objects: an array, or one object. */
- (NSArray *)_topObjects {
    id content = [self content];

    if(content == nil)
        return [NSArray array];
    if([content isKindOfClass:[NSArray class]])
        return content;
    if([content isKindOfClass:[NSSet class]])
        return [content allObjects];
    return [NSArray arrayWithObject:content];
}

/* An object's children, by the children key path. */
- (NSArray *)_childrenOfObject:(id)object {
    id children;

    if(_childrenKeyPath == nil)
        return nil;
    children = [object valueForKeyPath:_childrenKeyPath];
    if([children isKindOfClass:[NSSet class]])
        return [children allObjects];
    return children;
}

- (void)_addNodesFor:(NSArray *)objects to:(NSTreeNode *)parent {
    for(id object in objects) {
        NSTreeControllerTreeNode *node = [[NSTreeControllerTreeNode alloc] initWithRepresentedObject:object];

        if(_leafKeyPath != nil)
            [node _setLeaf:[[object valueForKeyPath:_leafKeyPath] boolValue]];
        [[parent mutableChildNodes] addObject:node];
        [self _addNodesFor:[self _childrenOfObject:object] to:node];
        [node release];
    }
}

- (void)_rebuildTree {
    _NSControllerTreeProxy *root = [[_NSControllerTreeProxy alloc] initWithRepresentedObject:nil];

    [self _addNodesFor:[self _topObjects] to:root];
    if([_sortDescriptors count] > 0)
        [root sortWithSortDescriptors:_sortDescriptors recursively:YES];
    [_root release];
    _root = root;
}

- (id)arrangedObjects {
    if(_root == nil)
        [self _rebuildTree];
    return _root;
}

/* The selection kept to the paths that still lead somewhere. */
- (NSArray *)_validPaths:(NSArray *)paths {
    NSMutableArray *valid = [NSMutableArray array];

    for(NSIndexPath *path in paths)
        if([path length] > 0 && [[self arrangedObjects] descendantNodeAtIndexPath:path] != nil)
            [valid addObject:path];
    return valid;
}

- (void)rearrangeObjects {
    [self willChangeValueForKey:@"arrangedObjects"];
    [self _rebuildTree];
    [self didChangeValueForKey:@"arrangedObjects"];
    [self _setSelectionIndexPathsQuietly:[self _validPaths:_selectionIndexPaths]];
}

- (void)setContent:(id)content {
    [self willChangeValueForKey:@"arrangedObjects"];
    [super setContent:content != nil ? content : [NSMutableArray array]];
    [self _rebuildTree];
    [self didChangeValueForKey:@"arrangedObjects"];
    /* As the reference: the first object selected when there is one. */
    if(_avoidsEmptySelection && [[_root childNodes] count] > 0)
        [self setSelectionIndexPath:[NSIndexPath indexPathWithIndex:0]];
    else
        [self setSelectionIndexPaths:[NSArray array]];
}

#pragma mark - Selection

- (void)_setSelectionIndexPathsQuietly:(NSArray *)paths {
    [self _selectionWillChange];
    [self willChangeValueForKey:@"selectionIndexPaths"];
    [self willChangeValueForKey:@"selectionIndexPath"];
    [self willChangeValueForKey:@"selectedObjects"];
    [self willChangeValueForKey:@"selectedNodes"];
    [_selectionIndexPaths release];
    _selectionIndexPaths = [[paths sortedArrayUsingSelector:@selector(compare:)] copy];
    [self didChangeValueForKey:@"selectedNodes"];
    [self didChangeValueForKey:@"selectedObjects"];
    [self didChangeValueForKey:@"selectionIndexPath"];
    [self didChangeValueForKey:@"selectionIndexPaths"];
    [self _selectionDidChange];
}

- (NSArray *)selectionIndexPaths {
    return _selectionIndexPaths;
}

- (NSIndexPath *)selectionIndexPath {
    return [_selectionIndexPaths count] > 0 ? [_selectionIndexPaths objectAtIndex:0] : nil;
}

- (BOOL)setSelectionIndexPaths:(NSArray *)paths {
    NSArray *valid = [self _validPaths:paths ?: [NSArray array]];

    if(_avoidsEmptySelection && [valid count] == 0 && [paths count] > 0 && [[[self arrangedObjects] childNodes] count] > 0)
        valid = [NSArray arrayWithObject:[NSIndexPath indexPathWithIndex:0]];
    [self _setSelectionIndexPathsQuietly:valid];
    return YES;
}

- (BOOL)setSelectionIndexPath:(NSIndexPath *)path {
    return [self setSelectionIndexPaths:path != nil ? [NSArray arrayWithObject:path] : [NSArray array]];
}

- (BOOL)addSelectionIndexPaths:(NSArray *)paths {
    NSMutableArray *all = [NSMutableArray arrayWithArray:_selectionIndexPaths];

    for(NSIndexPath *path in paths)
        if(![all containsObject:path])
            [all addObject:path];
    return [self setSelectionIndexPaths:all];
}

- (BOOL)removeSelectionIndexPaths:(NSArray *)paths {
    NSMutableArray *all = [NSMutableArray arrayWithArray:_selectionIndexPaths];

    [all removeObjectsInArray:paths];
    return [self setSelectionIndexPaths:all];
}

- (NSArray *)selectedNodes {
    NSMutableArray *nodes = [NSMutableArray array];

    if(_selectionIndexPaths == nil || _root == nil)
        return nodes;

    for(NSIndexPath *path in _selectionIndexPaths) {
        NSTreeNode *node = [[self arrangedObjects] descendantNodeAtIndexPath:path];
        if(node != nil)
            [nodes addObject:node];
    }
    return nodes;
}

- (NSArray *)selectedObjects {
    return [[self selectedNodes] valueForKey:@"representedObject"];
}

#pragma mark - Changing the tree

/* The mutable collection a path's parent keeps its children in: the
   content's top objects, or an object's children. */
- (NSMutableArray *)_mutableChildrenAtParentPath:(NSIndexPath *)parentPath {
    if([parentPath length] == 0) {
        if(![[self content] isKindOfClass:[NSMutableArray class]])
            [self _setContentWithoutKVO:[NSMutableArray arrayWithArray:[self _topObjects]]];
        return [self content];
    }
    NSTreeNode *parent = [[self arrangedObjects] descendantNodeAtIndexPath:parentPath];
    if(parent == nil || _childrenKeyPath == nil)
        return nil;
    return [[parent representedObject] mutableArrayValueForKeyPath:_childrenKeyPath];
}

static NSIndexPath *parentOf(NSIndexPath *path) {
    return [path length] > 0 ? [path indexPathByRemovingLastIndex] : path;
}

- (void)insertObject:(id)object atArrangedObjectIndexPath:(NSIndexPath *)path {
    [self insertObjects:[NSArray arrayWithObject:object] atArrangedObjectIndexPaths:[NSArray arrayWithObject:path]];
}

- (void)insertObjects:(NSArray *)objects atArrangedObjectIndexPaths:(NSArray *)paths {
    NSUInteger i;

    [self willChangeValueForKey:@"content"];
    for(i = 0; i < [objects count] && i < [paths count]; i++) {
        NSIndexPath *path = [paths objectAtIndex:i];
        NSMutableArray *children = [self _mutableChildrenAtParentPath:parentOf(path)];
        NSUInteger index = [path indexAtPosition:[path length] - 1];

        [children insertObject:[objects objectAtIndex:i] atIndex:MIN(index, [children count])];
    }
    [self didChangeValueForKey:@"content"];
    [self willChangeValueForKey:@"arrangedObjects"];
    [self _rebuildTree];
    [self didChangeValueForKey:@"arrangedObjects"];
    if(_selectsInsertedObjects)
        [self setSelectionIndexPaths:paths];
    else
        [self _setSelectionIndexPathsQuietly:[self _validPaths:_selectionIndexPaths]];
}

- (void)removeObjectAtArrangedObjectIndexPath:(NSIndexPath *)path {
    [self removeObjectsAtArrangedObjectIndexPaths:[NSArray arrayWithObject:path]];
}

/* Removed from their parents (the deepest and last first, so the paths
   still lead to them); in entity mode deleted from the context. */
- (void)removeObjectsAtArrangedObjectIndexPaths:(NSArray *)paths {
    NSArray *ordered = [[paths sortedArrayUsingSelector:@selector(compare:)] reverseObjectEnumerator].allObjects;
    NSMutableArray *removed = [NSMutableArray array];

    [self willChangeValueForKey:@"content"];
    for(NSIndexPath *path in ordered) {
        NSMutableArray *children = [self _mutableChildrenAtParentPath:parentOf(path)];
        NSUInteger index = [path indexAtPosition:[path length] - 1];

        if(index < [children count]) {
            [removed addObject:[children objectAtIndex:index]];
            [children removeObjectAtIndex:index];
        }
    }
    [self didChangeValueForKey:@"content"];
    [self rearrangeObjects];
    if([self _isEntityMode])
        for(id object in removed)
            if([object isKindOfClass:[NSManagedObject class]])
                [[self managedObjectContext] deleteObject:object];
}

/* A node's object moves to its new place: out of its parent's children,
   into those of the parent at the new path. */
- (void)moveNode:(NSTreeNode *)node toIndexPath:(NSIndexPath *)path {
    [self moveNodes:[NSArray arrayWithObject:node] toIndexPath:path];
}

- (void)moveNodes:(NSArray *)nodes toIndexPath:(NSIndexPath *)startingPath {
    NSIndexPath *parentPath = parentOf(startingPath);
    NSUInteger index = [startingPath indexAtPosition:[startingPath length] - 1];
    NSTreeNode *target = [[self arrangedObjects] descendantNodeAtIndexPath:parentPath];
    id targetObject = [target representedObject];
    NSMutableArray *objects = [NSMutableArray array];

    [self willChangeValueForKey:@"content"];
    for(NSTreeNode *node in nodes) {
        NSIndexPath *from = [node indexPath];
        NSMutableArray *children = [self _mutableChildrenAtParentPath:parentOf(from)];
        id object = [[[node representedObject] retain] autorelease];
        NSUInteger at = [children indexOfObjectIdenticalTo:object];

        if(at == NSNotFound)
            continue;
        /* Taken from before the target in the same parent: one place less. */
        if([[parentOf(from) description] isEqual:[parentPath description]] && at < index)
            index--;
        [children removeObjectAtIndex:at];
        [objects addObject:object];
    }
    /* The parent's collection again: removing may have rebuilt nothing,
       but the root's array may have been replaced. */
    NSMutableArray *destination = [parentPath length] == 0 ? [self _mutableChildrenAtParentPath:parentPath]
                                                         : [targetObject mutableArrayValueForKeyPath:_childrenKeyPath];
    for(id object in objects)
        [destination insertObject:object atIndex:MIN(index++, [destination count])];
    [self didChangeValueForKey:@"content"];
    [self rearrangeObjects];
}

#pragma mark - Actions

- (BOOL)_selectionTakesChildren {
    NSArray *nodes = [self selectedNodes];

    return [nodes count] == 1 && ![[nodes objectAtIndex:0] isLeaf];
}

- (BOOL)canAdd { return [self isEditable]; }
- (BOOL)canInsert { return [self isEditable]; }
- (BOOL)canAddChild { return [self isEditable] && [self _selectionTakesChildren]; }
- (BOOL)canInsertChild { return [self isEditable] && [self _selectionTakesChildren]; }
- (BOOL)canRemove { return [self isEditable] && [_selectionIndexPaths count] > 0; }

- (id)_newTreeObject {
    return [[self newObject] autorelease];
}

/* After the selection, in its parent (at the end of the top level when
   nothing is selected). */
- (void)add:(id)sender {
    NSIndexPath *selected = [self selectionIndexPath];
    NSIndexPath *path;

    if(![self canAdd])
        return;
    if(selected == nil)
        path = [NSIndexPath indexPathWithIndex:[[[self arrangedObjects] childNodes] count]];
    else {
        NSTreeNode *parent = [[self arrangedObjects] descendantNodeAtIndexPath:parentOf(selected)];
        path = [parentOf(selected) indexPathByAddingIndex:[[parent childNodes] count]];
    }
    [self insertObject:[self _newTreeObject] atArrangedObjectIndexPath:path];
}

- (void)insert:(id)sender {
    if(![self canInsert])
        return;
    [self insertObject:[self _newTreeObject] atArrangedObjectIndexPath:[self selectionIndexPath] ?: [NSIndexPath indexPathWithIndex:0]];
}

- (void)addChild:(id)sender {
    NSTreeNode *node;

    if(![self canAddChild])
        return;
    node = [[self selectedNodes] objectAtIndex:0];
    [self insertObject:[self _newTreeObject] atArrangedObjectIndexPath:[[node indexPath] indexPathByAddingIndex:[[node childNodes] count]]];
}

- (void)insertChild:(id)sender {
    if(![self canInsertChild])
        return;
    [self insertObject:[self _newTreeObject] atArrangedObjectIndexPath:[[self selectionIndexPath] indexPathByAddingIndex:0]];
}

- (void)remove:(id)sender {
    if(![self canRemove])
        return;
    [self removeObjectsAtArrangedObjectIndexPaths:_selectionIndexPaths];
}

#pragma mark - Core Data (entity mode)

/* The fetched objects are the top level (the fetch predicate picks the
   roots, "parent == nil" say). */
- (void)_setFetchedObjects:(NSArray *)objects merge:(BOOL)merge {
    if(merge && [self content] != nil) {
        NSMutableArray *merged = [NSMutableArray arrayWithArray:[self _topObjects]];

        for(id object in objects)
            if(![merged containsObject:object])
                [merged addObject:object];
        [self setContent:merged];
        return;
    }
    [self setContent:[NSMutableArray arrayWithArray:objects]];
}

- (void)_contextObjectsDidChange:(NSNotification *)note {
    NSDictionary *info = [note userInfo];
    NSPredicate *predicate = [self fetchPredicate];
    NSMutableArray *top = [NSMutableArray arrayWithArray:[self _topObjects]];
    BOOL changed = NO;

    for(NSManagedObject *object in [info objectForKey:NSInsertedObjectsKey])
        if([[[object entity] name] isEqualToString:[self entityName]] && (predicate == nil || [predicate evaluateWithObject:object]) &&
           ![top containsObject:object]) {
            [top addObject:object];
            changed = YES;
        }
    for(NSManagedObject *object in [info objectForKey:NSDeletedObjectsKey])
        if([top containsObject:object]) {
            [top removeObject:object];
            changed = YES;
        }
    if(changed) {
        [self willChangeValueForKey:@"content"];
        [self _setContentWithoutKVO:top];
        [self didChangeValueForKey:@"content"];
    }
    if(changed || [info objectForKey:NSUpdatedObjectsKey] != nil)
        [self rearrangeObjects];
}

/* Not in entity mode: an array with a new object, as the array controller. */
- (void)prepareContent {
    if([self entityName] != nil) {
        [super prepareContent];
        return;
    }
    [self setContent:[NSMutableArray arrayWithObject:[self _newTreeObject]]];
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

@end
