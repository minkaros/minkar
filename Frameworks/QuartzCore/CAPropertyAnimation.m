#import <QuartzCore/CAAnimation.h>
#import <Foundation/NSZone.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>

@implementation CAPropertyAnimation

-initWithKeyPath:(NSString *)keyPath {
   [super init];
   _keyPath=[keyPath copy];
   _additive=NO;
   _cumulative=NO;
   return self;
}

+animationWithKeyPath:(NSString *)keyPath {
   return [[[self alloc] initWithKeyPath:keyPath] autorelease];
}

-(void)dealloc {
   [_keyPath release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   CAPropertyAnimation *copy=[super copyWithZone:zone];

   copy->_keyPath=[_keyPath copy];
   copy->_additive=_additive;
   copy->_cumulative=_cumulative;
   return copy;
}

-(NSString *)keyPath {
   return _keyPath;
}

-(void)setKeyPath:(NSString *)value {
   value=[value copy];
   [_keyPath release];
   _keyPath=value;
}

-(BOOL)isAdditive {
   return _additive;
}

-(void)setAdditive:(BOOL)value {
   _additive=value;
}

-(BOOL)isCumulative {
   return _cumulative;
}

-(void)setCumulative:(BOOL)value {
   _cumulative=value;
}

@end
