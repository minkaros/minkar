/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/* Content filters: which windows a capture keeps, and the content's image
   as it is now (CGWindowListCreateImage for one window, the kept windows
   of a display composed over black for the others). */

#import "SCInternal.h"

typedef NS_ENUM(NSInteger, SCFilterKind) {
    SCFilterKindWindow,
    SCFilterKindDisplayExcludingWindows,
    SCFilterKindDisplayIncludingWindows,
    SCFilterKindDisplayIncludingApplications,
    SCFilterKindDisplayExcludingApplications,
};

static NSSet<NSNumber *> *windowIDs(NSArray<SCWindow *> *windows) {
    NSMutableSet *set = [NSMutableSet setWithCapacity:windows.count];
    for (SCWindow *w in windows)
        [set addObject:@(w.windowID)];
    return set;
}

static NSSet<NSNumber *> *processIDs(NSArray<SCRunningApplication *> *applications) {
    NSMutableSet *set = [NSMutableSet setWithCapacity:applications.count];
    for (SCRunningApplication *a in applications)
        [set addObject:@(a.processID)];
    return set;
}

/* The display a rectangle (global points) is most on; the main one if none. */
static CGDirectDisplayID displayForRect(CGRect rect) {
    uint32_t count = 0;
    CGDirectDisplayID best = CGMainDisplayID();
    if (CGGetActiveDisplayList(0, NULL, &count) != kCGErrorSuccess || count == 0)
        return best;
    CGDirectDisplayID ids[count];
    if (CGGetActiveDisplayList(count, ids, &count) != kCGErrorSuccess)
        return best;
    CGFloat bestArea = 0;
    for (uint32_t i = 0; i < count; i++) {
        CGRect r = CGRectIntersection(CGDisplayBounds(ids[i]), rect);
        CGFloat area = CGRectIsNull(r) ? 0 : r.size.width * r.size.height;
        if (area > bestArea) {
            bestArea = area;
            best = ids[i];
        }
    }
    return best;
}

static BOOL infoNumber(CFDictionaryRef info, CFStringRef key, CFNumberType type, void *value) {
    CFTypeRef n = CFDictionaryGetValue(info, key);
    return n != NULL && CFGetTypeID(n) == CFNumberGetTypeID() && CFNumberGetValue((CFNumberRef)n, type, value);
}

static CGRect infoBounds(CFDictionaryRef info) {
    CGRect r = CGRectNull;
    CFTypeRef d = CFDictionaryGetValue(info, kCGWindowBounds);
    if (d == NULL || CFGetTypeID(d) != CFDictionaryGetTypeID() ||
        !CGRectMakeWithDictionaryRepresentation((CFDictionaryRef)d, &r))
        return CGRectNull;
    return r;
}

@implementation SCContentFilter {
    SCFilterKind _kind;
    SCDisplay *_display;
    NSArray<SCWindow *> *_windows;
    NSArray<SCRunningApplication *> *_applications;
    NSSet<NSNumber *> *_windowIDs;
    NSSet<NSNumber *> *_processIDs;
    float _pointPixelScale;
    CGRect _contentRect;
    CGWindowLevel _menuBarLevel, _statusLevel;
}

@synthesize includeMenuBar = _includeMenuBar;

- (instancetype)_initWithKind:(SCFilterKind)kind
                      display:(SCDisplay *)display
                      windows:(NSArray<SCWindow *> *)windows
                 applications:(NSArray<SCRunningApplication *> *)applications {
    if ((self = [super init])) {
        _kind = kind;
        _display = display;
        _windows = [windows copy] ?: @[];
        _applications = [applications copy] ?: @[];
        _windowIDs = windowIDs(_windows);
        _processIDs = processIDs(_applications);
        _includeMenuBar = YES;
        _menuBarLevel = CGWindowLevelForKey(kCGMainMenuWindowLevelKey);
        _statusLevel = CGWindowLevelForKey(kCGStatusWindowLevelKey);
        if (kind == SCFilterKindWindow) {
            CGRect frame = _windows.firstObject.frame;
            _pointPixelScale = SCDisplayPointPixelScale(displayForRect(frame));
            _contentRect = CGRectMake(0, 0, frame.size.width, frame.size.height);
        } else {
            _pointPixelScale = SCDisplayPointPixelScale(display.displayID);
            _contentRect = CGRectMake(0, 0, display.frame.size.width, display.frame.size.height);
        }
    }
    return self;
}

- (instancetype)init {
    return [self _initWithKind:SCFilterKindDisplayExcludingWindows
                       display:[[SCDisplay alloc] _initWithDisplayID:CGMainDisplayID()]
                       windows:@[]
                  applications:@[]];
}

