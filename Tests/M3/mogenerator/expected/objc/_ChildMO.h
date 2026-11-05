// DO NOT EDIT. This file is machine-generated and constantly overwritten.
// Make changes to ChildMO.h instead.

#if __has_feature(modules)
    @import Foundation;
    @import CoreData;
#else
    #import <Foundation/Foundation.h>
    #import <CoreData/CoreData.h>
#endif

#import "HumanMO.h"

NS_ASSUME_NONNULL_BEGIN

@class ParentMO;

@interface ChildMOID : HumanMOID {}
@end

@interface _ChildMO : HumanMO
+ (instancetype)insertInManagedObjectContext:(NSManagedObjectContext *)moc_;
+ (NSString*)entityName;
+ (nullable NSEntityDescription*)entityInManagedObjectContext:(NSManagedObjectContext*)moc_;
@property (nonatomic, readonly, strong) ChildMOID *objectID;

@property (nonatomic, strong, nullable) NSString* childName;

@property (nonatomic, strong, nullable) NSNumber* type;

@property (atomic) int16_t typeValue;
- (int16_t)typeValue;
- (void)setTypeValue:(int16_t)value_;

@property (nonatomic, strong, nullable) ParentMO *parent;

+ (NSArray*)fetchByParent:(NSManagedObjectContext*)moc_ parent:(ParentMO*)parent_ ;
+ (NSArray*)fetchByParent:(NSManagedObjectContext*)moc_ parent:(ParentMO*)parent_ error:(NSError**)error_;

+ (NSArray*)fetchByParentName:(NSManagedObjectContext*)moc_ parentName:(NSString*)parentName_ ;
+ (NSArray*)fetchByParentName:(NSManagedObjectContext*)moc_ parentName:(NSString*)parentName_ error:(NSError**)error_;

@end

@interface _ChildMO (CoreDataGeneratedPrimitiveAccessors)

- (nullable NSString*)primitiveChildName;
- (void)setPrimitiveChildName:(nullable NSString*)value;

- (nullable ParentMO*)primitiveParent;
- (void)setPrimitiveParent:(nullable ParentMO*)value;

@end

@interface ChildMOAttributes: NSObject 
+ (NSString *)childName;
+ (NSString *)type;
@end

@interface ChildMORelationships: NSObject
+ (NSString *)parent;
@end

NS_ASSUME_NONNULL_END
