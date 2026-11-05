/* Copyright (c) 2008 Dan Knapp
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/Foundation.h>
#import <CoreData/CoreDataExports.h>

@class NSEntityDescription, NSManagedObjectContext;

NS_ASSUME_NONNULL_BEGIN

// What a fetch can give: managed objects, their IDs, dictionaries, counts.
@protocol NSFetchRequestResult <NSObject>
@end

@interface NSNumber (NSFetchedResultSupport) <NSFetchRequestResult>
@end

@interface NSDictionary (NSFetchedResultSupport) <NSFetchRequestResult>
@end

typedef NS_OPTIONS(NSUInteger, NSFetchRequestResultType) {
    NSManagedObjectResultType = 0x00,
    NSManagedObjectIDResultType = 0x01,
    NSDictionaryResultType = 0x02,
    NSCountResultType = 0x04,
};

@interface NSFetchRequest<__covariant ResultType : id<NSFetchRequestResult>> : NSObject <NSCoding, NSCopying> {
    NSFetchRequestResultType _resultType;
    NSEntityDescription *_entity;
    NSPredicate *_predicate;
    NSArray *_sortDescriptors;
    NSArray *_affectedStores;
    NSUInteger _fetchLimit;
    NSUInteger _fetchBatchSize;
    NSUInteger _fetchOffset;
    BOOL _includesPendingChanges;
    BOOL _includesPropertyValues;
    BOOL _includesSubentities;
    BOOL _returnsDistinctResults;
    BOOL _returnsObjectsAsFaults;
    NSArray *_propertiesToFetch;
    NSArray *_relationshipKeyPathsForPrefetching;
    NSString *_entityName;
}

+ (instancetype)fetchRequestWithEntityName:(NSString *)entityName;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithEntityName:(NSString *)entityName;

@property (nullable, nonatomic, strong) NSEntityDescription *entity;
@property (nullable, nonatomic, readonly, strong) NSString *entityName;
@property (nullable, nonatomic, strong) NSPredicate *predicate;
@property (nullable, nonatomic, strong) NSArray<NSSortDescriptor *> *sortDescriptors;
@property (nonatomic) NSUInteger fetchLimit;
@property (nullable, nonatomic, strong) NSArray *affectedStores;
@property (nonatomic) NSFetchRequestResultType resultType;
@property (nonatomic) BOOL includesSubentities;
@property (nonatomic) BOOL includesPropertyValues;
@property (nonatomic) BOOL returnsObjectsAsFaults;
@property (nullable, nonatomic, copy) NSArray<NSString *> *relationshipKeyPathsForPrefetching;
@property (nonatomic) BOOL includesPendingChanges;
@property (nonatomic) BOOL returnsDistinctResults;
@property (nullable, nonatomic, copy) NSArray *propertiesToFetch;
@property (nonatomic) NSUInteger fetchOffset;
@property (nonatomic) NSUInteger fetchBatchSize;

@end

NS_ASSUME_NONNULL_END
