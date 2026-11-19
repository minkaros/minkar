#import <CoreData/NSManagedObject.h>

@interface NSManagedObject (private)
- initWithObjectID:(NSManagedObjectID *)objectID managedObjectContext:(NSManagedObjectContext *)context;
+ (void)_registerEntity:(NSEntityDescription *)entity;
/* The value as kept: a relationship's by object ID (a to-many's a set). */
- (id)_primitiveValueForKey:(NSString *)key;
- (NSMutableSet *)_mutableIDSetForKey:(NSString *)key;
@end
