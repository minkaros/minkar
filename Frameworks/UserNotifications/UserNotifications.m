// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// Built with ARC. Requests wait for their trigger on the main queue; when
// it fires the notification is delivered: kept in the delivered list,
// handed to NSUserNotificationCenter (which shows nothing yet), and the
// delegate is asked how to present it.

#import <UserNotifications/UserNotifications.h>

NSString *const UNErrorDomain = @"UNErrorDomain";
NSString *const UNNotificationDefaultActionIdentifier = @"com.apple.UNNotificationDefaultActionIdentifier";
NSString *const UNNotificationDismissActionIdentifier = @"com.apple.UNNotificationDismissActionIdentifier";

#define SECURE_CODING \
   + (BOOL)supportsSecureCoding { return YES; } \
   - (id)copyWithZone:(NSZone *)zone { return self; }

@interface UNNotificationSound ()
@property (copy) NSString *name;
@end

@implementation UNNotificationSound
SECURE_CODING
+ (UNNotificationSound *)defaultSound {
    static UNNotificationSound *sound;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ sound = [self soundNamed:NSUserNotificationDefaultSoundName]; });
    return sound;
}
+ (instancetype)soundNamed:(NSString *)name {
    UNNotificationSound *sound = [self new];
    sound.name = name;
    return sound;
}
- (void)encodeWithCoder:(NSCoder *)coder { [coder encodeObject:self.name forKey:@"name"]; }
- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init]))
        _name = [coder decodeObjectOfClass:[NSString class] forKey:@"name"];
    return self;
}
@end

@interface UNNotificationAttachment ()
@property (readwrite, copy) NSString *identifier;
@property (readwrite, copy) NSURL *URL;
@property (readwrite, copy) NSString *type;
@end

@implementation UNNotificationAttachment
SECURE_CODING
+ (instancetype)attachmentWithIdentifier:(NSString *)identifier URL:(NSURL *)URL options:(NSDictionary *)options error:(NSError **)error {
    if (![URL isFileURL] || ![URL checkResourceIsReachableAndReturnError:error])
        return nil;
    UNNotificationAttachment *a = [self new];
    a.identifier = identifier.length ? identifier : [[NSUUID UUID] UUIDString];
    a.URL = URL;
    a.type = URL.pathExtension;
    return a;
}
- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.identifier forKey:@"identifier"];
    [coder encodeObject:self.URL forKey:@"URL"];
}
- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init])) {
        _identifier = [coder decodeObjectOfClass:[NSString class] forKey:@"identifier"];
        _URL = [coder decodeObjectOfClass:[NSURL class] forKey:@"URL"];
    }
    return self;
}
@end

@implementation UNNotificationContent {
@protected
    NSArray *_attachments;
    NSNumber *_badge;
    NSString *_body, *_categoryIdentifier, *_subtitle, *_threadIdentifier, *_title;
    UNNotificationSound *_sound;
    NSDictionary *_userInfo;
}
@synthesize attachments = _attachments, badge = _badge, body = _body, categoryIdentifier = _categoryIdentifier,
            subtitle = _subtitle, threadIdentifier = _threadIdentifier, title = _title, sound = _sound, userInfo = _userInfo;

+ (BOOL)supportsSecureCoding { return YES; }

- (instancetype)init {
    if ((self = [super init])) {
        _attachments = @[];
        _body = _categoryIdentifier = _subtitle = _threadIdentifier = _title = @"";
        _userInfo = @{};
    }
    return self;
}

- (void)_copyInto:(UNNotificationContent *)c {
    c->_attachments = [_attachments copy]; c->_badge = _badge; c->_body = [_body copy];
    c->_categoryIdentifier = [_categoryIdentifier copy]; c->_subtitle = [_subtitle copy];
    c->_threadIdentifier = [_threadIdentifier copy]; c->_title = [_title copy];
    c->_sound = _sound; c->_userInfo = [_userInfo copy];
}

