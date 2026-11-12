#import <QuartzCore/CATransition.h>
#import <Foundation/NSZone.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>

@implementation CATransition

-init {
   if((self=[super init])==nil)
    return nil;
   _endProgress=1;
   return self;
}

-(void)dealloc {
   [_type release];
   [_subtype release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   CATransition *copy=[super copyWithZone:zone];

   copy->_type=[_type copy];
   copy->_subtype=[_subtype copy];
   copy->_startProgress=_startProgress;
   copy->_endProgress=_endProgress;
   return copy;
}

-(NSString *)type {
   return _type;
}

-(void)setType:(NSString *)value {
   value=[value copy];
   [_type release];
   _type=value;
}

-(NSString *)subtype {
   return _subtype;
}

-(void)setSubtype:(NSString *)value {
   value=[value copy];
   [_subtype release];
   _subtype=value;
}

-(float)startProgress {
  return _startProgress;
}

-(void)setStartProgress:(float)value {
   _startProgress=value;
}

-(float)endProgress {
   return _endProgress;
}

-(void)setEndProgress:(float)value {
   _endProgress=value;
}

@end
