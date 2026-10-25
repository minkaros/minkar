/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import "NSNumber_BOOL.h"
#import <CoreFoundation/CFNumber.h>
#import <CoreFoundation/CFBridging.h>
#import <Foundation/NSException.h>
#import <Foundation/NSStringFormatter.h>
#include <limits.h>

/* The value is the identity: kCFBooleanTrue or kCFBooleanFalse. */
#define VALUE(self) ((CFBooleanRef)(self) == kCFBooleanTrue)

@implementation __NSCFBoolean

NSNumber *NSNumber_BOOLNew(NSZone *zone, BOOL value) {
   (void)zone;
   return (NSNumber *)(value ? kCFBooleanTrue : kCFBooleanFalse);
}

+ (id)allocWithZone:(NSZone *)zone {
   [NSException raise:NSInternalInconsistencyException format:@"Private class __NSCFBoolean is not intended to be alloced."];
   return nil;
}

// CF's constant booleans are never freed.
-(void)dealloc {
   return;
   [super dealloc];  // Silence compiler warning
}

-(id)retain {
   return self;
}

- (oneway void)release
{
}

-(id)autorelease {
   return self;
}

-(NSUInteger)retainCount {
   return UINT_MAX;
}

-(id)copyWithZone:(NSZone *)zone {
   return self;
}

-(void)getValue:(void *)value {
   *((BOOL *)value)=VALUE(self);
}

-(const char *)objCType {
   return @encode(BOOL);
}

-(char)charValue { return VALUE(self); }
-(unsigned char)unsignedCharValue { return VALUE(self); }
-(short)shortValue { return VALUE(self); }
-(unsigned short)unsignedShortValue { return VALUE(self); }
-(int)intValue { return VALUE(self); }
-(unsigned int)unsignedIntValue { return VALUE(self); }
-(long)longValue { return VALUE(self); }
-(unsigned long)unsignedLongValue { return VALUE(self); }
-(long long)longLongValue { return VALUE(self); }
-(unsigned long long)unsignedLongLongValue { return VALUE(self); }
-(float)floatValue { return VALUE(self); }
-(double)doubleValue { return VALUE(self); }
-(BOOL)boolValue { return VALUE(self); }
-(NSInteger)integerValue { return VALUE(self); }
-(NSUInteger)unsignedIntegerValue { return VALUE(self); }

-(NSString *)descriptionWithLocale:(NSDictionary *)locale {
   return NSStringWithFormatAndLocale(@"%i",locale,(int)VALUE(self));
}

-(BOOL)isEqual:other {
   if (other == self)
      return YES;
   /* A plain CF object (a CFNumber, say) cannot receive messages. */
   if (other == nil || _CFIsNativeCFObject((CFTypeRef)other))
      return NO;
   return [super isEqual:other];
}

/* CF asks for these (CFGetTypeID, CFEqual, CFHash) without calling back. */
- (CFTypeID)_cfTypeID {
   return CFBooleanGetTypeID();
}

@end
