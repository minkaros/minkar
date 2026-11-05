// DO NOT EDIT. This file is machine-generated and constantly overwritten.
// Make changes to ConcreteEntityWithCustomClass1.h instead.

#if __has_feature(modules)
    @import Foundation;
    @import CoreData;
#else
    #import <Foundation/Foundation.h>
    #import <CoreData/CoreData.h>
#endif

NS_ASSUME_NONNULL_BEGIN

@interface ConcreteEntityWithCustomClass1ID : NSManagedObjectID {}
@end

@interface _ConcreteEntityWithCustomClass1 : NSManagedObject
+ (instancetype)insertInManagedObjectContext:(NSManagedObjectContext *)moc_;
+ (NSString*)entityName;
+ (nullable NSEntityDescription*)entityInManagedObjectContext:(NSManagedObjectContext*)moc_;
@property (nonatomic, readonly, strong) ConcreteEntityWithCustomClass1ID *objectID;

@property (nonatomic, strong, nullable) NSNumber* dummy2;

@property (atomic) int16_t dummy2Value;
- (int16_t)dummy2Value;
- (void)setDummy2Value:(int16_t)value_;

@end

@interface _ConcreteEntityWithCustomClass1 (CoreDataGeneratedPrimitiveAccessors)

- (nullable NSNumber*)primitiveDummy2;
- (void)setPrimitiveDummy2:(nullable NSNumber*)value;

- (int16_t)primitiveDummy2Value;
- (void)setPrimitiveDummy2Value:(int16_t)value_;

@end

@interface ConcreteEntityWithCustomClass1Attributes: NSObject 
+ (NSString *)dummy2;
@end

NS_ASSUME_NONNULL_END
