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

/*
 * NSOperation, NSBlockOperation and NSInvocationOperation.
 *
 * An operation finishes once: when it says so through key-value observing
 * ("isFinished" goes to YES), which -start does for synchronous operations
 * and an asynchronous subclass does itself. Then the completion block runs
 * on a dispatch queue, the operations that depend on it become ready, and
 * its queue forgets it.
 */

#import <Foundation/NSOperation.h>
#import <Foundation/NSOperationQueue.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSException.h>
#import <Foundation/NSInvocation.h>
#import <Foundation/NSMethodSignature.h>
#import <Foundation/NSString.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSKeyValueObserving.h>
#import <CoreFoundation/CFArray.h>
#import "NSOperationInternal.h"
#include <pthread.h>

dispatch_qos_class_t _NSDispatchQoSClass(NSQualityOfService qos) {
    switch (qos) {
    case NSQualityOfServiceUserInteractive: return QOS_CLASS_USER_INTERACTIVE;
    case NSQualityOfServiceUserInitiated:   return QOS_CLASS_USER_INITIATED;
    case NSQualityOfServiceUtility:         return QOS_CLASS_UTILITY;
    case NSQualityOfServiceBackground:      return QOS_CLASS_BACKGROUND;
    default:                                return QOS_CLASS_DEFAULT;
    }
}

static NSOperationQueuePriority clampPriority(NSOperationQueuePriority p) {
    if (p <= NSOperationQueuePriorityVeryLow) return NSOperationQueuePriorityVeryLow;
    if (p <= NSOperationQueuePriorityLow) return NSOperationQueuePriorityLow;
    if (p < NSOperationQueuePriorityHigh) return NSOperationQueuePriorityNormal;
    if (p < NSOperationQueuePriorityVeryHigh) return NSOperationQueuePriorityHigh;
    return NSOperationQueuePriorityVeryHigh;
}

@implementation NSOperation {
    pthread_mutex_t _lock;
    pthread_cond_t _finishedCondition;
    BOOL _executing;
    BOOL _finished;
    BOOL _cancelled;
    BOOL _didFinish;
    BOOL _scheduled;
    NSMutableArray *_dependencies;
    CFMutableArrayRef _dependents;      // not retained: each removes itself
    NSOperationQueue *_queue;
    void (^_completionBlock)(void);
    NSOperationQueuePriority _queuePriority;
    NSQualityOfService _qualityOfService;
    double _threadPriority;
    NSString *_name;
}

- (instancetype)init {
    if ((self = [super init]) == nil)
        return nil;
    pthread_mutex_init(&_lock, NULL);
    pthread_cond_init(&_finishedCondition, NULL);
    _dependencies = [NSMutableArray new];
    _dependents = CFArrayCreateMutable(NULL, 0, NULL);
    _qualityOfService = NSQualityOfServiceDefault;
    _threadPriority = 0.5;
    return self;
}

- (void)dealloc {
    for (NSOperation *dependency in _dependencies)
        [dependency _removeDependent:self];
    CFRelease(_dependents);
    pthread_cond_destroy(&_finishedCondition);
    pthread_mutex_destroy(&_lock);
}

- (void)main {
}

- (void)start {
    pthread_mutex_lock(&_lock);
    BOOL executing = _executing, finished = _finished, cancelled = _cancelled;
    pthread_mutex_unlock(&_lock);

    if (executing || finished)
        [NSException raise:NSInvalidArgumentException
                    format:@"*** -[%@ start]: receiver is already %@", NSStringFromClass([self class]),
                           executing ? @"executing" : @"finished"];
    if (!cancelled && ![self isReady])
        [NSException raise:NSInvalidArgumentException
                    format:@"*** -[%@ start]: receiver is not yet ready to execute",
                           NSStringFromClass([self class])];

    if (!cancelled) {
        [self willChangeValueForKey:@"isExecuting"];
        pthread_mutex_lock(&_lock);
        _executing = YES;
        pthread_mutex_unlock(&_lock);
        [self didChangeValueForKey:@"isExecuting"];

        @autoreleasepool {
            if (![self isCancelled])
                [self main];
        }

        [self willChangeValueForKey:@"isExecuting"];
    }
    [self willChangeValueForKey:@"isFinished"];
    pthread_mutex_lock(&_lock);
    _executing = NO;
    _finished = YES;
    pthread_mutex_unlock(&_lock);
    [self didChangeValueForKey:@"isFinished"];
    if (!cancelled)
        [self didChangeValueForKey:@"isExecuting"];
}

