/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/* The content sharing picker: a small panel, in the calling application,
   that lists the displays, windows or applications its modes allow. The
   choice reaches the observers as a content filter; closing the panel
   cancels. */

#import <AppKit/AppKit.h>
#import "SCInternal.h"

static const SCContentSharingPickerMode allModes =
    SCContentSharingPickerModeSingleWindow | SCContentSharingPickerModeMultipleWindows |
    SCContentSharingPickerModeSingleApplication | SCContentSharingPickerModeMultipleApplications |
    SCContentSharingPickerModeSingleDisplay;

@implementation SCContentSharingPickerConfiguration

@synthesize allowedPickerModes = _allowedPickerModes, allowsChangingSelectedContent = _allowsChangingSelectedContent,
            excludedBundleIDs = _excludedBundleIDs, excludedWindowIDs = _excludedWindowIDs;

- (instancetype)init {
    if ((self = [super init])) {
        _allowedPickerModes = allModes;
        _allowsChangingSelectedContent = YES;
        _excludedBundleIDs = @[];
        _excludedWindowIDs = @[];
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    SCContentSharingPickerConfiguration *c = [[SCContentSharingPickerConfiguration allocWithZone:zone] init];
    c.allowedPickerModes = _allowedPickerModes;
    c.allowsChangingSelectedContent = _allowsChangingSelectedContent;
    c.excludedBundleIDs = _excludedBundleIDs ?: @[];
    c.excludedWindowIDs = _excludedWindowIDs ?: @[];
    return c;
}

@end

#pragma mark - Panel

typedef NS_ENUM(NSInteger, SCPickerKind) {
    SCPickerKindDisplays,
    SCPickerKindWindows,
    SCPickerKindApplications,
};

@class SCPickerPanel;

@interface SCContentSharingPicker ()
- (instancetype)_init;
- (void)_panel:(SCPickerPanel *)panel choseFilter:(SCContentFilter *)filter;
- (void)_panelCancelled:(SCPickerPanel *)panel;
@end

/* One presentation: the panel, what it lists, and the stream it is for. */
__attribute__((visibility("hidden")))
@interface SCPickerPanel : NSObject <NSTableViewDataSource, NSTableViewDelegate, NSWindowDelegate>
@property (readonly, strong) SCStream *stream;
- (instancetype)initWithPicker:(SCContentSharingPicker *)picker
                        stream:(SCStream *)stream
                 configuration:(SCContentSharingPickerConfiguration *)configuration
                         style:(SCShareableContentStyle)style;
/* NO when there is nothing it may list. */
- (BOOL)showWithError:(NSError **)error;
/* Closes it without telling the picker. */
- (void)dismiss;
@end

@implementation SCPickerPanel {
    __weak SCContentSharingPicker *_picker;
    SCContentSharingPickerConfiguration *_configuration;
    SCShareableContentStyle _style;
    NSPanel *_panel;
    NSPopUpButton *_kindButton;
    NSTableView *_table;
    NSButton *_shareButton;
    NSMutableArray<NSNumber *> *_kinds;
    NSArray<SCDisplay *> *_displays;
    NSArray<SCWindow *> *_windows;
    NSArray<SCRunningApplication *> *_applications;
    BOOL _finished;
}

- (instancetype)initWithPicker:(SCContentSharingPicker *)picker
                        stream:(SCStream *)stream
                 configuration:(SCContentSharingPickerConfiguration *)configuration
                         style:(SCShareableContentStyle)style {
    if ((self = [super init])) {
        _picker = picker;
        _stream = stream;
        _configuration = [configuration copy];
        _style = style;
    }
    return self;
}

/* What may be shared: the displays, the ordinary windows on screen, and the
   applications that own them, less what the configuration leaves out. */
- (void)_loadContent {
    SCShareableContent *content =
        [SCShareableContent _contentWithOptions:kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements
                               relativeToWindow:kCGNullWindowID
                             currentProcessOnly:NO
                                          error:NULL];
    NSSet *excludedWindows = [NSSet setWithArray:_configuration.excludedWindowIDs ?: @[]];
    NSSet *excludedBundles = [NSSet setWithArray:_configuration.excludedBundleIDs ?: @[]];
    CGWindowLevel normal = CGWindowLevelForKey(kCGNormalWindowLevelKey);
    NSInteger ownPanel = _panel.windowNumber;
    NSMutableArray *windows = [NSMutableArray array];
    NSMutableArray *applications = [NSMutableArray array];
    for (SCWindow *w in content.windows) {
        SCRunningApplication *app = w.owningApplication;
        if (w.windowLayer != normal || w.frame.size.width < 32 || w.frame.size.height < 32)
            continue;
        if ((NSInteger)w.windowID == ownPanel || [excludedWindows containsObject:@(w.windowID)])
            continue;
        if (app.bundleIdentifier.length > 0 && [excludedBundles containsObject:app.bundleIdentifier])
            continue;
        [windows addObject:w];
        if (app != nil && ![applications containsObject:app])
            [applications addObject:app];
    }
    _displays = content.displays ?: @[];
    _windows = windows;
    _applications = applications;
}

- (SCPickerKind)_kind {
    NSInteger i = _kindButton.indexOfSelectedItem;
    return i >= 0 && i < (NSInteger)_kinds.count ? (SCPickerKind)[_kinds[i] integerValue] : SCPickerKindDisplays;
}

- (NSArray *)_items {
    switch (self._kind) {
    case SCPickerKindWindows:
        return _windows;
    case SCPickerKindApplications:
        return _applications;
    default:
        return _displays;
    }
}

- (BOOL)_multipleAllowed {
    SCContentSharingPickerMode modes = _configuration.allowedPickerModes;
    switch (self._kind) {
    case SCPickerKindWindows:
        return (modes & SCContentSharingPickerModeMultipleWindows) != 0;
    case SCPickerKindApplications:
        return (modes & SCContentSharingPickerModeMultipleApplications) != 0;
    default:
        return NO;
    }
}

static NSButton *button(NSString *title, NSRect frame, id target, SEL action) {
    NSButton *b = [[NSButton alloc] initWithFrame:frame];
    [b setTitle:title];
    [b setBezelStyle:NSBezelStyleRounded];
    [b setTarget:target];
    [b setAction:action];
    return b;
}

- (BOOL)showWithError:(NSError **)error {
    SCContentSharingPickerMode modes = _configuration.allowedPickerModes;
    if (modes == 0)
        modes = allModes;
    _kinds = [NSMutableArray array];
    NSMutableArray *titles = [NSMutableArray array];
    if (modes & SCContentSharingPickerModeSingleDisplay) {
        [_kinds addObject:@(SCPickerKindDisplays)];
        [titles addObject:@"Displays"];
    }
    if (modes & (SCContentSharingPickerModeSingleWindow | SCContentSharingPickerModeMultipleWindows)) {
        [_kinds addObject:@(SCPickerKindWindows)];
        [titles addObject:@"Windows"];
    }
    if (modes & (SCContentSharingPickerModeSingleApplication | SCContentSharingPickerModeMultipleApplications)) {
        [_kinds addObject:@(SCPickerKindApplications)];
        [titles addObject:@"Applications"];
    }
    if (_kinds.count == 0) {
        if (error)
            *error = SCMakeError(SCStreamErrorInvalidParameter, @"The picker's modes allow nothing to be shared.");
        return NO;
    }

    NSRect frame = NSMakeRect(0, 0, 440, 340);
    _panel = [[NSPanel alloc] initWithContentRect:frame
                                        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
    [_panel setTitle:@"Share Screen"];
    [_panel setReleasedWhenClosed:NO];
    [_panel setDelegate:self];
    [_panel setLevel:NSFloatingWindowLevel];
    NSView *view = [_panel contentView];

    NSTextField *prompt = [NSTextField labelWithString:@"Choose what to share:"];
    [prompt setFrame:NSMakeRect(20, 298, 200, 20)];
    [view addSubview:prompt];

    _kindButton = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(240, 294, 180, 26) pullsDown:NO];
    for (NSString *t in titles)
        [_kindButton addItemWithTitle:t];
    [_kindButton setTarget:self];
    [_kindButton setAction:@selector(_kindChanged:)];
    [view addSubview:_kindButton];

    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 60, 400, 226)];
    [scroll setHasVerticalScroller:YES];
    [scroll setBorderType:NSBezelBorder];
    _table = [[NSTableView alloc] initWithFrame:NSMakeRect(0, 0, 400, 226)];
    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:@"name"];
    [column setWidth:380];
    [_table addTableColumn:column];
    [_table setHeaderView:nil];
    [_table setDataSource:self];
    [_table setDelegate:self];
    [_table setTarget:self];
    [_table setDoubleAction:@selector(_share:)];
    [scroll setDocumentView:_table];
    [view addSubview:scroll];

    NSButton *cancel = button(@"Cancel", NSMakeRect(220, 14, 96, 32), self, @selector(_cancel:));
    [cancel setKeyEquivalent:@"\033"];
    [view addSubview:cancel];
    _shareButton = button(@"Share", NSMakeRect(324, 14, 96, 32), self, @selector(_share:));
    [_shareButton setKeyEquivalent:@"\r"];
    [view addSubview:_shareButton];

    [self _loadContent];
    /* Start with the kind the content style asks for. */
    SCPickerKind wanted = _style == SCShareableContentStyleWindow ? SCPickerKindWindows :
                          _style == SCShareableContentStyleApplication ? SCPickerKindApplications :
                                                                          SCPickerKindDisplays;
    NSUInteger index = [_kinds indexOfObject:@(wanted)];
    [_kindButton selectItemAtIndex:index != NSNotFound ? (NSInteger)index : 0];
    [self _kindChanged:nil];

    [_panel center];
    [NSApp activateIgnoringOtherApps:YES];
    [_panel makeKeyAndOrderFront:nil];
    return YES;
}

