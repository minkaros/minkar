/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

/* Metadata queries without an index: when a query starts, its scopes are
 * walked on a background queue, each file's attributes read (from the file
 * system, and its tags from their extended attribute; hidden files are
 * left out, as an index leaves them), and the files that
 * match the predicate are the results. They are delivered on the query's
 * operation queue, or on the thread that started it, with
 * NSMetadataQueryDidFinishGatheringNotification. Live updates are not
 * made. */

NSString *const NSMetadataItemFSNameKey=@"kMDItemFSName";
NSString *const NSMetadataItemDisplayNameKey=@"kMDItemDisplayName";
NSString *const NSMetadataItemURLKey=@"kMDItemURL";
NSString *const NSMetadataItemPathKey=@"kMDItemPath";
NSString *const NSMetadataItemFSSizeKey=@"kMDItemFSSize";
NSString *const NSMetadataItemFSCreationDateKey=@"kMDItemFSCreationDate";
NSString *const NSMetadataItemFSContentChangeDateKey=@"kMDItemFSContentChangeDate";
NSString *const NSMetadataItemContentTypeKey=@"kMDItemContentType";
NSString *const NSMetadataItemContentTypeTreeKey=@"kMDItemContentTypeTree";

NSString *const NSMetadataQueryDidStartGatheringNotification=@"NSMetadataQueryDidStartGatheringNotification";
NSString *const NSMetadataQueryGatheringProgressNotification=@"NSMetadataQueryGatheringProgressNotification";
NSString *const NSMetadataQueryDidFinishGatheringNotification=@"NSMetadataQueryDidFinishGatheringNotification";
NSString *const NSMetadataQueryDidUpdateNotification=@"NSMetadataQueryDidUpdateNotification";

NSString *const NSMetadataQueryUpdateAddedItemsKey=@"kMDQueryUpdateAddedItems";
NSString *const NSMetadataQueryUpdateChangedItemsKey=@"kMDQueryUpdateChangedItems";
NSString *const NSMetadataQueryUpdateRemovedItemsKey=@"kMDQueryUpdateRemovedItems";
NSString *const NSMetadataQueryResultContentRelevanceAttribute=@"kMDQueryResultContentRelevance";

NSString *const NSMetadataQueryUserHomeScope=@"kMDQueryScopeHome";
NSString *const NSMetadataQueryLocalComputerScope=@"kMDQueryScopeComputer";
NSString *const NSMetadataQueryNetworkScope=@"kMDQueryScopeNetwork";
NSString *const NSMetadataQueryIndexedLocalComputerScope=@"kMDQueryScopeComputerIndexed";
NSString *const NSMetadataQueryIndexedNetworkScope=@"kMDQueryScopeNetworkIndexed";

static NSString *const userTagsKey=@"kMDItemUserTags";

@implementation NSMetadataItem

-(instancetype)initWithURL:(NSURL *)url {
   NSArray *keys=[NSArray arrayWithObjects:NSURLNameKey,NSURLFileSizeKey,NSURLCreationDateKey,NSURLContentModificationDateKey,NSURLTagNamesKey,NSURLTypeIdentifierKey,NSURLIsDirectoryKey,nil];
   NSDictionary        *values;
   NSMutableDictionary *attributes=[NSMutableDictionary dictionary];
   id                   value;

   if(![url isFileURL] || [url path]==nil){
    [self release];
    return nil;
   }
   values=[url resourceValuesForKeys:keys error:NULL];
   if(values==nil){
    [self release];
    return nil;
   }

   _URL=[url retain];
   [attributes setObject:[url path] forKey:NSMetadataItemPathKey];
   [attributes setObject:url forKey:NSMetadataItemURLKey];
   [attributes setObject:[[url path] lastPathComponent] forKey:NSMetadataItemFSNameKey];
   [attributes setObject:[[url path] lastPathComponent] forKey:NSMetadataItemDisplayNameKey];
   if((value=[values objectForKey:NSURLFileSizeKey])!=nil && ![[values objectForKey:NSURLIsDirectoryKey] boolValue])
    [attributes setObject:value forKey:NSMetadataItemFSSizeKey];
   if((value=[values objectForKey:NSURLCreationDateKey])!=nil)
    [attributes setObject:value forKey:NSMetadataItemFSCreationDateKey];
   if((value=[values objectForKey:NSURLContentModificationDateKey])!=nil){
    [attributes setObject:value forKey:NSMetadataItemFSContentChangeDateKey];
    [attributes setObject:value forKey:@"kMDItemContentModificationDate"];
   }
   if((value=[values objectForKey:NSURLTypeIdentifierKey])!=nil)
    [attributes setObject:value forKey:NSMetadataItemContentTypeKey];
   if([(value=[values objectForKey:NSURLTagNamesKey]) count]>0)
    [attributes setObject:value forKey:userTagsKey];
   _attributes=[attributes copy];
   return self;
}

