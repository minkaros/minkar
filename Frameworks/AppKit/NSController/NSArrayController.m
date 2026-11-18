/* Copyright (c) 2007-2008 Johannes Fortmann

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <CoreData/CoreData.h>
#import <AppKit/NSArrayController.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>
#import <Foundation/NSIndexSet.h>
#import <Foundation/NSException.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSPredicate.h>
#import <Foundation/NSKeyValueObserving.h>
#import <Foundation/NSKeyValueCoding.h>
#import <Foundation/NSSortDescriptor.h>
#import <AppKit/NSRaise.h>
#import "NSObservationProxy.h"
#import "_NSControllerArray.h"

/*
 * Implementation Notes
 *
 * _arrangedObjects is a cache of a particular arrangement of the content as established by invoking arrangeObjects: with the content array
 *
 * Unless the controller is configured to automatically rearrange its objects when there are changes to the content, it's up to the implementation
 * to keep the _arrangedObjects cache and the _content in sync. This means that objects need to be added and removed from both.
 *
 * Selection Indexes reference into the arrangedObjects array.
 */

@interface NSObjectController(private)
-(id)_defaultNewObject;
-(void)_selectionWillChange;
-(void)_selectionDidChange;
@end

@interface NSArrayController(forwardRefs)
- (void)_setArrangedObjects:(id)value;
- (void)_setContentArray:(id)value;
- (id)_contentArray;
@end

@interface NSArray (private)
-(NSUInteger)_insertObject:(id)obj inArraySortedByDescriptors:(id)desc;
@end

@implementation NSArrayController

#pragma mark -
#pragma mark Initialization and Tear Down

+(void)initialize
{
   [self setKeys:[NSArray arrayWithObjects:@"content", nil] triggerChangeNotificationsForDependentKey:@"contentArray"];
   [self setKeys:[NSArray arrayWithObjects:@"content", @"contentArray", @"selectionIndexes", nil] triggerChangeNotificationsForDependentKey:@"selection"];
   [self setKeys:[NSArray arrayWithObjects:@"content", @"contentArray", @"selectionIndexes", @"selection", nil]triggerChangeNotificationsForDependentKey:@"selectionIndex"];
   [self setKeys:[NSArray arrayWithObjects:@"content", @"contentArray", @"selectionIndexes", @"selection", nil] triggerChangeNotificationsForDependentKey:@"selectedObjects"];
	
   [self setKeys:[NSArray arrayWithObjects:@"selectionIndexes", nil] triggerChangeNotificationsForDependentKey:@"canRemove"];
   [self setKeys:[NSArray arrayWithObjects:@"selectionIndexes", nil] triggerChangeNotificationsForDependentKey:@"canSelectNext"];
   [self setKeys:[NSArray arrayWithObjects:@"selectionIndexes", nil] triggerChangeNotificationsForDependentKey:@"canSelectPrevious"];
}

-(id)initWithCoder:(NSCoder*)coder
{
	if((self = [super initWithCoder:coder])) {
		_flags.avoidsEmptySelection = [coder decodeBoolForKey:@"NSAvoidsEmptySelection"];
		_flags.clearsFilterPredicateOnInsertion = [coder decodeBoolForKey:@"NSClearsFilterPredicateOnInsertion"];
		_flags.filterRestrictsInsertion = [coder decodeBoolForKey:@"NSFilterRestrictsInsertion"];
		_flags.preservesSelection = [coder decodeBoolForKey:@"NSPreservesSelection"];
		_flags.selectsInsertedObjects = [coder decodeBoolForKey:@"NSSelectsInsertedObjects"];
		_flags.alwaysUsesMultipleValuesMarker = [coder decodeBoolForKey:@"NSAlwaysUsesMultipleValuesMarker"];

		id declaredKeys=[coder decodeObjectForKey:@"NSDeclaredKeys"];
		_flags.automaticallyPreparesContent = [coder decodeBoolForKey:@"NSAutomaticallyPreparesContent"];

		/* In entity mode the content is prepared once the nib's bindings
		   have given the context (awakeFromNib). */
		if([self automaticallyPreparesContent] && [self entityName]==nil) {
			[self prepareContent];
		} else {
			[self _setContentArray:[NSMutableArray array]];
		}

	}
	return self;
}

