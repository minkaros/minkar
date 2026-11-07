#import <Foundation/NSZone.h>
#import <Foundation/NSNumber.h>
#import <CoreFoundation/CFNumber.h>

@interface NSNumber_CF : NSNumber {
  @public
    /* CF's header after isa (CFRuntimeBase): NSObject has only isa. */
    uint32_t _cfinfo;
    uint32_t _rc;
    CFNumberType _type;
}

@end
