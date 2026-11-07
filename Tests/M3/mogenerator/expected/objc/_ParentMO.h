// DO NOT EDIT. This file is machine-generated and constantly overwritten.
// Make changes to ParentMO.h instead.

#if __has_feature(modules)
    @import Foundation;
    @import CoreData;
#else
    #import <Foundation/Foundation.h>
    #import <CoreData/CoreData.h>
#endif

#import "HumanMO.h"

NS_ASSUME_NONNULL_BEGIN

@class ChildMO;
@class ChildMO;

@class NSObject;

@interface ParentMOID : HumanMOID {}
@end

@interface _ParentMO : HumanMO
+ (instancetype)insertInManagedObjectContext:(NSManagedObjectContext *)moc_;
+ (NSString*)entityName;
+ (nullable NSEntityDescription*)entityInManagedObjectContext:(NSManagedObjectContext*)moc_;
@property (nonatomic, readonly, strong) ParentMOID *objectID;

@property (nonatomic, strong, nullable) NSData* myBinaryData;

@property (nonatomic, strong, nullable) NSNumber* myBoolean;

@property (atomic) BOOL myBooleanValue;
- (BOOL)myBooleanValue;
- (void)setMyBooleanValue:(BOOL)value_;

@property (nonatomic, strong, nullable) NSDate* myDate;

@property (nonatomic, strong, nullable) NSDecimalNumber* myDecimal;

@property (nonatomic, strong, nullable) NSNumber* myDouble;

@property (atomic) double myDoubleValue;
- (double)myDoubleValue;
- (void)setMyDoubleValue:(double)value_;

@property (nonatomic, strong, nullable) NSNumber* myFloat;

@property (atomic) float myFloatValue;
- (float)myFloatValue;
- (void)setMyFloatValue:(float)value_;

@property (nonatomic, strong, nullable) NSNumber* myInt16;

@property (atomic) int16_t myInt16Value;
- (int16_t)myInt16Value;
- (void)setMyInt16Value:(int16_t)value_;

@property (nonatomic, strong, nullable) NSNumber* myInt16Transient;

@property (atomic) int16_t myInt16TransientValue;
- (int16_t)myInt16TransientValue;
- (void)setMyInt16TransientValue:(int16_t)value_;

@property (nonatomic, strong, nullable) NSNumber* myInt32;

@property (atomic) int32_t myInt32Value;
- (int32_t)myInt32Value;
- (void)setMyInt32Value:(int32_t)value_;

@property (nonatomic, strong, nullable) NSNumber* myInt64;

@property (atomic) int64_t myInt64Value;
- (int64_t)myInt64Value;
- (void)setMyInt64Value:(int64_t)value_;

@property (nonatomic, strong, nullable) NSString* myString;

@property (nonatomic, strong, nullable) id myTransformableSansClassName;

@property (nonatomic, strong, nullable) NSString* parentName;

@property (nonatomic, strong, nullable) NSSet<ChildMO*> *children;
- (nullable NSMutableSet<ChildMO*>*)childrenSet;

@property (nonatomic, strong, nullable) NSOrderedSet<ChildMO*> *orderedChildren;
- (nullable NSMutableOrderedSet<ChildMO*>*)orderedChildrenSet;

@end

@interface _ParentMO (ChildrenCoreDataGeneratedAccessors)
- (void)addChildren:(NSSet<ChildMO*>*)value_;
- (void)removeChildren:(NSSet<ChildMO*>*)value_;
- (void)addChildrenObject:(ChildMO*)value_;
- (void)removeChildrenObject:(ChildMO*)value_;

@end

@interface _ParentMO (OrderedChildrenCoreDataGeneratedAccessors)
- (void)addOrderedChildren:(NSOrderedSet<ChildMO*>*)value_;
- (void)removeOrderedChildren:(NSOrderedSet<ChildMO*>*)value_;
- (void)addOrderedChildrenObject:(ChildMO*)value_;
- (void)removeOrderedChildrenObject:(ChildMO*)value_;

