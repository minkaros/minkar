/* Copyright (c) 2006-2007 Christopher J. W. Lloyd, 2007-2008 Johannes Fortmann

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <AppKit/NSObjectController.h>
#import <Foundation/NSString.h>
#import <Foundation/NSKeyedArchiver.h>
#import <Foundation/NSKeyValueObserving.h>
#import <Foundation/NSSet.h>
#import "NSControllerSelectionProxy.h"
#import "NSObservationProxy.h"
#import "../CoreData_/_NSManagedProxy.h"
#import <CoreData/CoreData.h>
#import <objc/runtime.h>

@interface NSObjectController(forward)
-(void)_selectionWillChange;
-(void)_selectionDidChange;

@end

@implementation NSObjectController
+(void)initialize {
   [self setKeys:[NSArray arrayWithObjects:@"editable", nil] triggerChangeNotificationsForDependentKey:@"canAdd"];
   [self setKeys:[NSArray arrayWithObjects:@"editable", nil] triggerChangeNotificationsForDependentKey:@"canInsert"];
   [self setKeys:[NSArray arrayWithObjects:@"editable", @"selection", nil] triggerChangeNotificationsForDependentKey:@"canRemove"];
   [self setKeys:[NSArray arrayWithObjects:@"content", nil] triggerChangeNotificationsForDependentKey:@"contentObject"];
}

/* Editable, of NSMutableDictionary objects, no content: as the
   reference's init (initWithContent:nil). Not through initWithContent:,
   which an array controller overrides by calling init. */
static void setUpObjectController(NSObjectController *self_, NSString **objectClassName, BOOL *editable,
                                  BOOL *prepares, NSCountedSet **observedKeys, id *selection)
{
	*objectClassName=@"NSMutableDictionary";
	*editable=YES;
	*prepares=NO;
	*observedKeys=[[NSCountedSet alloc] init];
	*selection=[[NSControllerSelectionProxy alloc] initWithController:self_];
}

- (id)init
{
	if ((self=[super init]))
		setUpObjectController(self,&_objectClassName,&_editable,&_automaticallyPreparesContent,&_observedKeys,&_selection);
	return self;
}

- (id)initWithContent:(id)content
{
	if ((self=[super init])) {
		setUpObjectController(self,&_objectClassName,&_editable,&_automaticallyPreparesContent,&_observedKeys,&_selection);
		_content=[content retain];
	}

	return self;
}

-initWithCoder:(NSCoder*)coder {
   if((self=[super init])) {
   
    _objectClassName=[[coder decodeObjectForKey:@"NSObjectClassName"] copy];
    if(_objectClassName==nil)
     _objectClassName=@"NSMutableDictionary";
     
    _editable = [coder decodeBoolForKey:@"NSEditable"];
    _automaticallyPreparesContent = [coder decodeBoolForKey:@"NSAutomaticallyPreparesContent"];
    /* Entity mode from the nib: the entity and fetch predicate the
       managed proxy keeps; the context comes by its binding. */
    if([coder decodeBoolForKey:@"_NSIsUsingManagedProxy"]){
     _NSManagedProxy *proxy=[coder decodeObjectForKey:@"_NSManagedProxy"];
     NSPredicate     *predicate=nil;

     [self setEntityName:[proxy entityName]];
     @try { predicate=[[proxy valueForKey:@"_fetchRequest"] predicate]; } @catch(NSException *e) { predicate=nil; }
     [self setFetchPredicate:predicate];
    }
    /* Older nibs keep the entity and fetch predicate on the controller
       itself, with no managed proxy. */
    else if([coder containsValueForKey:@"NSEntityName"] || [coder containsValueForKey:@"NSFetchPredicate"]){
     NSString    *entityName=[coder decodeObjectForKey:@"NSEntityName"];
     NSPredicate *predicate=[coder decodeObjectForKey:@"NSFetchPredicate"];

     if([entityName isKindOfClass:[NSString class]])
      [self setEntityName:entityName];
     if([predicate isKindOfClass:[NSPredicate class]])
      [self setFetchPredicate:predicate];
    }
    if([coder containsValueForKey:@"NSUsesLazyFetching"])
     [self setUsesLazyFetching:[coder decodeBoolForKey:@"NSUsesLazyFetching"]];
     
    _observedKeys=[[NSCountedSet alloc] init];
    _selection=[[NSControllerSelectionProxy alloc] initWithController:self];
   }
   return self;
}

