/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

// NSProgress (macOS 10.9 and later). Built with ARC. Minkar: the class did
// not exist.
//
// A progress counts completedUnitCount out of totalUnitCount; a child added
// with a pending unit count adds that part of its fraction to its parent's,
// and when it finishes the units become completed in the parent. The
// current progress of a thread (becomeCurrentWithPendingUnitCount:) is the
// implicit parent of progresses made on that thread; units of a current
// progress that got no child are completed when it resigns. Changes are
// sent to key-value observers by hand (automatic notification is off).

#import <Foundation/NSProgress.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSKeyValueObserving.h>
#import <Foundation/NSLock.h>
#import <Foundation/NSString.h>
#import <Foundation/NSThread.h>
#import <Foundation/NSValue.h>

NSProgressKind const NSProgressKindFile = @"NSProgressKindFile";
NSProgressUserInfoKey const NSProgressEstimatedTimeRemainingKey = @"NSProgressEstimatedTimeRemainingKey";
NSProgressUserInfoKey const NSProgressThroughputKey = @"NSProgressThroughputKey";

static NSString *const kCurrentStackKey = @"NSProgressCurrentStack";

// One entry of a thread's current-progress stack.
@interface _NSProgressCurrent : NSObject
@property (nonatomic, strong) NSProgress *progress;
@property (nonatomic) int64_t pending;
@property (nonatomic) BOOL used;            // a child took the pending units
@end

@implementation _NSProgressCurrent
@end

@implementation NSProgress {
    NSRecursiveLock *_lock;
    int64_t _total;
    int64_t _completed;
    __weak NSProgress *_parent;
    int64_t _portionOfParent;               // our pending units in the parent
    NSMutableArray *_children;
    NSMutableDictionary *_userInfo;
    NSString *_description;
    NSString *_additionalDescription;
    BOOL _cancelled, _paused, _cancellable, _pausable;
}

@synthesize cancellationHandler = _cancellationHandler;
@synthesize pausingHandler = _pausingHandler;
@synthesize resumingHandler = _resumingHandler;
@synthesize kind = _kind;

+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key {
    if ([key isEqualToString:@"completedUnitCount"] || [key isEqualToString:@"totalUnitCount"] ||
        [key isEqualToString:@"fractionCompleted"] || [key isEqualToString:@"cancelled"] ||
        [key isEqualToString:@"paused"])
        return NO;
    return [super automaticallyNotifiesObserversForKey:key];
}

static NSMutableArray *currentStack(BOOL create) {
    NSMutableDictionary *d = [[NSThread currentThread] threadDictionary];
    NSMutableArray *stack = [d objectForKey:kCurrentStackKey];

    if (stack == nil && create) {
        stack = [NSMutableArray array];
        [d setObject:stack forKey:kCurrentStackKey];
    }
    return stack;
}

+ (NSProgress *)currentProgress {
    return [[currentStack(NO) lastObject] progress];
}

- (instancetype)initWithParent:(NSProgress *)parent userInfo:(NSDictionary *)userInfo {
    if ((self = [super init]) != nil) {
        _lock = [NSRecursiveLock new];
        _children = [NSMutableArray array];
        _userInfo = userInfo ? [userInfo mutableCopy] : [NSMutableDictionary dictionary];
        _cancellable = YES;
        if (parent != nil) {
            // The implicit parent: the thread's current progress.
            _NSProgressCurrent *current = [currentStack(NO) lastObject];
            if (current != nil && current.progress == parent && !current.used) {
                current.used = YES;
                [parent addChild:self withPendingUnitCount:current.pending];
            }
        }
    }
    return self;
}

- (instancetype)init {
    return [self initWithParent:nil userInfo:nil];
}

+ (NSProgress *)progressWithTotalUnitCount:(int64_t)unitCount {
    NSProgress *p = [[self alloc] initWithParent:[self currentProgress] userInfo:nil];
    p.totalUnitCount = unitCount;
    return p;
}

