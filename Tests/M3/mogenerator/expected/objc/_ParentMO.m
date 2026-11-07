// DO NOT EDIT. This file is machine-generated and constantly overwritten.
// Make changes to ParentMO.m instead.

#import "_ParentMO.h"

@implementation ParentMOID
@end

@implementation _ParentMO

+ (instancetype)insertInManagedObjectContext:(NSManagedObjectContext *)moc_ {
	NSParameterAssert(moc_);
	return [NSEntityDescription insertNewObjectForEntityForName:@"Parent" inManagedObjectContext:moc_];
}

+ (NSString*)entityName {
	return @"Parent";
}

+ (NSEntityDescription*)entityInManagedObjectContext:(NSManagedObjectContext*)moc_ {
	NSParameterAssert(moc_);
	return [NSEntityDescription entityForName:@"Parent" inManagedObjectContext:moc_];
}

- (ParentMOID*)objectID {
	return (ParentMOID*)[super objectID];
}

+ (NSSet*)keyPathsForValuesAffectingValueForKey:(NSString*)key {
	NSSet *keyPaths = [super keyPathsForValuesAffectingValueForKey:key];

	if ([key isEqualToString:@"myBooleanValue"]) {
		NSSet *affectingKey = [NSSet setWithObject:@"myBoolean"];
		keyPaths = [keyPaths setByAddingObjectsFromSet:affectingKey];
		return keyPaths;
	}
	if ([key isEqualToString:@"myDoubleValue"]) {
		NSSet *affectingKey = [NSSet setWithObject:@"myDouble"];
		keyPaths = [keyPaths setByAddingObjectsFromSet:affectingKey];
		return keyPaths;
	}
	if ([key isEqualToString:@"myFloatValue"]) {
		NSSet *affectingKey = [NSSet setWithObject:@"myFloat"];
		keyPaths = [keyPaths setByAddingObjectsFromSet:affectingKey];
		return keyPaths;
	}
	if ([key isEqualToString:@"myInt16Value"]) {
		NSSet *affectingKey = [NSSet setWithObject:@"myInt16"];
		keyPaths = [keyPaths setByAddingObjectsFromSet:affectingKey];
		return keyPaths;
	}
	if ([key isEqualToString:@"myInt16TransientValue"]) {
		NSSet *affectingKey = [NSSet setWithObject:@"myInt16Transient"];
		keyPaths = [keyPaths setByAddingObjectsFromSet:affectingKey];
		return keyPaths;
	}
	if ([key isEqualToString:@"myInt32Value"]) {
		NSSet *affectingKey = [NSSet setWithObject:@"myInt32"];
		keyPaths = [keyPaths setByAddingObjectsFromSet:affectingKey];
		return keyPaths;
	}
	if ([key isEqualToString:@"myInt64Value"]) {
		NSSet *affectingKey = [NSSet setWithObject:@"myInt64"];
		keyPaths = [keyPaths setByAddingObjectsFromSet:affectingKey];
		return keyPaths;
	}

	return keyPaths;
}

@dynamic myBinaryData;

@dynamic myBoolean;

- (BOOL)myBooleanValue {
	NSNumber *result = [self myBoolean];
	return [result boolValue];
}

- (void)setMyBooleanValue:(BOOL)value_ {
	[self setMyBoolean:@(value_)];
}

- (BOOL)primitiveMyBooleanValue {
	NSNumber *result = [self primitiveMyBoolean];
	return [result boolValue];
}

- (void)setPrimitiveMyBooleanValue:(BOOL)value_ {
	[self setPrimitiveMyBoolean:@(value_)];
}

@dynamic myDate;

@dynamic myDecimal;

@dynamic myDouble;

- (double)myDoubleValue {
	NSNumber *result = [self myDouble];
	return [result doubleValue];
}

- (void)setMyDoubleValue:(double)value_ {
	[self setMyDouble:@(value_)];
}

- (double)primitiveMyDoubleValue {
	NSNumber *result = [self primitiveMyDouble];
	return [result doubleValue];
}

- (void)setPrimitiveMyDoubleValue:(double)value_ {
	[self setPrimitiveMyDouble:@(value_)];
}

@dynamic myFloat;

- (float)myFloatValue {
	NSNumber *result = [self myFloat];
	return [result floatValue];
}

- (void)setMyFloatValue:(float)value_ {
	[self setMyFloat:@(value_)];
}

- (float)primitiveMyFloatValue {
	NSNumber *result = [self primitiveMyFloat];
	return [result floatValue];
}

- (void)setPrimitiveMyFloatValue:(float)value_ {
	[self setPrimitiveMyFloat:@(value_)];
}

@dynamic myInt16;

