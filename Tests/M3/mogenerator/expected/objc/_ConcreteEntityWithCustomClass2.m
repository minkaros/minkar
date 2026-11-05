// DO NOT EDIT. This file is machine-generated and constantly overwritten.
// Make changes to ConcreteEntityWithCustomClass2.m instead.

#import "_ConcreteEntityWithCustomClass2.h"

@implementation ConcreteEntityWithCustomClass2ID
@end

@implementation _ConcreteEntityWithCustomClass2

+ (instancetype)insertInManagedObjectContext:(NSManagedObjectContext *)moc_ {
	NSParameterAssert(moc_);
	return [NSEntityDescription insertNewObjectForEntityForName:@"ConcreteEntityWithCustomClass2" inManagedObjectContext:moc_];
}

+ (NSString*)entityName {
	return @"ConcreteEntityWithCustomClass2";
}

+ (NSEntityDescription*)entityInManagedObjectContext:(NSManagedObjectContext*)moc_ {
	NSParameterAssert(moc_);
	return [NSEntityDescription entityForName:@"ConcreteEntityWithCustomClass2" inManagedObjectContext:moc_];
}

- (ConcreteEntityWithCustomClass2ID*)objectID {
	return (ConcreteEntityWithCustomClass2ID*)[super objectID];
}

+ (NSSet*)keyPathsForValuesAffectingValueForKey:(NSString*)key {
	NSSet *keyPaths = [super keyPathsForValuesAffectingValueForKey:key];

	if ([key isEqualToString:@"dummy3Value"]) {
		NSSet *affectingKey = [NSSet setWithObject:@"dummy3"];
		keyPaths = [keyPaths setByAddingObjectsFromSet:affectingKey];
		return keyPaths;
	}

	return keyPaths;
}

@dynamic dummy3;

- (int16_t)dummy3Value {
	NSNumber *result = [self dummy3];
	return [result shortValue];
}

- (void)setDummy3Value:(int16_t)value_ {
	[self setDummy3:@(value_)];
}

- (int16_t)primitiveDummy3Value {
	NSNumber *result = [self primitiveDummy3];
	return [result shortValue];
}

- (void)setPrimitiveDummy3Value:(int16_t)value_ {
	[self setPrimitiveDummy3:@(value_)];
}

@end

@implementation ConcreteEntityWithCustomClass2Attributes 
+ (NSString *)dummy3 {
	return @"dummy3";
}
@end

