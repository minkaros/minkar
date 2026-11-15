/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <Foundation/Foundation.h>
#import <Foundation/NSUserNotification.h>
#include <dlfcn.h>
#include <mach/mach.h>
#include <objc/message.h>
#include <servers/bootstrap.h>
#include "NotificationCenterTypes.h"
#include "NotificationCenterUser.h"     /* MIG, generated */

/* The app's user notifications. A notification is delivered now
 * (deliverNotification:) or at its delivery date, on the main run loop,
 * and again at each repeat interval; one with the identifier of another
 * replaces it. The delegate hears of each delivery.
 *
 * A delivered notification is presented, unless the app is active or its
 * delegate says otherwise: it goes to notificationd (Atlas.md, 5.6, N0),
 * which has SystemUIServer show its banner and play its sound. A click on
 * the banner comes back on the app's own port, on the main run loop, and
 * the delegate hears of the activation. */

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
@property (readwrite) NSUserNotificationActivationType activationType;
@property (copy) NSString *_centerKey;         /* its delivery's, for notificationd */
@property (nullable, readwrite, copy) NSAttributedString *response;
@property (nullable, readwrite, copy) NSUserNotificationAction *additionalActivationAction;
/* UserNotifications' (N2): its category's actions, as the presenter takes
   them (kNCKeyActions), instead of this class's buttons; and, once one is
   chosen, its identifier and a reply's text. */
@property (nullable, copy) NSArray<NSDictionary *> *_centerActions;
@property (nullable, copy) NSString *_activatedAction;
@property (nullable, copy) NSString *_replyText;
@end

/* UserNotifications' hook: activations of the notifications it delivered. */
@interface NSUserNotificationCenter (UserNotificationsSPI)
@property (copy) void (^_activationHandler)(NSUserNotification *);
- (void)_deliverNotification:(NSUserNotification *)notification present:(NSInteger)present;
- (BOOL)_setBadgeLabel:(NSString *)label;
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
    copy._centerActions = __centerActions;
    return copy;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@ %p: %@>", [self class], self, _title];
}

@end

/* ---- notificationd ---- */

static mach_port_t ncServer = MACH_PORT_NULL, ncAppPort = MACH_PORT_NULL;
static BOOL ncCheckedIn;

static mach_port_t lookUpServer(BOOL again)
{
    @synchronized([NSUserNotificationCenter class]) {
        if (again && ncServer != MACH_PORT_NULL) {
            mach_port_deallocate(mach_task_self(), ncServer);
            ncServer = MACH_PORT_NULL;
            ncCheckedIn = NO;
        }
        if (ncServer == MACH_PORT_NULL &&
            bootstrap_look_up(bootstrap_port, kNCServiceName, &ncServer) != KERN_SUCCESS)
            ncServer = MACH_PORT_NULL;
        return ncServer;
    }
}

static void received(CFMachPortRef port, void *message, CFIndex size, void *info);

/* The app's port, on the main run loop, given to notificationd once (and
   again to a new one, after it went away). */
static BOOL checkIn(mach_port_t server)
{
    int32_t result = kNCSuccess;

    @synchronized([NSUserNotificationCenter class]) {
        if (ncCheckedIn)
            return YES;
        if (ncAppPort == MACH_PORT_NULL) {
            CFMachPortContext context = { 0, NULL, NULL, NULL, NULL };
            CFMachPortRef port = CFMachPortCreate(NULL, received, &context, NULL);
            CFRunLoopSourceRef source;

            if (port == NULL)
                return NO;
            ncAppPort = CFMachPortGetPort(port);
            source = CFMachPortCreateRunLoopSource(NULL, port, 0);
            CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);
            CFRelease(source);
        }
        if (ncc_check_in(server, ncAppPort, &result) != KERN_SUCCESS || result != kNCSuccess)
            return NO;
        ncCheckedIn = YES;
        return YES;
    }
}

/* Said once: the notifications are not shown. */
static void unreachable(const char *why, kern_return_t kr)
{
    static BOOL said;

    if (!said)
        fprintf(stderr, "NSUserNotificationCenter: the notification center is not reachable (%s, 0x%x)\n", why, kr);
    said = YES;
}

/* A routine to notificationd; once more to a new one if it went away. */
static BOOL callServer(kern_return_t (^call)(mach_port_t server))
{
    for (int again = 0; again < 2; again++) {
        mach_port_t server = lookUpServer(again);
        if (server == MACH_PORT_NULL) {
            unreachable("no service", 0);
            return NO;
        }
        if (!checkIn(server)) {
            unreachable("check-in", 0);
            continue;
        }
        kern_return_t kr = call(server);
        if (kr == KERN_SUCCESS)
            return YES;
        if (kr != MACH_SEND_INVALID_DEST && kr != MIG_SERVER_DIED) {
            unreachable("call", kr);
            return NO;
        }
    }
    return NO;
}

