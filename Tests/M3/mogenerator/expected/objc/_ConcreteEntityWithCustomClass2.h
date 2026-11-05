// DO NOT EDIT. This file is machine-generated and constantly overwritten.
// Make changes to ConcreteEntityWithCustomClass2.h instead.

#if __has_feature(modules)
    @import Foundation;
    @import CoreData;
#else
    #import <Foundation/Foundation.h>
    #import <CoreData/CoreData.h>
#endif

NS_ASSUME_NONNULL_BEGIN

@interface ConcreteEntityWithCustomClass2ID : NSManagedObjectID {}
@end

@interface _ConcreteEntityWithCustomClass2 : NSManagedObject
+ (instancetype)insertInManagedObjectContext:(NSManagedObjectContext *)moc_;
+ (NSString*)entityName;
+ (nullable NSEntityDescription*)entityInManagedObjectContext:(NSManagedObjectContext*)moc_;
@property (nonatomic, readonly, strong) ConcreteEntityWithCustomClass2ID *objectID;

@property (nonatomic, strong, nullable) NSNumber* dummy3;

@property (atomic) int16_t dummy3Value;
- (int16_t)dummy3Value;
- (void)setDummy3Value:(int16_t)value_;

@end

@interface _ConcreteEntityWithCustomClass2 (CoreDataGeneratedPrimitiveAccessors)

- (nullable NSNumber*)primitiveDummy3;
- (void)setPrimitiveDummy3:(nullable NSNumber*)value;

- (int16_t)primitiveDummy3Value;
- (void)setPrimitiveDummy3Value:(int16_t)value_;

@end

@interface ConcreteEntityWithCustomClass2Attributes: NSObject 
+ (NSString *)dummy3;
@end

NS_ASSUME_NONNULL_END
