#import <QuartzCore/CABasicAnimation.h>
#import <Foundation/NSZone.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>

@implementation CABasicAnimation

-(void)dealloc {
   [_fromValue release];
   [_toValue release];
   [_byValue release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
   CABasicAnimation *copy=[super copyWithZone:zone];

   copy->_fromValue=[_fromValue retain];
   copy->_toValue=[_toValue retain];
   copy->_byValue=[_byValue retain];
   return copy;
}

-fromValue {
   return _fromValue;
}

-(void)setFromValue:value {
   value=[value retain];
   [_fromValue release];
   _fromValue=value;
}

-toValue {
   return _toValue;
}

-(void)setToValue:value {
   value=[value retain];
   [_toValue release];
   _toValue=value;
}

-byValue {
   return _byValue;
}

-(void)setByValue:value {
   value=[value retain];
   [_byValue release];
   _byValue=value;
}

@end
