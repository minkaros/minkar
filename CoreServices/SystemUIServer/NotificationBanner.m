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
static const CGFloat kColumnWidth = 80, kReplyHeight = 40;

NSNotificationName const NotificationPresenterListDidChange = @"NotificationPresenterListDidChange";

@class NotificationBannerView;

/* One banner: the notification, its panel and its view. */
@interface NotificationBanner : NSObject
@property (copy) NSDictionary *notification;
@property (strong) NSPanel *panel;
@property (strong) NSTimer *timer;
@property BOOL alert;		/* stays until closed */
@property (strong) NSTextField *reply;		/* a reply being written (N2) */
@property (copy) NSDictionary *replyAction;
@end

@implementation NotificationBanner
@end

@interface NotificationPresenter ()
- (void)clicked:(NotificationBanner *)banner;
- (void)dismiss:(NotificationBanner *)banner;
- (void)banner:(NotificationBanner *)banner chose:(NSDictionary *)action;
- (void)closed:(NotificationBanner *)banner;
- (void)sendReply:(NotificationBanner *)banner;
- (void)received:(const NCMessage *)message;
@end

/* The banner's contents: the app's icon, the title, the subtitle and the
   text, on a rounded card. A click anywhere on it activates it. */
@interface NotificationBannerView : NSView
@property (weak) NotificationPresenter *presenter;
@property (weak) NotificationBanner *banner;
@property (strong) NSImage *icon;
@end

static void drawCentered(NSString *text, NSFont *font, NSRect rect);

/* The API's own default titles, in the user's language; an app's, as given. */
static NSString *localizedTitle(NSString *title)
{
    if ([title isEqualToString:@"Show"])
        return NSLocalizedString(@"Show", @"an alert's default action");
    if ([title isEqualToString:@"Reply"])
        return NSLocalizedString(@"Reply", @"an alert's reply");
    return title ?: @"";
}

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
    CGFloat x = 60, y = 12, width = NSWidth(self.bounds) - x - 12 - (alert ? kColumnWidth : 0);

    [[NSColor colorWithGenericGamma22White:0.97 alpha:0.98] setFill];
    [path fill];
    [[NSColor colorWithGenericGamma22White:0.72 alpha:1] setStroke];
    [path stroke];
    [self.icon drawInRect:NSMakeRect(12, (kHeight - 36) / 2, 36, 36)
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
        /* An alert's column, on the right: its action (or Options, for
           several) over Close; Close alone if it has none. */
        CGFloat left = NSWidth(self.bounds) - kColumnWidth;
        NSArray *actions = self.actions;
        NSString *close = n[@kNCKeyCloseTitle];
        NSFont *font = [NSFont systemFontOfSize:12];
        NSColor *line = [NSColor colorWithGenericGamma22White:0.72 alpha:1];

        if (close.length == 0 || [close isEqualToString:@"Close"])
            close = NSLocalizedString(@"Close", @"an alert's button");
        [line setFill];
        NSRectFill(NSMakeRect(left, 1, 1, kHeight - 2));
        if (actions.count > 0) {
            NSString *title = actions.count == 1 ? localizedTitle(actions[0][@kNCActionTitle])
                                                 : NSLocalizedString(@"Options", @"an alert's actions");
            NSRectFill(NSMakeRect(left, kHeight / 2, kColumnWidth - 1, 1));
            drawCentered(title, font, NSMakeRect(left, 0, kColumnWidth, kHeight / 2));
            drawCentered(close, font, NSMakeRect(left, kHeight / 2, kColumnWidth, kHeight / 2));
        } else
            drawCentered(close, font, NSMakeRect(left, 0, kColumnWidth, kHeight));
        if (self.banner.reply != nil) {
            [line setFill];
            NSRectFill(NSMakeRect(1, kHeight, NSWidth(self.bounds) - 2, 1));
        }
    }
}