- (void)_kindChanged:(id)sender {
    [_table setAllowsMultipleSelection:self._multipleAllowed];
    [_table reloadData];
    if (self._items.count > 0)
        [_table selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
    [_shareButton setEnabled:self._items.count > 0];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    return (NSInteger)self._items.count;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row {
    NSArray *items = self._items;
    if (row < 0 || row >= (NSInteger)items.count)
        return nil;
    id item = items[row];
    if ([item isKindOfClass:[SCDisplay class]]) {
        SCDisplay *d = item;
        return [NSString stringWithFormat:@"Display %ld (%ld × %ld)", (long)row + 1, (long)d.width, (long)d.height];
    }
    if ([item isKindOfClass:[SCWindow class]]) {
        SCWindow *w = item;
        NSString *app = w.owningApplication.applicationName.length > 0 ? w.owningApplication.applicationName : @"Window";
        return w.title.length > 0 ? [NSString stringWithFormat:@"%@ — %@", app, w.title] :
                                    [NSString stringWithFormat:@"%@ — %ld × %ld", app, (long)w.frame.size.width,
                                                               (long)w.frame.size.height];
    }
    SCRunningApplication *a = item;
    return a.applicationName.length > 0 ? a.applicationName : a.bundleIdentifier;
}

/* The display most of a set of windows is on. */
- (SCDisplay *)_displayForWindows:(NSArray<SCWindow *> *)windows {
    SCDisplay *best = _displays.firstObject;
    CGFloat bestArea = 0;
    for (SCDisplay *d in _displays) {
        CGFloat area = 0;
        for (SCWindow *w in windows) {
            CGRect r = CGRectIntersection(d.frame, w.frame);
            if (!CGRectIsNull(r))
                area += r.size.width * r.size.height;
        }
        if (area > bestArea) {
            bestArea = area;
            best = d;
        }
    }
    return best;
}

- (void)_share:(id)sender {
    NSArray *items = self._items;
    NSMutableArray *chosen = [NSMutableArray array];
    NSIndexSet *rows = [_table selectedRowIndexes];
    for (NSUInteger i = rows.firstIndex; i != NSNotFound; i = [rows indexGreaterThanIndex:i])
        if (i < items.count)
            [chosen addObject:items[i]];
    if (chosen.count == 0)
        return;
    if (!self._multipleAllowed && chosen.count > 1)
        [chosen removeObjectsInRange:NSMakeRange(1, chosen.count - 1)];

    SCContentFilter *filter = nil;
    switch (self._kind) {
    case SCPickerKindDisplays:
        filter = [[SCContentFilter alloc] initWithDisplay:chosen.firstObject excludingWindows:@[]];
        break;
    case SCPickerKindWindows:
        if (chosen.count == 1)
            filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:chosen.firstObject];
        else
            filter = [[SCContentFilter alloc] initWithDisplay:[self _displayForWindows:chosen] includingWindows:chosen];
        break;
    case SCPickerKindApplications: {
        NSMutableArray *windows = [NSMutableArray array];
        for (SCWindow *w in _windows)
            if ([chosen containsObject:w.owningApplication])
                [windows addObject:w];
        filter = [[SCContentFilter alloc] initWithDisplay:[self _displayForWindows:windows]
                                    includingApplications:chosen
                                         exceptingWindows:@[]];
        break;
    }
    }
    if (filter == nil)
        return;
    _finished = YES;
    [_panel orderOut:nil];
    [_panel close];
    [_picker _panel:self choseFilter:filter];
}

- (void)_cancel:(id)sender {
    if (_finished)
        return;
    _finished = YES;
    [_panel orderOut:nil];
    [_panel close];
    [_picker _panelCancelled:self];
}

- (void)windowWillClose:(NSNotification *)notification {
    [self _cancel:nil];
}

- (void)dismiss {
    _finished = YES;
    [_panel orderOut:nil];
    [_panel close];
}

@end

#pragma mark - Picker

@implementation SCContentSharingPicker {
    NSHashTable *_observers;
    NSMapTable<SCStream *, SCContentSharingPickerConfiguration *> *_streamConfigurations;
    SCContentSharingPickerConfiguration *_defaultConfiguration;
    NSNumber *_maximumStreamCount;
    BOOL _active;
    SCPickerPanel *_panel; /* main thread */
}

+ (SCContentSharingPicker *)sharedPicker {
    static SCContentSharingPicker *shared;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
      shared = [[SCContentSharingPicker alloc] _init];
    });
    return shared;
}

