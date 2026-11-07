// DO NOT EDIT. This file is machine-generated and constantly overwritten.
// Make changes to HumanMO.m instead.

#import "_HumanMO.h"

@implementation HumanMOID
@end

@implementation _HumanMO

+ (instancetype)insertInManagedObjectContext:(NSManagedObjectContext *)moc_ {
	NSParameterAssert(moc_);
	return [NSEntityDescription insertNewObjectForEntityForName:@"Human" inManagedObjectContext:moc_];
}

+ (NSString*)entityName {
	return @"Human";
}

+ (NSEntityDescription*)entityInManagedObjectContext:(NSManagedObjectContext*)moc_ {
	NSParameterAssert(moc_);
	return [NSEntityDescription entityForName:@"Human" inManagedObjectContext:moc_];
}

- (HumanMOID*)objectID {
	return (HumanMOID*)[super objectID];
}

+ (NSSet*)keyPathsForValuesAffectingValueForKey:(NSString*)key {
	NSSet *keyPaths = [super keyPathsForValuesAffectingValueForKey:key];

	if ([key isEqualToString:@"ageValue"]) {
		NSSet *affectingKey = [NSSet setWithObject:@"age"];
		keyPaths = [keyPaths setByAddingObjectsFromSet:affectingKey];
		return keyPaths;
	}
	if ([key isEqualToString:@"genderValue"]) {
		NSSet *affectingKey = [NSSet setWithObject:@"gender"];
		keyPaths = [keyPaths setByAddingObjectsFromSet:affectingKey];
		return keyPaths;
	}

	return keyPaths;
}

@dynamic age;

- (uint16_t)ageValue {
	NSNumber *result = [self age];
	return [result unsignedShortValue];
}

- (uint16_t)primitiveAgeValue {
	NSNumber *result = [self primitiveAge];
	return [result unsignedShortValue];
}

- (void)setPrimitiveAgeValue:(uint16_t)value_ {
	[self setPrimitiveAge:@(value_)];
}

@dynamic gender;

- (Gender)genderValue {
	NSNumber *result = [self gender];
	return [result shortValue];
}

- (void)setGenderValue:(Gender)value_ {
	[self setGender:@(value_)];
}

- (Gender)primitiveGenderValue {
	NSNumber *result = [self primitiveGender];
	return [result shortValue];
}

- (void)setPrimitiveGenderValue:(Gender)value_ {
	[self setPrimitiveGender:@(value_)];
}

@dynamic humanName;

@dynamic meaninglessRelationship;

+ (NSArray*)fetchByHumanName:(NSManagedObjectContext*)moc_ humanName:(NSString*)humanName_ {
	NSError *error = nil;
	NSArray *result = [self fetchByHumanName:moc_ humanName:humanName_ error:&error];
	if (error) {
#ifdef NSAppKitVersionNumber10_0
		[NSApp presentError:error];
#else
		NSLog(@"error: %@", error);
#endif
	}
	return result;
}
+ (NSArray*)fetchByHumanName:(NSManagedObjectContext*)moc_ humanName:(NSString*)humanName_ error:(NSError**)error_ {
	NSParameterAssert(moc_);
	NSError *error = nil;

	NSManagedObjectModel *model = [[moc_ persistentStoreCoordinator] managedObjectModel];

	NSDictionary *substitutionVariables = @{@"humanName": humanName_};

	NSFetchRequest *fetchRequest = [model fetchRequestFromTemplateWithName:@"byHumanName"
													 substitutionVariables:substitutionVariables];
	NSAssert(fetchRequest, @"Can't find fetch request named \"byHumanName\".");

	NSArray *result = [moc_ executeFetchRequest:fetchRequest error:&error];
	if (error_) *error_ = error;
	return result;
}

+ (instancetype)fetchOneByHumanName:(NSManagedObjectContext*)moc_ humanName:(NSString*)humanName_ {
	NSError *error = nil;
	id result = [self fetchOneByHumanName:moc_ humanName:humanName_ error:&error];
	if (error) {
#ifdef NSAppKitVersionNumber10_0
		[NSApp presentError:error];
#else
		NSLog(@"error: %@", error);
#endif
	}
	return result;
}
+ (instancetype)fetchOneByHumanName:(NSManagedObjectContext*)moc_ humanName:(NSString*)humanName_ error:(NSError**)error_ {
	NSParameterAssert(moc_);
	NSError *error = nil;

	NSManagedObjectModel *model = [[moc_ persistentStoreCoordinator] managedObjectModel];

	NSDictionary *substitutionVariables = @{@"humanName": humanName_};

	NSFetchRequest *fetchRequest = [model fetchRequestFromTemplateWithName:@"oneByHumanName"
													 substitutionVariables:substitutionVariables];
	NSAssert(fetchRequest, @"Can't find fetch request named \"oneByHumanName\".");

	id result = nil;
	NSArray *results = [moc_ executeFetchRequest:fetchRequest error:&error];

	if (!error) {
		switch ([results count]) {
			case 0:
				//	Nothing found matching the fetch request. That's cool, though: we'll just return nil.
				break;
			case 1:
				result = results.firstObject;
				break;
			default:
				NSLog(@"WARN fetch request oneByHumanName: 0 or 1 objects expected, %lu found (substitutionVariables:%@, results:%@)",
					(unsigned long)[results count],
					substitutionVariables,
					results);
		}
	}

	if (error_) *error_ = error;
	return result;
}

+ (NSArray*)fetchAllHumans:(NSManagedObjectContext*)moc_ {
	NSError *error = nil;
	NSArray *result = [self fetchAllHumans:moc_ error:&error];
	if (error) {
#ifdef NSAppKitVersionNumber10_0
		[NSApp presentError:error];
#else
		NSLog(@"error: %@", error);
#endif
	}
	return result;
}
+ (NSArray*)fetchAllHumans:(NSManagedObjectContext*)moc_ error:(NSError**)error_ {
	NSParameterAssert(moc_);
	NSError *error = nil;

	NSManagedObjectModel *model = [[moc_ persistentStoreCoordinator] managedObjectModel];

	NSDictionary *substitutionVariables = @{};

	NSFetchRequest *fetchRequest = [model fetchRequestFromTemplateWithName:@"allHumans"
													 substitutionVariables:substitutionVariables];
	NSAssert(fetchRequest, @"Can't find fetch request named \"allHumans\".");

	NSArray *result = [moc_ executeFetchRequest:fetchRequest error:&error];
	if (error_) *error_ = error;
	return result;
}

@end

@implementation HumanMOAttributes 
+ (NSString *)age {
	return @"age";
}
+ (NSString *)gender {
	return @"gender";
}
+ (NSString *)humanName {
	return @"humanName";
}
@end

@implementation HumanMORelationships 
+ (NSString *)meaninglessRelationship {
	return @"meaninglessRelationship";
}
@end

@implementation HumanMOUserInfo 
+ (NSString *)additionalHeaderFileName {
	return @"Gender.h";
}
@end

