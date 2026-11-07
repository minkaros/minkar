#import <CoreData/NSEntityDescription.h>

@interface NSEntityDescription (private)
- (BOOL)_isKindOfEntity:(NSEntityDescription *)other;
- (void)_checkMutable;
- (void)_invalidateCaches;
- (void)_setManagedObjectModel:(NSManagedObjectModel *)model;
- (Class)_managedObjectClass;
@end
