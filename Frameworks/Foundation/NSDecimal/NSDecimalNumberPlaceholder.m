#import "NSDecimalNumberPlaceholder.h"
#import <Foundation/NSRaise.h>
#import <Foundation/NSCoder.h>

@implementation NSDecimalNumberPlaceholder


/* Archives name this class for decimal numbers: the real one decodes. */
- initWithCoder:(NSCoder *)coder
{
    [self dealloc];
    return (NSDecimalNumberPlaceholder *)[[NSDecimalNumber alloc] initWithCoder:coder];
}

@end