static void drawCentered(NSString *text, NSFont *font, NSRect rect)
{
    NSMutableParagraphStyle *style = [[NSMutableParagraphStyle alloc] init];
    NSSize size = [text sizeWithAttributes:@{ NSFontAttributeName : font }];

    style.alignment = NSTextAlignmentCenter;
    style.lineBreakMode = NSLineBreakByTruncatingTail;
    [text drawInRect:NSMakeRect(NSMinX(rect) + 4, NSMinY(rect) + (NSHeight(rect) - size.height) / 2,
                                NSWidth(rect) - 8, size.height)
      withAttributes:@{ NSFontAttributeName : font, NSForegroundColorAttributeName : [NSColor blackColor],
                        NSParagraphStyleAttributeName : style }];
}

- (NSArray<NSDictionary *> *)actions
{
    NSArray *actions = self.banner.notification[@kNCKeyActions];

    return [actions isKindOfClass:[NSArray class]] ? actions : @[];
}

- (void)replySent:(id)sender
{
    [self.presenter sendReply:self.banner];
}

- (void)menuChose:(NSMenuItem *)item
{
    [self.presenter banner:self.banner chose:item.representedObject];
}

- (void)mouseUp:(NSEvent *)event
{
    NSPoint where = [self convertPoint:event.locationInWindow fromView:nil];

    if (!NSPointInRect(where, self.bounds))
        return;
    if (where.y > kHeight)
        return;		/* the reply's part: its field and button */
    if (self.banner.alert && where.x >= NSWidth(self.bounds) - kColumnWidth) {
        NSArray *actions = self.actions;
        if (actions.count > 0 && where.y < kHeight / 2) {
            if (actions.count == 1) {
                [self.presenter banner:self.banner chose:actions[0]];
                return;
            }
            NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
            for (NSDictionary *action in actions) {
                NSMenuItem *item = [menu addItemWithTitle:localizedTitle(action[@kNCActionTitle]) action:@selector(menuChose:)
                                            keyEquivalent:@""];
                item.target = self;
                item.representedObject = action;
            }
            fprintf(stderr, "SystemUIServer: alert options, %lu\n", (unsigned long)actions.count);
            [NSMenu popUpContextMenu:menu withEvent:event forView:self];
            return;
        }
        fprintf(stderr, "SystemUIServer: alert \"%s\" closed\n", [self.banner.notification[@kNCKeyTitle] ?: @"" UTF8String]);
        [self.presenter closed:self.banner];
        return;
    }
    [self.presenter clicked:self.banner];
}

@end

/* A panel that takes the keyboard only for a reply: otherwise the app in
   front keeps it. */
@interface NotificationPanel : NSPanel
@property BOOL takesKeys;
@end