- (instancetype)_init {
    if ((self = [super init])) {
        _observers = [NSHashTable weakObjectsHashTable];
        _streamConfigurations = [NSMapTable weakToStrongObjectsMapTable];
        _defaultConfiguration = [[SCContentSharingPickerConfiguration alloc] init];
    }
    return self;
}

- (SCContentSharingPickerConfiguration *)defaultConfiguration {
    @synchronized(self) {
        return [_defaultConfiguration copy];
    }
}

- (void)setDefaultConfiguration:(SCContentSharingPickerConfiguration *)configuration {
    @synchronized(self) {
        _defaultConfiguration = [configuration copy] ?: [[SCContentSharingPickerConfiguration alloc] init];
    }
}

- (NSNumber *)maximumStreamCount {
    @synchronized(self) {
        return _maximumStreamCount;
    }
}

- (void)setMaximumStreamCount:(NSNumber *)count {
    @synchronized(self) {
        _maximumStreamCount = count;
    }
}

- (BOOL)isActive {
    @synchronized(self) {
        return _active;
    }
}

- (void)setActive:(BOOL)active {
    @synchronized(self) {
        _active = active;
    }
    if (!active) {
        /* An inactive picker shows nothing: one on screen goes. */
        dispatch_async(dispatch_get_main_queue(), ^{
          SCPickerPanel *panel = self->_panel;
          self->_panel = nil;
          [panel dismiss];
          if (panel != nil)
              [self _notifyCancelForStream:panel.stream];
        });
    }
}

