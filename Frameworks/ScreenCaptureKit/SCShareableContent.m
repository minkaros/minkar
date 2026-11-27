/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/* The shareable content: the active displays (CGGetActiveDisplayList), the
   window server's windows (CGWindowListCopyWindowInfo) and the running
   applications (NSRunningApplication), taken together as one snapshot. */

#import <AppKit/AppKit.h>
#import <unistd.h>
#import "SCInternal.h"

NSString *const SCStreamErrorDomain = @"com.apple.ScreenCaptureKit.SCStreamErrorDomain";

NSError *SCMakeError(SCStreamErrorCode code, NSString *description) {
    return [NSError errorWithDomain:SCStreamErrorDomain
                               code:code
                           userInfo:@{NSLocalizedDescriptionKey : description}];
}

float SCDisplayPointPixelScale(CGDirectDisplayID display) {
    CGRect bounds = CGDisplayBounds(display);
    size_t pixels = CGDisplayPixelsWide(display);
    if (bounds.size.width <= 0 || pixels == 0)
        return 1;
    return (float)((CGFloat)pixels / bounds.size.width);
}

@implementation SCRunningApplication {
    NSString *_bundleIdentifier;
    NSString *_applicationName;
    pid_t _processID;
}

- (instancetype)_initWithProcessID:(pid_t)pid bundleIdentifier:(NSString *)bundleIdentifier applicationName:(NSString *)name {
    if ((self = [super init])) {
        _processID = pid;
        _bundleIdentifier = [bundleIdentifier copy];
        _applicationName = [name copy];
    }
    return self;
}

- (NSString *)bundleIdentifier {
    return _bundleIdentifier;
}

- (NSString *)applicationName {
    return _applicationName;
}

- (pid_t)processID {
    return _processID;
}

- (BOOL)isEqual:(id)other {
    return [other isKindOfClass:[SCRunningApplication class]] && [other processID] == _processID;
}

- (NSUInteger)hash {
    return (NSUInteger)_processID;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<SCRunningApplication %p pid=%d %@ (%@)>", self, _processID, _applicationName,
                                      _bundleIdentifier];
}

@end

static NSNumber *numberIn(NSDictionary *info, CFStringRef key) {
    id value = info[(__bridge NSString *)key];
    return [value isKindOfClass:[NSNumber class]] ? value : nil;
}

@implementation SCWindow {
    CGWindowID _windowID;
    CGRect _frame;
    NSString *_title;
    NSInteger _windowLayer;
    SCRunningApplication *_owningApplication;
    BOOL _onScreen;
    BOOL _active;
}

- (instancetype)_initWithInfo:(NSDictionary *)info application:(SCRunningApplication *)application active:(BOOL)active {
    NSNumber *number = numberIn(info, kCGWindowNumber);
    if (number == nil)
        return nil;
    if ((self = [super init])) {
        _windowID = (CGWindowID)[number unsignedIntValue];
        id bounds = info[(__bridge NSString *)kCGWindowBounds];
        if (![bounds isKindOfClass:[NSDictionary class]] ||
            !CGRectMakeWithDictionaryRepresentation((__bridge CFDictionaryRef)bounds, &_frame))
            _frame = CGRectZero;
        id title = info[(__bridge NSString *)kCGWindowName];
        _title = [title isKindOfClass:[NSString class]] ? [title copy] : nil;
        _windowLayer = [numberIn(info, kCGWindowLayer) integerValue];
        _onScreen = [numberIn(info, kCGWindowIsOnscreen) boolValue];
        _owningApplication = application;
        _active = active;
    }
    return self;
}

- (CGWindowID)windowID {
    return _windowID;
}

- (CGRect)frame {
    return _frame;
}

- (NSString *)title {
    return _title;
}

- (NSInteger)windowLayer {
    return _windowLayer;
}

- (SCRunningApplication *)owningApplication {
    return _owningApplication;
}

- (BOOL)isOnScreen {
    return _onScreen;
}

- (BOOL)isActive {
    return _active;
}

- (BOOL)isEqual:(id)other {
    return [other isKindOfClass:[SCWindow class]] && [other windowID] == _windowID;
}

- (NSUInteger)hash {
    return _windowID;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<SCWindow %p id=%u %@ %@ layer=%ld%@>", self, _windowID,
                                      _owningApplication.applicationName, NSStringFromRect(NSRectFromCGRect(_frame)),
                                      (long)_windowLayer, _onScreen ? @" on screen" : @""];
}

