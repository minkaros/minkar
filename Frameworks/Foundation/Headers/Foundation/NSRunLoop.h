/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSDate.h>
#import <Foundation/NSMapTable.h>
#import <CoreFoundation/CFRunLoop.h>

@class NSTimer, NSDate, NSArray<ObjectType>, NSMutableArray<ObjectType>, NSInputSource, NSPort, NSPipe;

NS_ASSUME_NONNULL_BEGIN

typedef NSString *NSRunLoopMode NS_TYPED_EXTENSIBLE_ENUM NS_SWIFT_NAME(RunLoop.Mode);

FOUNDATION_EXPORT NSRunLoopMode const NSDefaultRunLoopMode NS_SWIFT_NAME(default);
FOUNDATION_EXPORT NSRunLoopMode const NSRunLoopCommonModes NS_SWIFT_NAME(common);

@interface NSRunLoop : NSObject {
    NSMapTable *_modes;
    NSMutableArray *_commonModes;
    NSString *_currentMode;
    NSMutableArray *_continue;
    NSMutableArray *_orderedPerforms;
    void *_cfRunLoop;
}

@property (class, readonly, strong) NSRunLoop *currentRunLoop;
@property (class, readonly, strong) NSRunLoop *mainRunLoop;

@property (nullable, readonly, copy) NSRunLoopMode currentMode;
- (CFRunLoopRef)getCFRunLoop CF_RETURNS_NOT_RETAINED;

- (nullable NSDate *)limitDateForMode:(NSRunLoopMode)mode;
- (void)acceptInputForMode:(NSRunLoopMode)mode beforeDate:(NSDate *)limitDate;
- (BOOL)runMode:(NSRunLoopMode)mode beforeDate:(NSDate *)limitDate;
- (void)runUntilDate:(NSDate *)limitDate;
- (void)run;

- (void)addPort:(NSPort *)aPort forMode:(NSRunLoopMode)mode;
- (void)removePort:(NSPort *)aPort forMode:(NSRunLoopMode)mode;
- (void)addInputSource:(NSInputSource *)source forMode:(NSRunLoopMode)mode;
- (void)removeInputSource:(NSInputSource *)source forMode:(NSRunLoopMode)mode;
- (void)addTimer:(NSTimer *)timer forMode:(NSRunLoopMode)mode;

- (void)performSelector:(SEL)aSelector target:(id)target argument:(nullable id)arg order:(NSUInteger)order modes:(NSArray<NSRunLoopMode> *)modes;
- (void)cancelPerformSelector:(SEL)aSelector target:(id)target argument:(nullable id)arg;
- (void)cancelPerformSelectorsWithTarget:(id)target;

/* A block run once, at the run loop's next pass in one of the modes. */
- (void)performInModes:(NSArray<NSRunLoopMode> *)modes block:(void (^)(void))block;
- (void)performBlock:(void (^)(void))block;

@end

@interface NSObject (NSDelayedPerforming)
- (void)performSelector:(SEL)aSelector withObject:(nullable id)anArgument afterDelay:(NSTimeInterval)delay;
- (void)performSelector:(SEL)aSelector withObject:(nullable id)anArgument afterDelay:(NSTimeInterval)delay inModes:(NSArray<NSRunLoopMode> *)modes;
+ (void)cancelPreviousPerformRequestsWithTarget:(id)aTarget selector:(SEL)aSelector object:(nullable id)anArgument;
+ (void)cancelPreviousPerformRequestsWithTarget:(id)aTarget;
@end

NS_ASSUME_NONNULL_END
