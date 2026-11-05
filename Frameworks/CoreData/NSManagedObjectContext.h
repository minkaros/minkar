/* Copyright(c)2008 Dan Knapp

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>
#import <CoreData/CoreDataExports.h>

@class NSManagedObject, NSManagedObjectID, NSFetchRequest, NSPersistentStore, NSPersistentStoreCoordinator;

COREDATA_EXPORT NSString *const NSManagedObjectContextWillSaveNotification;
COREDATA_EXPORT NSString *const NSManagedObjectContextDidSaveNotification;

COREDATA_EXPORT NSString *const NSInsertedObjectsKey;
COREDATA_EXPORT NSString *const NSUpdatedObjectsKey;
COREDATA_EXPORT NSString *const NSDeletedObjectsKey;
COREDATA_EXPORT NSString *const NSRefreshedObjectsKey;
COREDATA_EXPORT NSString *const NSInvalidatedObjectsKey;
COREDATA_EXPORT NSString *const NSInvalidatedAllObjectsKey;


typedef NS_ENUM(NSUInteger, NSManagedObjectContextConcurrencyType) {
    NSConfinementConcurrencyType = 0x00,
    NSPrivateQueueConcurrencyType = 0x01,
    NSMainQueueConcurrencyType = 0x02,
};

NS_ASSUME_NONNULL_BEGIN

@interface NSManagedObjectContext : NSObject <NSLocking> {
    NSLock *_lock;
    NSPersistentStoreCoordinator *_storeCoordinator;
    NSUndoManager *_undoManager;
    BOOL _retainsRegisteredObjects;
    BOOL _propagatesDeletesAtEndOfEvent;
    NSTimeInterval _stalenessInterval;
    id _mergePolicy;

    NSMutableSet *_registeredObjects;

    NSMutableSet *_insertedObjects;
    NSMutableSet *_updatedObjects;
    NSMutableSet *_deletedObjects;

    NSMapTable *_objectIdToObject;

    BOOL _hasChanges;

    //
    BOOL _requestedProcessPendingChanges;
    NSManagedObjectContextConcurrencyType _concurrencyType;
    dispatch_queue_t _queue;
    NSString *_name;
}

- (instancetype)initWithConcurrencyType:(NSManagedObjectContextConcurrencyType)type;
@property (readonly) NSManagedObjectContextConcurrencyType concurrencyType;
- (void)performBlock:(void (^)(void))block;
- (void)performBlockAndWait:(void (NS_NOESCAPE ^)(void))block;
@property (nullable, copy) NSString *name;


@property (nullable, strong) NSPersistentStoreCoordinator *persistentStoreCoordinator;
@property (nullable, nonatomic, strong) NSUndoManager *undoManager;
@property (nonatomic) BOOL retainsRegisteredObjects;
@property BOOL propagatesDeletesAtEndOfEvent;
@property NSTimeInterval stalenessInterval;
@property (strong) id mergePolicy;

@property (nonatomic, readonly, strong) NSSet<__kindof NSManagedObject *> *registeredObjects;
@property (nonatomic, readonly, strong) NSSet<__kindof NSManagedObject *> *insertedObjects;
@property (nonatomic, readonly, strong) NSSet<__kindof NSManagedObject *> *updatedObjects;
@property (nonatomic, readonly, strong) NSSet<__kindof NSManagedObject *> *deletedObjects;
@property (nonatomic, readonly) BOOL hasChanges;

- (void)lock;
- (void)unlock;
- (BOOL)tryLock;

- (void)undo;
- (void)redo;
- (void)reset;
- (void)rollback;

- (nullable __kindof NSManagedObject *)objectRegisteredForID:(NSManagedObjectID *)objectID;

- (__kindof NSManagedObject *)objectWithID:(NSManagedObjectID *)objectID;

- (nullable NSArray *)executeFetchRequest:(NSFetchRequest *)request error:(NSError **)error;
- (NSUInteger)countForFetchRequest:(NSFetchRequest *)request error:(NSError **)error __attribute__((swift_error(nonnull_error)));

- (void)insertObject:(NSManagedObject *)object;
- (void)deleteObject:(NSManagedObject *)object;

- (void)assignObject:(id)object toPersistentStore:(NSPersistentStore *)store;

- (void)detectConflictsForObject:(NSManagedObject *)object;

- (void)refreshObject:(NSManagedObject *)object mergeChanges:(BOOL)flag;

- (void)processPendingChanges;

- (BOOL)obtainPermanentIDsForObjects:(NSArray<NSManagedObject *> *)objects error:(NSError **)error;
- (BOOL)save:(NSError **)error;
- (void)mergeChangesFromContextDidSaveNotification:(NSNotification *)notification;

- (BOOL)commitEditing;
- (void)commitEditingWithDelegate:(nullable id)delegate didCommitSelector:(nullable SEL)didCommitSelector contextInfo:(nullable void *)contextInfo;
- (void)discardEditing;
- (void)objectDidBeginEditing:(id)editor;
- (void)objectDidEndEditing:(id)editor;

@end

NS_ASSUME_NONNULL_END