- (id)copyWithZone:(NSZone *)zone {
    UNNotificationContent *c = [UNNotificationContent new];
    [self _copyInto:c];
    return c;
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    UNMutableNotificationContent *c = [UNMutableNotificationContent new];
    [self _copyInto:c];
    return c;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_title forKey:@"title"];
    [coder encodeObject:_subtitle forKey:@"subtitle"];
    [coder encodeObject:_body forKey:@"body"];
    [coder encodeObject:_badge forKey:@"badge"];
    [coder encodeObject:_categoryIdentifier forKey:@"categoryIdentifier"];
    [coder encodeObject:_threadIdentifier forKey:@"threadIdentifier"];
    [coder encodeObject:_userInfo forKey:@"userInfo"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [self init])) {
        NSSet *strings = [NSSet setWithObject:[NSString class]];
        _title = [coder decodeObjectOfClasses:strings forKey:@"title"] ?: @"";
        _subtitle = [coder decodeObjectOfClasses:strings forKey:@"subtitle"] ?: @"";
        _body = [coder decodeObjectOfClasses:strings forKey:@"body"] ?: @"";
        _badge = [coder decodeObjectOfClass:[NSNumber class] forKey:@"badge"];
        _categoryIdentifier = [coder decodeObjectOfClasses:strings forKey:@"categoryIdentifier"] ?: @"";
        _threadIdentifier = [coder decodeObjectOfClasses:strings forKey:@"threadIdentifier"] ?: @"";
        _userInfo = [coder decodeObjectOfClass:[NSDictionary class] forKey:@"userInfo"] ?: @{};
    }
    return self;
}
@end

@implementation UNMutableNotificationContent
@dynamic attachments, badge, body, categoryIdentifier, sound, subtitle, threadIdentifier, title, userInfo;
- (void)setAttachments:(NSArray *)v { _attachments = [v copy]; }
- (void)setBadge:(NSNumber *)v { _badge = [v copy]; }
- (void)setBody:(NSString *)v { _body = [v copy] ?: @""; }
- (void)setCategoryIdentifier:(NSString *)v { _categoryIdentifier = [v copy] ?: @""; }
- (void)setSound:(UNNotificationSound *)v { _sound = v; }
- (void)setSubtitle:(NSString *)v { _subtitle = [v copy] ?: @""; }
- (void)setThreadIdentifier:(NSString *)v { _threadIdentifier = [v copy] ?: @""; }
- (void)setTitle:(NSString *)v { _title = [v copy] ?: @""; }
- (void)setUserInfo:(NSDictionary *)v { _userInfo = [v copy] ?: @{}; }
@end

@interface UNNotificationTrigger ()
@property (readwrite) BOOL repeats;
- (NSDate *)_fireDateAfter:(NSDate *)date;
@end

@implementation UNNotificationTrigger
SECURE_CODING
- (void)encodeWithCoder:(NSCoder *)coder { [coder encodeBool:self.repeats forKey:@"repeats"]; }
- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init]))
        _repeats = [coder decodeBoolForKey:@"repeats"];
    return self;
}
- (NSDate *)_fireDateAfter:(NSDate *)date { return nil; }
@end

@implementation UNPushNotificationTrigger
@end

@interface UNTimeIntervalNotificationTrigger ()
@property (readwrite) NSTimeInterval timeInterval;
@property (strong) NSDate *start;
@end

@implementation UNTimeIntervalNotificationTrigger
+ (instancetype)triggerWithTimeInterval:(NSTimeInterval)timeInterval repeats:(BOOL)repeats {
    if (timeInterval <= 0 || (repeats && timeInterval < 60))
        [NSException raise:NSInternalInconsistencyException
                    format:@"time interval must be greater than 0 (and at least 60 if repeating)"];
    UNTimeIntervalNotificationTrigger *t = [self new];
    t.timeInterval = timeInterval;
    t.repeats = repeats;
    t.start = [NSDate date];
    return t;
}
- (NSDate *)nextTriggerDate { return [self _fireDateAfter:[NSDate date]]; }
- (NSDate *)_fireDateAfter:(NSDate *)date {
    NSDate *fire = [self.start dateByAddingTimeInterval:self.timeInterval];
    while (self.repeats && [fire compare:date] != NSOrderedDescending)
        fire = [fire dateByAddingTimeInterval:self.timeInterval];
    return fire;
}
@end

@interface UNCalendarNotificationTrigger ()
@property (readwrite, copy) NSDateComponents *dateComponents;
@end