+ (NSProgress *)discreteProgressWithTotalUnitCount:(int64_t)unitCount {
    NSProgress *p = [[self alloc] initWithParent:nil userInfo:nil];
    p.totalUnitCount = unitCount;
    return p;
}

+ (NSProgress *)progressWithTotalUnitCount:(int64_t)unitCount parent:(NSProgress *)parent
                          pendingUnitCount:(int64_t)portion {
    NSProgress *p = [self discreteProgressWithTotalUnitCount:unitCount];
    [parent addChild:p withPendingUnitCount:portion];
    return p;
}

/* ---- The tree ------------------------------------------------------------ */

- (void)addChild:(NSProgress *)child withPendingUnitCount:(int64_t)units {
    [_lock lock];
    [self willChangeValueForKey:@"fractionCompleted"];
    child->_parent = self;
    child->_portionOfParent = units;
    [_children addObject:child];
    if (_cancelled)
        [child cancel];
    [self didChangeValueForKey:@"fractionCompleted"];
    [_lock unlock];
    [child _childChanged];
}

- (void)becomeCurrentWithPendingUnitCount:(int64_t)unitCount {
    _NSProgressCurrent *entry = [_NSProgressCurrent new];
    entry.progress = self;
    entry.pending = unitCount;
    [currentStack(YES) addObject:entry];
}

- (void)resignCurrent {
    NSMutableArray *stack = currentStack(NO);
    _NSProgressCurrent *entry = [stack lastObject];

    if (entry == nil || entry.progress != self)
        [NSException raise:NSInternalInconsistencyException
                    format:@"-[NSProgress resignCurrent] by a progress that is not current"];
    [stack removeLastObject];
    if (!entry.used)
        self.completedUnitCount = self.completedUnitCount + entry.pending;
}

// Called on a child after it changed: tell the parent, and when the child
// finished, make its units completed there.
- (void)_childChanged {
    NSProgress *parent = _parent;

    if (parent == nil)
        return;
    if (self.finished) {
        [parent->_lock lock];
        if ([parent->_children containsObject:self]) {
            int64_t portion = _portionOfParent;
            [parent->_children removeObject:self];
            _parent = nil;
            [parent->_lock unlock];
            parent.completedUnitCount = parent.completedUnitCount + portion;
            return;
        }
        [parent->_lock unlock];
        return;
    }
    [parent willChangeValueForKey:@"fractionCompleted"];
    [parent didChangeValueForKey:@"fractionCompleted"];
    [parent _childChanged];
}

/* ---- Counts --------------------------------------------------------------- */

- (int64_t)totalUnitCount {
    [_lock lock];
    int64_t v = _total;
    [_lock unlock];
    return v;
}

- (void)setTotalUnitCount:(int64_t)total {
    [self willChangeValueForKey:@"totalUnitCount"];
    [self willChangeValueForKey:@"fractionCompleted"];
    [_lock lock];
    _total = total;
    [_lock unlock];
    [self didChangeValueForKey:@"fractionCompleted"];
    [self didChangeValueForKey:@"totalUnitCount"];
    [self _childChanged];
}

- (int64_t)completedUnitCount {
    [_lock lock];
    int64_t v = _completed;
    [_lock unlock];
    return v;
}

- (void)setCompletedUnitCount:(int64_t)completed {
    [self willChangeValueForKey:@"completedUnitCount"];
    [self willChangeValueForKey:@"fractionCompleted"];
    [_lock lock];
    _completed = completed;
    [_lock unlock];
    [self didChangeValueForKey:@"fractionCompleted"];
    [self didChangeValueForKey:@"completedUnitCount"];
    [self _childChanged];
}

- (double)fractionCompleted {
    double done;
    int64_t total;

    [_lock lock];
    total = _total;
    if (total <= 0) {
        [_lock unlock];
        return 0.0;
    }
    done = (double)(_completed < 0 ? 0 : _completed);
    for (NSProgress *child in _children)
        done += child.fractionCompleted * (double)child->_portionOfParent;
    [_lock unlock];
    return done / (double)total;
}