/* Its defaults with no content: as the reference's init. */
- (id)init
{
	return [self initWithContent:nil];
}

- (id)initWithContent:(id)content
{
	if ((self = [super init])) {
		_flags.avoidsEmptySelection = YES;
		_flags.clearsFilterPredicateOnInsertion = YES;
		_flags.filterRestrictsInsertion = NO;
		_flags.preservesSelection = YES;
		_flags.selectsInsertedObjects = YES;
		_flags.alwaysUsesMultipleValuesMarker = NO;
		
		[self setAutomaticallyPreparesContent:NO];
		/* No content is an empty array, as on the reference. */
		[self _setContentArray:content!=nil?content:[NSMutableArray array]];
	}
	return self;
}

-(void)dealloc
{
	// Autorelease things, don't release them - [super dealloc] is doing 
	// some cleaning that need these things to be still alive
	[_selectionIndexes autorelease];
	[_sortDescriptors autorelease];
	[_filterPredicate autorelease];
	[_arrangedObjects autorelease];
	[super dealloc];
}

-(void)awakeFromNib
{
   [self _selectionWillChange];
   [self _selectionDidChange];
   if([self automaticallyPreparesContent] && [self entityName]!=nil)
    [self prepareContent];
}

#pragma mark -
#pragma mark Managing Sort Descriptors

- (void)setSortDescriptors:(NSArray *)value
{
	[_sortDescriptors release];
	_sortDescriptors=[value copy];
	
	if ([self automaticallyRearrangesObjects]) {
		[self rearrangeObjects];
	}
}

- (NSArray *)sortDescriptors
{
	return [[_sortDescriptors retain] autorelease];
}

#pragma mark -
#pragma mark Arranging Objects

- (void)_setArrangedObjects:(id)value
{
	[_arrangedObjects autorelease];
	_arrangedObjects = [[_NSControllerArray alloc] initWithArray:value];
}

-(NSArray*)arrangeObjects:(NSArray*)objects
{
	id sortedObjects=objects;
    
	if([self filterPredicate]) {
		sortedObjects=[sortedObjects filteredArrayUsingPredicate:[self filterPredicate]];
	}
	
	if([self sortDescriptors]) {
		sortedObjects=[sortedObjects sortedArrayUsingDescriptors:[self sortDescriptors]];
	}
	
	return sortedObjects;
}

-(id)arrangedObjects
{
	return [[_arrangedObjects retain] autorelease];
}

-(void)rearrangeObjects
{
	[self _selectionWillChange];
	
	// We may need to restore selection
	NSArray* selectedObjects = [self selectedObjects];

	// Arrange the content
	NSArray* arrangedObjects = [self arrangeObjects:[self _contentArray]];

	[self willChangeValueForKey: @"arrangedObjects"];

	// Cache the new arrangement
	[self _setArrangedObjects: arrangedObjects];

	[self didChangeValueForKey: @"arrangedObjects"];

	if ([self preservesSelection]) {
		// restore the selection
		[self setSelectedObjects: selectedObjects];
	}
	[self _selectionDidChange];
}

#pragma mark -
#pragma mark Managing Content

-(void)setContent:(id)value
{
	// Convert the value, if necessary, into an array so it's compatible with the array controller
	if(value!=nil && ![value isKindOfClass:[NSArray class]]) {
		value=[NSArray arrayWithObject:value];
    }
	
	[super setContent:[[value mutableCopy] autorelease]];
	
	if(_flags.clearsFilterPredicateOnInsertion) {
		[self setFilterPredicate:nil];
	}
	
	// Rearrange objects will preserve selection if needed
	[self rearrangeObjects];
	
}