-(void)dealloc {
   [_URL release];
   [_attributes release];
   [super dealloc];
}

-valueForAttribute:(NSString *)key {
   return [_attributes objectForKey:key];
}

-(NSDictionary *)valuesForAttributes:(NSArray *)keys {
   NSMutableDictionary *result=[NSMutableDictionary dictionary];

   for(NSString *key in keys){
    id value=[_attributes objectForKey:key];

    if(value!=nil)
     [result setObject:value forKey:key];
   }
   return result;
}

-(NSArray *)attributes {
   return [_attributes allKeys];
}

// Predicates and sort descriptors read attributes by their names.
-valueForKey:(NSString *)key {
   return [_attributes objectForKey:key];
}

-(NSString *)description {
   return [NSString stringWithFormat:@"<%@ %p %@>",[self class],self,[_URL path]];
}

@end

@implementation NSMetadataQueryAttributeValueTuple

-(instancetype)_initWithAttribute:(NSString *)attribute value:value count:(NSUInteger)count {
   _attribute=[attribute copy];
   _value=[value retain];
   _count=count;
   return self;
}

-(void)dealloc {
   [_attribute release];
   [_value release];
   [super dealloc];
}

-(NSString *)attribute {
   return _attribute;
}

-value {
   return _value;
}

-(NSUInteger)count {
   return _count;
}

-(NSString *)description {
   return [NSString stringWithFormat:@"<%@ %p %@=%@ (%lu)>",[self class],self,_attribute,_value,(unsigned long)_count];
}

@end

@implementation NSMetadataQueryResultGroup

-(instancetype)_initWithAttribute:(NSString *)attribute value:value results:(NSArray *)results {
   _attribute=[attribute copy];
   _value=[value retain];
   _results=[results copy];
   return self;
}

-(void)dealloc {
   [_attribute release];
   [_value release];
   [_results release];
   [super dealloc];
}

-(NSString *)attribute {
   return _attribute;
}

-value {
   return _value;
}

-(NSArray *)subgroups {
   return nil;
}

-(NSUInteger)resultCount {
   return [_results count];
}

-resultAtIndex:(NSUInteger)index {
   return [_results objectAtIndex:index];
}

-(NSArray *)results {
   return _results;
}

@end