- (int16_t)myInt16Value {
	NSNumber *result = [self myInt16];
	return [result shortValue];
}

- (void)setMyInt16Value:(int16_t)value_ {
	[self setMyInt16:@(value_)];
}

- (int16_t)primitiveMyInt16Value {
	NSNumber *result = [self primitiveMyInt16];
	return [result shortValue];
}

- (void)setPrimitiveMyInt16Value:(int16_t)value_ {
	[self setPrimitiveMyInt16:@(value_)];
}

@dynamic myInt16Transient;

- (int16_t)myInt16TransientValue {
	NSNumber *result = [self myInt16Transient];
	return [result shortValue];
}

- (void)setMyInt16TransientValue:(int16_t)value_ {
	[self setMyInt16Transient:@(value_)];
}

- (int16_t)primitiveMyInt16TransientValue {
	NSNumber *result = [self primitiveMyInt16Transient];
	return [result shortValue];
}

- (void)setPrimitiveMyInt16TransientValue:(int16_t)value_ {
	[self setPrimitiveMyInt16Transient:@(value_)];
}

@dynamic myInt32;

- (int32_t)myInt32Value {
	NSNumber *result = [self myInt32];
	return [result intValue];
}

- (void)setMyInt32Value:(int32_t)value_ {
	[self setMyInt32:@(value_)];
}

- (int32_t)primitiveMyInt32Value {
	NSNumber *result = [self primitiveMyInt32];
	return [result intValue];
}

- (void)setPrimitiveMyInt32Value:(int32_t)value_ {
	[self setPrimitiveMyInt32:@(value_)];
}

@dynamic myInt64;

- (int64_t)myInt64Value {
	NSNumber *result = [self myInt64];
	return [result longLongValue];
}

- (void)setMyInt64Value:(int64_t)value_ {
	[self setMyInt64:@(value_)];
}

- (int64_t)primitiveMyInt64Value {
	NSNumber *result = [self primitiveMyInt64];
	return [result longLongValue];
}

- (void)setPrimitiveMyInt64Value:(int64_t)value_ {
	[self setPrimitiveMyInt64:@(value_)];
}

@dynamic myString;

@dynamic myTransformableSansClassName;

@dynamic parentName;

@dynamic children;

- (NSMutableSet<ChildMO*>*)childrenSet {
	[self willAccessValueForKey:@"children"];

	NSMutableSet<ChildMO*> *result = (NSMutableSet<ChildMO*>*)[self mutableSetValueForKey:@"children"];

	[self didAccessValueForKey:@"children"];
	return result;
}

@dynamic orderedChildren;

- (NSMutableOrderedSet<ChildMO*>*)orderedChildrenSet {
	[self willAccessValueForKey:@"orderedChildren"];

	NSMutableOrderedSet<ChildMO*> *result = (NSMutableOrderedSet<ChildMO*>*)[self mutableOrderedSetValueForKey:@"orderedChildren"];

	[self didAccessValueForKey:@"orderedChildren"];
	return result;
}

@end

