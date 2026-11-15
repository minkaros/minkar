/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The player view: a layer tree it hosts (black, the player's layer over
 * it), the content overlay view, and the controls: a bar floating over
 * the bottom of the video, rounded (play and pause, the time, a position
 * slider, the time left, mute), a play button alone (minimal), or none.
 * Atlas draws the inline (default) style as the floating one: the API's
 * styles keep their values, the look is one.
 * Space plays and pauses; the arrow keys step a frame when paused.
 */

#import <AVKit/AVPlayerView.h>

/* The play and pause button, drawn as its symbol. */
@interface AVPlayButton : NSButton
@property BOOL showsPause;
@end

@implementation AVPlayButton

- (void)drawRect:(NSRect)dirty {
    /* Large (alone over the video): on a dark disc, seen on any picture. */
    if (self.bounds.size.width > 40) {
        [[NSColor colorWithCalibratedWhite:0.08 alpha:0.6] setFill];
        [[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(self.bounds, 1, 1)] fill];
    }
    NSRect b = NSInsetRect(self.bounds, self.bounds.size.width * 0.28, self.bounds.size.height * 0.28);
    if (!_showsPause)
        b = NSOffsetRect(b, b.size.width * 0.08, 0);	/* the triangle's weight centred */
    [[NSColor colorWithCalibratedWhite:1 alpha:self.isHighlighted ? 0.6 : 0.95] setFill];
    if (_showsPause) {
        CGFloat w = b.size.width * 0.32;
        NSRectFill(NSMakeRect(NSMinX(b), NSMinY(b), w, b.size.height));
        NSRectFill(NSMakeRect(NSMaxX(b) - w, NSMinY(b), w, b.size.height));
    } else {
        NSBezierPath *p = [NSBezierPath bezierPath];
        [p moveToPoint:NSMakePoint(NSMinX(b), NSMinY(b))];
        [p lineToPoint:NSMakePoint(NSMaxX(b), NSMidY(b))];
        [p lineToPoint:NSMakePoint(NSMinX(b), NSMaxY(b))];
        [p closePath];
        [p fill];
    }
}

@end

/* The mute button: a speaker, crossed out when muted. */
@interface AVMuteButton : NSButton
@property BOOL muted;
@end

@implementation AVMuteButton

- (void)drawRect:(NSRect)dirty {
    NSRect b = NSInsetRect(self.bounds, self.bounds.size.width * 0.25, self.bounds.size.height * 0.3);
    [[NSColor colorWithCalibratedWhite:1 alpha:0.95] set];
    NSBezierPath *p = [NSBezierPath bezierPath];
    CGFloat x = NSMinX(b), y = NSMinY(b), w = b.size.width, h = b.size.height;
    [p moveToPoint:NSMakePoint(x, y + h * 0.3)];
    [p lineToPoint:NSMakePoint(x + w * 0.25, y + h * 0.3)];
    [p lineToPoint:NSMakePoint(x + w * 0.55, y)];
    [p lineToPoint:NSMakePoint(x + w * 0.55, y + h)];
    [p lineToPoint:NSMakePoint(x + w * 0.25, y + h * 0.7)];
    [p lineToPoint:NSMakePoint(x, y + h * 0.7)];
    [p closePath];
    [p fill];
    NSBezierPath *l = [NSBezierPath bezierPath];
    l.lineWidth = 1.5;
    if (_muted) {
        [l moveToPoint:NSMakePoint(x + w * 0.7, y + h * 0.25)];
        [l lineToPoint:NSMakePoint(x + w, y + h * 0.75)];
        [l moveToPoint:NSMakePoint(x + w * 0.7, y + h * 0.75)];
        [l lineToPoint:NSMakePoint(x + w, y + h * 0.25)];
    } else {
        [l appendBezierPathWithArcWithCenter:NSMakePoint(x + w * 0.55, y + h / 2) radius:w * 0.3
                                  startAngle:-45 endAngle:45];
    }
    [l stroke];
}

@end

/* The controls' bar: dark, a little see-through; rounded when it floats. */
@interface AVControlsBar : NSView
@property CGFloat cornerRadius;
@end

@implementation AVControlsBar

- (void)drawRect:(NSRect)dirty {
    [[NSColor colorWithCalibratedWhite:0.08 alpha:0.78] setFill];
    if (_cornerRadius > 0)
        [[NSBezierPath bezierPathWithRoundedRect:self.bounds xRadius:_cornerRadius yRadius:_cornerRadius] fill];
    else
        NSRectFillUsingOperation(self.bounds, NSCompositingOperationSourceOver);
}

@end

/* Elapsed time in whole seconds gone by; time left in those to come. */
static NSString *timeString(double seconds, BOOL up) {
    if (!isfinite(seconds) || seconds < 0)
        seconds = 0;
    long s = (long)(up ? ceil(seconds - 0.001) : floor(seconds + 0.001));
    if (s >= 3600)
        return [NSString stringWithFormat:@"%ld:%02ld:%02ld", s / 3600, s / 60 % 60, s % 60];
    return [NSString stringWithFormat:@"%ld:%02ld", s / 60, s % 60];
}

