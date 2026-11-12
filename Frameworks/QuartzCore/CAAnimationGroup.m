#import <QuartzCore/CAAnimationGroup.h>
#import <Foundation/NSZone.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>

@implementation CAAnimationGroup

-(void)dealloc {
   [_animations release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   CAAnimationGroup *copy=[super copyWithZone:zone];

   copy->_animations=[_animations copy];
   return copy;
}

-(NSArray *)animations {
   return _animations;
}

-(void)setAnimations:(NSArray *)value {
   value=[value copy];
   [_animations release];
   _animations=value;
}

@end