- (instancetype)initWithDesktopIndependentWindow:(SCWindow *)window {
    return [self _initWithKind:SCFilterKindWindow
                       display:nil
                       windows:window != nil ? @[ window ] : @[]
                  applications:@[]];
}

- (instancetype)initWithDisplay:(SCDisplay *)display excludingWindows:(NSArray<SCWindow *> *)excluded {
    return [self _initWithKind:SCFilterKindDisplayExcludingWindows display:display windows:excluded applications:@[]];
}

- (instancetype)initWithDisplay:(SCDisplay *)display includingWindows:(NSArray<SCWindow *> *)includedWindows {
    return [self _initWithKind:SCFilterKindDisplayIncludingWindows
                       display:display
                       windows:includedWindows
                  applications:@[]];
}

- (instancetype)initWithDisplay:(SCDisplay *)display
          includingApplications:(NSArray<SCRunningApplication *> *)applications
               exceptingWindows:(NSArray<SCWindow *> *)exceptingWindows {
    return [self _initWithKind:SCFilterKindDisplayIncludingApplications
                       display:display
                       windows:exceptingWindows
                  applications:applications];
}

- (instancetype)initWithDisplay:(SCDisplay *)display
          excludingApplications:(NSArray<SCRunningApplication *> *)applications
               exceptingWindows:(NSArray<SCWindow *> *)exceptingWindows {
    return [self _initWithKind:SCFilterKindDisplayExcludingApplications
                       display:display
                       windows:exceptingWindows
                  applications:applications];
}

- (SCStreamType)streamType {
    return _kind == SCFilterKindWindow ? SCStreamTypeWindow : SCStreamTypeDisplay;
}

- (SCShareableContentStyle)style {
    switch (_kind) {
    case SCFilterKindWindow:
    case SCFilterKindDisplayIncludingWindows:
        return SCShareableContentStyleWindow;
    case SCFilterKindDisplayIncludingApplications:
        return SCShareableContentStyleApplication;
    default:
        return SCShareableContentStyleDisplay;
    }
}

- (float)pointPixelScale {
    return _pointPixelScale;
}

- (CGRect)contentRect {
    @synchronized(self) {
        return _contentRect;
    }
}

/* The capture follows the content's size (a window resized). */
- (void)_setContentSize:(CGSize)size {
    @synchronized(self) {
        _contentRect = CGRectMake(0, 0, size.width, size.height);
    }
}

- (NSArray<SCDisplay *> *)includedDisplays {
    return _display != nil ? @[ _display ] : @[];
}

/* Whether a window of a display's capture is kept. */
- (BOOL)_keepsWindow:(CGWindowID)window ownerPID:(pid_t)pid layer:(CGWindowLevel)layer {
    BOOL listed = [_windowIDs containsObject:@(window)];
    if (!_includeMenuBar && (layer == _menuBarLevel || layer == _statusLevel))
        return NO;
    switch (_kind) {
    case SCFilterKindWindow:
    case SCFilterKindDisplayIncludingWindows:
        return listed;
    case SCFilterKindDisplayExcludingWindows:
        return !listed;
    case SCFilterKindDisplayIncludingApplications:
        return [_processIDs containsObject:@(pid)] && !listed;
    case SCFilterKindDisplayExcludingApplications:
        return ![_processIDs containsObject:@(pid)] || listed;
    }
    return NO;
}

/* The windows on the display the capture keeps, front to back. */
- (NSArray<SCWindow *> *)includedWindows {
    if (_kind == SCFilterKindWindow || _kind == SCFilterKindDisplayIncludingWindows)
        return _windows;
    SCShareableContent *content = [SCShareableContent _contentWithOptions:kCGWindowListOptionOnScreenOnly
                                                          relativeToWindow:kCGNullWindowID
                                                        currentProcessOnly:NO
                                                                     error:NULL];
    NSMutableArray *kept = [NSMutableArray array];
    CGRect bounds = _display.frame;
    for (SCWindow *w in content.windows) {
        if (CGRectIntersectsRect(w.frame, bounds) &&
            [self _keepsWindow:w.windowID ownerPID:w.owningApplication.processID layer:(CGWindowLevel)w.windowLayer])
            [kept addObject:w];
    }
    return kept;
}

