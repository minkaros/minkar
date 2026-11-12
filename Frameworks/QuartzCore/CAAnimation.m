#import <QuartzCore/CAAnimation.h>
#import <QuartzCore/CATransaction.h>
#import <Foundation/NSRaise.h>

NSString *const kCAFillModeForwards=@"forwards";
NSString *const kCAFillModeBackwards=@"backwards";
NSString *const kCAFillModeBoth=@"both";
NSString *const kCAFillModeRemoved=@"removed";

@implementation CAAnimation

+animation {
   return [[[self alloc] init] autorelease];
}

/* As on the reference platform: removed when it completes, at normal
 * speed, with no fill. */
-init {
   if((self=[super init])==nil)
    return nil;
   _duration=[CATransaction animationDuration];
   _timingFunction=[[CATransaction animationTimingFunction] retain];
   _removedOnCompletion=YES;
   _speed=1;
   _fillMode=[kCAFillModeRemoved copy];
   return self;
}

-(void)dealloc {
   [_delegate release];
   [_timingFunction release];
   [_fillMode release];
   [super dealloc];
}

/* A layer keeps a copy of what it is given: its delegate, timing and
 * fill go with it. */
-copyWithZone:(NSZone *)zone {
   CAAnimation *copy=[[[self class] allocWithZone:zone] init];

   copy->_delegate=[_delegate retain];
   copy->_removedOnCompletion=_removedOnCompletion;
   [copy->_timingFunction release];
   copy->_timingFunction=[_timingFunction retain];
   copy->_autoreverses=_autoreverses;
   copy->_beginTime=_beginTime;
   copy->_duration=_duration;
   [copy->_fillMode release];
   copy->_fillMode=[_fillMode copy];
   copy->_repeatCount=_repeatCount;
   copy->_repeatDuration=_repeatDuration;
   copy->_speed=_speed;
   copy->_timeOffset=_timeOffset;
   return copy;
}

-delegate {
   return _delegate;
}

-(void)setDelegate:object {
   object=[object retain];
   [_delegate release];
   _delegate=object;
}

-(BOOL)isRemovedOnCompletion {
   return _removedOnCompletion;
}

-(void)setRemovedOnCompletion:(BOOL)value {
   _removedOnCompletion=value;
}

-(CAMediaTimingFunction *)timingFunction {
   return _timingFunction;
}

-(void)setTimingFunction:(CAMediaTimingFunction *)value {
   value=[value retain];
   [_timingFunction release];
   _timingFunction=value;
}

-(BOOL)autoreverses {
   return _autoreverses;
}

-(void)setAutoreverses:(BOOL)value {
   _autoreverses=value;
}

-(CFTimeInterval)beginTime {
   return _beginTime;
}

-(void)setBeginTime:(CFTimeInterval)value {
   _beginTime=value;
}

-(CFTimeInterval)duration {
   return _duration;
}

-(void)setDuration:(CFTimeInterval)value {
   _duration=value;
}

-(NSString *)fillMode {
   return _fillMode;
}

-(void)setFillMode:(NSString *)value {
   value=[value copy];
   [_fillMode release];
   _fillMode=value;
}

-(float)repeatCount {
   return _repeatCount;
}

-(void)setRepeatCount:(float)value {
   _repeatCount=value;
}

-(CFTimeInterval)repeatDuration {
   return _repeatDuration;
}

-(void)setRepeatDuration:(CFTimeInterval)value {
   _repeatDuration=value;
}

-(float)speed {
   return _speed;
}

-(void)setSpeed:(float)value {
   _speed=value;
}

-(CFTimeInterval)timeOffset {
   return _timeOffset;
}

-(void)setTimeOffset:(CFTimeInterval)value {
   _timeOffset=value;
}

@end