static const CGFloat kBarHeight = 34;

@implementation AVPlayerView {
    AVPlayerLayer *_playerLayer;
    AVControlsBar *_bar;
    AVPlayButton *_playButton, *_bigPlayButton;
    AVMuteButton *_muteButton;
    NSTextField *_elapsed, *_remaining;
    NSSlider *_slider;
    id _timeObserver;
    AVPlayer *_observed;
    BOOL _scrubbing;
}

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame]) != nil)
        [self _setUp];
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super initWithCoder:coder]) != nil)
        [self _setUp];
    return self;
}

- (void)dealloc {
    [self _stopObserving];
}

- (void)_setUp {
    CALayer *root = [CALayer layer];
    root.backgroundColor = CGColorCreateGenericGray(0, 1);
    self.layer = root;
    self.wantsLayer = YES;
    _playerLayer = [AVPlayerLayer playerLayerWithPlayer:nil];
    [root addSublayer:_playerLayer];
    _videoGravity = AVLayerVideoGravityResizeAspect;
    _controlsStyle = AVPlayerViewControlsStyleInline;
    _showsFullScreenToggleButton = NO;

    _contentOverlayView = [[NSView alloc] initWithFrame:self.bounds];
    _contentOverlayView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [self addSubview:_contentOverlayView];

    _bar = [[AVControlsBar alloc] initWithFrame:NSZeroRect];
    _playButton = [[AVPlayButton alloc] initWithFrame:NSZeroRect];
    _playButton.bordered = NO;
    _playButton.target = self;
    _playButton.action = @selector(_togglePlay:);
    _elapsed = [self _label];
    _remaining = [self _label];
    _slider = [[NSSlider alloc] initWithFrame:NSZeroRect];
    _slider.minValue = 0;
    _slider.maxValue = 1;
    _slider.continuous = YES;
    _slider.target = self;
    _slider.action = @selector(_scrub:);
    _muteButton = [[AVMuteButton alloc] initWithFrame:NSZeroRect];
    _muteButton.bordered = NO;
    _muteButton.target = self;
    _muteButton.action = @selector(_toggleMute:);
    for (NSView *v in @[ _playButton, _elapsed, _slider, _remaining, _muteButton ])
        [_bar addSubview:v];
    [self addSubview:_bar];

    _bigPlayButton = [[AVPlayButton alloc] initWithFrame:NSZeroRect];
    _bigPlayButton.bordered = NO;
    _bigPlayButton.target = self;
    _bigPlayButton.action = @selector(_togglePlay:);
    [self addSubview:_bigPlayButton];
    [self _layOut];
    [self _updateControls];
}

- (NSTextField *)_label {
    NSTextField *t = [NSTextField labelWithString:@"0:00"];
    t.textColor = [NSColor colorWithCalibratedWhite:1 alpha:0.9];
    t.font = [NSFont monospacedDigitSystemFontOfSize:11 weight:NSFontWeightRegular];
    t.alignment = NSTextAlignmentCenter;
    return t;
}