// A comparison on an attribute with several values (tags) holds when one
// of them makes it hold, as in a metadata query.
static BOOL itemMatches(NSPredicate *predicate,NSMetadataItem *item) {
   if(predicate==nil)
    return YES;

   if([predicate isKindOfClass:[NSCompoundPredicate class]]){
    NSCompoundPredicate *compound=(NSCompoundPredicate *)predicate;

    switch([compound compoundPredicateType]){
     case NSNotPredicateType:
      return !itemMatches([[compound subpredicates] objectAtIndex:0],item);
     case NSAndPredicateType:
      for(NSPredicate *sub in [compound subpredicates])
       if(!itemMatches(sub,item))
        return NO;
      return YES;
     case NSOrPredicateType:
      for(NSPredicate *sub in [compound subpredicates])
       if(itemMatches(sub,item))
        return YES;
      return NO;
    }
   }

   if([predicate isKindOfClass:[NSComparisonPredicate class]]){
    NSComparisonPredicate *comparison=(NSComparisonPredicate *)predicate;
    NSExpression          *left=[comparison leftExpression];

    if([comparison comparisonPredicateModifier]==NSDirectPredicateModifier &&
       [comparison predicateOperatorType]!=NSCustomSelectorPredicateOperatorType &&
       [left expressionType]==NSKeyPathExpressionType &&
       [[item valueForAttribute:[left keyPath]] isKindOfClass:[NSArray class]]){
     NSPredicate *any=[NSComparisonPredicate predicateWithLeftExpression:left rightExpression:[comparison rightExpression]
      modifier:NSAnyPredicateModifier type:[comparison predicateOperatorType] options:[comparison options]];

     return [any evaluateWithObject:item];
    }
   }

   @try {
    return [predicate evaluateWithObject:item];
   }
   @catch(NSException *exception){
    return NO;
   }
}

@implementation NSMetadataQuery

-init {
   _sortDescriptors=[[NSArray alloc] init];
   _valueListAttributes=[[NSArray alloc] init];
   _searchScopes=[[NSArray alloc] init];
   _results=[[NSArray alloc] init];
   _valueLists=[[NSDictionary alloc] init];
   _notificationBatchingInterval=1.0;
   return self;
}

-(void)dealloc {
   [_predicate release];
   [_sortDescriptors release];
   [_valueListAttributes release];
   [_groupingAttributes release];
   [_searchScopes release];
   [_searchItems release];
   [_operationQueue release];
   [_results release];
   [_valueLists release];
   [_thread release];
   [super dealloc];
}

-delegate { return _delegate; }
-(void)setDelegate:delegate { _delegate=delegate; }

#define COPY_PROPERTY(getter,setter,ivar) \
-getter { return ivar; } \
-(void)setter:value { value=[value copy]; [ivar release]; ivar=value; }

COPY_PROPERTY(predicate,setPredicate,_predicate)
COPY_PROPERTY(sortDescriptors,setSortDescriptors,_sortDescriptors)
COPY_PROPERTY(valueListAttributes,setValueListAttributes,_valueListAttributes)
COPY_PROPERTY(groupingAttributes,setGroupingAttributes,_groupingAttributes)
COPY_PROPERTY(searchScopes,setSearchScopes,_searchScopes)
COPY_PROPERTY(searchItems,setSearchItems,_searchItems)

-(NSOperationQueue *)operationQueue {
   return _operationQueue;
}

-(void)setOperationQueue:(NSOperationQueue *)queue {
   queue=[queue retain];
   [_operationQueue release];
   _operationQueue=queue;
}

-(NSTimeInterval)notificationBatchingInterval {
   return _notificationBatchingInterval;
}

-(void)setNotificationBatchingInterval:(NSTimeInterval)interval {
   _notificationBatchingInterval=interval;
}

-(BOOL)isStarted { return _started; }
-(BOOL)isGathering { return _gathering; }
-(BOOL)isStopped { return _stopped; }

-(void)disableUpdates { _disabled++; }
-(void)enableUpdates { if(_disabled>0) _disabled--; }

// The folders a scope names; no scope at all is the whole computer.
static NSArray *rootsForScopes(NSArray *scopes) {
   NSMutableArray *result=[NSMutableArray array];

   if([scopes count]==0)
    scopes=[NSArray arrayWithObject:NSMetadataQueryLocalComputerScope];
   for(id scope in scopes){
    if([scope isKindOfClass:[NSURL class]])
     [result addObject:[scope path]];
    else if([scope isEqual:NSMetadataQueryUserHomeScope])
     [result addObject:NSHomeDirectory()];
    else if([scope isEqual:NSMetadataQueryLocalComputerScope] || [scope isEqual:NSMetadataQueryIndexedLocalComputerScope])
     [result addObject:@"/"];
    else if([scope isKindOfClass:[NSString class]] && [scope isAbsolutePath])
     [result addObject:scope];
   }
   return result;
}

