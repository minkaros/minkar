/* Copyright (c) 2008 Dan Knapp
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <CoreData/NSFetchRequest.h>
#import <CoreData/NSManagedObject.h>
#import <CoreData/NSManagedObjectContext.h>
#import <CoreData/NSEntityDescription.h>
#import <Foundation/NSRaise.h>
#import <Foundation/Foundation.h>

@implementation NSFetchRequest

-init {
   _includesPendingChanges=YES;
   _includesPropertyValues=YES;
   _includesSubentities=YES;
   _returnsObjectsAsFaults=YES;
   return self;
}

-initWithEntityName:(NSString *)entityName {
   if((self=[self init])==nil)
    return nil;
   _entityName=[entityName copy];
   return self;
}

+(instancetype)fetchRequestWithEntityName:(NSString *)entityName {
   return [[[self alloc] initWithEntityName:entityName] autorelease];
}

-initWithCoder:(NSCoder *)coder {
   if(![coder allowsKeyedCoding])
    [NSException raise:NSInvalidArgumentException format:@"%@ can not initWithCoder:%@",[self class],[coder class]];

   _entity=[[coder decodeObjectForKey:@"NSEntity"] retain];
   _entityName=[[coder decodeObjectForKey:@"NSEntityName"] copy];
   _predicate=[[coder decodeObjectForKey:@"NSPredicate"] retain];
   _sortDescriptors=[[coder decodeObjectForKey:@"NSSortDescriptors"] copy];
   _propertiesToFetch=[[coder decodeObjectForKey:@"NSPropertiesToFetch"] copy];
   _relationshipKeyPathsForPrefetching=[[coder decodeObjectForKey:@"NSRelationshipKeyPathsForPrefetching"] copy];
   _resultType=[coder decodeIntegerForKey:@"NSResultType"];
   _fetchLimit=[coder decodeIntegerForKey:@"NSFetchLimit"];
   _fetchBatchSize=[coder decodeIntegerForKey:@"NSFetchBatchSize"];
   _fetchOffset=[coder decodeIntegerForKey:@"NSFetchOffset"];
   _includesPendingChanges=![coder decodeBoolForKey:@"NSExcludesPendingChanges"];
   _includesPropertyValues=![coder decodeBoolForKey:@"NSExcludesPropertyValues"];
   _includesSubentities=![coder decodeBoolForKey:@"NSExcludesSubentities"];
   _returnsDistinctResults=[coder decodeBoolForKey:@"NSReturnsDistinctResults"];
   _returnsObjectsAsFaults=![coder decodeBoolForKey:@"NSReturnsObjectsAsValues"];
   return self;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   if(![coder allowsKeyedCoding])
    [NSException raise:NSInvalidArgumentException format:@"%@ can not encodeWithCoder:%@",[self class],[coder class]];

   [coder encodeObject:_entity forKey:@"NSEntity"];
   [coder encodeObject:[self entityName] forKey:@"NSEntityName"];
   [coder encodeObject:_predicate forKey:@"NSPredicate"];
   [coder encodeObject:_sortDescriptors forKey:@"NSSortDescriptors"];
   [coder encodeObject:_propertiesToFetch forKey:@"NSPropertiesToFetch"];
   [coder encodeObject:_relationshipKeyPathsForPrefetching forKey:@"NSRelationshipKeyPathsForPrefetching"];
   [coder encodeInteger:_resultType forKey:@"NSResultType"];
   [coder encodeInteger:_fetchLimit forKey:@"NSFetchLimit"];
   [coder encodeInteger:_fetchBatchSize forKey:@"NSFetchBatchSize"];
   [coder encodeInteger:_fetchOffset forKey:@"NSFetchOffset"];
   [coder encodeBool:!_includesPendingChanges forKey:@"NSExcludesPendingChanges"];
   [coder encodeBool:!_includesPropertyValues forKey:@"NSExcludesPropertyValues"];
   [coder encodeBool:!_includesSubentities forKey:@"NSExcludesSubentities"];
   [coder encodeBool:_returnsDistinctResults forKey:@"NSReturnsDistinctResults"];
   [coder encodeBool:!_returnsObjectsAsFaults forKey:@"NSReturnsObjectsAsValues"];
}

-copyWithZone:(NSZone *)zone {
   NSFetchRequest *result=[[[self class] allocWithZone:zone] init];

   result->_resultType=_resultType;
   result->_entity=[_entity retain];
   result->_entityName=[_entityName copy];
   result->_predicate=[_predicate copy];
   result->_sortDescriptors=[_sortDescriptors copy];
   result->_affectedStores=[_affectedStores copy];
   result->_fetchLimit=_fetchLimit;
   result->_fetchBatchSize=_fetchBatchSize;
   result->_fetchOffset=_fetchOffset;
   result->_includesPendingChanges=_includesPendingChanges;
   result->_includesPropertyValues=_includesPropertyValues;
   result->_includesSubentities=_includesSubentities;
   result->_returnsDistinctResults=_returnsDistinctResults;
   result->_returnsObjectsAsFaults=_returnsObjectsAsFaults;
   result->_propertiesToFetch=[_propertiesToFetch copy];
   result->_relationshipKeyPathsForPrefetching=[_relationshipKeyPathsForPrefetching copy];
   return result;
}

-(void)dealloc {
   [_entity release];
   [_entityName release];
   [_predicate release];
   [_sortDescriptors release];
   [_affectedStores release];
   [_propertiesToFetch release];
   [_relationshipKeyPathsForPrefetching release];
   [super dealloc];
}

-(NSString *)entityName {
   return (_entity!=nil)?[_entity name]:_entityName;
}

-(NSString *)description {
   return [NSString stringWithFormat:@"<%@: %p> (entity: %@; predicate: (%@); sortDescriptors: (%@); type: %lu; )",[self class],self,[self entityName],[_predicate predicateFormat],_sortDescriptors,(unsigned long)_resultType];
}

-(NSFetchRequestResultType)resultType {
   return _resultType;
}

-(NSEntityDescription *)entity {
   return _entity;
}

-(NSPredicate *)predicate {
   return _predicate;
}

-(NSArray *)sortDescriptors {
   return _sortDescriptors;
}

-(NSArray *)affectedStores {
   return _affectedStores;
}

-(NSUInteger)fetchLimit {
   return _fetchLimit;
}

-(NSUInteger)fetchBatchSize {
   return _fetchBatchSize;
}

-(NSUInteger)fetchOffset {
   return _fetchOffset;
}

-(BOOL)includesPendingChanges {
   return _includesPendingChanges;
}

-(BOOL)includesPropertyValues {
   return _includesPropertyValues;
}

-(BOOL)includesSubentities {
   return _includesSubentities;
}

-(BOOL)returnsDistinctResults {
   return _returnsDistinctResults;
}

-(BOOL)returnsObjectsAsFaults {
   return _returnsObjectsAsFaults;
}

-(NSArray *)propertiesToFetch {
   return _propertiesToFetch;
}

-(NSArray *)relationshipKeyPathsForPrefetching {
   return _relationshipKeyPathsForPrefetching;
}

-(void)setResultType:(NSFetchRequestResultType)type {
   _resultType=type;
}

-(void)setEntity:(NSEntityDescription *)value {
   value=[value retain];
   [_entity release];
   _entity=value;
}

-(void)setPredicate:(NSPredicate *)value {
   value=[value retain];
   [_predicate release];
   _predicate=value;
}

-(void)setSortDescriptors:(NSArray *)value {
   value=[value copy];
   [_sortDescriptors release];
   _sortDescriptors=value;
}

-(void)setAffectedStores:(NSArray *)value {
   value=[value copy];
   [_affectedStores release];
   _affectedStores=value;
}

-(void)setFetchLimit:(NSUInteger)value {
   _fetchLimit=value;
}

-(void)setFetchBatchSize:(NSUInteger)value {
   _fetchBatchSize=value;
}

-(void)setFetchOffset:(NSUInteger)value {
   _fetchOffset=value;
}

-(void)setIncludesPendingChanges:(BOOL)value {
   _includesPendingChanges=value;
}

-(void)setIncludesPropertyValues:(BOOL)value {
   _includesPropertyValues=value;
}

-(void)setIncludesSubentities:(BOOL)value {
   _includesSubentities=value;
}

-(void)setReturnsDistinctResults:(BOOL)value {
   _returnsDistinctResults=value;
}

-(void)setReturnsObjectsAsFaults:(BOOL)value {
   _returnsObjectsAsFaults=value;
}

-(void)setPropertiesToFetch:(NSArray *)value {
   value=[value copy];
   [_propertiesToFetch release];
   _propertiesToFetch=value;
}

-(void)setRelationshipKeyPathsForPrefetching:(NSArray *)value {
   value=[value copy];
   [_relationshipKeyPathsForPrefetching release];
   _relationshipKeyPathsForPrefetching=value;
}

@end

@implementation NSNumber (NSFetchedResultSupport)
@end

@implementation NSDictionary (NSFetchedResultSupport)
@end
