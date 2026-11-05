// DO NOT EDIT. This file is machine-generated and constantly overwritten.
// Make changes to HumanMO.h instead.

#if __has_feature(modules)
    @import Foundation;
    @import CoreData;
#else
    #import <Foundation/Foundation.h>
    #import <CoreData/CoreData.h>
#endif

#import "MyBaseClass.h"

#import "Gender.h"

NS_ASSUME_NONNULL_BEGIN

@class NSManagedObject;

@interface HumanMOID : NSManagedObjectID {}
@end

@interface _HumanMO : MyBaseClass
+ (instancetype)insertInManagedObjectContext:(NSManagedObjectContext *)moc_;
+ (NSString*)entityName;
+ (nullable NSEntityDescription*)entityInManagedObjectContext:(NSManagedObjectContext*)moc_;
@property (nonatomic, readonly, strong) HumanMOID *objectID;

@property (nonatomic, strong, readonly) NSNumber* age;

@property (atomic, readonly) uint16_t ageValue;
- (uint16_t)ageValue;

@property (nonatomic, strong) NSNumber* gender;

@property (atomic) Gender genderValue;
- (Gender)genderValue;
- (void)setGenderValue:(Gender)value_;

@property (nonatomic, strong, nullable) NSString* humanName;

@property (nonatomic, strong, nullable) NSManagedObject *meaninglessRelationship;

+ (NSArray*)fetchByHumanName:(NSManagedObjectContext*)moc_ humanName:(NSString*)humanName_ ;
+ (NSArray*)fetchByHumanName:(NSManagedObjectContext*)moc_ humanName:(NSString*)humanName_ error:(NSError**)error_;

+ (instancetype)fetchOneByHumanName:(NSManagedObjectContext*)moc_ humanName:(NSString*)humanName_ ;
+ (instancetype)fetchOneByHumanName:(NSManagedObjectContext*)moc_ humanName:(NSString*)humanName_ error:(NSError**)error_;

+ (NSArray*)fetchAllHumans:(NSManagedObjectContext*)moc_ ;
+ (NSArray*)fetchAllHumans:(NSManagedObjectContext*)moc_ error:(NSError**)error_;

@end

@interface _HumanMO (CoreDataGeneratedPrimitiveAccessors)

- (NSNumber*)primitiveAge;
- (void)setPrimitiveAge:(NSNumber*)value;

- (uint16_t)primitiveAgeValue;
- (void)setPrimitiveAgeValue:(uint16_t)value_;

- (NSNumber*)primitiveGender;
- (void)setPrimitiveGender:(NSNumber*)value;

- (Gender)primitiveGenderValue;
- (void)setPrimitiveGenderValue:(Gender)value_;

- (nullable NSString*)primitiveHumanName;
- (void)setPrimitiveHumanName:(nullable NSString*)value;

- (nullable NSManagedObject*)primitiveMeaninglessRelationship;
- (void)setPrimitiveMeaninglessRelationship:(nullable NSManagedObject*)value;

@end

@interface HumanMOAttributes: NSObject 
+ (NSString *)age;
+ (NSString *)gender;
+ (NSString *)humanName;
@end

@interface HumanMORelationships: NSObject
+ (NSString *)meaninglessRelationship;
@end

@interface HumanMOUserInfo: NSObject 
+ (NSString *)additionalHeaderFileName;
@end

NS_ASSUME_NONNULL_END