- (void)_setContentArrayForMultipleSelection:(id)value 
{
	NSUnimplementedMethod();
}

- (void)_setContentArray:(id)value 
{
	[self setContent:value];
}

- (id)_contentArray
{
    id result=[self content];
    return result;
}

-(id)_contentSet
{
	return [NSSet setWithArray:_content];
}

-(void)_setContentSet:(NSSet *)set
{
	[self setContent:[set allObjects]];
}

-(void)setAutomaticallyPreparesContent:(BOOL)flag
{
	_flags.automaticallyPreparesContent = flag;
}

-(BOOL)automaticallyPreparesContent
{
	return _flags.automaticallyPreparesContent;
}

-(void)prepareContent
{
	if([self entityName]!=nil) {
		[super prepareContent];   /* entity mode: fetched at the run loop's next turn */
		return;
	}
	id array=[NSMutableArray array];
	[array addObject:[[self newObject] autorelease]];
	[self _setContentArray:array];
}

#pragma mark -
#pragma mark Core Data (entity mode)

/* The fetched objects: the content, or added to it when merging. */
-(void)_setFetchedObjects:(NSArray *)objects merge:(BOOL)merge
{
	if(merge && _content!=nil) {
		NSMutableArray *merged=[NSMutableArray arrayWithArray:_content];

		for(id object in objects)
			if(![merged containsObject:object])
				[merged addObject:object];
		[self setContent:merged];
		return;
	}
	[self setContent:objects];
}

static BOOL objectMatchesController(NSArrayController *controller,NSManagedObject *object)
{
	NSEntityDescription *entity=[object entity];
	NSPredicate *predicate=[controller fetchPredicate];

	while(entity!=nil && ![[entity name] isEqualToString:[controller entityName]])
		entity=[entity superentity];
	return entity!=nil && (predicate==nil || [predicate evaluateWithObject:object]);
}

static BOOL removingForContext;

/* The context's inserted objects of the entity (that the fetch predicate
   takes) join the content; its deleted ones leave it. */
-(void)_contextObjectsDidChange:(NSNotification *)note
{
	NSDictionary *info=[note userInfo];

	for(NSManagedObject *object in [info objectForKey:NSInsertedObjectsKey])
		if(objectMatchesController(self,object) && ![_content containsObject:object])
			[self addObject:object];
	removingForContext=YES;
	@try {
		for(NSManagedObject *object in [info objectForKey:NSDeletedObjectsKey])
			if([_arrangedObjects containsObject:object])
				[self removeObject:object];
			else if([_content containsObject:object]) {
				[self willChangeValueForKey:@"content"];
				[_content removeObject:object];
				[self didChangeValueForKey:@"content"];
			}
	}
	@finally {
		removingForContext=NO;
	}
}

/* Objects removed from an entity-mode controller are deleted from its
   context, as on the reference. */
-(void)_deleteRemovedObjects:(NSArray *)objects
{
	if(removingForContext || [self entityName]==nil || [self managedObjectContext]==nil)
		return;
	for(id object in objects)
		if([object isKindOfClass:[NSManagedObject class]] && [object managedObjectContext]==[self managedObjectContext])
			[[self managedObjectContext] deleteObject:object];
}

#pragma mark -
#pragma mark Selection Attributes

-(void)setAvoidsEmptySelection:(BOOL)value
{
	_flags.avoidsEmptySelection=value;
}

- (BOOL)avoidsEmptySelection
{
	return _flags.avoidsEmptySelection;
}

-(void)setPreservesSelection:(BOOL)value
{
	_flags.preservesSelection=value;
}

- (BOOL)preservesSelection
{
	return _flags.preservesSelection;
}

- (void)setAlwaysUsesMultipleValuesMarker:(BOOL)flag
{
	_flags.alwaysUsesMultipleValuesMarker = flag;
}

-(BOOL)alwaysUsesMultipleValuesMarker
{
	return _flags.alwaysUsesMultipleValuesMarker;
}

#pragma mark -
#pragma mark Managing Selections