@implementation NotificationPanel
- (BOOL)canBecomeKeyWindow
{
    return self.takesKeys;
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
        CGFloat height = kHeight + (banner.reply != nil ? kReplyHeight : 0);
        NSRect frame = NSMakeRect(NSMaxX(visible) - kMargin - kWidth, top - height, kWidth, height);
        [banner.panel setFrame:frame display:YES];
        [banner.panel.contentView setFrame:NSMakeRect(0, 0, kWidth, height)];
        top -= height + kGap;
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

/* An alert closed by the user: gone, and, when its category asks for it
   (N2), the app told with the dismiss action, staying where it is. */
- (void)closed:(NotificationBanner *)banner
{
    NSDictionary *n = banner.notification;

    [self dismiss:banner];
    if ([n[@kNCKeyReportsDismiss] boolValue])
        [self activate:n action:@{ @kNCActionIdentifier : @kNCDismissAction } text:nil];
}

/* An alert's action (N2): a reply opens its field; any other is the app's. */
- (void)banner:(NotificationBanner *)banner chose:(NSDictionary *)action
{
    if (![action isKindOfClass:[NSDictionary class]])
        return;
    if ([action[@kNCActionReply] boolValue]) {
        [self openReply:banner action:action];
        return;
    }
    [self dismiss:banner];
    [self activate:banner.notification action:action text:nil];
}

/* The alert grows a text field and its button; the panel takes the keys. */
- (void)openReply:(NotificationBanner *)banner action:(NSDictionary *)action
{
    NotificationPanel *panel = (NotificationPanel *)banner.panel;
    NotificationBannerView *view = (NotificationBannerView *)panel.contentView;
    NSString *send = action[@kNCActionButton];
    NSTextField *field;
    NSButton *button;

    if (banner.reply != nil)
        return;
    if (send.length == 0 || [send isEqualToString:@"Send"])
        send = NSLocalizedString(@"Send", @"a reply's button");
    field = [[NSTextField alloc] initWithFrame:NSMakeRect(12, kHeight + 8, kWidth - 12 - 12 - 76, 24)];
    [[field cell] setPlaceholderString:action[@kNCActionPlaceholder] ?: @""];
    [field setTarget:view];
    [field setAction:@selector(replySent:)];
    button = [[NSButton alloc] initWithFrame:NSMakeRect(kWidth - 12 - 70, kHeight + 7, 70, 26)];
    [button setTitle:send];
    [button setBezelStyle:NSBezelStyleRounded];
    [button setTarget:view];
    [button setAction:@selector(replySent:)];
    [view addSubview:field];
    [view addSubview:button];
    banner.reply = field;
    banner.replyAction = action;
    [banner.timer invalidate];
    banner.timer = nil;
    [self layOut];
    panel.takesKeys = YES;
    [NSApp activateIgnoringOtherApps:YES];
    [panel makeKeyAndOrderFront:nil];
    [panel makeFirstResponder:field];
    [view setNeedsDisplay:YES];
    fprintf(stderr, "SystemUIServer: reply to \"%s\" open\n", [banner.notification[@kNCKeyTitle] ?: @"" UTF8String]);
}

- (void)sendReply:(NotificationBanner *)banner
{
    NSString *text = banner.reply.stringValue ?: @"";

    [self dismiss:banner];
    [self activate:banner.notification action:banner.replyAction text:text];
}

/* The app to the front, and told which notification it was. */
- (void)activateDelivered:(NSDictionary *)n
{
    [self activate:n action:nil text:nil];
}

/* The app to the front, told of its notification, the action chosen (nil:
   the notification itself) and a reply's text. */
- (void)activate:(NSDictionary *)n action:(nullable NSDictionary *)action text:(nullable NSString *)text
{
    pid_t pid = [n[@kNCKeyPID] intValue];
    NSMutableDictionary *activation = [NSMutableDictionary dictionaryWithObject:n[@kNCKey] ?: @"" forKey:@kNCActivationKey];
    int32_t result = kNCNoService;
    NSUserNotificationActivationType type = NSUserNotificationActivationTypeContentsClicked;

    if (action[@kNCActionIdentifier] != nil) {
        activation[@kNCActivationAction] = action[@kNCActionIdentifier];
        type = text != nil ? NSUserNotificationActivationTypeReplied : NSUserNotificationActivationTypeActionButtonClicked;
    }
    if (text != nil)
        activation[@kNCActivationText] = text;
    NSData *data = [NSPropertyListSerialization dataWithPropertyList:activation format:NSPropertyListBinaryFormat_v1_0
                                                             options:0 error:NULL];

    /* The app to the front, unless the notification was only dismissed. */
    if (![action[@kNCActionIdentifier] isEqual:@kNCDismissAction])
        [[NSRunningApplication runningApplicationWithProcessIdentifier:pid]
            activateWithOptions:NSApplicationActivateIgnoringOtherApps];
    if (self.server != MACH_PORT_NULL && data != nil)
        ncc_activated(self.server, pid, (char *)data.bytes, (mach_msg_type_number_t)data.length, (int32_t)type, &result);
    if (action != nil)
        fprintf(stderr, "SystemUIServer: action %s of \"%s\"%s%s (%d)\n", [action[@kNCActionIdentifier] UTF8String],
                [n[@kNCKeyTitle] ?: @"" UTF8String], text != nil ? ", reply " : "", text != nil ? text.UTF8String : "",
                result);
    fprintf(stderr, "SystemUIServer: activated \"%s\" of %d (%d)\n", [n[@kNCKeyTitle] ?: @"" UTF8String], pid, result);
    for (NotificationBanner *banner in [_banners copy])
        if (sameDelivery(banner.notification, n))
            [self dismiss:banner];
    /* Dismissed, it stays in the notification center's list. */
    if (![action[@kNCActionIdentifier] isEqual:@kNCDismissAction])
        [self unlist:n];
}

@end
