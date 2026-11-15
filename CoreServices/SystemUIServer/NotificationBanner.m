/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The notification banners (Atlas.md, 5.6, N0). notificationd's messages
 * come on a port of the main run loop; each banner is a borderless panel
 * of its own at the status level, newest on top, the others moved down.
 * N1: the settings (NotificationSettings) decide what shows and sounds;
 * the delivered notifications are kept for the notification center's panel.
 */

#import "NotificationBanner.h"
#import "NotificationSettings.h"
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include "NotificationCenterTypes.h"
#include "NotificationCenterUser.h"     /* MIG, generated */

static const CGFloat kWidth = 344, kHeight = 72, kMargin = 12, kGap = 8;
static const NSTimeInterval kShown = 5;
static const NSUInteger kMaxBanners = 4;
static const NSUInteger kMaxDelivered = 50;
static const CGFloat kCloseWidth = 64;

NSNotificationName const NotificationPresenterListDidChange = @"NotificationPresenterListDidChange";

@class NotificationBannerView;

/* One banner: the notification, its panel and its view. */
@interface NotificationBanner : NSObject
@property (copy) NSDictionary *notification;
@property (strong) NSPanel *panel;
@property (strong) NSTimer *timer;
@property BOOL alert;		/* stays until closed */
@end

@implementation NotificationBanner
@end

@interface NotificationPresenter ()
- (void)clicked:(NotificationBanner *)banner;
- (void)dismiss:(NotificationBanner *)banner;
- (void)received:(const NCMessage *)message;
@end

/* The banner's contents: the app's icon, the title, the subtitle and the
   text, on a rounded card. A click anywhere on it activates it. */
@interface NotificationBannerView : NSView
@property (weak) NotificationPresenter *presenter;
@property (weak) NotificationBanner *banner;
@property (strong) NSImage *icon;
@end

@implementation NotificationBannerView

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return YES;
}

static void drawLine(NSString *text, NSFont *font, NSColor *color, NSRect rect)
{
    NSMutableParagraphStyle *style = [[NSMutableParagraphStyle alloc] init];

    style.lineBreakMode = NSLineBreakByTruncatingTail;
    [text drawInRect:rect withAttributes:@{
        NSFontAttributeName : font,
        NSForegroundColorAttributeName : color,
        NSParagraphStyleAttributeName : style,
    }];
}

- (void)drawRect:(NSRect)dirty
{
    NSRect card = NSInsetRect(self.bounds, 0.5, 0.5);
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:card xRadius:10 yRadius:10];
    NSDictionary *n = self.banner.notification;
    NSString *title = n[@kNCKeyTitle] ?: n[@kNCKeyAppName] ?: @"";
    NSString *subtitle = n[@kNCKeySubtitle];
    NSString *text = n[@kNCKeyText];
    BOOL alert = self.banner.alert;
    CGFloat x = 60, y = 12, width = NSWidth(self.bounds) - x - 12 - (alert ? kCloseWidth : 0);

    [[NSColor colorWithCalibratedWhite:0.97 alpha:0.98] setFill];
    [path fill];
    [[NSColor colorWithCalibratedWhite:0.72 alpha:1] setStroke];
    [path stroke];
    [self.icon drawInRect:NSMakeRect(12, (NSHeight(self.bounds) - 36) / 2, 36, 36)
                 fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1];
    drawLine(title, [NSFont boldSystemFontOfSize:13], [NSColor blackColor], NSMakeRect(x, y, width, 17));
    y += 18;
    if (subtitle.length > 0) {
        drawLine(subtitle, [NSFont systemFontOfSize:12], [NSColor blackColor], NSMakeRect(x, y, width, 16));
        y += 16;
    }
    if (text.length > 0)
        drawLine(text, [NSFont systemFontOfSize:12], [NSColor darkGrayColor], NSMakeRect(x, y, width, 16));
    if (alert) {
        /* An alert's close part, on the right. */
        CGFloat left = NSWidth(self.bounds) - kCloseWidth;
        NSString *close = NSLocalizedString(@"Close", @"an alert's button");
        NSFont *font = [NSFont systemFontOfSize:12];
        NSSize size = [close sizeWithAttributes:@{ NSFontAttributeName : font }];

        [[NSColor colorWithCalibratedWhite:0.72 alpha:1] setFill];
        NSRectFill(NSMakeRect(left, 1, 1, NSHeight(self.bounds) - 2));
        [close drawAtPoint:NSMakePoint(left + (kCloseWidth - size.width) / 2, (NSHeight(self.bounds) - size.height) / 2)
            withAttributes:@{ NSFontAttributeName : font, NSForegroundColorAttributeName : [NSColor blackColor] }];
    }
}

