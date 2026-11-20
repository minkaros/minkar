#import <Foundation/NSObject.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSZone.h>

@class NSScriptObjectSpecifier, NSString, NSSpecifierTest;

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

typedef NS_ENUM(NSUInteger, NSTestComparisonOperation) {
    NSEqualToComparison = 0,
    NSLessThanOrEqualToComparison,
    NSLessThanComparison,
    NSGreaterThanOrEqualToComparison,
    NSGreaterThanComparison,
    NSBeginsWithComparison,
    NSEndsWithComparison,
    NSContainsComparison
};

/* A test of a "whose" clause, true or false for each object tested. */
@interface NSScriptWhoseTest : NSObject <NSCoding>
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;
- (BOOL)isTrue;
@end

/* Tests joined: all true, any true, or one false. */
@interface NSLogicalTest : NSScriptWhoseTest
- (instancetype)initAndTestWithTests:(NSArray<NSSpecifierTest *> *)subTests NS_DESIGNATED_INITIALIZER;
- (instancetype)initOrTestWithTests:(NSArray<NSSpecifierTest *> *)subTests NS_DESIGNATED_INITIALIZER;
- (instancetype)initNotTestWithTest:(NSScriptWhoseTest *)subTest NS_DESIGNATED_INITIALIZER;
@end

/* An object's value (by a specifier relative to it) compared with
   another. */
@interface NSSpecifierTest : NSScriptWhoseTest
- (instancetype)initWithCoder:(NSCoder *)inCoder NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithObjectSpecifier:(nullable NSScriptObjectSpecifier *)obj1 comparisonOperator:(NSTestComparisonOperation)compOp testObject:(nullable id)obj2 NS_DESIGNATED_INITIALIZER;
@end

NS_ASSUME_NONNULL_END