- (BOOL)isFlipped {
    return NO;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

/* ---- Layout -------------------------------------------------------------------- */

- (void)setFrameSize:(NSSize)size {
    [super setFrameSize:size];
    [self _layOut];
}

- (void)_layOut {
    NSRect b = self.bounds;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    self.layer.bounds = NSRectToCGRect(b);
    _playerLayer.frame = NSRectToCGRect(b);
    [CATransaction commit];

    BOOL bar = _controlsStyle == AVPlayerViewControlsStyleInline || _controlsStyle == AVPlayerViewControlsStyleFloating;
    _bar.hidden = !bar;
    _bigPlayButton.hidden = _controlsStyle != AVPlayerViewControlsStyleMinimal;
    /* Floating over the video, rounded; a narrow view gets it its whole
       width less a margin. */
    CGFloat margin = b.size.width < 200 ? 6 : 20;
    CGFloat w = MIN(460, b.size.width - 2 * margin);
    _bar.frame = NSMakeRect(NSMidX(b) - w / 2, b.size.height < 120 ? 6 : 16, w, kBarHeight);
    _bar.cornerRadius = 8;
    [_bar setNeedsDisplay:YES];
    NSRect r = _bar.bounds;
    CGFloat x = 6, h = r.size.height;
    _playButton.frame = NSMakeRect(x, 2, h - 4, h - 4);
    x += h;
    _elapsed.frame = NSMakeRect(x, (h - 16) / 2, 52, 16);
    x += 56;
    CGFloat right = r.size.width - 6 - (h - 4);
    _muteButton.frame = NSMakeRect(right, 2, h - 4, h - 4);
    right -= 56;
    _remaining.frame = NSMakeRect(right, (h - 16) / 2, 52, 16);
    _slider.frame = NSMakeRect(x, (h - 20) / 2, MAX(right - x - 4, 20), 20);
    CGFloat big = MIN(64, MIN(b.size.width, b.size.height) / 3);
    _bigPlayButton.frame = NSMakeRect(NSMidX(b) - big / 2, NSMidY(b) - big / 2, big, big);
}

- (void)setControlsStyle:(AVPlayerViewControlsStyle)style {
    _controlsStyle = style;
    [self _layOut];
}

- (void)setVideoGravity:(AVLayerVideoGravity)gravity {
    _videoGravity = [gravity copy] ?: AVLayerVideoGravityResizeAspect;
    _playerLayer.videoGravity = _videoGravity;
}

- (BOOL)isReadyForDisplay {
    return _playerLayer.readyForDisplay;
}

- (NSRect)videoBounds {
    return NSRectFromCGRect(_playerLayer.videoRect);
}

/* ---- The player ---------------------------------------------------------------- */

- (void)setPlayer:(AVPlayer *)player {
    if (player == _player)
        return;
    [self _stopObserving];
    _player = player;
    _playerLayer.player = player;
    if (player != nil) {
        __weak AVPlayerView *weakSelf = self;
        _timeObserver = [player addPeriodicTimeObserverForInterval:CMTimeMake(1, 4) queue:dispatch_get_main_queue()
                                                        usingBlock:^(CMTime t) { [weakSelf _updateControls]; }];
        [player addObserver:self forKeyPath:@"rate" options:0 context:NULL];
        [player addObserver:self forKeyPath:@"currentItem.status" options:0 context:NULL];
        _observed = player;
    }
    [self _updateControls];
}

- (void)_stopObserving {
    if (_observed == nil)
        return;
    [_observed removeTimeObserver:_timeObserver];
    [_observed removeObserver:self forKeyPath:@"rate"];
    [_observed removeObserver:self forKeyPath:@"currentItem.status"];
    _observed = nil;
    _timeObserver = nil;
}

- (void)observeValueForKeyPath:(NSString *)path ofObject:(id)object change:(NSDictionary *)change context:(void *)context {
    if ([NSThread isMainThread])
        [self _updateControls];
    else
        dispatch_async(dispatch_get_main_queue(), ^{ [self _updateControls]; });
}

- (void)_updateControls {
    AVPlayerItem *item = _player.currentItem;
    double duration = item != nil && CMTIME_IS_NUMERIC(item.duration) ? CMTimeGetSeconds(item.duration) : 0;
    double now = _player != nil ? CMTimeGetSeconds(_player.currentTime) : 0;
    BOOL playing = _player.rate != 0;
    _playButton.showsPause = playing;
    _bigPlayButton.showsPause = playing;
    [_playButton setNeedsDisplay:YES];
    [_bigPlayButton setNeedsDisplay:YES];
    _muteButton.muted = _player.muted;
    [_muteButton setNeedsDisplay:YES];
    _elapsed.stringValue = timeString(now, NO);
    _remaining.stringValue = [@"-" stringByAppendingString:timeString(duration - now, YES)];
    _slider.enabled = duration > 0;
    if (!_scrubbing) {
        _slider.maxValue = duration > 0 ? duration : 1;
        _slider.doubleValue = now;
    }
}

/* ---- Actions ------------------------------------------------------------------- */

- (void)_togglePlay:(id)sender {
    if (_player == nil)
        return;
    if (_player.rate != 0)
        [_player pause];
    else {
        AVPlayerItem *item = _player.currentItem;
        /* At the end, play starts over. */
        if (item != nil && CMTIME_IS_NUMERIC(item.duration) &&
            CMTimeCompare(_player.currentTime, item.duration) >= 0)
            [_player seekToTime:kCMTimeZero];
        [_player play];
    }
    [self _updateControls];
}

- (void)_scrub:(id)sender {
    _scrubbing = YES;
    [_player seekToTime:CMTimeMakeWithSeconds(_slider.doubleValue, 600)];
    _scrubbing = NO;
    [self _updateControls];
}

- (void)_toggleMute:(id)sender {
    _player.muted = !_player.muted;
    [self _updateControls];
}

- (void)keyDown:(NSEvent *)event {
    NSString *c = event.charactersIgnoringModifiers;
    if ([c isEqual:@" "]) {
        [self _togglePlay:nil];
        return;
    }
    unichar k = c.length > 0 ? [c characterAtIndex:0] : 0;
    if ((k == NSRightArrowFunctionKey || k == NSLeftArrowFunctionKey) && _player.rate == 0) {
        [_player.currentItem stepByCount:k == NSRightArrowFunctionKey ? 1 : -1];
        [self _updateControls];
        return;
    }
    [super keyDown:event];
}

- (void)mouseDown:(NSEvent *)event {
    [self.window makeFirstResponder:self];
    if (event.clickCount == 2 && _showsFullScreenToggleButton)
        [self.window toggleFullScreen:nil];
}

/* ---- What is not offered -------------------------------------------------------- */

/* No trimming: there is no editing of assets yet. */
- (BOOL)canBeginTrimming {
    return NO;
}

- (void)beginTrimmingWithCompletionHandler:(void (^)(AVPlayerViewTrimResult))handler {
    if (handler != nil)
        dispatch_async(dispatch_get_main_queue(), ^{ handler(AVPlayerViewTrimCancelButton); });
}

- (void)flashChapterNumber:(NSUInteger)chapterNumber chapterTitle:(NSString *)chapterTitle {
}

@end