- (void)insertObject:(ChildMO*)value inOrderedChildrenAtIndex:(NSUInteger)idx;
- (void)removeObjectFromOrderedChildrenAtIndex:(NSUInteger)idx;
- (void)insertOrderedChildren:(NSArray *)value atIndexes:(NSIndexSet *)indexes;
- (void)removeOrderedChildrenAtIndexes:(NSIndexSet *)indexes;
- (void)replaceObjectInOrderedChildrenAtIndex:(NSUInteger)idx withObject:(ChildMO*)value;
- (void)replaceOrderedChildrenAtIndexes:(NSIndexSet *)indexes withOrderedChildren:(NSArray *)values;

@end

@interface _ParentMO (CoreDataGeneratedPrimitiveAccessors)

- (nullable NSData*)primitiveMyBinaryData;
- (void)setPrimitiveMyBinaryData:(nullable NSData*)value;

- (nullable NSNumber*)primitiveMyBoolean;
- (void)setPrimitiveMyBoolean:(nullable NSNumber*)value;

- (BOOL)primitiveMyBooleanValue;
- (void)setPrimitiveMyBooleanValue:(BOOL)value_;

- (nullable NSDate*)primitiveMyDate;
- (void)setPrimitiveMyDate:(nullable NSDate*)value;

- (nullable NSDecimalNumber*)primitiveMyDecimal;
- (void)setPrimitiveMyDecimal:(nullable NSDecimalNumber*)value;

- (nullable NSNumber*)primitiveMyDouble;
- (void)setPrimitiveMyDouble:(nullable NSNumber*)value;

- (double)primitiveMyDoubleValue;
- (void)setPrimitiveMyDoubleValue:(double)value_;

- (nullable NSNumber*)primitiveMyFloat;
- (void)setPrimitiveMyFloat:(nullable NSNumber*)value;

- (float)primitiveMyFloatValue;
- (void)setPrimitiveMyFloatValue:(float)value_;

- (nullable NSNumber*)primitiveMyInt16;
- (void)setPrimitiveMyInt16:(nullable NSNumber*)value;

- (int16_t)primitiveMyInt16Value;
- (void)setPrimitiveMyInt16Value:(int16_t)value_;

- (nullable NSNumber*)primitiveMyInt16Transient;
- (void)setPrimitiveMyInt16Transient:(nullable NSNumber*)value;

- (int16_t)primitiveMyInt16TransientValue;
- (void)setPrimitiveMyInt16TransientValue:(int16_t)value_;

- (nullable NSNumber*)primitiveMyInt32;
- (void)setPrimitiveMyInt32:(nullable NSNumber*)value;

- (int32_t)primitiveMyInt32Value;
- (void)setPrimitiveMyInt32Value:(int32_t)value_;

- (nullable NSNumber*)primitiveMyInt64;
- (void)setPrimitiveMyInt64:(nullable NSNumber*)value;

- (int64_t)primitiveMyInt64Value;
- (void)setPrimitiveMyInt64Value:(int64_t)value_;

- (nullable NSString*)primitiveMyString;
- (void)setPrimitiveMyString:(nullable NSString*)value;

- (nullable id)primitiveMyTransformableSansClassName;
- (void)setPrimitiveMyTransformableSansClassName:(nullable id)value;

- (nullable NSString*)primitiveParentName;
- (void)setPrimitiveParentName:(nullable NSString*)value;

- (NSMutableSet<ChildMO*>*)primitiveChildren;
- (void)setPrimitiveChildren:(NSMutableSet<ChildMO*>*)value;

- (NSMutableOrderedSet<ChildMO*>*)primitiveOrderedChildren;
- (void)setPrimitiveOrderedChildren:(NSMutableOrderedSet<ChildMO*>*)value;

@end

@interface ParentMOAttributes: NSObject 
+ (NSString *)myBinaryData;
+ (NSString *)myBoolean;
+ (NSString *)myDate;
+ (NSString *)myDecimal;
+ (NSString *)myDouble;
+ (NSString *)myFloat;
+ (NSString *)myInt16;
+ (NSString *)myInt16Transient;
+ (NSString *)myInt32;
+ (NSString *)myInt64;
+ (NSString *)myString;
+ (NSString *)myTransformableSansClassName;
+ (NSString *)parentName;
@end

@interface ParentMORelationships: NSObject
+ (NSString *)children;
+ (NSString *)orderedChildren;
@end

NS_ASSUME_NONNULL_END