- (void)addObserver:(id<SCContentSharingPickerObserver>)observer {
    if (observer == nil)
        return;
    @synchronized(self) {
        [_observers addObject:observer];
    }
}

- (void)removeObserver:(id<SCContentSharingPickerObserver>)observer {
    @synchronized(self) {
        [_observers removeObject:observer];
    }
}

- (void)setConfiguration:(SCContentSharingPickerConfiguration *)pickerConfig forStream:(SCStream *)stream {
    if (stream == nil)
        return;
    @synchronized(self) {
        if (pickerConfig != nil)
            [_streamConfigurations setObject:[pickerConfig copy] forKey:stream];
        else
            [_streamConfigurations removeObjectForKey:stream];
    }
}

- (NSArray<id<SCContentSharingPickerObserver>> *)_observers {
    @synchronized(self) {
        return [_observers allObjects];
    }
}

- (void)_notifyFailure:(NSError *)error {
    for (id<SCContentSharingPickerObserver> o in self._observers)
        if ([o respondsToSelector:@selector(contentSharingPickerStartDidFailWithError:)])
            [o contentSharingPickerStartDidFailWithError:error];
}

- (void)_notifyCancelForStream:(SCStream *)stream {
    for (id<SCContentSharingPickerObserver> o in self._observers)
        if ([o respondsToSelector:@selector(contentSharingPicker:didCancelForStream:)])
            [o contentSharingPicker:self didCancelForStream:stream];
}