// The operation's own bookkeeping follows its key-value notifications, so
// that asynchronous subclasses (which report their state themselves) are
// treated like synchronous ones.
- (void)didChangeValueForKey:(NSString *)key {
    [super didChangeValueForKey:key];
    if ([key isEqualToString:@"isFinished"] || [key isEqualToString:@"finished"]) {
        if ([self isFinished])
            [self _didFinish];
    } else if ([key isEqualToString:@"isReady"] || [key isEqualToString:@"ready"]) {
        [[self _queue] _operationReadinessChanged];
    }
}

- (void)_didFinish {
    pthread_mutex_lock(&_lock);
    if (_didFinish) {
        pthread_mutex_unlock(&_lock);
        return;
    }
    _didFinish = YES;
    void (^completion)(void) = _completionBlock;
    _completionBlock = nil;
    NSOperationQueue *queue = _queue;
    NSMutableArray *dependents = [NSMutableArray array];
    for (CFIndex i = 0; i < CFArrayGetCount(_dependents); i++)
        [dependents addObject:(__bridge id)CFArrayGetValueAtIndex(_dependents, i)];
    NSQualityOfService qos = _qualityOfService;
    pthread_cond_broadcast(&_finishedCondition);
    pthread_mutex_unlock(&_lock);

    if (completion != nil)
        dispatch_async(dispatch_get_global_queue(_NSDispatchQoSClass(qos), 0), completion);
    for (NSOperation *dependent in dependents)
        [dependent _dependencyDidFinish];
    [queue _operationDidFinish:self];
}

- (void)_dependencyDidFinish {
    [self willChangeValueForKey:@"isReady"];
    [self didChangeValueForKey:@"isReady"];
}

- (void)_addDependent:(NSOperation *)operation {
    pthread_mutex_lock(&_lock);
    CFArrayAppendValue(_dependents, (__bridge const void *)operation);
    pthread_mutex_unlock(&_lock);
}

- (void)_removeDependent:(NSOperation *)operation {
    pthread_mutex_lock(&_lock);
    CFIndex i = CFArrayGetFirstIndexOfValue(_dependents,
        CFRangeMake(0, CFArrayGetCount(_dependents)), (__bridge const void *)operation);
    if (i != kCFNotFound)
        CFArrayRemoveValueAtIndex(_dependents, i);
    pthread_mutex_unlock(&_lock);
}

- (BOOL)isCancelled {
    pthread_mutex_lock(&_lock);
    BOOL result = _cancelled;
    pthread_mutex_unlock(&_lock);
    return result;
}

- (void)cancel {
    pthread_mutex_lock(&_lock);
    BOOL skip = _cancelled || _finished;
    pthread_mutex_unlock(&_lock);
    if (skip)
        return;
    [self willChangeValueForKey:@"isCancelled"];
    pthread_mutex_lock(&_lock);
    _cancelled = YES;
    pthread_mutex_unlock(&_lock);
    [self didChangeValueForKey:@"isCancelled"];
    // A cancelled operation is ready: its queue starts it, and it finishes.
    [self _dependencyDidFinish];
}

- (BOOL)isExecuting {
    pthread_mutex_lock(&_lock);
    BOOL result = _executing;
    pthread_mutex_unlock(&_lock);
    return result;
}

- (BOOL)isFinished {
    pthread_mutex_lock(&_lock);
    BOOL result = _finished;
    pthread_mutex_unlock(&_lock);
    return result;
}

- (BOOL)isAsynchronous {
    return NO;
}

- (BOOL)isConcurrent {
    return [self isAsynchronous];
}

- (BOOL)isReady {
    pthread_mutex_lock(&_lock);
    BOOL cancelled = _cancelled;
    NSArray *dependencies = [_dependencies copy];
    pthread_mutex_unlock(&_lock);
    if (cancelled)
        return YES;
    for (NSOperation *dependency in dependencies)
        if (![dependency isFinished])
            return NO;
    return YES;
}