#pragma mark - Core Data (entity mode)

/* The managed object context, entity, fetch predicate and lazy fetching
   of a controller in entity mode, kept beside the object. */
static char contextKey, entityNameKey, fetchPredicateKey, lazyKey, watchingKey;

-(NSManagedObjectContext *)managedObjectContext {
   return objc_getAssociatedObject(self,&contextKey);
}

-(void)setManagedObjectContext:(NSManagedObjectContext *)context {
   NSManagedObjectContext *old=[self managedObjectContext];

   if(context==old)
    return;
   if(old!=nil && objc_getAssociatedObject(self,&watchingKey)!=nil){
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSManagedObjectContextObjectsDidChangeNotification object:old];
    objc_setAssociatedObject(self,&watchingKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
   }
   objc_setAssociatedObject(self,&contextKey,context,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

-(NSString *)entityName {
   return objc_getAssociatedObject(self,&entityNameKey);
}

-(void)setEntityName:(NSString *)name {
   objc_setAssociatedObject(self,&entityNameKey,name,OBJC_ASSOCIATION_COPY_NONATOMIC);
}

-(NSPredicate *)fetchPredicate {
   return objc_getAssociatedObject(self,&fetchPredicateKey);
}

-(void)setFetchPredicate:(NSPredicate *)predicate {
   objc_setAssociatedObject(self,&fetchPredicateKey,predicate,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

-(BOOL)usesLazyFetching {
   return [objc_getAssociatedObject(self,&lazyKey) boolValue];
}

-(void)setUsesLazyFetching:(BOOL)flag {
   objc_setAssociatedObject(self,&lazyKey,[NSNumber numberWithBool:flag],OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

-(BOOL)_isEntityMode {
   return [self entityName]!=nil && [self managedObjectContext]!=nil;
}

/* The request a fetch makes: the entity in the context's model (which
   must have it) and the fetch predicate. */
-(NSFetchRequest *)defaultFetchRequest {
   NSManagedObjectContext *context=[self managedObjectContext];
   NSEntityDescription    *entity;
   NSFetchRequest         *request;

   if([self entityName]==nil || context==nil)
    return nil;
   entity=[NSEntityDescription entityForName:[self entityName] inManagedObjectContext:context];
   if(entity==nil)
    [NSException raise:NSInternalInconsistencyException format:@"Cannot perform operation without a managed object context with an entity named '%@'",[self entityName]];
   request=[[[NSFetchRequest alloc] init] autorelease];
   [request setEntity:entity];
   [request setPredicate:[self fetchPredicate]];
   return request;
}

/* The fetched objects as the content: an object controller's is the
   first of them (kept when merging). */
-(void)_setFetchedObjects:(NSArray *)objects merge:(BOOL)merge {
   if(merge && [self content]!=nil)
    return;
   [self setContent:[objects count]>0?[objects objectAtIndex:0]:nil];
}

-(BOOL)fetchWithRequest:(NSFetchRequest *)request merge:(BOOL)merge error:(NSError **)error {
   NSArray *objects;

   if(request==nil)
    request=[self defaultFetchRequest];
   if(request==nil)
    return NO;
   objects=[[self managedObjectContext] executeFetchRequest:request error:error];
   if(objects==nil)
    return NO;
   [self _setFetchedObjects:objects merge:merge];
   return YES;
}

-(void)fetch:(id)sender {
   [self fetchWithRequest:nil merge:NO error:NULL];
}

/* Objects inserted into or deleted from the context, while prepared: an
   object controller drops its content when it is deleted (an array
   controller adds and removes; it overrides this). */
-(void)_contextObjectsDidChange:(NSNotification *)note {
   NSSet *deleted=[[note userInfo] objectForKey:NSDeletedObjectsKey];

   if([self content]!=nil && [deleted containsObject:[self content]])
    [self setContent:nil];
}

-(void)_watchContext {
   NSManagedObjectContext *context=[self managedObjectContext];

   if(context==nil || objc_getAssociatedObject(self,&watchingKey)!=nil)
    return;
   [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_contextObjectsDidChange:)
     name:NSManagedObjectContextObjectsDidChangeNotification object:context];
   objc_setAssociatedObject(self,&watchingKey,context,OBJC_ASSOCIATION_ASSIGN);
}

-(void)_fetchPreparedContent {
   if([self _isEntityMode])
    [self fetchWithRequest:nil merge:NO error:NULL];
}

/* In entity mode the content is fetched at the run loop's next turn, as
   the reference does, and follows the context's changes from then on;
   otherwise a new object. */
-(void)prepareContent {
   if([self _isEntityMode]){
    [self _watchContext];
    [self performSelector:@selector(_fetchPreparedContent) withObject:nil afterDelay:0];
    return;
   }
   if([self content]==nil && [self entityName]==nil)
    [self setContent:[[self newObject] autorelease]];
}

-(void)awakeFromNib {
   if([self automaticallyPreparesContent]){
    [self prepareContent];
   }
}


- (id)content {
    return [[_content retain] autorelease];
}

- (void)setContent:(id)value {
   if(value!=_content)
   {
       [self _selectionWillChange];
       
        [_content release];
        _content = [value retain];
       
		[self _selectionDidChange];
    }
}

-(void)_setContentWithoutKVO:(id)value
{
   if (_content != value) {
      [_content release];
      _content = [value retain];
   }
}


-(NSArray *)selectedObjects
{
	NSArray *result=[NSArray arrayWithObject:_content];
    return result;
}

-(id)selection
{
	return _selection;
}

-(id)_defaultNewObject
{
	return [[NSClassFromString(_objectClassName) alloc] init];
}

/* In entity mode a new object of the entity, inserted into the context. */
-(id)newObject
{
	if([self _isEntityMode])
	 return [[NSEntityDescription insertNewObjectForEntityForName:[self entityName] inManagedObjectContext:[self managedObjectContext]] retain];
	return [self _defaultNewObject];
}

-(void)_selectionWillChange
{
   [self willChangeValueForKey:@"selection"];
   [_selection controllerWillChange];
}


-(void)_selectionDidChange
{
   [_selection controllerDidChange];
	[self didChangeValueForKey:@"selection"];	
}
 
-(void)dealloc
{
	/* A pending fetch keeps the controller alive (performSelector retains
	   it), so there is none to cancel here. */
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	[_selection release];
	[_objectClassName release];
	[_content release];
    [_observedKeys release];
	[super dealloc];
}


-(BOOL)canAdd;
{
	return [self isEditable];
}

-(BOOL)canRemove;
{
	return [self isEditable] && [[self selectedObjects] count];
}

- (BOOL)isEditable
{
	return _editable;
}

-(void)setEditable:(BOOL)value
{
	_editable=value;
}

- (BOOL)automaticallyPreparesContent {
    return _automaticallyPreparesContent;
}

- (void)setAutomaticallyPreparesContent:(BOOL)value {
	_automaticallyPreparesContent = value;
}

- (void)addObserver:(NSObject *)observer forKeyPath:(NSString *)keyPath options:(NSKeyValueObservingOptions)options context:(void *)context
{
   [_observedKeys addObject:keyPath];
   [super addObserver:observer forKeyPath:keyPath options:options context:context];
}

- (void)removeObserver:(NSObject *)observer forKeyPath:(NSString *)keyPath
{
   [_observedKeys removeObject:keyPath];
   [super removeObserver:observer forKeyPath:keyPath];
}

-(id)_observedKeys
{
   return _observedKeys;
}

-(id)_contentObject {
   return [self content];
}

-(void)_setContentObject:(id)val {
   [self setContent:val];
}

-(Class)objectClass {
   return NSClassFromString(_objectClassName);
}

-(void)setObjectClass:(Class)cls {
   NSString *name=[NSStringFromClass(cls) copy];
   [_objectClassName release];
   _objectClassName=name;
}

@end