-(NSUInteger)selectionIndex
{
	if (_selectionIndexes == nil) {
		return NSNotFound;
	} else {
		return [_selectionIndexes firstIndex];
	}
}

-(BOOL)setSelectionIndex:(NSUInteger)index
{
	return [self setSelectionIndexes:[NSIndexSet indexSetWithIndex:index]];
}

- (void)setSelectsInsertedObjects:(BOOL)flag
{
	_flags.selectsInsertedObjects = flag;
}

- (BOOL)selectsInsertedObjects
{
	return _flags.selectsInsertedObjects;
}

- (BOOL)setSelectionIndexes:(NSIndexSet *)value
{
	
	if(_flags.avoidsEmptySelection && [value count]==0 && [[self arrangedObjects] count]) {
		value=[NSIndexSet indexSetWithIndex:0];
	}
	
	if ([_selectionIndexes isEqualToIndexSet: value]) {
		// The selection isn't going to change
		return NO;
	}
	
	[self willChangeValueForKey:@"selectionIndexes"];
	[self _selectionWillChange];
	
	[_selectionIndexes release];
	_selectionIndexes = [value mutableCopy];
	[self _selectionDidChange];
	
	[self didChangeValueForKey:@"selectionIndexes"];
	return YES;
}

- (NSIndexSet *)selectionIndexes
{
    return [[_selectionIndexes retain] autorelease];
}

- (BOOL)addSelectionIndexes:(NSIndexSet *)indices
{
	if ([_selectionIndexes containsIndexes: indices] == YES) {
		// they already selected so the selection won't change
		return NO;
	}
	
	[self willChangeValueForKey: @"selectionIndexes"];
	[self _selectionWillChange];

	[_selectionIndexes addIndexes: indices];

	[self _selectionDidChange];
	[self didChangeValueForKey: @"selectionIndexes"];

	return YES;
}

- (BOOL)removeSelectionIndexes:(NSIndexSet *)indices
{	
	if ([_selectionIndexes containsIndexes: indices] == NO) {
		// they already deselected so the selection won't change
		return NO;
	}
	
	[self willChangeValueForKey: @"selectionIndexes"];
	[self _selectionWillChange];

	[_selectionIndexes removeIndexes: indices];

	[self _selectionDidChange];
	[self didChangeValueForKey: @"selectionIndexes"];
	return YES;
}

- (NSMutableIndexSet*)_indexesOfArrangedObjects:(NSArray*)objects
{
	id set=[NSMutableIndexSet indexSet];
	int i, count=[objects count];
	for(i=0; i<[objects count]; i++) {
		unsigned idx=[[self arrangedObjects] indexOfObject:[objects objectAtIndex:i]];
		if(idx!=NSNotFound) {
			[set addIndex:idx];
		}
	}
	return set;
}

- (BOOL)setSelectedObjects:(NSArray *)objects
{
	NSMutableIndexSet* set = [self _indexesOfArrangedObjects: objects];
	BOOL selectionChanged = [self setSelectionIndexes:set];
	return selectionChanged;
}

-(NSArray *)selectedObjects
{
	id idxs=[self selectionIndexes];
	if(idxs) {
		return [[self arrangedObjects] objectsAtIndexes:idxs];
	}
	return [NSArray array];
}

- (BOOL)addSelectedObjects:(NSArray *)objects
{
	NSMutableIndexSet* set = [self _indexesOfArrangedObjects: objects];
	BOOL selectionChanged = [self addSelectionIndexes: set];
	return selectionChanged;
}

- (BOOL)removeSelectedObjects:(NSArray *)objects
{
	NSMutableIndexSet* set = [self _indexesOfArrangedObjects: objects];
	BOOL selectionChanged = [self removeSelectionIndexes: set];
	return selectionChanged;
}