- (void)addDependency:(NSOperation *)operation {
    if (operation == nil)
        [NSException raise:NSInvalidArgumentException format:@"*** -[%@ addDependency:]: nil operation",
                     NSStringFromClass([self class])];
    if (operation == self)
        return;
    [self willChangeValueForKey:@"isReady"];
    [self willChangeValueForKey:@"dependencies"];
    pthread_mutex_lock(&_lock);
    BOOL added = [_dependencies indexOfObjectIdenticalTo:operation] == NSNotFound;
    if (added)
        [_dependencies addObject:operation];
    pthread_mutex_unlock(&_lock);
    if (added)
        [operation _addDependent:self];
    [self didChangeValueForKey:@"dependencies"];
    [self didChangeValueForKey:@"isReady"];
}

- (void)removeDependency:(NSOperation *)operation {
    if (operation == nil)
        return;
    [self willChangeValueForKey:@"isReady"];
    [self willChangeValueForKey:@"dependencies"];
    pthread_mutex_lock(&_lock);
    NSUInteger i = [_dependencies indexOfObjectIdenticalTo:operation];
    if (i != NSNotFound)
        [_dependencies removeObjectAtIndex:i];
    pthread_mutex_unlock(&_lock);
    if (i != NSNotFound)
        [operation _removeDependent:self];
    [self didChangeValueForKey:@"dependencies"];
    [self didChangeValueForKey:@"isReady"];
}

- (NSArray *)dependencies {
    pthread_mutex_lock(&_lock);
    NSArray *result = [_dependencies copy];
    pthread_mutex_unlock(&_lock);
    return result;
}

- (NSOperationQueuePriority)queuePriority {
    pthread_mutex_lock(&_lock);
    NSOperationQueuePriority result = _queuePriority;
    pthread_mutex_unlock(&_lock);
    return result;
}

- (void)setQueuePriority:(NSOperationQueuePriority)priority {
    pthread_mutex_lock(&_lock);
    _queuePriority = clampPriority(priority);
    pthread_mutex_unlock(&_lock);
}

- (void (^)(void))completionBlock {
    pthread_mutex_lock(&_lock);
    void (^result)(void) = _completionBlock;
    pthread_mutex_unlock(&_lock);
    return result;
}

- (void)setCompletionBlock:(void (^)(void))block {
    void (^copy)(void) = [block copy];
    pthread_mutex_lock(&_lock);
    _completionBlock = _didFinish ? nil : copy;
    pthread_mutex_unlock(&_lock);
}

- (void)waitUntilFinished {
    pthread_mutex_lock(&_lock);
    while (!_didFinish)
        pthread_cond_wait(&_finishedCondition, &_lock);
    pthread_mutex_unlock(&_lock);
}

- (double)threadPriority {
    return _threadPriority;
}

- (void)setThreadPriority:(double)priority {
    _threadPriority = priority < 0 ? 0 : priority > 1 ? 1 : priority;
}

- (NSQualityOfService)qualityOfService {
    return _qualityOfService;
}

- (void)setQualityOfService:(NSQualityOfService)qos {
    _qualityOfService = qos;
}

- (NSString *)name {
    pthread_mutex_lock(&_lock);
    NSString *result = _name;
    pthread_mutex_unlock(&_lock);
    return result;
}

- (void)setName:(NSString *)name {
    NSString *copy = [name copy];
    pthread_mutex_lock(&_lock);
    _name = copy;
    pthread_mutex_unlock(&_lock);
}

- (NSString *)description {
    NSString *name = [self name];
    if (name == nil)
        return [super description];
    return [NSString stringWithFormat:@"<%@ %p>{name = '%@'}", NSStringFromClass([self class]),
                                      (__bridge void *)self, name];
}

- (NSOperationQueue *)_queue {
    pthread_mutex_lock(&_lock);
    NSOperationQueue *result = _queue;
    pthread_mutex_unlock(&_lock);
    return result;
}

- (void)_setQueue:(NSOperationQueue *)queue {
    NSOperationQueue *old;
    pthread_mutex_lock(&_lock);
    old = _queue;
    _queue = queue;
    pthread_mutex_unlock(&_lock);
    (void)old;   // released here, outside the lock
}

- (BOOL)_isScheduled {
    pthread_mutex_lock(&_lock);
    BOOL result = _scheduled;
    pthread_mutex_unlock(&_lock);
    return result;
}

