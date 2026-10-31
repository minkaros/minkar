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
 * NSOperationQueue on dispatch. The queue keeps its operations in the
 * order they were added; each ready one (highest priority first, while
 * fewer than maxConcurrentOperationCount run) is started on a dispatch
 * queue: the main queue for mainQueue, the underlying queue if one is set,
 * otherwise a global queue of the queue's quality of service.
 */

#import <Foundation/NSOperationQueue.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSException.h>
#import <Foundation/NSNull.h>
#import <Foundation/NSProgress.h>
#import <Foundation/NSString.h>
#import <Foundation/NSThread.h>
#import <Foundation/NSKeyValueObserving.h>
#import "NSOperationInternal.h"
#include <pthread.h>

static pthread_key_t currentQueueKey;
static dispatch_once_t currentQueueKeyOnce;

static void makeCurrentQueueKey(void *unused __attribute__((unused))) {
    pthread_key_create(&currentQueueKey, NULL);
}

@implementation NSOperationQueue {
    pthread_mutex_t _lock;
    pthread_cond_t _emptyCondition;
    NSMutableArray *_operations;
    NSInteger _maxConcurrentOperationCount;
    NSInteger _running;
    BOOL _suspended;
    BOOL _isMainQueue;
    NSString *_name;
    NSQualityOfService _qualityOfService;
    dispatch_queue_t _underlyingQueue;
    NSProgress *_progress;
    NSOperation *_barrier;
}

- (instancetype)init {
    if ((self = [super init]) == nil)
        return nil;
    pthread_mutex_init(&_lock, NULL);
    pthread_cond_init(&_emptyCondition, NULL);
    _operations = [NSMutableArray new];
    _maxConcurrentOperationCount = NSOperationQueueDefaultMaxConcurrentOperationCount;
    _qualityOfService = NSQualityOfServiceDefault;
    _name = [NSString stringWithFormat:@"NSOperationQueue %p", (__bridge void *)self];
    return self;
}

- (void)dealloc {
#if !OS_OBJECT_USE_OBJC
    if (_underlyingQueue != NULL)
        dispatch_release(_underlyingQueue);
#endif
    pthread_cond_destroy(&_emptyCondition);
    pthread_mutex_destroy(&_lock);
}

+ (NSOperationQueue *)mainQueue {
    static NSOperationQueue *mainQueue;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        mainQueue = [[NSOperationQueue alloc] init];
        mainQueue->_isMainQueue = YES;
        mainQueue->_maxConcurrentOperationCount = 1;
        mainQueue->_qualityOfService = NSQualityOfServiceUserInteractive;
        mainQueue->_name = @"NSOperationQueue Main Queue";
    });
    return mainQueue;
}

+ (NSOperationQueue *)currentQueue {
    dispatch_once_f(&currentQueueKeyOnce, NULL, makeCurrentQueueKey);
    NSOperationQueue *queue = (__bridge NSOperationQueue *)pthread_getspecific(currentQueueKey);
    if (queue != nil)
        return queue;
    if (pthread_main_np())
        return [self mainQueue];
    return nil;
}

- (dispatch_queue_t)_targetQueueForOperation:(NSOperation *)operation {
    if (_isMainQueue)
        return dispatch_get_main_queue();
    if (_underlyingQueue != NULL)
        return _underlyingQueue;
    NSQualityOfService qos = [operation qualityOfService];
    if (qos == NSQualityOfServiceDefault)
        qos = _qualityOfService;
    return dispatch_get_global_queue(_NSDispatchQoSClass(qos), 0);
}

- (NSInteger)_width {
    if (_isMainQueue)
        return 1;
    return _maxConcurrentOperationCount < 0 ? NSIntegerMax : _maxConcurrentOperationCount;
}