- (NSArray<SCRunningApplication *> *)includedApplications {
    if (_kind == SCFilterKindDisplayIncludingApplications)
        return _applications;
    NSMutableArray *apps = [NSMutableArray array];
    for (SCWindow *w in self.includedWindows) {
        SCRunningApplication *app = w.owningApplication;
        if (app != nil && ![apps containsObject:app])
            [apps addObject:app];
    }
    return apps;
}

- (CGImageRef)_copyWindowImageWithSize:(CGSize *)size exists:(BOOL *)exists {
    CGWindowID window = _windows.firstObject.windowID;
    CFArrayRef infos = window != kCGNullWindowID ?
        CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow, window) : NULL;
    CGRect frame = CGRectNull;
    for (CFIndex i = 0; infos != NULL && i < CFArrayGetCount(infos); i++) {
        CFDictionaryRef info = CFArrayGetValueAtIndex(infos, i);
        uint32_t number = 0;
        if (infoNumber(info, kCGWindowNumber, kCFNumberSInt32Type, &number) && number == window) {
            frame = infoBounds(info);
            break;
        }
    }
    if (infos != NULL)
        CFRelease(infos);
    if (CGRectIsNull(frame)) {
        *exists = NO;
        return NULL;
    }
    *exists = YES;
    *size = frame.size;
    [self _setContentSize:frame.size];
    return CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, window,
                                   kCGWindowImageBoundsIgnoreFraming);
}

- (CGImageRef)_copyDisplayImageWithSize:(CGSize *)size exists:(BOOL *)exists {
    CGDirectDisplayID display = _display.displayID;
    CGRect bounds = CGRectIntegral(CGDisplayBounds(display));
    if (CGRectIsNull(bounds) || CGRectIsEmpty(bounds)) {
        *exists = NO;
        return NULL;
    }
    *exists = YES;
    *size = bounds.size;
    [self _setContentSize:bounds.size];

    /* All of the display: the window server composes it. */
    if (_kind == SCFilterKindDisplayExcludingWindows && _windows.count == 0 && _includeMenuBar)
        return CGDisplayCreateImage(display);

    CFArrayRef infos = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    CFMutableArrayRef kept = CFArrayCreateMutable(NULL, 0, NULL);
    for (CFIndex i = 0; infos != NULL && i < CFArrayGetCount(infos); i++) {
        CFDictionaryRef info = CFArrayGetValueAtIndex(infos, i);
        uint32_t number = 0;
        int32_t pid = 0, layer = 0;
        if (!infoNumber(info, kCGWindowNumber, kCFNumberSInt32Type, &number))
            continue;
        infoNumber(info, kCGWindowOwnerPID, kCFNumberSInt32Type, &pid);
        infoNumber(info, kCGWindowLayer, kCFNumberSInt32Type, &layer);
        CGRect frame = infoBounds(info);
        if (CGRectIsNull(frame) || !CGRectIntersectsRect(frame, bounds))
            continue;
        if ([self _keepsWindow:number ownerPID:pid layer:layer])
            CFArrayAppendValue(kept, (const void *)(uintptr_t)number);
    }
    if (infos != NULL)
        CFRelease(infos);

    size_t w = (size_t)bounds.size.width, h = (size_t)bounds.size.height;
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(NULL, w, h, 8, w * 4, space,
                                                 kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(space);
    CGImageRef image = NULL;
    if (context != NULL) {
        CGContextSetRGBFillColor(context, 0, 0, 0, 1);
        CGContextFillRect(context, CGRectMake(0, 0, w, h));
        if (CFArrayGetCount(kept) > 0) {
            CGImageRef windows = CGWindowListCreateImageFromArray(bounds, kept, kCGWindowImageDefault);
            if (windows != NULL) {
                CGContextDrawImage(context, CGRectMake(0, 0, w, h), windows);
                CGImageRelease(windows);
            }
        }
        image = CGBitmapContextCreateImage(context);
        CGContextRelease(context);
    }
    CFRelease(kept);
    return image;
}

- (CGImageRef)_copyContentImageWithSize:(CGSize *)size exists:(BOOL *)exists {
    *size = CGSizeZero;
    if (_kind == SCFilterKindWindow)
        return [self _copyWindowImageWithSize:size exists:exists];
    return [self _copyDisplayImageWithSize:size exists:exists];
}

- (NSString *)description {
    static NSString *const kinds[] = {@"window", @"display excluding windows", @"display including windows",
                                      @"display including applications", @"display excluding applications"};
    return [NSString stringWithFormat:@"<SCContentFilter %p %@ display=%u windows=%lu applications=%lu>", self,
                                      kinds[_kind], _display.displayID, (unsigned long)_windows.count,
                                      (unsigned long)_applications.count];
}

@end