// Not walked from /: devices, the system, and what is not users' files.
static BOOL skippedFromRoot(NSString *path) {
   static NSSet *skipped;

   if(skipped==nil)
    skipped=[[NSSet alloc] initWithObjects:@"/dev",@"/System",@"/private",@"/usr",@"/bin",@"/sbin",@"/cores",@"/Volumes",@"/.vol",@"/etc",@"/var",@"/tmp",nil];
   return [skipped containsObject:path];
}

static NSArray *gather(NSArray *roots,NSPredicate *predicate) {
   NSMutableArray *result=[NSMutableArray array];
   NSMutableSet   *seen=[NSMutableSet set];
   NSFileManager  *manager=[[[NSFileManager alloc] init] autorelease];

   for(NSString *root in roots){
    NSDirectoryEnumerator *walk;
    NSURL                 *url;
    BOOL                   fromRoot=[root isEqualToString:@"/"];

    walk=[manager enumeratorAtURL:[NSURL fileURLWithPath:root] includingPropertiesForKeys:nil options:NSDirectoryEnumerationSkipsHiddenFiles errorHandler:^BOOL(NSURL *u,NSError *e){ return YES; }];
    while((url=[walk nextObject])!=nil){
     @autoreleasepool {
      NSString       *path=[url path];
      NSMetadataItem *item;

      if(fromRoot && skippedFromRoot(path)){
       [walk skipDescendants];
       continue;
      }
      if([seen containsObject:path])
       continue;
      [seen addObject:path];
      item=[[NSMetadataItem alloc] initWithURL:url];
      if(item!=nil && itemMatches(predicate,item))
       [result addObject:item];
      [item release];
     }
    }
   }
   return result;
}

// The values of the attributes asked for, and how many results have each;
// a result without one counts for NSNull.
static NSDictionary *valueListsFor(NSArray *attributes,NSArray *results) {
   NSMutableDictionary *lists=[NSMutableDictionary dictionary];

   for(NSString *attribute in attributes){
    NSCountedSet   *counts=[[[NSCountedSet alloc] init] autorelease];
    NSMutableArray *tuples=[NSMutableArray array];

    for(NSMetadataItem *item in results){
     id value=[item valueForAttribute:attribute];

     if(value==nil)
      [counts addObject:[NSNull null]];
     else if([value isKindOfClass:[NSArray class]])
      for(id element in value)
       [counts addObject:element];
     else
      [counts addObject:value];
    }
    for(id value in counts){
     NSMetadataQueryAttributeValueTuple *tuple=[[NSMetadataQueryAttributeValueTuple alloc] _initWithAttribute:attribute value:value count:[counts countForObject:value]];

     [tuples addObject:tuple];
     [tuple release];
    }
    [lists setObject:tuples forKey:attribute];
   }
   return lists;
}

-(void)_finishWithResults:(NSArray *)results {
   if(_stopped)
    return;

   NSMutableArray *sorted=[NSMutableArray arrayWithArray:results];

   if([_sortDescriptors count])
    [sorted sortUsingDescriptors:_sortDescriptors];
   if([_delegate respondsToSelector:@selector(metadataQuery:replacementObjectForResultObject:)])
    for(NSUInteger i=0;i<[sorted count];i++)
     [sorted replaceObjectAtIndex:i withObject:[_delegate metadataQuery:self replacementObjectForResultObject:[sorted objectAtIndex:i]]];

   [_results release];
   _results=[sorted copy];
   [_valueLists release];
   _valueLists=[valueListsFor(_valueListAttributes,results) copy];
   _gathering=NO;
   [[NSNotificationCenter defaultCenter] postNotificationName:NSMetadataQueryDidFinishGatheringNotification object:self];
}