- (void)_setScheduled {
    pthread_mutex_lock(&_lock);
    _scheduled = YES;
    pthread_mutex_unlock(&_lock);
}

@end

@implementation NSBlockOperation {
    NSMutableArray *_blocks;
}

+ (instancetype)blockOperationWithBlock:(void (^)(void))block {
    NSBlockOperation *operation = [[self alloc] init];
    [operation addExecutionBlock:block];
    return operation;
}

- (instancetype)init {
    if ((self = [super init]) != nil)
        _blocks = [NSMutableArray new];
    return self;
}

- (void)addExecutionBlock:(void (^)(void))block {
    if (block == nil)
        [NSException raise:NSInvalidArgumentException
                    format:@"*** -[NSBlockOperation addExecutionBlock:]: block is nil"];
    if ([self isExecuting] || [self isFinished])
        [NSException raise:NSInvalidArgumentException
                    format:@"*** -[NSBlockOperation addExecutionBlock:]: blocks cannot be added "
                           @"after the operation has started executing or finished"];
    @synchronized(self) {
        [_blocks addObject:[block copy]];
    }
}

- (NSArray *)executionBlocks {
    @synchronized(self) {
        return [_blocks copy];
    }
}

// The first block runs here; with more, they run concurrently.
- (void)main {
    NSArray *blocks = [self executionBlocks];
    NSUInteger count = [blocks count];
    if (count == 1) {
        ((void (^)(void))blocks[0])();
    } else if (count > 1) {
        dispatch_queue_t queue =
            dispatch_get_global_queue(_NSDispatchQoSClass([self qualityOfService]), 0);
        dispatch_apply(count, queue, ^(size_t i) {
            @autoreleasepool {
                ((void (^)(void))blocks[i])();
            }
        });
    }
}

@end

NSString *const NSInvocationOperationVoidResultException = @"NSInvocationOperationVoidResultException";
NSString *const NSInvocationOperationCancelledException = @"NSInvocationOperationCancelledException";

// The return type without its qualifiers (const, in, out...).
static const char *returnType(NSInvocation *invocation) {
    const char *type = [[invocation methodSignature] methodReturnType];
    while (*type == 'r' || *type == 'n' || *type == 'N' || *type == 'o' || *type == 'O' ||
           *type == 'R' || *type == 'V')
        type++;
    return type;
}

@implementation NSInvocationOperation {
    NSInvocation *_invocation;
    NSException *_exception;
    id _result;     // an object result, kept past -start's autorelease pool
}

- (instancetype)initWithInvocation:(NSInvocation *)invocation {
    if ((self = [super init]) == nil)
        return nil;
    _invocation = invocation;
    [_invocation retainArguments];
    return self;
}

- (instancetype)initWithTarget:(id)target selector:(SEL)selector object:(id)argument {
    NSMethodSignature *signature = [target methodSignatureForSelector:selector];
    if (signature == nil)
        return nil;
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setTarget:target];
    [invocation setSelector:selector];
    if ([signature numberOfArguments] > 2)
        [invocation setArgument:&argument atIndex:2];
    return [self initWithInvocation:invocation];
}

- (NSInvocation *)invocation {
    return _invocation;
}

- (void)main {
    @try {
        [_invocation invoke];
        const char *type = returnType(_invocation);
        if (*type == '@' || *type == '#') {
            __unsafe_unretained id object = nil;
            [_invocation getReturnValue:&object];
            _result = object;
        }
    } @catch (NSException *exception) {
        _exception = exception;
    }
}

- (id)result {
    if ([self isCancelled])
        [NSException raise:NSInvocationOperationCancelledException
                    format:@"*** -[NSInvocationOperation result]: operation was cancelled"];
    if (_exception != nil)
        @throw _exception;
    if (![self isFinished])
        return nil;

    const char *type = returnType(_invocation);
    if (*type == 'v')
        [NSException raise:NSInvocationOperationVoidResultException
                    format:@"*** -[NSInvocationOperation result]: the method returns void"];
    if (*type == '@' || *type == '#')
        return _result;
    NSUInteger length = [[_invocation methodSignature] methodReturnLength];
    char buffer[length > 0 ? length : 1];
    [_invocation getReturnValue:buffer];
    return [NSValue valueWithBytes:buffer objCType:type];
}

@end