@implementation UNCalendarNotificationTrigger
+ (instancetype)triggerWithDateMatchingComponents:(NSDateComponents *)dateComponents repeats:(BOOL)repeats {
    UNCalendarNotificationTrigger *t = [self new];
    t.dateComponents = dateComponents;
    t.repeats = repeats;
    return t;
}
- (NSDate *)nextTriggerDate { return [self _fireDateAfter:[NSDate date]]; }
/* The first second after the date whose components match: searched
   minute by minute for up to a year (the components name at most a time
   of day, a weekday or a date). */
- (NSDate *)_fireDateAfter:(NSDate *)date {
    NSCalendar *calendar = self.dateComponents.calendar ?: [NSCalendar currentCalendar];
    NSDateComponents *want = self.dateComponents;
    NSCalendarUnit units = NSCalendarUnitYear | NSCalendarUnitMonth | NSCalendarUnitDay | NSCalendarUnitHour |
                           NSCalendarUnitMinute | NSCalendarUnitWeekday;
    NSDate *t = [NSDate dateWithTimeIntervalSinceReferenceDate:
        floor([date timeIntervalSinceReferenceDate] / 60) * 60 + 60];
    for (int i = 0; i < 366 * 24 * 60; i++, t = [t dateByAddingTimeInterval:60]) {
        NSDateComponents *have = [calendar components:units fromDate:t];
#define MATCH(f) (want.f == NSDateComponentUndefined || want.f == have.f)
        if (MATCH(year) && MATCH(month) && MATCH(day) && MATCH(hour) && MATCH(minute) && MATCH(weekday))
            return t;
#undef MATCH
    }
    return nil;
}
@end

@interface UNNotificationRequest ()
@property (readwrite, copy) NSString *identifier;
@property (readwrite, copy) UNNotificationContent *content;
@property (readwrite, copy) UNNotificationTrigger *trigger;
@end

@implementation UNNotificationRequest
SECURE_CODING
+ (instancetype)requestWithIdentifier:(NSString *)identifier content:(UNNotificationContent *)content trigger:(UNNotificationTrigger *)trigger {
    UNNotificationRequest *r = [self new];
    r.identifier = identifier;
    r.content = content;
    r.trigger = trigger;
    return r;
}
- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.identifier forKey:@"identifier"];
    [coder encodeObject:self.content forKey:@"content"];
}
- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init])) {
        _identifier = [coder decodeObjectOfClass:[NSString class] forKey:@"identifier"];
        _content = [coder decodeObjectOfClass:[UNNotificationContent class] forKey:@"content"];
    }
    return self;
}
@end

@interface UNNotification ()
@property (readwrite, copy) NSDate *date;
@property (readwrite, copy) UNNotificationRequest *request;
@end

@implementation UNNotification
SECURE_CODING
- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.date forKey:@"date"];
    [coder encodeObject:self.request forKey:@"request"];
}
- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init])) {
        _date = [coder decodeObjectOfClass:[NSDate class] forKey:@"date"];
        _request = [coder decodeObjectOfClass:[UNNotificationRequest class] forKey:@"request"];
    }
    return self;
}
@end

@interface UNNotificationResponse ()
@property (readwrite, copy) NSString *actionIdentifier;
@property (readwrite, copy) UNNotification *notification;
@end

@implementation UNNotificationResponse
SECURE_CODING
- (void)encodeWithCoder:(NSCoder *)coder { [coder encodeObject:self.actionIdentifier forKey:@"action"]; }
- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init]))
        _actionIdentifier = [coder decodeObjectOfClass:[NSString class] forKey:@"action"];
    return self;
}
@end

@interface UNNotificationAction ()
@property (readwrite, copy) NSString *identifier;
@property (readwrite, copy) NSString *title;
@end

@implementation UNNotificationAction
SECURE_CODING
+ (instancetype)actionWithIdentifier:(NSString *)identifier title:(NSString *)title options:(NSUInteger)options {
    UNNotificationAction *a = [self new];
    a.identifier = identifier;
    a.title = title;
    return a;
}
- (void)encodeWithCoder:(NSCoder *)coder { [coder encodeObject:self.identifier forKey:@"identifier"]; }
- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init]))
        _identifier = [coder decodeObjectOfClass:[NSString class] forKey:@"identifier"];
    return self;
}
@end

@interface UNNotificationCategory ()
@property (readwrite, copy) NSString *identifier;
@property (readwrite, copy) NSArray *actions;
@end