// The block outlives this call: a copy, off the stack.
-(void)_post:(void (^)(void))block {
   block=[[block copy] autorelease];
   if(_operationQueue!=nil)
    [_operationQueue addOperationWithBlock:block];
   else
    [self performSelector:@selector(_runBlock:) onThread:_thread withObject:block waitUntilDone:NO
     modes:[NSArray arrayWithObject:NSRunLoopCommonModes]];
}

-(void)_runBlock:(void (^)(void))block {
   block();
}

-(BOOL)startQuery {
   NSArray     *roots;
   NSPredicate *predicate;

   if(_started)
    return NO;
   if(_predicate==nil && [_searchItems count]==0)
    [NSException raise:NSInternalInconsistencyException format:@"NSMetadataQuery: a query needs a predicate to start"];

   _started=YES;
   _gathering=YES;
   _stopped=NO;
   [_thread release];
   _thread=[[NSThread currentThread] retain];
   // The results come back on this thread's run loop: it must have one.
   [NSRunLoop currentRunLoop];

   roots=[rootsForScopes(_searchScopes) retain];
   predicate=[_predicate retain];
   [self retain];
   [[NSNotificationCenter defaultCenter] postNotificationName:NSMetadataQueryDidStartGatheringNotification object:self];

   dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT,0),^{
    @autoreleasepool {
     NSArray *results=[gather(roots,predicate) retain];

     [roots release];
     [predicate release];
     [self _post:^{
      [self _finishWithResults:results];
      [results release];
      [self release];
     }];
    }
   });
   return YES;
}

-(void)stopQuery {
   _stopped=YES;
   _gathering=NO;
}

-(NSUInteger)resultCount {
   return [_results count];
}

-resultAtIndex:(NSUInteger)index {
   return [_results objectAtIndex:index];
}

-(NSArray *)results {
   return _results;
}

-(NSUInteger)indexOfResult:result {
   return [_results indexOfObjectIdenticalTo:result];
}

-(void)enumerateResultsUsingBlock:(void (^)(id,NSUInteger,BOOL *))block {
   [_results enumerateObjectsUsingBlock:block];
}

-(void)enumerateResultsWithOptions:(NSEnumerationOptions)options usingBlock:(void (^)(id,NSUInteger,BOOL *))block {
   [_results enumerateObjectsWithOptions:options usingBlock:block];
}

-(NSDictionary *)valueLists {
   return _valueLists;
}

// One group for each value of the first grouping attribute.
-(NSArray *)groupedResults {
   NSString            *attribute=[_groupingAttributes firstObject];
   NSMutableDictionary *byValue=[NSMutableDictionary dictionary];
   NSMutableArray      *groups=[NSMutableArray array];

   if(attribute==nil)
    return [NSArray array];
   for(NSMetadataItem *item in _results){
    id              value=[item valueForAttribute:attribute];
    NSMutableArray *members;

    if(value==nil)
     value=[NSNull null];
    if((members=[byValue objectForKey:value])==nil)
     [byValue setObject:(members=[NSMutableArray array]) forKey:value];
    [members addObject:item];
   }
   for(id value in byValue){
    NSMetadataQueryResultGroup *group=[[NSMetadataQueryResultGroup alloc] _initWithAttribute:attribute value:value results:[byValue objectForKey:value]];

    [groups addObject:group];
    [group release];
   }
   return groups;
}

-valueOfAttribute:(NSString *)name forResultAtIndex:(NSUInteger)index {
   id value=[[_results objectAtIndex:index] valueForAttribute:name];

   if([_delegate respondsToSelector:@selector(metadataQuery:replacementValueForAttribute:value:)])
    value=[_delegate metadataQuery:self replacementValueForAttribute:name value:value];
   return value;
}

@end