/* The app in front takes its own notifications (AppKit's NSApp, if any). */
static BOOL appIsActive(void)
{
    __unsafe_unretained id *app = (__unsafe_unretained id *)dlsym(RTLD_DEFAULT, "NSApp");

    if (app == NULL || *app == nil || ![*app respondsToSelector:@selector(isActive)])
        return NO;
    return ((BOOL (*)(id, SEL))objc_msgSend)(*app, @selector(isActive));
}

@implementation NSUserNotificationCenter {
    NSMutableArray<NSUserNotification *> *_scheduled;
    NSMutableArray<NSUserNotification *> *_delivered;
    NSMapTable<NSUserNotification *, NSTimer *> *_timers;
    void (^_activationHandler)(NSUserNotification *);
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

- (void (^)(NSUserNotification *))_activationHandler
{
    @synchronized(self) {
        return _activationHandler;
    }
}

- (void)set_activationHandler:(void (^)(NSUserNotification *))handler
{
    @synchronized(self) {
        _activationHandler = [handler copy];
    }
}

/* An alert's buttons (N2): Reply, which takes the place of the action
   button, or the action button; then the additional actions. Their
   identifiers are this class's own (kReply, kAction, kAdditional...). */
static NSString *const kReply = @"_reply", *const kAction = @"_action", *const kAdditional = @"_additional.";

static NSArray<NSDictionary *> *actionsOf(NSUserNotification *n)
{
    NSMutableArray *actions = [NSMutableArray array];

    if (n.hasReplyButton)
        [actions addObject:@{
            @kNCActionIdentifier : kReply,
            @kNCActionTitle : @"Reply",
            @kNCActionReply : @YES,
            @kNCActionPlaceholder : n.responsePlaceholder ?: @"",
            @kNCActionButton : @"Send",
        }];
    else if (n.hasActionButton && n.actionButtonTitle.length > 0)
        [actions addObject:@{ @kNCActionIdentifier : kAction, @kNCActionTitle : n.actionButtonTitle }];
    for (NSUserNotificationAction *a in n.additionalActions)
        if (a.identifier != nil && a.title.length > 0)
            [actions addObject:@{
                @kNCActionIdentifier : [kAdditional stringByAppendingString:a.identifier],
                @kNCActionTitle : a.title,
            }];
    return actions;
}

/* The notification to notificationd, to be shown. */
- (BOOL)presentNotification:(NSUserNotification *)notification
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    NSBundle *bundle = [NSBundle mainBundle];
    NSString *name = [bundle objectForInfoDictionaryKey:@"CFBundleDisplayName"] ?:
                     [bundle objectForInfoDictionaryKey:@"CFBundleName"] ?:
                     [[NSProcessInfo processInfo] processName];

    dict[@kNCKey] = notification._centerKey;
    if (notification.identifier != nil)
        dict[@kNCKeyIdentifier] = notification.identifier;
    if (notification.title != nil)
        dict[@kNCKeyTitle] = notification.title;
    if (notification.subtitle != nil)
        dict[@kNCKeySubtitle] = notification.subtitle;
    if (notification.informativeText != nil)
        dict[@kNCKeyText] = notification.informativeText;
    if (notification.soundName != nil)
        dict[@kNCKeySound] = [notification.soundName isEqualToString:NSUserNotificationDefaultSoundName]
                                 ? @"default" : notification.soundName;
    if (bundle.bundleIdentifier != nil)
        dict[@kNCKeyBundleIdentifier] = bundle.bundleIdentifier;
    if (name != nil)
        dict[@kNCKeyAppName] = name;
    if ([bundle.bundlePath.pathExtension isEqualToString:@"app"])
        dict[@kNCKeyAppPath] = bundle.bundlePath;
    dict[@kNCKeyActions] = notification._centerActions ?: actionsOf(notification);
    if (notification.otherButtonTitle.length > 0)
        dict[@kNCKeyCloseTitle] = notification.otherButtonTitle;

    NSData *data = [NSPropertyListSerialization dataWithPropertyList:dict
                                                              format:NSPropertyListBinaryFormat_v1_0
                                                             options:0 error:NULL];
    if (data == nil || data.length > kNCMaxPayload) {
        unreachable("notification too large", 0);
        return NO;
    }
    return callServer(^kern_return_t(mach_port_t server) {
        int32_t result = kNCSuccess;
        kern_return_t kr = ncc_post(server, (char *)data.bytes, (mach_msg_type_number_t)data.length, &result);
        return kr;
    });
}

- (void)removePresented:(NSUserNotification *)notification
{
    NSData *key = [notification._centerKey ?: @"" dataUsingEncoding:NSUTF8StringEncoding];

    if (!ncCheckedIn)
        return;
    callServer(^kern_return_t(mach_port_t server) {
        int32_t result = kNCSuccess;
        return ncc_remove(server, (char *)key.bytes, (mach_msg_type_number_t)key.length, &result);
    });
}

/* A click on a banner of the app's, or on one of its actions (on the main
   run loop): action, the action's identifier; text, a reply's. */
