/* Copyright (c) 2008 Dan Knapp
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/Foundation.h>
#import <CoreData/CoreDataExports.h>
#import <CoreData/NSFetchRequest.h>

@class NSManagedObjectID, NSEntityDescription, NSManagedObjectContext, NSPropertyDescription;

NS_ASSUME_NONNULL_BEGIN

@interface NSManagedObject : NSObject <NSFetchRequestResult> {
    NSManagedObjectID *_objectID;
    NSManagedObjectContext *_context;
    BOOL _isInserted, _isUpdated, _isDeleted, _isFault;
    NSDictionary *_committedValues;
    NSMutableDictionary *_changedValues;
    void *_observationInfo;
}

+ (BOOL)contextShouldIgnoreUnmodeledPropertyChanges;
+ (NSEntityDescription *)entity;
+ (NSFetchRequest *)fetchRequest;

- (instancetype)initWithEntity:(NSEntityDescription *)entity insertIntoManagedObjectContext:(nullable NSManagedObjectContext *)context NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithContext:(NSManagedObjectContext *)moc;

@property (nullable, nonatomic, readonly, assign) NSManagedObjectContext *managedObjectContext;
@property (nonatomic, readonly, strong) NSEntityDescription *entity;
@property (nonatomic, readonly, strong) NSManagedObjectID *objectID;
@property (nonatomic, getter=isInserted, readonly) BOOL inserted;
@property (nonatomic, getter=isUpdated, readonly) BOOL updated;
@property (nonatomic, getter=isDeleted, readonly) BOOL deleted;
@property (nonatomic, readonly) BOOL hasChanges;
@property (nonatomic, readonly) BOOL hasPersistentChangedValues;
@property (nonatomic, getter=isFault, readonly) BOOL fault;

- (BOOL)hasFaultForRelationshipNamed:(NSString *)key;

- (void)awakeFromFetch;
- (void)awakeFromInsert;
- (NSDictionary<NSString *, id> *)changedValues;
- (NSDictionary<NSString *, id> *)committedValuesForKeys:(nullable NSArray<NSString *> *)keys;
- (void)didSave;
- (void)willTurnIntoFault;
- (void)didTurnIntoFault;
- (void)willSave;

- (nullable id)valueForKey:(NSString *)key;
- (void)setValue:(nullable id)value forKey:(NSString *)key;
- (NSMutableSet *)mutableSetValueForKey:(NSString *)key;
- (nullable id)primitiveValueForKey:(NSString *)key;
- (void)setPrimitiveValue:(nullable id)value forKey:(NSString *)key;

- (BOOL)validateValue:(id _Nullable * _Nonnull)value forKey:(NSString *)key error:(NSError **)error;
- (BOOL)validateForDelete:(NSError **)error;
- (BOOL)validateForInsert:(NSError **)error;
- (BOOL)validateForUpdate:(NSError **)error;

+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key;
- (void)didAccessValueForKey:(nullable NSString *)key;
- (void)willAccessValueForKey:(nullable NSString *)key;

@end

NS_ASSUME_NONNULL_END
