/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSDistributedNotificationCenter over distnoted. The process checks in
 * with a port on the main run loop and tells distnoted each name and
 * object it observes (counted: told once, taken back with the last
 * observer). A notification posted is a binary property list (name,
 * object, user info) that distnoted hands to the processes of the user
 * observing it, the poster too. Observers are called on the main thread;
 * while the center is suspended each one's behavior holds, coalesces or
 * drops what comes, unless it or the notification is to be delivered
 * immediately. If distnoted goes away, the process checks in again with
 * its observations.
 */

#import <Foundation/NSDistributedNotificationCenter.h>
#import <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>
#include <servers/bootstrap.h>
#include <mach/mach.h>

#include "DistributedNotificationTypes.h"
#include "DistributedNotificationsUser.h"	/* MIG, generated */

NSDistributedNotificationCenterType const NSLocalNotificationCenterType = @"NSLocalNotificationCenterType";

/* One observer: a target and selector, or a block on a queue. */
@interface NSDistributedObserver : NSObject {
@public
    __unsafe_unretained id observer;    /* not retained, as NSNotificationCenter */
    SEL selector;
    NSString *name;
    NSString *object;
    NSNotificationSuspensionBehavior behavior;
    void (^block)(NSNotification *);
    NSOperationQueue *queue;
}
@end

@implementation NSDistributedObserver
@end

@interface NSDistributedNotificationCenter ()
- (void)_received:(NSData *)data options:(int32_t)options;
@end

static NSDistributedNotificationCenter *defaultCenter;

static void received(CFMachPortRef port, void *message, CFIndex size, void *info) {
    const DNMessage *msg = message;

    (void)port;
    (void)info;
    if (msg->header.msgh_id != kDNMessageNotification || (CFIndex)sizeof(DNMessage) > size ||
        !(msg->header.msgh_bits & MACH_MSGH_BITS_COMPLEX) || msg->body.msgh_descriptor_count != 1)
        return;
    NSData *data = nil;
    if (msg->notification.size <= kDNMaxNotification)
        data = [NSData dataWithBytes:msg->notification.address length:msg->notification.size];
    vm_deallocate(mach_task_self(), (vm_address_t)msg->notification.address, msg->notification.size);
    if (data != nil)
        [defaultCenter _received:data options:msg->options];
}

@implementation NSDistributedNotificationCenter {
    NSMutableArray<NSDistributedObserver *> *_observers;
    NSCountedSet<NSString *> *_observed;    /* "name\0object", as told to distnoted */
    NSMutableArray *_pending;               /* [record, notification] held while suspended */
    BOOL _suspended;
    mach_port_t _server;
    mach_port_t _port;
    BOOL _checkedIn;
}

+ (NSDistributedNotificationCenter *)defaultCenter {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        defaultCenter = [[NSDistributedNotificationCenter alloc] init];
    });
    return defaultCenter;
}

+ (NSDistributedNotificationCenter *)notificationCenterForType:(NSDistributedNotificationCenterType)type {
    return [type isEqualToString:NSLocalNotificationCenterType] ? [self defaultCenter] : nil;
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _observers = [NSMutableArray array];
        _observed = [NSCountedSet set];
        _pending = [NSMutableArray array];
    }
    return self;
}

/* ---- distnoted ---- */

static NSString *observedKey(NSString *name, NSString *object) {
    return [NSString stringWithFormat:@"%@%C%@", name ?: @"", (unichar)0, object ?: @""];
}

/* Locked. The port on the main run loop, given to distnoted (again with
   every observation when distnoted is new). */
- (BOOL)_checkIn {
    if (_checkedIn)
        return YES;
    if (_server == MACH_PORT_NULL && bootstrap_look_up(bootstrap_port, kDNServiceName, &_server) != KERN_SUCCESS) {
        _server = MACH_PORT_NULL;
        return NO;
    }
    if (_port == MACH_PORT_NULL) {
        CFMachPortContext context = { 0, NULL, NULL, NULL, NULL };
        CFMachPortRef port = CFMachPortCreate(NULL, received, &context, NULL);
        if (port == NULL)
            return NO;
        _port = CFMachPortGetPort(port);
        CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(NULL, port, 0);
        CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);
        CFRelease(source);
    }
    int32_t result = kDNSuccess;
    if (dnc_check_in(_server, _port, &result) != KERN_SUCCESS || result != kDNSuccess)
        return NO;
    _checkedIn = YES;
    for (NSString *key in _observed) {
        NSArray *parts = [key componentsSeparatedByString:[NSString stringWithFormat:@"%C", (unichar)0]];
        [self _tell:YES name:parts[0] object:parts[1]];
    }
    return YES;
}

/* Locked: distnoted again after it went away. */
- (void)_lostServer {
    if (_server != MACH_PORT_NULL)
        mach_port_deallocate(mach_task_self(), _server);
    _server = MACH_PORT_NULL;
    _checkedIn = NO;
}