- (void)mouseUp:(NSEvent *)event
{
    NSPoint where = [self convertPoint:event.locationInWindow fromView:nil];

    if (!NSPointInRect(where, self.bounds))
        return;
    if (self.banner.alert && where.x >= NSWidth(self.bounds) - kCloseWidth) {
        fprintf(stderr, "SystemUIServer: alert \"%s\" closed\n", [self.banner.notification[@kNCKeyTitle] ?: @"" UTF8String]);
        [self.presenter dismiss:self.banner];
        return;
    }
    [self.presenter clicked:self.banner];
}

@end

/* A panel that never takes the keyboard: the app in front keeps it. */
@interface NotificationPanel : NSPanel
@end

@implementation NotificationPanel
- (BOOL)canBecomeKeyWindow
{
    return NO;
}

- (BOOL)canBecomeMainWindow
{
    return NO;
}
@end

static void received(CFMachPortRef port, void *message, CFIndex size, void *info)
{
    [(__bridge NotificationPresenter *)info received:message];
}

static void serverGone(CFMachPortRef port, void *info);

@implementation NotificationPresenter {
    NSMutableArray<NotificationBanner *> *_banners;  /* newest first */
    NSMutableArray<NSDictionary *> *_delivered;      /* newest first */
    CFMachPortRef _port;
    CFMachPortRef _server;
}

- (void)start
{
    CFMachPortContext context = { 0, (__bridge void *)self, NULL, NULL, NULL };
    CFRunLoopSourceRef source;

    _banners = [NSMutableArray array];
    _delivered = [NSMutableArray array];
    _port = CFMachPortCreate(NULL, received, &context, NULL);
    if (_port == NULL)
        return;
    source = CFMachPortCreateRunLoopSource(NULL, _port, 0);
    CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);
    CFRelease(source);
    [self checkIn];
}

- (mach_port_t)server
{
    return _server != NULL ? CFMachPortGetPort(_server) : MACH_PORT_NULL;
}

/* The port given to notificationd; again when it comes back. */
- (void)checkIn
{
    mach_port_t server = MACH_PORT_NULL;
    int32_t result = kNCNoService;

    if (_server != NULL) {
        CFMachPortInvalidate(_server);
        CFRelease(_server);
        _server = NULL;
    }
    if (bootstrap_look_up(bootstrap_port, kNCServiceName, &server) == KERN_SUCCESS &&
        ncc_presenter_check_in(server, CFMachPortGetPort(_port), &result) == KERN_SUCCESS &&
        result == kNCSuccess) {
        CFMachPortContext context = { 0, (__bridge void *)self, NULL, NULL, NULL };
        _server = CFMachPortCreateWithPort(NULL, server, NULL, &context, NULL);
        if (_server != NULL)
            CFMachPortSetInvalidationCallBack(_server, serverGone);
        fprintf(stderr, "SystemUIServer: presenting notifications\n");
        return;
    }
    if (server != MACH_PORT_NULL)
        mach_port_deallocate(mach_task_self(), server);
    fprintf(stderr, "SystemUIServer: no notification center (%d); again in 5 s\n", result);
    [self performSelector:@selector(checkIn) withObject:nil afterDelay:5];
}

- (void)serverGone
{
    if (_server != NULL) {
        CFRelease(_server);
        _server = NULL;
    }
    [self performSelector:@selector(checkIn) withObject:nil afterDelay:1];
}

static void serverGone(CFMachPortRef port, void *info)
{
    NotificationPresenter *presenter = (__bridge NotificationPresenter *)info;

    dispatch_async(dispatch_get_main_queue(), ^{
        [presenter serverGone];
    });
}