@implementation _ParentMO (OrderedChildrenCoreDataGeneratedAccessors)
- (void)addOrderedChildren:(NSOrderedSet<ChildMO*>*)value_ {
	[self.orderedChildrenSet unionOrderedSet:value_];
}
- (void)removeOrderedChildren:(NSOrderedSet<ChildMO*>*)value_ {
	[self.orderedChildrenSet minusOrderedSet:value_];
}
- (void)addOrderedChildrenObject:(ChildMO*)value_ {
	[self.orderedChildrenSet addObject:value_];
}
- (void)removeOrderedChildrenObject:(ChildMO*)value_ {
	[self.orderedChildrenSet removeObject:value_];
}
- (void)insertObject:(ChildMO*)value inOrderedChildrenAtIndex:(NSUInteger)idx {
    NSIndexSet* indexes = [NSIndexSet indexSetWithIndex:idx];
    [self willChange:NSKeyValueChangeInsertion valuesAtIndexes:indexes forKey:@"orderedChildren"];
    NSMutableOrderedSet *tmpOrderedSet = [NSMutableOrderedSet orderedSetWithOrderedSet:[self orderedChildren] ?: [NSOrderedSet orderedSet]];
    [tmpOrderedSet insertObject:value atIndex:idx];
    [self setPrimitiveValue:tmpOrderedSet forKey:@"orderedChildren"];
    [self didChange:NSKeyValueChangeInsertion valuesAtIndexes:indexes forKey:@"orderedChildren"];
}
- (void)removeObjectFromOrderedChildrenAtIndex:(NSUInteger)idx {
    NSIndexSet* indexes = [NSIndexSet indexSetWithIndex:idx];
    [self willChange:NSKeyValueChangeRemoval valuesAtIndexes:indexes forKey:@"orderedChildren"];
    NSMutableOrderedSet *tmpOrderedSet = [NSMutableOrderedSet orderedSetWithOrderedSet:[self orderedChildren] ?: [NSOrderedSet orderedSet]];
    [tmpOrderedSet removeObjectAtIndex:idx];
    [self setPrimitiveValue:tmpOrderedSet forKey:@"orderedChildren"];
    [self didChange:NSKeyValueChangeRemoval valuesAtIndexes:indexes forKey:@"orderedChildren"];
}
- (void)insertOrderedChildren:(NSArray *)value atIndexes:(NSIndexSet *)indexes {
    [self willChange:NSKeyValueChangeInsertion valuesAtIndexes:indexes forKey:@"orderedChildren"];
    NSMutableOrderedSet *tmpOrderedSet = [NSMutableOrderedSet orderedSetWithOrderedSet:[self orderedChildren] ?: [NSOrderedSet orderedSet]];
    [tmpOrderedSet insertObjects:value atIndexes:indexes];
    [self setPrimitiveValue:tmpOrderedSet forKey:@"orderedChildren"];
    [self didChange:NSKeyValueChangeInsertion valuesAtIndexes:indexes forKey:@"orderedChildren"];
}
- (void)removeOrderedChildrenAtIndexes:(NSIndexSet *)indexes {
    [self willChange:NSKeyValueChangeRemoval valuesAtIndexes:indexes forKey:@"orderedChildren"];
    NSMutableOrderedSet *tmpOrderedSet = [NSMutableOrderedSet orderedSetWithOrderedSet:[self orderedChildren] ?: [NSOrderedSet orderedSet]];
    [tmpOrderedSet removeObjectsAtIndexes:indexes];
    [self setPrimitiveValue:tmpOrderedSet forKey:@"orderedChildren"];
    [self didChange:NSKeyValueChangeRemoval valuesAtIndexes:indexes forKey:@"orderedChildren"];
}
- (void)replaceObjectInOrderedChildrenAtIndex:(NSUInteger)idx withObject:(ChildMO*)value {
    NSIndexSet* indexes = [NSIndexSet indexSetWithIndex:idx];
    [self willChange:NSKeyValueChangeReplacement valuesAtIndexes:indexes forKey:@"orderedChildren"];
    NSMutableOrderedSet *tmpOrderedSet = [NSMutableOrderedSet orderedSetWithOrderedSet:[self orderedChildren] ?: [NSOrderedSet orderedSet]];
    [tmpOrderedSet replaceObjectAtIndex:idx withObject:value];
    [self setPrimitiveValue:tmpOrderedSet forKey:@"orderedChildren"];
    [self didChange:NSKeyValueChangeReplacement valuesAtIndexes:indexes forKey:@"orderedChildren"];
}
- (void)replaceOrderedChildrenAtIndexes:(NSIndexSet *)indexes withOrderedChildren:(NSArray *)value {
    [self willChange:NSKeyValueChangeReplacement valuesAtIndexes:indexes forKey:@"orderedChildren"];
    NSMutableOrderedSet *tmpOrderedSet = [NSMutableOrderedSet orderedSetWithOrderedSet:[self orderedChildren] ?: [NSOrderedSet orderedSet]];
    [tmpOrderedSet replaceObjectsAtIndexes:indexes withObjects:value];
    [self setPrimitiveValue:tmpOrderedSet forKey:@"orderedChildren"];
    [self didChange:NSKeyValueChangeReplacement valuesAtIndexes:indexes forKey:@"orderedChildren"];
}
@end

@implementation ParentMOAttributes 
+ (NSString *)myBinaryData {
	return @"myBinaryData";
}
+ (NSString *)myBoolean {
	return @"myBoolean";
}
+ (NSString *)myDate {
	return @"myDate";
}
+ (NSString *)myDecimal {
	return @"myDecimal";
}
+ (NSString *)myDouble {
	return @"myDouble";
}
+ (NSString *)myFloat {
	return @"myFloat";
}
+ (NSString *)myInt16 {
	return @"myInt16";
}
+ (NSString *)myInt16Transient {
	return @"myInt16Transient";
}
+ (NSString *)myInt32 {
	return @"myInt32";
}
+ (NSString *)myInt64 {
	return @"myInt64";
}
+ (NSString *)myString {
	return @"myString";
}
+ (NSString *)myTransformableSansClassName {
	return @"myTransformableSansClassName";
}
+ (NSString *)parentName {
	return @"parentName";
}
@end

@implementation ParentMORelationships 
+ (NSString *)children {
	return @"children";
}
+ (NSString *)orderedChildren {
	return @"orderedChildren";
}
@end