-(void)selectNext:(id)sender
{
	id idxs=[[[self selectionIndexes] mutableCopy] autorelease];
	if(!idxs) {
		[self setSelectionIndexes:[NSIndexSet indexSetWithIndex:0]];
        return;
    }
	[idxs shiftIndexesStartingAtIndex:0 by:1];
	
	if([idxs lastIndex]<[[self arrangedObjects] count]) {
		[self setSelectionIndexes:idxs];
	}
}

-(BOOL)canSelectNext
{
	id idxs=[[[self selectionIndexes] mutableCopy] autorelease];
	
	if(idxs && [idxs lastIndex]<[[self arrangedObjects] count]-1) {
		return YES;
	}
	
	return NO;
}

-(void)selectPrevious:(id)sender {
	id idxs=[[[self selectionIndexes] mutableCopy] autorelease];
	if(!idxs) {
		[self setSelectionIndexes:[NSIndexSet indexSetWithIndex:0]];
        return;
	}
	if([idxs firstIndex]>0) {
		[idxs shiftIndexesStartingAtIndex:0 by:-1];
		
		[self setSelectionIndexes:idxs];
	}
}

-(BOOL)canSelectPrevious
{
	id idxs=[[[self selectionIndexes] mutableCopy] autorelease];
	
	if(idxs && [idxs firstIndex]>0) {
		return YES;
	}
	return NO;
}

#pragma mark -
#pragma mark Inserting

-(BOOL)canInsert
{
	return [self isEditable];
}

-(void)insert:(id)sender
{
	if(![self canInsert]) {
		return;
	}
	
	id toAdd=nil;
	if([self automaticallyPreparesContent] || [self entityName]!=nil) {
		toAdd=[[self newObject] autorelease];
	} else {
		toAdd=[[self _defaultNewObject] autorelease];
	}
	
	[self addObject:toAdd];
}

#pragma mark -
#pragma mark Adding and Removing Objects

-(void)add:(id)sender {
	
	if(![self canAdd]) {
		return;
	}
	
	[self insert:sender];
}

- (void)addObject:(id)object {
	// Don't check canAdd here as this can be used programmatically to add objects
	[self insertObject: object atArrangedObjectIndex: [[self arrangedObjects] count]];
}

- (void)addObjects:(NSArray *)objects
{
	for (id object in objects) {
		[self addObject: object];
	}
}

-(void)insertObject:object atArrangedObjectIndex:(NSUInteger)index
{
	if(_flags.clearsFilterPredicateOnInsertion) {
		[self setFilterPredicate:nil];
	}
	
	[self willChangeValueForKey: @"content"];
	[self willChangeValueForKey: @"arrangedObjects"];
	[_content addObject:object];
	[_arrangedObjects insertObject: object atIndex: index];
	[self didChangeValueForKey: @"content"];
	[self didChangeValueForKey: @"arrangedObjects"];

	if (_flags.selectsInsertedObjects) {
		[self addSelectedObjects: [NSArray arrayWithObject: object]];
	}

	if ([self automaticallyRearrangesObjects]) {
		[self rearrangeObjects];
	}
	
}

-(void)insertObjects:(NSArray *)objects atArrangedObjectIndexes:(NSIndexSet *)indices
{
	if(_flags.clearsFilterPredicateOnInsertion) {
		[self setFilterPredicate:nil];
	}
	
	// Batch up the changes to avoid over observing
	[self willChangeValueForKey: @"content"];
	[self willChangeValueForKey: @"arrangedObjects"];

	int i = 0;
	NSUInteger index = [indices firstIndex];
	while (index != NSNotFound) {
		id object = [objects objectAtIndex: i++];
		[_content addObject:object];
		[_arrangedObjects insertObject: object atIndex: index];
		index = [indices indexGreaterThanIndex: index];
	}
	
	[self didChangeValueForKey: @"content"];
	[self didChangeValueForKey: @"arrangedObjects"];

	if (_flags.selectsInsertedObjects) {
		[self addSelectedObjects: objects];
	}
	
	if ([self automaticallyRearrangesObjects]) {
		[self rearrangeObjects];
	}
}