// Starts the ready operations there is room for. The operations' own code
// (isReady may be overridden) runs outside the queue's lock.
- (void)_schedule {
    pthread_mutex_lock(&_lock);
    if (_suspended || _running >= [self _width]) {
        pthread_mutex_unlock(&_lock);
        return;
    }
    NSMutableArray *waiting = [NSMutableArray array];
    for (NSOperation *operation in _operations)
        if (![operation _isScheduled])
            [waiting addObject:operation];
    pthread_mutex_unlock(&_lock);

    NSMutableArray *ready = [NSMutableArray array];
    for (NSOperation *operation in waiting)
        if ([operation isReady])
            [ready addObject:operation];
    if ([ready count] == 0)
        return;
    // Highest priority first; the order of addition among equals.
    NSMutableArray *ordered = [NSMutableArray arrayWithCapacity:[ready count]];
    for (NSInteger p = NSOperationQueuePriorityVeryHigh; p >= NSOperationQueuePriorityVeryLow; p -= 4)
        for (NSOperation *operation in ready)
            if ([operation queuePriority] == p)
                [ordered addObject:operation];

    NSMutableArray *start = [NSMutableArray array];
    pthread_mutex_lock(&_lock);
    for (NSOperation *operation in ordered) {
        if (_suspended || _running >= [self _width])
            break;
        if ([operation _isScheduled] ||
            [_operations indexOfObjectIdenticalTo:operation] == NSNotFound)
            continue;
        [operation _setScheduled];
        _running++;
        [start addObject:operation];
    }
    pthread_mutex_unlock(&_lock);

    for (NSOperation *operation in start) {
        dispatch_async([self _targetQueueForOperation:operation], ^{
            [self _runOperation:operation];
        });
    }
}

- (void)_runOperation:(NSOperation *)operation {
    dispatch_once_f(&currentQueueKeyOnce, NULL, makeCurrentQueueKey);
    void *previous = pthread_getspecific(currentQueueKey);
    pthread_setspecific(currentQueueKey, (__bridge void *)self);
    @autoreleasepool {
        [operation start];
    }
    pthread_setspecific(currentQueueKey, previous);
}

- (void)_operationReadinessChanged {
    [self _schedule];
}

- (void)_operationDidFinish:(NSOperation *)operation {
    // The operation held the queue: keep it until the end of this method.
    __attribute__((objc_precise_lifetime)) NSOperationQueue *keep = self;
    (void)keep;

    [self willChangeValueForKey:@"operations"];
    [self willChangeValueForKey:@"operationCount"];
    pthread_mutex_lock(&_lock);
    NSUInteger i = [_operations indexOfObjectIdenticalTo:operation];
    if (i != NSNotFound) {
        [_operations removeObjectAtIndex:i];
        if ([operation _isScheduled])
            _running--;
    }
    if (_barrier == operation)
        _barrier = nil;
    if ([_operations count] == 0)
        pthread_cond_broadcast(&_emptyCondition);
    NSProgress *progress = _progress;
    pthread_mutex_unlock(&_lock);
    [self didChangeValueForKey:@"operationCount"];
    [self didChangeValueForKey:@"operations"];

    // Each operation is one unit of the queue's progress, once a total is set.
    if (i != NSNotFound && progress != nil && [progress totalUnitCount] > 0 &&
        [progress completedUnitCount] < [progress totalUnitCount])
        [progress setCompletedUnitCount:[progress completedUnitCount] + 1];
    [operation _setQueue:nil];
    [self _schedule];
}

- (void)addOperation:(NSOperation *)operation {
    [self addOperations:[NSArray arrayWithObject:operation ?: (id)[NSNull null]] waitUntilFinished:NO];
}

- (void)addOperations:(NSArray *)operations waitUntilFinished:(BOOL)wait {
    for (NSOperation *operation in operations) {
        if (![operation isKindOfClass:[NSOperation class]])
            [NSException raise:NSInvalidArgumentException
                        format:@"*** -[NSOperationQueue addOperation:]: %@ is not an NSOperation",
                               operation == (id)[NSNull null] ? @"nil" : (id)operation];
        if ([operation _queue] != nil)
            [NSException raise:NSInvalidArgumentException
                        format:@"*** -[NSOperationQueue addOperation:]: operation is already enqueued on a queue"];
        if ([operation isExecuting] || [operation isFinished])
            [NSException raise:NSInvalidArgumentException
                        format:@"*** -[NSOperationQueue addOperation:]: operation is %@ and cannot be enqueued",
                               [operation isExecuting] ? @"executing" : @"finished"];
    }

    [self willChangeValueForKey:@"operations"];
    [self willChangeValueForKey:@"operationCount"];
    for (NSOperation *operation in operations) {
        pthread_mutex_lock(&_lock);
        NSOperation *barrier = _barrier;
        pthread_mutex_unlock(&_lock);
        // Operations added after a barrier wait for it.
        if (barrier != nil && barrier != operation)
            [operation addDependency:barrier];
        [operation _setQueue:self];
        pthread_mutex_lock(&_lock);
        [_operations addObject:operation];
        pthread_mutex_unlock(&_lock);
    }
    [self didChangeValueForKey:@"operationCount"];
    [self didChangeValueForKey:@"operations"];

    [self _schedule];
    if (wait)
        for (NSOperation *operation in operations)
            [operation waitUntilFinished];
}