- (void)_tell:(BOOL)observe name:(NSString *)name object:(NSString *)object {
    const char *n = name.UTF8String, *o = object.UTF8String;
    int32_t result = kDNSuccess;
    kern_return_t kr = observe ? dnc_observe(_server, (char *)n, (mach_msg_type_number_t)strlen(n), (char *)o,
                                             (mach_msg_type_number_t)strlen(o), &result)
                               : dnc_unobserve(_server, (char *)n, (mach_msg_type_number_t)strlen(n), (char *)o,
                                               (mach_msg_type_number_t)strlen(o), &result);
    if (kr == MACH_SEND_INVALID_DEST || kr == MIG_SERVER_DIED)
        [self _lostServer];
}

/* Locked: one more observer of this name and object (one fewer). */
- (void)_observe:(BOOL)observe name:(NSString *)name object:(NSString *)object {
    NSString *key = observedKey(name, object);
    if (observe) {
        [_observed addObject:key];
        if ([_observed countForObject:key] == 1 && [self _checkIn])
            [self _tell:YES name:name ?: @"" object:object ?: @""];
    } else if ([_observed countForObject:key] > 0) {
        [_observed removeObject:key];
        if ([_observed countForObject:key] == 0 && _checkedIn)
            [self _tell:NO name:name ?: @"" object:object ?: @""];
    }
}

/* ---- Observers ---- */

- (void)_add:(NSDistributedObserver *)record {
    if (record->object != nil && ![record->object isKindOfClass:[NSString class]])
        record->object = nil;
    @synchronized(self) {
        [_observers addObject:record];
        [self _observe:YES name:record->name object:record->object];
    }
}

- (void)addObserver:(id)observer selector:(SEL)selector name:(NSNotificationName)name object:(NSString *)object
 suspensionBehavior:(NSNotificationSuspensionBehavior)behavior {
    if (observer == nil || selector == NULL)
        return;
    NSDistributedObserver *r = [NSDistributedObserver new];
    r->observer = observer;
    r->selector = selector;
    r->name = [name copy];
    r->object = [object copy];
    r->behavior = behavior;
    [self _add:r];
}

- (void)addObserver:(id)observer selector:(SEL)selector name:(NSNotificationName)name object:(id)object {
    [self addObserver:observer selector:selector name:name object:object
        suspensionBehavior:NSNotificationSuspensionBehaviorCoalesce];
}

- (id<NSObject>)addObserverForName:(NSNotificationName)name object:(id)object queue:(NSOperationQueue *)queue
                        usingBlock:(void (^)(NSNotification *))block {
    if (block == nil)
        return nil;
    NSDistributedObserver *r = [NSDistributedObserver new];
    r->name = [name copy];
    r->object = [object copy];
    r->behavior = NSNotificationSuspensionBehaviorCoalesce;
    r->block = [block copy];
    r->queue = queue;
    r->observer = r;    /* the token is the record */
    [self _add:r];
    return r;
}

- (void)removeObserver:(id)observer name:(NSNotificationName)name object:(id)object {
    @synchronized(self) {
        for (NSInteger i = (NSInteger)_observers.count - 1; i >= 0; i--) {
            NSDistributedObserver *r = _observers[(NSUInteger)i];
            if (r->observer != observer || (name != nil && ![name isEqual:r->name]) ||
                (object != nil && ![object isEqual:r->object]))
                continue;
            [self _observe:NO name:r->name object:r->object];
            [_observers removeObjectAtIndex:(NSUInteger)i];
            for (NSInteger p = (NSInteger)_pending.count - 1; p >= 0; p--)
                if (_pending[(NSUInteger)p][0] == r)
                    [_pending removeObjectAtIndex:(NSUInteger)p];
        }
    }
}

- (void)removeObserver:(id)observer {
    [self removeObserver:observer name:nil object:nil];
}

/* ---- Posting ---- */