- (void)_panel:(SCPickerPanel *)panel choseFilter:(SCContentFilter *)filter {
    if (panel != _panel)
        return;
    _panel = nil;
    for (id<SCContentSharingPickerObserver> o in self._observers)
        if ([o respondsToSelector:@selector(contentSharingPicker:didUpdateWithFilter:forStream:)])
            [o contentSharingPicker:self didUpdateWithFilter:filter forStream:panel.stream];
}

- (void)_panelCancelled:(SCPickerPanel *)panel {
    if (panel != _panel)
        return;
    _panel = nil;
    [self _notifyCancelForStream:panel.stream];
}

- (void)present {
    [self presentPickerForStream:nil usingContentStyle:SCShareableContentStyleNone];
}

- (void)presentPickerUsingContentStyle:(SCShareableContentStyle)contentStyle {
    [self presentPickerForStream:nil usingContentStyle:contentStyle];
}

- (void)presentPickerForStream:(SCStream *)stream {
    [self presentPickerForStream:stream usingContentStyle:SCShareableContentStyleNone];
}

- (void)presentPickerForStream:(SCStream *)stream usingContentStyle:(SCShareableContentStyle)contentStyle {
    SCContentSharingPickerConfiguration *configuration;
    BOOL active;
    NSNumber *maximum;
    @synchronized(self) {
        active = _active;
        maximum = _maximumStreamCount;
        configuration = (stream != nil ? [_streamConfigurations objectForKey:stream] : nil) ?: _defaultConfiguration;
        configuration = [configuration copy];
    }
    dispatch_async(dispatch_get_main_queue(), ^{
      if (!active) {
          [self _notifyFailure:SCMakeError(SCStreamErrorFailedToStart, @"The content sharing picker is not active.")];
          return;
      }
      /* A new stream past the limit: nothing is shown. */
      if (stream == nil && maximum != nil && [SCStream _runningStreamCount] >= maximum.integerValue)
          return;
      if (stream != nil && !configuration.allowsChangingSelectedContent)
          return;
      if (NSApp == nil) {
          [self _notifyFailure:SCMakeError(SCStreamErrorFailedNoMatchingApplicationContext,
                                           @"The picker needs an application to show its panel in.")];
          return;
      }
      /* One panel at a time: a new request replaces the one shown. */
      SCPickerPanel *previous = self->_panel;
      self->_panel = nil;
      if (previous != nil) {
          [previous dismiss];
          [self _notifyCancelForStream:previous.stream];
      }
      SCPickerPanel *panel = [[SCPickerPanel alloc] initWithPicker:self
                                                             stream:stream
                                                      configuration:configuration
                                                              style:contentStyle];
      self->_panel = panel;
      NSError *error = nil;
      if (![panel showWithError:&error]) {
          self->_panel = nil;
          [self _notifyFailure:error];
      }
    });
}

@end