- (void)addOperationWithBlock:(void (^)(void))block {
    [self addOperation:[NSBlockOperation blockOperationWithBlock:block]];
}

// Runs once every operation added before it has finished; the operations
// added after it wait for it.
- (void)addBarrierBlock:(void (^)(void))block {
    NSBlockOperation *barrier = [NSBlockOperation blockOperationWithBlock:block];
    for (NSOperation *operation in [self operations])
        [barrier addDependency:operation];
    pthread_mutex_lock(&_lock);
    _barrier = barrier;
    pthread_mutex_unlock(&_lock);
    [self addOperation:barrier];
}

- (NSArray *)operations {
    pthread_mutex_lock(&_lock);
    NSArray *result = [_operations copy];
    pthread_mutex_unlock(&_lock);
    return result;
}

- (NSUInteger)operationCount {
    pthread_mutex_lock(&_lock);
    NSUInteger result = [_operations count];
    pthread_mutex_unlock(&_lock);
    return result;
}

- (void)cancelAllOperations {
    for (NSOperation *operation in [self operations])
        [operation cancel];
}

- (void)waitUntilAllOperationsAreFinished {
    pthread_mutex_lock(&_lock);
    while ([_operations count] > 0)
        pthread_cond_wait(&_emptyCondition, &_lock);
    pthread_mutex_unlock(&_lock);
}

- (NSInteger)maxConcurrentOperationCount {
    pthread_mutex_lock(&_lock);
    NSInteger result = _maxConcurrentOperationCount;
    pthread_mutex_unlock(&_lock);
    return result;
}

- (void)setMaxConcurrentOperationCount:(NSInteger)count {
    if (count < NSOperationQueueDefaultMaxConcurrentOperationCount)
        [NSException raise:NSInvalidArgumentException
                    format:@"*** -[NSOperationQueue setMaxConcurrentOperationCount:]: count (%ld) cannot be negative",
                           (long)count];
    if (_isMainQueue)
        return;
    pthread_mutex_lock(&_lock);
    _maxConcurrentOperationCount = count;
    pthread_mutex_unlock(&_lock);
    [self _schedule];
}

- (BOOL)isSuspended {
    pthread_mutex_lock(&_lock);
    BOOL result = _suspended;
    pthread_mutex_unlock(&_lock);
    return result;
}

- (void)setSuspended:(BOOL)suspended {
    pthread_mutex_lock(&_lock);
    _suspended = suspended;
    pthread_mutex_unlock(&_lock);
    if (!suspended)
        [self _schedule];
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

- (NSQualityOfService)qualityOfService {
    return _qualityOfService;
}

- (void)setQualityOfService:(NSQualityOfService)qos {
    _qualityOfService = qos;
}

- (dispatch_queue_t)underlyingQueue {
    pthread_mutex_lock(&_lock);
    dispatch_queue_t result = _underlyingQueue;
    pthread_mutex_unlock(&_lock);
    return result;
}

- (void)setUnderlyingQueue:(dispatch_queue_t)queue {
    if (_isMainQueue || queue == dispatch_get_main_queue())
        [NSException raise:NSInvalidArgumentException
                    format:@"*** -[NSOperationQueue setUnderlyingQueue:]: the main queue cannot be used"];
    if ([self operationCount] > 0)
        [NSException raise:NSInvalidArgumentException
                    format:@"*** -[NSOperationQueue setUnderlyingQueue:]: operationCount must be zero"];
    pthread_mutex_lock(&_lock);
#if OS_OBJECT_USE_OBJC
    _underlyingQueue = queue;
#else
    if (queue != NULL)
        dispatch_retain(queue);
    if (_underlyingQueue != NULL)
        dispatch_release(_underlyingQueue);
    _underlyingQueue = queue;
#endif
    pthread_mutex_unlock(&_lock);
}

- (NSProgress *)progress {
    pthread_mutex_lock(&_lock);
    if (_progress == nil)
        _progress = [NSProgress discreteProgressWithTotalUnitCount:0];
    NSProgress *result = _progress;
    pthread_mutex_unlock(&_lock);
    return result;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p>{name = '%@'}", NSStringFromClass([self class]),
                                      (__bridge void *)self, [self name]];
}

@end
