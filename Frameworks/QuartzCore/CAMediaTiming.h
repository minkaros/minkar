#import <QuartzCore/CABase.h>
#import <CoreFoundation/CoreFoundation.h>

CA_EXPORT NSString *const kCAFillModeForwards;
CA_EXPORT NSString *const kCAFillModeBackwards;
CA_EXPORT NSString *const kCAFillModeBoth;
CA_EXPORT NSString *const kCAFillModeRemoved;

@protocol CAMediaTiming

@property BOOL autoreverses;

@property CFTimeInterval beginTime;

@property CFTimeInterval duration;

@property(copy) NSString *fillMode;

@property float repeatCount;

@property CFTimeInterval repeatDuration;

@property float speed;

@property CFTimeInterval timeOffset;

@end
