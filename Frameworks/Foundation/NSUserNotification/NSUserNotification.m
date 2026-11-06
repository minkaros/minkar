/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <Foundation/Foundation.h>
#import <Foundation/NSUserNotification.h>

/* The app's user notifications. A notification is delivered now
 * (deliverNotification:) or at its delivery date, on the main run loop,
 * and again at each repeat interval; one with the identifier of another
 * replaces it. The delegate hears of each delivery.
 *
 * SHORTCUT(Atlas): there is no notification banner yet, so nothing is
 * shown: a delivered notification is not presented and is never
 * activated. */

NSString *const NSUserNotificationDefaultSoundName = @"NSUserNotificationDefaultSoundName";

@implementation NSUserNotificationAction

+ (instancetype)actionWithIdentifier:(NSString *)identifier title:(NSString *)title
{
    NSUserNotificationAction *action = [[self alloc] init];

    action->_identifier = [identifier copy];
    action->_title = [title copy];
    return action;
}

- (id)copyWithZone:(NSZone *)zone
{
    return self;
}

@end

@interface NSUserNotification ()
@property (nullable, readwrite, copy) NSDate *actualDeliveryDate;
@property (readwrite, getter=isPresented) BOOL presented;
@end

@implementation NSUserNotification

- (instancetype)init
{
    if ((self = [super init]) != nil) {
        _actionButtonTitle = @"Show";
        _otherButtonTitle = @"Close";
        _hasActionButton = YES;
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone
{
    NSUserNotification *copy = [[[self class] alloc] init];

    copy.title = _title;
    copy.subtitle = _subtitle;
    copy.informativeText = _informativeText;
    copy.actionButtonTitle = _actionButtonTitle;
    copy.userInfo = _userInfo;
    copy.deliveryDate = _deliveryDate;
    copy.deliveryTimeZone = _deliveryTimeZone;
    copy.deliveryRepeatInterval = _deliveryRepeatInterval;
    copy.soundName = _soundName;
    copy.hasActionButton = _hasActionButton;
    copy.otherButtonTitle = _otherButtonTitle;
    copy.identifier = _identifier;
    copy.contentImage = _contentImage;
    copy.hasReplyButton = _hasReplyButton;
    copy.responsePlaceholder = _responsePlaceholder;
    copy.additionalActions = _additionalActions;
    return copy;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@ %p: %@>", [self class], self, _title];
}

@end

@implementation NSUserNotificationCenter {
    NSMutableArray<NSUserNotification *> *_scheduled;
    NSMutableArray<NSUserNotification *> *_delivered;
    NSMapTable<NSUserNotification *, NSTimer *> *_timers;
}

+ (NSUserNotificationCenter *)defaultUserNotificationCenter
{
    static NSUserNotificationCenter *center;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        center = [[NSUserNotificationCenter alloc] init];
    });
    return center;
}

- (instancetype)init
{
    if ((self = [super init]) != nil) {
        _scheduled = [NSMutableArray array];
        _delivered = [NSMutableArray array];
        _timers = [NSMapTable strongToStrongObjectsMapTable];
    }
    return self;
}

- (NSArray<NSUserNotification *> *)scheduledNotifications
{
    @synchronized(self) {
        return [_scheduled copy];
    }
}

- (void)setScheduledNotifications:(NSArray<NSUserNotification *> *)notifications
{
    for (NSUserNotification *n in [self scheduledNotifications])
        [self removeScheduledNotification:n];
    for (NSUserNotification *n in notifications)
        [self scheduleNotification:n];
}

- (NSArray<NSUserNotification *> *)deliveredNotifications
{
    @synchronized(self) {
        return [_delivered copy];
    }
}

/* Its timer, on the main run loop (scheduling may come from any thread). */
- (void)startTimerFor:(NSUserNotification *)notification
{
    NSTimer *timer = [[NSTimer alloc] initWithFireDate:notification.deliveryDate interval:0
                                                target:self selector:@selector(timerFired:)
                                              userInfo:notification repeats:NO];
    @synchronized(self) {
        [_timers setObject:timer forKey:notification];
    }
    [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
}

- (void)scheduleNotification:(NSUserNotification *)notification
{
    if (notification.deliveryDate == nil) {
        [self deliverNotification:notification];
        return;
    }
    @synchronized(self) {
        if ([_scheduled indexOfObjectIdenticalTo:notification] != NSNotFound)
            return;
        [_scheduled addObject:notification];
    }
    if ([NSThread isMainThread])
        [self startTimerFor:notification];
    else
        [self performSelectorOnMainThread:@selector(startTimerFor:) withObject:notification
                            waitUntilDone:NO];
}

- (void)removeScheduledNotification:(NSUserNotification *)notification
{
    NSTimer *timer;

    @synchronized(self) {
        [_scheduled removeObjectIdenticalTo:notification];
        timer = [_timers objectForKey:notification];
        [_timers removeObjectForKey:notification];
    }
    [timer invalidate];
}

- (void)timerFired:(NSTimer *)timer
{
    NSUserNotification *notification = timer.userInfo;
    NSDateComponents *repeat = notification.deliveryRepeatInterval;

    @synchronized(self) {
        [_timers removeObjectForKey:notification];
        if (repeat == nil)
            [_scheduled removeObjectIdenticalTo:notification];
    }
    [self deliverNotification:notification];
    if (repeat != nil) {
        NSCalendar *calendar = [NSCalendar currentCalendar];
        if (notification.deliveryTimeZone != nil)
            calendar.timeZone = notification.deliveryTimeZone;
        NSDate *next = [calendar dateByAddingComponents:repeat toDate:notification.deliveryDate options:0];
        if (next != nil && [next compare:notification.deliveryDate] == NSOrderedDescending) {
            notification.deliveryDate = next;
            [self startTimerFor:notification];
        }
    }
}

- (void)deliverNotification:(NSUserNotification *)notification
{
    notification.actualDeliveryDate = [NSDate date];
    notification.presented = NO;
    @synchronized(self) {
        if (notification.identifier != nil)
            for (NSUserNotification *old in [_delivered copy])
                if (old != notification && [old.identifier isEqualToString:notification.identifier])
                    [_delivered removeObjectIdenticalTo:old];
        if ([_delivered indexOfObjectIdenticalTo:notification] == NSNotFound)
            [_delivered addObject:notification];
    }
    id<NSUserNotificationCenterDelegate> delegate = self.delegate;
    if ([delegate respondsToSelector:@selector(userNotificationCenter:shouldPresentNotification:)])
        (void)[delegate userNotificationCenter:self shouldPresentNotification:notification];
    if ([delegate respondsToSelector:@selector(userNotificationCenter:didDeliverNotification:)])
        [delegate userNotificationCenter:self didDeliverNotification:notification];
}

- (void)removeDeliveredNotification:(NSUserNotification *)notification
{
    @synchronized(self) {
        [_delivered removeObjectIdenticalTo:notification];
    }
}

- (void)removeAllDeliveredNotifications
{
    @synchronized(self) {
        [_delivered removeAllObjects];
    }
}

@end