@implementation UNNotificationCategory
SECURE_CODING
+ (instancetype)categoryWithIdentifier:(NSString *)identifier actions:(NSArray *)actions
                     intentIdentifiers:(NSArray *)intentIdentifiers options:(NSUInteger)options {
    UNNotificationCategory *c = [self new];
    c.identifier = identifier;
    c.actions = actions;
    return c;
}
- (void)encodeWithCoder:(NSCoder *)coder { [coder encodeObject:self.identifier forKey:@"identifier"]; }
- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init]))
        _identifier = [coder decodeObjectOfClass:[NSString class] forKey:@"identifier"];
    return self;
}
- (NSUInteger)hash { return self.identifier.hash; }
- (BOOL)isEqual:(id)other {
    return [other isKindOfClass:[UNNotificationCategory class]] && [self.identifier isEqual:[other identifier]];
}
@end

@interface UNNotificationSettings ()
@property (readwrite) UNAuthorizationStatus authorizationStatus;
@end

@implementation UNNotificationSettings
SECURE_CODING
- (UNNotificationSetting)soundSetting { return self.authorizationStatus == UNAuthorizationStatusAuthorized ? UNNotificationSettingEnabled : UNNotificationSettingDisabled; }
- (UNNotificationSetting)badgeSetting { return self.soundSetting; }
- (UNNotificationSetting)alertSetting { return self.soundSetting; }
- (UNNotificationSetting)notificationCenterSetting { return self.soundSetting; }
- (UNNotificationSetting)lockScreenSetting { return UNNotificationSettingNotSupported; }
- (UNAlertStyle)alertStyle { return self.authorizationStatus == UNAuthorizationStatusAuthorized ? UNAlertStyleBanner : UNAlertStyleNone; }
- (void)encodeWithCoder:(NSCoder *)coder { [coder encodeInteger:self.authorizationStatus forKey:@"status"]; }
- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init]))
        _authorizationStatus = [coder decodeIntegerForKey:@"status"];
    return self;
}
@end

@implementation UNUserNotificationCenter {
    NSMutableDictionary<NSString *, UNNotificationRequest *> *_pending;
    NSMutableArray<UNNotification *> *_delivered;
    NSSet *_categories;
    BOOL _authorized;
    NSUInteger _generation;
}

+ (UNUserNotificationCenter *)currentNotificationCenter {
    static UNUserNotificationCenter *center;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ center = [self new]; });
    return center;
}

- (instancetype)init {
    if ((self = [super init])) {
        _pending = [NSMutableDictionary dictionary];
        _delivered = [NSMutableArray array];
        _categories = [NSSet set];
    }
    return self;
}

- (BOOL)supportsContentExtensions { return NO; }

/* There is no prompt: what an app asks for is granted. */
- (void)requestAuthorizationWithOptions:(UNAuthorizationOptions)options completionHandler:(void (^)(BOOL, NSError *))completionHandler {
    @synchronized (self) { _authorized = YES; }
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{ completionHandler(YES, nil); });
}

- (void)getNotificationSettingsWithCompletionHandler:(void (^)(UNNotificationSettings *))completionHandler {
    UNNotificationSettings *settings = [UNNotificationSettings new];
    @synchronized (self) {
        settings.authorizationStatus = _authorized ? UNAuthorizationStatusAuthorized : UNAuthorizationStatusNotDetermined;
    }
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{ completionHandler(settings); });
}

- (void)setNotificationCategories:(NSSet *)categories {
    @synchronized (self) { _categories = [categories copy]; }
}

- (void)getNotificationCategoriesWithCompletionHandler:(void (^)(NSSet *))completionHandler {
    NSSet *categories;
    @synchronized (self) { categories = _categories; }
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{ completionHandler(categories); });
}

