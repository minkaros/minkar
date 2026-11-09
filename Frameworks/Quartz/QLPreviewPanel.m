// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
// Built with ARC.

#import <Quartz/QLPreviewPanel.h>

@implementation NSURL (QLPreviewConvenienceAdditions)
- (NSURL *)previewItemURL { return self; }
- (NSString *)previewItemTitle { return self.lastPathComponent; }
@end

static QLPreviewPanel *sShared;

@implementation QLPreviewPanel {
    NSImageView *_imageView;
    NSScrollView *_textScroll;
    NSTextView *_textView;
    NSTextField *_nameField;
    id _currentController;
    NSInteger _index;
}

@synthesize dataSource = _dataSource, displayState = _displayState;

+ (QLPreviewPanel *)sharedPreviewPanel {
    if (sShared == nil)
        sShared = [[QLPreviewPanel alloc] initWithContentRect:NSMakeRect(0, 0, 520, 420)
            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskUtilityWindow
              backing:NSBackingStoreBuffered defer:YES];
    return sShared;
}

+ (BOOL)sharedPreviewPanelExists {
    return sShared != nil;
}

- (instancetype)initWithContentRect:(NSRect)rect styleMask:(NSWindowStyleMask)style backing:(NSBackingStoreType)backing defer:(BOOL)defer {
    if ((self = [super initWithContentRect:rect styleMask:style backing:backing defer:defer])) {
        NSView *content = self.contentView;
        NSRect bounds = content.bounds;
        _imageView = [[NSImageView alloc] initWithFrame:NSInsetRect(bounds, 12, 36)];
        _imageView.imageScaling = NSImageScaleProportionallyUpOrDown;
        _imageView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        _textScroll = [[NSScrollView alloc] initWithFrame:NSInsetRect(bounds, 12, 36)];
        _textScroll.hasVerticalScroller = YES;
        _textScroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        _textView = [[NSTextView alloc] initWithFrame:_textScroll.contentView.bounds];
        _textView.editable = NO;
        _textScroll.documentView = _textView;
        _nameField = [NSTextField labelWithString:@""];
        _nameField.frame = NSMakeRect(12, 8, bounds.size.width - 24, 20);
        _nameField.alignment = NSTextAlignmentCenter;
        _nameField.autoresizingMask = NSViewWidthSizable;
        [content addSubview:_imageView];
        [content addSubview:_textScroll];
        [content addSubview:_nameField];
        self.title = @"Preview";
        self.releasedWhenClosed = NO;
        [self center];
    }
    return self;
}

- (id)currentController { return _currentController; }

/* The first responder of the key window, or one after it in the chain,
   that accepts the panel. */
- (void)updateController {
    NSResponder *responder = [[NSApplication sharedApplication] keyWindow].firstResponder ?: [[NSApplication sharedApplication] mainWindow].firstResponder;
    id controller = nil;
    for (; responder != nil; responder = responder.nextResponder)
        if ([responder respondsToSelector:@selector(acceptsPreviewPanelControl:)] && [responder acceptsPreviewPanelControl:self]) {
            controller = responder;
            break;
        }
    if (controller == nil && [[[NSApplication sharedApplication] delegate] respondsToSelector:@selector(acceptsPreviewPanelControl:)] &&
        [(id)[[NSApplication sharedApplication] delegate] acceptsPreviewPanelControl:self])
        controller = [[NSApplication sharedApplication] delegate];
    if (controller == _currentController)
        return;
    if ([_currentController respondsToSelector:@selector(endPreviewPanelControl:)])
        [_currentController endPreviewPanelControl:self];
    _currentController = controller;
    if ([controller respondsToSelector:@selector(beginPreviewPanelControl:)])
        [controller beginPreviewPanelControl:self];
    [self reloadData];
}

- (void)makeKeyAndOrderFront:(id)sender {
    [self updateController];
    [super makeKeyAndOrderFront:sender];
}

- (void)orderOut:(id)sender {
    [super orderOut:sender];
    if ([_currentController respondsToSelector:@selector(endPreviewPanelControl:)])
        [_currentController endPreviewPanelControl:self];
    _currentController = nil;
}

- (NSInteger)currentPreviewItemIndex { return _index; }

- (void)setCurrentPreviewItemIndex:(NSInteger)index {
    _index = index;
    [self refreshCurrentPreviewItem];
}

- (id<QLPreviewItem>)currentPreviewItem {
    id<QLPreviewPanelDataSource> source = self.dataSource;
    NSInteger count = [source numberOfPreviewItemsInPreviewPanel:self];
    if (source == nil || _index < 0 || _index >= count)
        return nil;
    return [source previewPanel:self previewItemAtIndex:_index];
}

- (void)reloadData {
    NSInteger count = [self.dataSource numberOfPreviewItemsInPreviewPanel:self];
    if (_index >= count)
        _index = MAX(0, count - 1);
    [self refreshCurrentPreviewItem];
}

/* Images as images, text as text, anything else as its icon. */
- (void)refreshCurrentPreviewItem {
    id<QLPreviewItem> item = self.currentPreviewItem;
    NSURL *url = item.previewItemURL;
    NSString *title = [item respondsToSelector:@selector(previewItemTitle)] ? item.previewItemTitle : url.lastPathComponent;
    NSImage *image = nil;
    NSString *text = nil;

    if (url.isFileURL) {
        NSString *type = [[NSWorkspace sharedWorkspace] typeOfFile:url.path error:NULL];
        if (type != nil && [[NSWorkspace sharedWorkspace] type:type conformsToType:@"public.image"])
            image = [[NSImage alloc] initWithContentsOfURL:url];
        else if (type != nil && [[NSWorkspace sharedWorkspace] type:type conformsToType:@"public.text"])
            text = [NSString stringWithContentsOfURL:url usedEncoding:NULL error:NULL];
        if (image == nil && text == nil)
            image = [[NSWorkspace sharedWorkspace] iconForFile:url.path];
    }
    _imageView.image = image;
    _imageView.hidden = (image == nil);
    _textView.string = text ?: @"";
    _textScroll.hidden = (text == nil);
    _nameField.stringValue = title ?: @"";
    self.title = title.length ? title : @"Preview";
}

- (void)keyDown:(NSEvent *)event {
    id<QLPreviewPanelDelegate> delegate = (id)self.delegate;
    if ([delegate respondsToSelector:@selector(previewPanel:handleEvent:)] && [delegate previewPanel:self handleEvent:event])
        return;
    NSString *keys = event.charactersIgnoringModifiers;
    unichar key = keys.length ? [keys characterAtIndex:0] : 0;
    if (key == ' ' || key == 27) {
        [self orderOut:nil];
        return;
    }
    if (key == NSRightArrowFunctionKey || key == NSDownArrowFunctionKey) {
        if (_index + 1 < [self.dataSource numberOfPreviewItemsInPreviewPanel:self])
            self.currentPreviewItemIndex = _index + 1;
        return;
    }
    if (key == NSLeftArrowFunctionKey || key == NSUpArrowFunctionKey) {
        if (_index > 0)
            self.currentPreviewItemIndex = _index - 1;
        return;
    }
    [super keyDown:event];
}

- (BOOL)enterFullScreenMode:(NSScreen *)screen withOptions:(NSDictionary *)options {
    [self setFrame:(screen ?: [NSScreen mainScreen]).visibleFrame display:YES];
    return YES;
}

- (void)exitFullScreenModeWithOptions:(NSDictionary *)options {
    [self center];
}

- (BOOL)isInFullScreenMode {
    return NO;
}

@end
