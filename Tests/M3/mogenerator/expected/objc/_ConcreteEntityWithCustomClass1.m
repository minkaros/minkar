// DO NOT EDIT. This file is machine-generated and constantly overwritten.
// Make changes to ConcreteEntityWithCustomClass1.m instead.

#import "_ConcreteEntityWithCustomClass1.h"

@implementation ConcreteEntityWithCustomClass1ID
@end

@implementation _ConcreteEntityWithCustomClass1

+ (instancetype)insertInManagedObjectContext:(NSManagedObjectContext *)moc_ {
	NSParameterAssert(moc_);
	return [NSEntityDescription insertNewObjectForEntityForName:@"ConcreteEntityWithCustomClass1" inManagedObjectContext:moc_];
}

+ (NSString*)entityName {
	return @"ConcreteEntityWithCustomClass1";
}

+ (NSEntityDescription*)entityInManagedObjectContext:(NSManagedObjectContext*)moc_ {
	NSParameterAssert(moc_);
	return [NSEntityDescription entityForName:@"ConcreteEntityWithCustomClass1" inManagedObjectContext:moc_];
}

- (ConcreteEntityWithCustomClass1ID*)objectID {
	return (ConcreteEntityWithCustomClass1ID*)[super objectID];
}

+ (NSSet*)keyPathsForValuesAffectingValueForKey:(NSString*)key {
	NSSet *keyPaths = [super keyPathsForValuesAffectingValueForKey:key];

	if ([key isEqualToString:@"dummy2Value"]) {
		NSSet *affectingKey = [NSSet setWithObject:@"dummy2"];
		keyPaths = [keyPaths setByAddingObjectsFromSet:affectingKey];
		return keyPaths;
	}

	return keyPaths;
}

@dynamic dummy2;

- (int16_t)dummy2Value {
	NSNumber *result = [self dummy2];
	return [result shortValue];
}

- (void)setDummy2Value:(int16_t)value_ {
	[self setDummy2:@(value_)];
}

- (int16_t)primitiveDummy2Value {
	NSNumber *result = [self primitiveDummy2];
	return [result shortValue];
}

- (void)setPrimitiveDummy2Value:(int16_t)value_ {
	[self setPrimitiveDummy2:@(value_)];
}

@end

@implementation ConcreteEntityWithCustomClass1Attributes 
+ (NSString *)dummy2 {
	return @"dummy2";
}
@end