- (void)postNotificationName:(NSNotificationName)name object:(NSString *)object userInfo:(NSDictionary *)userInfo
                     options:(NSDistributedNotificationOptions)options {
    if (name == nil)
        [NSException raise:NSInvalidArgumentException format:@"%@: a notification needs a name", NSStringFromClass([self class])];
    NSMutableDictionary *plist = [NSMutableDictionary dictionaryWithObject:name forKey:@kDNKeyName];
    if ([object isKindOfClass:[NSString class]])
        plist[@kDNKeyObject] = object;
    if (userInfo != nil)
        plist[@kDNKeyUserInfo] = userInfo;
    NSError *error = nil;
    NSData *data = [NSPropertyListSerialization dataWithPropertyList:plist format:NSPropertyListBinaryFormat_v1_0
                                                             options:0 error:&error];
    if (data == nil)
        [NSException raise:NSInvalidArgumentException format:@"%@: the user info is not a property list (%@)",
                                                               NSStringFromClass([self class]), error];
    if (data.length > kDNMaxNotification)
        [NSException raise:NSInvalidArgumentException format:@"%@: the notification is too large",
                                                               NSStringFromClass([self class])];
    int32_t flags = ((options & NSDistributedNotificationDeliverImmediately) ? kDNPostDeliverImmediately : 0) |
                    ((options & NSDistributedNotificationPostToAllSessions) ? kDNPostToAllSessions : 0);
    @synchronized(self) {
        for (int attempt = 0; attempt < 2; attempt++) {
            if (![self _checkIn])
                return;
            int32_t result = kDNSuccess;
            kern_return_t kr = dnc_post(_server, (char *)data.bytes, (mach_msg_type_number_t)data.length, flags, &result);
            if (kr != MACH_SEND_INVALID_DEST && kr != MIG_SERVER_DIED)
                return;
            [self _lostServer];
        }
    }
}

- (void)postNotificationName:(NSNotificationName)name object:(NSString *)object userInfo:(NSDictionary *)userInfo
          deliverImmediately:(BOOL)deliverImmediately {
    [self postNotificationName:name object:object userInfo:userInfo
                       options:deliverImmediately ? NSDistributedNotificationDeliverImmediately : 0];
}

- (void)postNotificationName:(NSNotificationName)name object:(id)object userInfo:(NSDictionary *)userInfo {
    [self postNotificationName:name object:object userInfo:userInfo options:0];
}

- (void)postNotificationName:(NSNotificationName)name object:(id)object {
    [self postNotificationName:name object:object userInfo:nil options:0];
}

- (void)postNotification:(NSNotification *)notification {
    [self postNotificationName:notification.name object:notification.object userInfo:notification.userInfo options:0];
}

/* ---- Delivery ---- */

static void deliver(NSDistributedObserver *r, NSNotification *note) {
    if (r->block != nil) {
        if (r->queue != nil)
            [r->queue addOperationWithBlock:^{ r->block(note); }];
        else
            r->block(note);
    } else {
        void (*send)(id, SEL, id) = (void (*)(id, SEL, id))[r->observer methodForSelector:r->selector];
        if (send != NULL)
            send(r->observer, r->selector, note);
    }
}

/* On the main thread: to each observer of it, now or held. */
- (void)_received:(NSData *)data options:(int32_t)options {
    NSDictionary *plist = [NSPropertyListSerialization propertyListWithData:data options:0 format:NULL error:NULL];
    NSString *name = [plist isKindOfClass:[NSDictionary class]] ? plist[@kDNKeyName] : nil;
    if (![name isKindOfClass:[NSString class]])
        return;
    NSString *object = plist[@kDNKeyObject];
    NSDictionary *userInfo = plist[@kDNKeyUserInfo];
    NSNotification *note = [NSNotification notificationWithName:name
                                                          object:[object isKindOfClass:[NSString class]] ? object : nil
                                                        userInfo:[userInfo isKindOfClass:[NSDictionary class]] ? userInfo : nil];
    NSMutableArray *now = [NSMutableArray array];
    @synchronized(self) {
        for (NSDistributedObserver *r in _observers) {
            if ((r->name != nil && ![r->name isEqualToString:name]) ||
                (r->object != nil && ![r->object isEqual:note.object]))
                continue;
            if (!_suspended || (options & kDNPostDeliverImmediately) ||
                r->behavior == NSNotificationSuspensionBehaviorDeliverImmediately) {
                [now addObject:r];
                continue;
            }
            switch (r->behavior) {
            case NSNotificationSuspensionBehaviorDrop:
                break;
            case NSNotificationSuspensionBehaviorCoalesce: {
                /* The last of each name for this observer. */
                NSUInteger i = [_pending indexOfObjectPassingTest:^BOOL(NSArray *p, NSUInteger idx, BOOL *stop) {
                    return p[0] == r && [((NSNotification *)p[1]).name isEqualToString:name];
                }];
                if (i != NSNotFound)
                    [_pending removeObjectAtIndex:i];
                [_pending addObject:@[ r, note ]];
                break;
            }
            default:
                [_pending addObject:@[ r, note ]];
                break;
            }
        }
    }
    for (NSDistributedObserver *r in now)
        deliver(r, note);
}

- (BOOL)suspended {
    @synchronized(self) {
        return _suspended;
    }
}

/* Unsuspended: what was held comes, in order. */
- (void)setSuspended:(BOOL)value {
    NSArray *held = nil;
    @synchronized(self) {
        _suspended = value;
        if (!value && _pending.count > 0) {
            held = [_pending copy];
            [_pending removeAllObjects];
        }
    }
    for (NSArray *p in held)
        deliver(p[0], p[1]);
}

@end
