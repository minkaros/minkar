#import "NSManagedObjectSetEnumerator.h"
#import <CoreData/NSManagedObjectContext.h>
#import <CoreData/NSManagedObject.h>

@implementation NSManagedObjectSetEnumerator

-initWithManagedObjectContext:(NSManagedObjectContext *)context objectEnumerator:(NSEnumerator *)enumerator {
   _context=[context retain];
   _enumerator=[enumerator retain];
   return self;
}

-(void)dealloc {
   [_context release];
   [_enumerator release];
   [super dealloc];
}

-nextObject {
   id objectID=[_enumerator nextObject];
   
   if(objectID==nil)
    return nil;
   
   /* An object kept as itself (by an older primitive change) is itself. */
   if([objectID isKindOfClass:[NSManagedObject class]])
    return objectID;
   return [_context objectWithID:objectID];
}

@end