- (void)received:(const NCMessage *)message
{
    if (message->header.msgh_size < offsetof(NCMessage, payload) || message->length > kNCMaxPayload ||
        offsetof(NCMessage, payload) + message->length > message->header.msgh_size)
        return;
    NSData *data = [NSData dataWithBytes:message->payload length:message->length];
    if (message->header.msgh_id == kNCMessageShow) {
        id plist = [NSPropertyListSerialization propertyListWithData:data options:0 format:NULL error:NULL];
        if ([plist isKindOfClass:[NSDictionary class]])
            [self deliver:plist];
    } else if (message->header.msgh_id == kNCMessageRemove) {
        NSString *key = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        for (NotificationBanner *banner in [_banners copy])
            if ([banner.notification[@kNCKeyPID] intValue] == message->arg &&
                (key.length == 0 || [banner.notification[@kNCKey] isEqualToString:key]))
                [self dismiss:banner];
        for (NSDictionary *n in [_delivered copy])
            if ([n[@kNCKeyPID] intValue] == message->arg && (key.length == 0 || [n[@kNCKey] isEqualToString:key]))
                [self unlist:n];
    }
}

/* The banners from the top right down, newest first. */
- (void)layOut
{
    NSScreen *screen = [NSScreen mainScreen];
    NSRect visible = screen != nil ? screen.visibleFrame : NSMakeRect(0, 0, 1024, 746);
    CGFloat top = NSMaxY(visible) - kMargin;

    for (NotificationBanner *banner in _banners) {
        NSRect frame = NSMakeRect(NSMaxX(visible) - kMargin - kWidth, top - kHeight, kWidth, kHeight);
        [banner.panel setFrame:frame display:YES];
        top -= kHeight + kGap;
    }
}

- (NSImage *)iconFor:(NSDictionary *)n
{
    NSString *path = n[@kNCKeyAppPath];
    NSImage *icon = path != nil ? [[NSWorkspace sharedWorkspace] iconForFile:path] : nil;

    return icon ?: [[NSWorkspace sharedWorkspace] iconForFileType:@"app"];
}

- (void)playSoundOf:(NSDictionary *)n
{
    NSString *name = n[@kNCKeySound];
    NSSound *sound = nil;

    if (name.length == 0)
        return;
    if ([name isEqualToString:@"default"])
        sound = [NSSound soundNamed:@"Notice"];
    else {
        /* The app's own sound, else one of the system's. */
        NSString *app = n[@kNCKeyAppPath];
        NSString *path = app != nil ? [[NSBundle bundleWithPath:app] pathForSoundResource:name] : nil;
        sound = path != nil ? [[NSSound alloc] initWithContentsOfFile:path byReference:YES]
                            : [NSSound soundNamed:name];
    }
    [sound play];
    fprintf(stderr, "SystemUIServer: sound %s%s\n", name.UTF8String, sound != nil ? "" : " (not found)");
}

static BOOL sameDelivery(NSDictionary *a, NSDictionary *b)
{
    return [a[@kNCKeyPID] isEqual:b[@kNCKeyPID]] && [a[@kNCKey] isEqual:b[@kNCKey]];
}

- (NSArray<NSDictionary *> *)delivered
{
    return [_delivered copy];
}

- (void)unlist:(NSDictionary *)n
{
    NSUInteger before = _delivered.count;

    [_delivered removeObjectsInArray:[_delivered filteredArrayUsingPredicate:
        [NSPredicate predicateWithBlock:^BOOL(NSDictionary *d, NSDictionary *bindings) {
            return sameDelivery(d, n);
        }]]];
    if (_delivered.count != before)
        [[NSNotificationCenter defaultCenter] postNotificationName:NotificationPresenterListDidChange object:self];
}

- (void)clearDelivered
{
    [_delivered removeAllObjects];
    fprintf(stderr, "SystemUIServer: notifications cleared\n");
    [[NSNotificationCenter defaultCenter] postNotificationName:NotificationPresenterListDidChange object:self];
}

/* A notification from notificationd: listed if its app is allowed, then
   shown and heard as its settings and Do Not Disturb say. */