@end

@implementation SCDisplay {
    CGDirectDisplayID _displayID;
    CGRect _frame;
}

- (instancetype)_initWithDisplayID:(CGDirectDisplayID)displayID {
    if ((self = [super init])) {
        _displayID = displayID;
        _frame = CGDisplayBounds(displayID);
    }
    return self;
}

- (CGDirectDisplayID)displayID {
    return _displayID;
}

- (NSInteger)width {
    return (NSInteger)_frame.size.width;
}

- (NSInteger)height {
    return (NSInteger)_frame.size.height;
}

- (CGRect)frame {
    return _frame;
}

- (BOOL)isEqual:(id)other {
    return [other isKindOfClass:[SCDisplay class]] && [other displayID] == _displayID;
}

- (NSUInteger)hash {
    return _displayID;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<SCDisplay %p id=%u %@>", self, _displayID,
                                      NSStringFromRect(NSRectFromCGRect(_frame))];
}

@end

@implementation SCShareableContentInfo {
    SCShareableContentStyle _style;
    float _pointPixelScale;
    CGRect _contentRect;
}

- (instancetype)_initWithStyle:(SCShareableContentStyle)style pointPixelScale:(float)scale contentRect:(CGRect)rect {
    if ((self = [super init])) {
        _style = style;
        _pointPixelScale = scale;
        _contentRect = rect;
    }
    return self;
}

- (SCShareableContentStyle)style {
    return _style;
}

- (float)pointPixelScale {
    return _pointPixelScale;
}

- (CGRect)contentRect {
    return _contentRect;
}

@end

@implementation SCShareableContent {
    NSArray<SCWindow *> *_windows;
    NSArray<SCDisplay *> *_displays;
    NSArray<SCRunningApplication *> *_applications;
}

- (instancetype)_initWithDisplays:(NSArray *)displays windows:(NSArray *)windows applications:(NSArray *)applications {
    if ((self = [super init])) {
        _displays = [displays copy];
        _windows = [windows copy];
        _applications = [applications copy];
    }
    return self;
}

- (NSArray<SCWindow *> *)windows {
    return _windows;
}

- (NSArray<SCDisplay *> *)displays {
    return _displays;
}

- (NSArray<SCRunningApplication *> *)applications {
    return _applications;
}

+ (SCShareableContent *)_contentWithOptions:(CGWindowListOption)options
                           relativeToWindow:(CGWindowID)relativeTo
                         currentProcessOnly:(BOOL)currentProcessOnly
                                      error:(NSError **)error {
    uint32_t count = 0;
    if (CGGetActiveDisplayList(0, NULL, &count) != kCGErrorSuccess || count == 0) {
        if (error)
            *error = SCMakeError(SCStreamErrorNoDisplayList, @"There are no displays.");
        return nil;
    }
    CGDirectDisplayID ids[count];
    if (CGGetActiveDisplayList(count, ids, &count) != kCGErrorSuccess || count == 0) {
        if (error)
            *error = SCMakeError(SCStreamErrorNoDisplayList, @"There are no displays.");
        return nil;
    }
    NSMutableArray *displays = [NSMutableArray arrayWithCapacity:count];
    for (uint32_t i = 0; i < count; i++)
        [displays addObject:[[SCDisplay alloc] _initWithDisplayID:ids[i]]];

    CFArrayRef list = CGWindowListCopyWindowInfo(options, relativeTo);
    if (list == NULL) {
        if (error)
            *error = SCMakeError(SCStreamErrorNoWindowList, @"The window server gave no list of windows.");
        return nil;
    }
    NSArray *infos = CFBridgingRelease(list);
    pid_t me = getpid();

    /* The applications: the running ones, and any other process that owns a
       window. */
    NSMutableDictionary<NSNumber *, SCRunningApplication *> *byPID = [NSMutableDictionary dictionary];
    NSMutableArray *applications = [NSMutableArray array];
    pid_t activePID = -1;
    for (NSRunningApplication *app in [[NSWorkspace sharedWorkspace] runningApplications]) {
        pid_t pid = app.processIdentifier;
        if (app.terminated || pid <= 0 || byPID[@(pid)] != nil)
            continue;
        if (currentProcessOnly && pid != me)
            continue;
        if (app.active)
            activePID = pid;
        SCRunningApplication *sc = [[SCRunningApplication alloc] _initWithProcessID:pid
                                                                     bundleIdentifier:app.bundleIdentifier ?: @""
                                                                      applicationName:app.localizedName ?: @""];
        byPID[@(pid)] = sc;
        [applications addObject:sc];
    }

    NSMutableArray *windows = [NSMutableArray arrayWithCapacity:infos.count];
    for (NSDictionary *info in infos) {
        if (![info isKindOfClass:[NSDictionary class]])
            continue;
        pid_t pid = (pid_t)[numberIn(info, kCGWindowOwnerPID) intValue];
        if (currentProcessOnly && pid != me)
            continue;
        SCRunningApplication *owner = pid > 0 ? byPID[@(pid)] : nil;
        if (owner == nil && pid > 0) {
            NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
            id ownerName = info[(__bridge NSString *)kCGWindowOwnerName];
            NSString *name = app.localizedName;
            if (name == nil)
                name = [ownerName isKindOfClass:[NSString class]] ? ownerName : @"";
            owner = [[SCRunningApplication alloc] _initWithProcessID:pid
                                                      bundleIdentifier:app.bundleIdentifier ?: @""
                                                       applicationName:name];
            byPID[@(pid)] = owner;
            [applications addObject:owner];
        }
        BOOL onScreen = [numberIn(info, kCGWindowIsOnscreen) boolValue];
        SCWindow *window = [[SCWindow alloc] _initWithInfo:info
                                               application:owner
                                                    active:onScreen || (activePID > 0 && pid == activePID)];
        if (window != nil)
            [windows addObject:window];
    }
    return [[SCShareableContent alloc] _initWithDisplays:displays windows:windows applications:applications];
}

