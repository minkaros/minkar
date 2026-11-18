/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <AppKit/NSObjectController.h>

@class NSIndexPath, NSTreeNode, NSArray, NSSortDescriptor;

/* A controller of a tree of objects (API 10.4, nodes since 10.5): each
   object's children at childrenKeyPath, its arranged objects a tree of
   NSTreeNode, selected by index paths. */
@interface NSTreeController : NSObjectController <NSCoding>

@property (copy) NSString *childrenKeyPath;
@property (copy) NSString *countKeyPath;
@property (copy) NSString *leafKeyPath;
- (void)setLeafPathKey:(NSString *)key;   /* the older name */
@property (copy) NSArray *sortDescriptors;
@property BOOL alwaysUsesMultipleValuesMarker;
@property BOOL avoidsEmptySelection;
@property BOOL selectsInsertedObjects;
@property BOOL preservesSelection;

/* The root of the arranged tree: no represented object, the content's
   objects as its children. */
- (id)arrangedObjects;
- (void)rearrangeObjects;

- (void)insertObject:(id)object atArrangedObjectIndexPath:(NSIndexPath *)indexPath;
- (void)insertObjects:(NSArray *)objects atArrangedObjectIndexPaths:(NSArray *)indexPaths;
- (void)removeObjectAtArrangedObjectIndexPath:(NSIndexPath *)indexPath;
- (void)removeObjectsAtArrangedObjectIndexPaths:(NSArray *)indexPaths;
- (void)moveNode:(NSTreeNode *)node toIndexPath:(NSIndexPath *)indexPath;
- (void)moveNodes:(NSArray *)nodes toIndexPath:(NSIndexPath *)startingIndexPath;

- (NSArray *)selectedObjects;
@property (readonly, copy) NSArray *selectedNodes;
- (NSIndexPath *)selectionIndexPath;
- (NSArray *)selectionIndexPaths;
- (BOOL)setSelectionIndexPath:(NSIndexPath *)indexPath;
- (BOOL)setSelectionIndexPaths:(NSArray *)indexPaths;
- (BOOL)addSelectionIndexPaths:(NSArray *)indexPaths;
- (BOOL)removeSelectionIndexPaths:(NSArray *)indexPaths;

- (BOOL)canInsert;
- (BOOL)canAddChild;
- (BOOL)canInsertChild;
- (void)add:(id)sender;
- (void)addChild:(id)sender;
- (void)insert:(id)sender;
- (void)insertChild:(id)sender;
- (void)remove:(id)sender;

- (NSString *)childrenKeyPathForNode:(NSTreeNode *)node;
- (NSString *)countKeyPathForNode:(NSTreeNode *)node;
- (NSString *)leafKeyPathForNode:(NSTreeNode *)node;

@end