-(void)removeObjectAtArrangedObjectIndex:(NSUInteger)index
{
	[self removeSelectionIndexes: [NSIndexSet indexSetWithIndex: index]];
	
	id object = [[[_arrangedObjects objectAtIndex: index] retain] autorelease];
	[self willChangeValueForKey: @"content"];
	[self willChangeValueForKey: @"arrangedObjects"];
	[_arrangedObjects removeObjectAtIndex: index];
	[_content removeObject: object];
	[self didChangeValueForKey: @"content"];
	[self didChangeValueForKey: @"arrangedObjects"];
	[self _deleteRemovedObjects:[NSArray arrayWithObject:object]];

	if ([self automaticallyRearrangesObjects]) {
		[self rearrangeObjects];
	}	
}

-(void)removeObjectsAtArrangedObjectIndexes:(NSIndexSet*)indexes
{	
	[self removeSelectionIndexes: indexes];

	// Batch up the changes to avoid over observing
	[self willChangeValueForKey: @"content"];
	[self willChangeValueForKey: @"arrangedObjects"];

	// Work in reverse order to avoid messing up the array
	NSMutableArray *removed = [NSMutableArray array];
	NSUInteger index = [indexes lastIndex];
	while (index != NSNotFound) {
		id object = [_arrangedObjects objectAtIndex: index];
		[removed addObject: object];
		[_arrangedObjects removeObjectAtIndex: index];
		[_content removeObject: object];
		index = [indexes indexLessThanIndex: index];
	}

	[self didChangeValueForKey: @"content"];
	[self didChangeValueForKey: @"arrangedObjects"];
	[self _deleteRemovedObjects: removed];

	if ([self automaticallyRearrangesObjects]) {
		[self rearrangeObjects];
	}
}

-(void)remove:(id)sender
{
	if(![self canRemove]) {
		return;
    }
	
	[self removeObjectsAtArrangedObjectIndexes: [self selectionIndexes]];
}

-(void)removeObject:(id)object
{
	// Don't check canremove/editable here as this can be used programmatically to remove objects
	
	NSUInteger index=[_arrangedObjects indexOfObject:object];
	[self removeObjectAtArrangedObjectIndex: index];
}

- (void)removeObjects:(NSArray *)objects {
	// Don't check canRemove here as this can be used programmatically to remove objects
	
	NSMutableIndexSet* indexes = [NSMutableIndexSet indexSet];

	for (id object in objects) {
		NSUInteger index = [_arrangedObjects indexOfObject: object];
		[indexes addIndex: index];
	}
	[self removeObjectsAtArrangedObjectIndexes: indexes];
}

#pragma mark -
#pragma mark Filtering Content

-(void)setClearsFilterPredicateOnInsertion:(BOOL)flag
{
	_flags.clearsFilterPredicateOnInsertion = flag;
}

-(BOOL)clearsFilterPredicateOnInsertion
{
	return _flags.clearsFilterPredicateOnInsertion;
}

- (NSPredicate *)filterPredicate
{
   return [[_filterPredicate retain] autorelease];
}

- (void)setFilterPredicate:(NSPredicate *)value
{
	value=[value copy];
	[_filterPredicate release];
	_filterPredicate = value;
	[self rearrangeObjects];
}

#pragma mark -
#pragma mark Automatic Content Rearranging

- (void)setAutomaticallyRearrangesObjects:(BOOL)flag
{
	_flags.automaticallyRearrangesObjects = flag;
}

- (BOOL)automaticallyRearrangesObjects
{
	return _flags.automaticallyRearrangesObjects;
}

- (NSArray*)automaticRearrangementKeyPaths
{
	NSUnimplementedMethod();
	return nil;
}

- (void)didChangeArrangementCriteria
{
	NSUnimplementedMethod();
}

#pragma mark -
#pragma mark Debugging

- (NSString*)description
{
	return [NSString stringWithFormat: @"<%@ %x> content: %@ arrangedObjects: %@", [self class], self, _content, _arrangedObjects];
}

@end