typedef void (^SCContentHandler)(SCShareableContent *, NSError *);

static void deliver(CGWindowListOption options, CGWindowID relativeTo, BOOL currentProcessOnly,
                    SCContentHandler handler) {
    if (handler == nil)
        return;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
      @autoreleasepool {
          NSError *error = nil;
          SCShareableContent *content = [SCShareableContent _contentWithOptions:options
                                                                relativeToWindow:relativeTo
                                                              currentProcessOnly:currentProcessOnly
                                                                           error:&error];
          handler(content, content != nil ? nil : error);
      }
    });
}

+ (void)getShareableContentWithCompletionHandler:(SCContentHandler)completionHandler {
    deliver(kCGWindowListOptionAll, kCGNullWindowID, NO, completionHandler);
}

+ (void)getCurrentProcessShareableContentWithCompletionHandler:(SCContentHandler)completionHandler {
    deliver(kCGWindowListOptionAll, kCGNullWindowID, YES, completionHandler);
}

+ (void)getShareableContentExcludingDesktopWindows:(BOOL)excludeDesktopWindows
                               onScreenWindowsOnly:(BOOL)onScreenWindowsOnly
                                 completionHandler:(SCContentHandler)completionHandler {
    CGWindowListOption options = onScreenWindowsOnly ? kCGWindowListOptionOnScreenOnly : kCGWindowListOptionAll;
    if (excludeDesktopWindows)
        options |= kCGWindowListExcludeDesktopElements;
    deliver(options, kCGNullWindowID, NO, completionHandler);
}

+ (void)getShareableContentExcludingDesktopWindows:(BOOL)excludeDesktopWindows
                    onScreenWindowsOnlyBelowWindow:(SCWindow *)window
                                 completionHandler:(SCContentHandler)completionHandler {
    CGWindowListOption options = kCGWindowListOptionOnScreenBelowWindow;
    if (excludeDesktopWindows)
        options |= kCGWindowListExcludeDesktopElements;
    deliver(options, window.windowID, NO, completionHandler);
}

+ (void)getShareableContentExcludingDesktopWindows:(BOOL)excludeDesktopWindows
                    onScreenWindowsOnlyAboveWindow:(SCWindow *)window
                                 completionHandler:(SCContentHandler)completionHandler {
    CGWindowListOption options = kCGWindowListOptionOnScreenAboveWindow;
    if (excludeDesktopWindows)
        options |= kCGWindowListExcludeDesktopElements;
    deliver(options, window.windowID, NO, completionHandler);
}

+ (SCShareableContentInfo *)infoForFilter:(SCContentFilter *)filter {
    return [[SCShareableContentInfo alloc] _initWithStyle:filter.style
                                          pointPixelScale:filter.pointPixelScale
                                              contentRect:filter.contentRect];
}

@end
