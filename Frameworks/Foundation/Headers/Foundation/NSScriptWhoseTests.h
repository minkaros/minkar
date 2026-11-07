#import <Foundation/NSObject.h>

@class NSString;

NS_ASSUME_NONNULL_BEGIN

@interface NSObject (NSComparisonMethods)
- (BOOL)isEqualTo:(nullable id)object;
- (BOOL)isLessThanOrEqualTo:(nullable id)object;
- (BOOL)isLessThan:(nullable id)object;
- (BOOL)isGreaterThanOrEqualTo:(nullable id)object;
- (BOOL)isGreaterThan:(nullable id)object;
- (BOOL)isNotEqualTo:(nullable id)object;
- (BOOL)doesContain:(id)object;
- (BOOL)isLike:(NSString *)object;
- (BOOL)isCaseInsensitiveLike:(NSString *)object;
@end

NS_ASSUME_NONNULL_END