- (void)activatedKey:(NSString *)key type:(NSUserNotificationActivationType)type
              action:(NSString *)action text:(NSString *)text
{
    NSUserNotification *notification = nil;
    void (^handler)(NSUserNotification *);

    @synchronized(self) {
        for (NSUserNotification *n in _delivered)
            if ([n._centerKey isEqualToString:key]) {
                notification = n;
                break;
            }
        handler = _activationHandler;
    }
    if (notification == nil)
        return;
    notification._activatedAction = action;
    notification._replyText = text;
    notification.response = nil;
    notification.additionalActivationAction = nil;
    if ([action isEqualToString:kReply]) {
        type = NSUserNotificationActivationTypeReplied;
        notification.response = [[NSAttributedString alloc] initWithString:text ?: @""];
    } else if ([action isEqualToString:kAction])
        type = NSUserNotificationActivationTypeActionButtonClicked;
    else if ([action hasPrefix:kAdditional]) {
        NSString *identifier = [action substringFromIndex:kAdditional.length];
        type = NSUserNotificationActivationTypeAdditionalActionClicked;
        for (NSUserNotificationAction *a in notification.additionalActions)
            if ([a.identifier isEqualToString:identifier])
                notification.additionalActivationAction = a;
    } else if (action != nil && notification._centerActions == nil)
        return;
    notification.activationType = type;
    id<NSUserNotificationCenterDelegate> delegate = self.delegate;
    if ([delegate respondsToSelector:@selector(userNotificationCenter:didActivateNotification:)])
        [delegate userNotificationCenter:self didActivateNotification:notification];
    if (handler != nil)
        handler(notification);
}

- (void)deliverNotification:(NSUserNotification *)notification
{
    [self _deliverNotification:notification present:-1];
}

/* present: 1 or 0 as UserNotifications decided, -1 as this center does. */
- (void)_deliverNotification:(NSUserNotification *)notification present:(NSInteger)present
{
    notification.actualDeliveryDate = [NSDate date];
    notification.presented = NO;
    notification.activationType = NSUserNotificationActivationTypeNone;
    notification._centerKey = [[NSUUID UUID] UUIDString];
    @synchronized(self) {
        if (notification.identifier != nil)
            for (NSUserNotification *old in [_delivered copy])
                if (old != notification && [old.identifier isEqualToString:notification.identifier])
                    [_delivered removeObjectIdenticalTo:old];
        if ([_delivered indexOfObjectIdenticalTo:notification] == NSNotFound)
            [_delivered addObject:notification];
    }
    id<NSUserNotificationCenterDelegate> delegate = self.delegate;
    BOOL show = present >= 0 ? present != 0 : !appIsActive();
    if (present < 0 && [delegate respondsToSelector:@selector(userNotificationCenter:shouldPresentNotification:)])
        show = [delegate userNotificationCenter:self shouldPresentNotification:notification];
    if (show)
        notification.presented = [self presentNotification:notification];
    if ([delegate respondsToSelector:@selector(userNotificationCenter:didDeliverNotification:)])
        [delegate userNotificationCenter:self didDeliverNotification:notification];
}

- (void)removeDeliveredNotification:(NSUserNotification *)notification
{
    @synchronized(self) {
        [_delivered removeObjectIdenticalTo:notification];
    }
    if (notification.presented)
        [self removePresented:notification];
}

/* The app's badge on its Dock tile (N2), for NSDockTile and
   UserNotifications: nil or empty takes it away. */
- (BOOL)_setBadgeLabel:(NSString *)label
{
    NSData *bytes = [label ?: @"" dataUsingEncoding:NSUTF8StringEncoding];

    if (bytes.length > 256)
        return NO;
    return callServer(^kern_return_t(mach_port_t server) {
        int32_t result = kNCSuccess;
        return ncc_set_badge(server, (char *)bytes.bytes, (mach_msg_type_number_t)bytes.length, &result);
    });
}

- (void)removeAllDeliveredNotifications
{
    @synchronized(self) {
        [_delivered removeAllObjects];
    }
    [self removePresented:[NSUserNotification new]];
}

@end

/* notificationd's message: a notification activated. */
static void received(CFMachPortRef port, void *message, CFIndex size, void *info)
{
    const NCMessage *msg = message;

    if (msg->header.msgh_id != kNCMessageActivated || msg->header.msgh_size < offsetof(NCMessage, payload) ||
        msg->length > kNCMaxPayload || offsetof(NCMessage, payload) + msg->length > msg->header.msgh_size)
        return;
    NSData *data = [NSData dataWithBytes:msg->payload length:msg->length];
    NSDictionary *activation = [NSPropertyListSerialization propertyListWithData:data options:0 format:NULL error:NULL];
    if (![activation isKindOfClass:[NSDictionary class]] || ![activation[@kNCActivationKey] isKindOfClass:[NSString class]])
        return;
    NSString *action = activation[@kNCActivationAction], *text = activation[@kNCActivationText];
    [[NSUserNotificationCenter defaultUserNotificationCenter]
        activatedKey:activation[@kNCActivationKey] type:msg->arg
              action:[action isKindOfClass:[NSString class]] ? action : nil
                text:[text isKindOfClass:[NSString class]] ? text : nil];
}