- (BOOL)isIndeterminate {
    return self.totalUnitCount < 0 || self.completedUnitCount < 0 ||
           (self.totalUnitCount == 0 && self.completedUnitCount == 0);
}

- (BOOL)isFinished {
    int64_t total = self.totalUnitCount, completed = self.completedUnitCount;
    return total > 0 && completed >= total;
}

/* ---- Descriptions --------------------------------------------------------- */

- (NSString *)localizedDescription {
    if (_description != nil)
        return _description;
    return [NSString stringWithFormat:@"%d%% completed", (int)(self.fractionCompleted * 100.0)];
}

- (void)setLocalizedDescription:(NSString *)s {
    _description = [s copy];
}

- (NSString *)localizedAdditionalDescription {
    if (_additionalDescription != nil)
        return _additionalDescription;
    return [NSString stringWithFormat:@"%lld of %lld", (long long)self.completedUnitCount,
                                      (long long)self.totalUnitCount];
}

- (void)setLocalizedAdditionalDescription:(NSString *)s {
    _additionalDescription = [s copy];
}

/* ---- Cancel and pause ----------------------------------------------------- */

- (BOOL)isCancellable { return _cancellable; }
- (void)setCancellable:(BOOL)flag { _cancellable = flag; }
- (BOOL)isPausable { return _pausable; }
- (void)setPausable:(BOOL)flag { _pausable = flag; }
- (BOOL)isCancelled { return _cancelled; }
- (BOOL)isPaused { return _paused; }

- (void)cancel {
    NSArray *children;
    void (^handler)(void);

    [_lock lock];
    if (_cancelled) {
        [_lock unlock];
        return;
    }
    [self willChangeValueForKey:@"cancelled"];
    _cancelled = YES;
    [self didChangeValueForKey:@"cancelled"];
    children = [_children copy];
    handler = _cancellationHandler;
    [_lock unlock];
    if (handler != nil)
        handler();
    for (NSProgress *child in children)
        [child cancel];
}

- (void)pause {
    NSArray *children;
    void (^handler)(void);

    [_lock lock];
    if (_paused) {
        [_lock unlock];
        return;
    }
    [self willChangeValueForKey:@"paused"];
    _paused = YES;
    [self didChangeValueForKey:@"paused"];
    children = [_children copy];
    handler = _pausingHandler;
    [_lock unlock];
    if (handler != nil)
        handler();
    for (NSProgress *child in children)
        [child pause];
}

- (void)resume {
    NSArray *children;
    void (^handler)(void);

    [_lock lock];
    if (!_paused) {
        [_lock unlock];
        return;
    }
    [self willChangeValueForKey:@"paused"];
    _paused = NO;
    [self didChangeValueForKey:@"paused"];
    children = [_children copy];
    handler = _resumingHandler;
    [_lock unlock];
    if (handler != nil)
        handler();
    for (NSProgress *child in children)
        [child resume];
}

/* ---- User info ------------------------------------------------------------ */

- (NSDictionary *)userInfo {
    [_lock lock];
    NSDictionary *d = [_userInfo copy];
    [_lock unlock];
    return d;
}

- (void)setUserInfoObject:(id)object forKey:(NSProgressUserInfoKey)key {
    [self willChangeValueForKey:@"userInfo"];
    [_lock lock];
    if (object == nil)
        [_userInfo removeObjectForKey:key];
    else
        [_userInfo setObject:object forKey:key];
    [_lock unlock];
    [self didChangeValueForKey:@"userInfo"];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@: %p> Fraction completed: %.4f / Completed: %lld of %lld",
                                      [self class], (void *)self, self.fractionCompleted,
                                      (long long)self.completedUnitCount, (long long)self.totalUnitCount];
}

@end