- (void)deliver:(NSDictionary *)posted
{
    NotificationSettings *settings = [NotificationSettings shared];
    NSMutableDictionary *n = [posted mutableCopy];
    const char *title = [n[@kNCKeyTitle] ?: @"" UTF8String], *app = [n[@kNCKeyAppName] ?: @"?" UTF8String];

    [settings noteAppOf:n];
    if (![settings allowsNotification:n]) {
        fprintf(stderr, "SystemUIServer: \"%s\" from %s not allowed\n", title, app);
        return;
    }
    n[@kNCKeyDate] = [NSDate date];
    [self unlist:n];
    [_delivered insertObject:n atIndex:0];
    while (_delivered.count > kMaxDelivered)
        [_delivered removeLastObject];
    [[NSNotificationCenter defaultCenter] postNotificationName:NotificationPresenterListDidChange object:self];

    BOOL quiet = settings.doNotDisturb;
    NotificationStyle style = quiet ? NotificationStyleNone : [settings styleOfNotification:n];
    BOOL sound = !quiet && [settings soundsNotification:n];
    fprintf(stderr, "SystemUIServer: listed \"%s\" from %s: %s, sound %s%s\n", title, app,
            style == NotificationStyleAlert ? "alert" : style == NotificationStyleBanner ? "banner" : "not shown",
            sound ? "on" : "off", quiet ? " (do not disturb)" : "");
    if (style != NotificationStyleNone)
        [self show:n alert:style == NotificationStyleAlert];
    if (sound)
        [self playSoundOf:n];
}

- (void)show:(NSDictionary *)n alert:(BOOL)alert
{
    NotificationBanner *banner = [[NotificationBanner alloc] init];
    NSRect frame = NSMakeRect(0, 0, kWidth, kHeight);
    NotificationPanel *panel = [[NotificationPanel alloc] initWithContentRect:frame
                                                                    styleMask:NSWindowStyleMaskBorderless |
                                                                              NSWindowStyleMaskNonactivatingPanel
                                                                      backing:NSBackingStoreBuffered
                                                                        defer:NO];
    NotificationBannerView *view = [[NotificationBannerView alloc] initWithFrame:frame];

    /* The app's notification of the same key, replaced. */
    for (NotificationBanner *old in [_banners copy])
        if (sameDelivery(old.notification, n))
            [self dismiss:old];
    banner.notification = n;
    banner.alert = alert;
    banner.panel = panel;
    view.presenter = self;
    view.banner = banner;
    view.icon = [self iconFor:n];
    panel.contentView = view;
    panel.level = NSStatusWindowLevel;
    panel.opaque = NO;
    panel.backgroundColor = [NSColor clearColor];
    panel.hidesOnDeactivate = NO;
    panel.releasedWhenClosed = NO;
    [_banners insertObject:banner atIndex:0];
    while (_banners.count > kMaxBanners)
        [self dismiss:_banners.lastObject];
    [self layOut];
    [panel orderFrontRegardless];
    if (!alert)
        banner.timer = [NSTimer scheduledTimerWithTimeInterval:kShown target:self selector:@selector(expired:)
                                                      userInfo:banner repeats:NO];
    fprintf(stderr, "SystemUIServer: %s \"%s\" from %s (%d)\n", alert ? "alert" : "banner",
            [n[@kNCKeyTitle] ?: @"" UTF8String], [n[@kNCKeyAppName] ?: @"?" UTF8String],
            [n[@kNCKeyPID] intValue]);
}

- (void)expired:(NSTimer *)timer
{
    [self dismiss:timer.userInfo];
}

- (void)dismiss:(NotificationBanner *)banner
{
    if (![_banners containsObject:banner])
        return;
    [banner.timer invalidate];
    [banner.panel close];
    [_banners removeObject:banner];
    [self layOut];
}

- (void)clicked:(NotificationBanner *)banner
{
    [self dismiss:banner];
    [self activateDelivered:banner.notification];
}

/* The app to the front, and told which notification it was. */
- (void)activateDelivered:(NSDictionary *)n
{
    pid_t pid = [n[@kNCKeyPID] intValue];
    NSData *key = [n[@kNCKey] ?: @"" dataUsingEncoding:NSUTF8StringEncoding];
    int32_t result = kNCNoService;

    [[NSRunningApplication runningApplicationWithProcessIdentifier:pid]
        activateWithOptions:NSApplicationActivateIgnoringOtherApps];
    if (self.server != MACH_PORT_NULL)
        ncc_activated(self.server, pid, (char *)key.bytes, (mach_msg_type_number_t)key.length,
                      NSUserNotificationActivationTypeContentsClicked, &result);
    fprintf(stderr, "SystemUIServer: activated \"%s\" of %d (%d)\n", [n[@kNCKeyTitle] ?: @"" UTF8String], pid, result);
    for (NotificationBanner *banner in [_banners copy])
        if (sameDelivery(banner.notification, n))
            [self dismiss:banner];
    [self unlist:n];
}

@end