- (void)_deliver:(UNNotificationRequest *)request {
    UNNotification *notification = [UNNotification new];
    notification.date = [NSDate date];
    notification.request = request;
    @synchronized (self) {
        for (UNNotification *old in [_delivered copy])
            if ([old.request.identifier isEqualToString:request.identifier])
                [_delivered removeObjectIdenticalTo:old];
        [_delivered addObject:notification];
    }
    NSUserNotification *legacy = [NSUserNotification new];
    legacy.identifier = request.identifier;
    legacy.title = request.content.title;
    legacy.subtitle = request.content.subtitle;
    legacy.informativeText = request.content.body;
    legacy.userInfo = request.content.userInfo;
    [[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:legacy];
    id<UNUserNotificationCenterDelegate> delegate = self.delegate;
    if ([delegate respondsToSelector:@selector(userNotificationCenter:willPresentNotification:withCompletionHandler:)])
        [delegate userNotificationCenter:self willPresentNotification:notification withCompletionHandler:^(UNNotificationPresentationOptions o) {}];
}

- (void)_schedule:(UNNotificationRequest *)request generation:(NSUInteger)generation {
    NSDate *fire = [request.trigger _fireDateAfter:[NSDate date]];
    NSTimeInterval wait = fire ? MAX(0, [fire timeIntervalSinceNow]) : 0;
    if (request.trigger != nil && fire == nil)
        return;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(wait * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        @synchronized (self) {
            /* Removed or replaced since. */
            if (self->_pending[request.identifier] != request || self->_generation != generation)
                return;
            if (!request.trigger.repeats)
                [self->_pending removeObjectForKey:request.identifier];
        }
        [self _deliver:request];
        if (request.trigger.repeats)
            [self _schedule:request generation:generation];
    });
}

- (void)addNotificationRequest:(UNNotificationRequest *)request withCompletionHandler:(void (^)(NSError *))completionHandler {
    NSUInteger generation;
    if (request.identifier.length == 0) {
        if (completionHandler)
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
                completionHandler([NSError errorWithDomain:UNErrorDomain code:1 userInfo:nil]);
            });
        return;
    }
    @synchronized (self) {
        _pending[request.identifier] = request;
        generation = _generation;
    }
    [self _schedule:request generation:generation];
    if (completionHandler)
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{ completionHandler(nil); });
}

- (void)getPendingNotificationRequestsWithCompletionHandler:(void (^)(NSArray *))completionHandler {
    NSArray *requests;
    @synchronized (self) { requests = _pending.allValues; }
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{ completionHandler(requests); });
}

- (void)removePendingNotificationRequestsWithIdentifiers:(NSArray *)identifiers {
    @synchronized (self) { [_pending removeObjectsForKeys:identifiers]; }
}

- (void)removeAllPendingNotificationRequests {
    @synchronized (self) {
        [_pending removeAllObjects];
        _generation++;
    }
}

- (void)getDeliveredNotificationsWithCompletionHandler:(void (^)(NSArray *))completionHandler {
    NSArray *delivered;
    @synchronized (self) { delivered = [_delivered copy]; }
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{ completionHandler(delivered); });
}

- (void)removeDeliveredNotificationsWithIdentifiers:(NSArray *)identifiers {
    @synchronized (self) {
        for (UNNotification *n in [_delivered copy])
            if ([identifiers containsObject:n.request.identifier])
                [_delivered removeObjectIdenticalTo:n];
    }
}

- (void)removeAllDeliveredNotifications {
    @synchronized (self) { [_delivered removeAllObjects]; }
    [[NSUserNotificationCenter defaultUserNotificationCenter] removeAllDeliveredNotifications];
}

@end

@implementation NSString (UNUserNotificationCenterSupport)
/* The key localized in the main bundle, the arguments put in. */
+ (NSString *)localizedUserNotificationStringForKey:(NSString *)key arguments:(NSArray *)arguments {
    NSString *format = [[NSBundle mainBundle] localizedStringForKey:key value:key table:nil];
    NSMutableString *result = [NSMutableString string];
    NSUInteger next = 0;
    NSScanner *scanner = [NSScanner scannerWithString:format];
    scanner.charactersToBeSkipped = nil;
    while (!scanner.atEnd) {
        NSString *chunk = nil;
        if ([scanner scanUpToString:@"%" intoString:&chunk])
            [result appendString:chunk];
        if (scanner.atEnd)
            break;
        scanner.scanLocation += 1;
        if ([scanner scanString:@"%" intoString:NULL]) {
            [result appendString:@"%"];
            continue;
        }
        NSInteger position = 0;
        if ([scanner scanInteger:&position] && [scanner scanString:@"$" intoString:NULL])
            next = (NSUInteger)(position - 1);
        [scanner scanString:@"@" intoString:NULL] || [scanner scanString:@"d" intoString:NULL];
        if (next < arguments.count)
            [result appendString:[arguments[next] description]];
        next++;
    }
    return result;
}
@end
