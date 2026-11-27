/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The keyboard's sound keys (VolumeKeys.h).
 */

#import "VolumeKeys.h"
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioServices.h>
#include <SkyLight/SkyLight.h>
#include <os/log.h>

static const int kSteps = 16;

static AudioObjectPropertyAddress outputAddress(AudioObjectPropertySelector selector)
{
    return (AudioObjectPropertyAddress){ selector, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain };
}

static AudioDeviceID defaultOutput(void)
{
    AudioObjectPropertyAddress a = { kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal,
                                     kAudioObjectPropertyElementMain };
    AudioDeviceID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);

    AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, &device);
    return device;
}

/* -- The level on the screen -- */

@interface VolumeLevelView : NSView
@property (nonatomic) float level;     /* 0 to 1 */
@property (nonatomic) BOOL muted;
@end

@implementation VolumeLevelView

- (void)drawRect:(NSRect)dirty
{
    NSRect b = [self bounds];

    [[NSColor clearColor] setFill];
    NSRectFillUsingOperation(b, NSCompositingOperationCopy);
    [[NSColor colorWithCalibratedWhite:0.12 alpha:0.78] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:b xRadius:18 yRadius:18] fill];

    /* The speaker, its waves as the level has them. */
    NSString *name = self.muted || self.level <= 0.001 ? @"audio-volume-muted"
                   : self.level < 0.34 ? @"audio-volume-low"
                   : self.level < 0.67 ? @"audio-volume-medium" : @"audio-volume-high";
    NSImage *speaker = [[NSImage alloc] initWithContentsOfFile:
        [@"/System/Library/MenuBarIcons" stringByAppendingPathComponent:[name stringByAppendingPathExtension:@"png"]]];
    if (speaker != nil) {
        /* White: the template's black turned to the color. */
        NSImage *white = [[NSImage alloc] initWithSize:NSMakeSize(96, 96)];
        [white lockFocus];
        [speaker drawInRect:NSMakeRect(0, 0, 96, 96) fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1];
        [[NSColor whiteColor] setFill];
        NSRectFillUsingOperation(NSMakeRect(0, 0, 96, 96), NSCompositingOperationSourceIn);
        [white unlockFocus];
        [white drawInRect:NSMakeRect(NSMidX(b) - 48, NSMidY(b) - 30, 96, 96) fromRect:NSZeroRect
                operation:NSCompositingOperationSourceOver fraction:0.95];
    }

    /* Sixteen steps, those up to the level lit (quarters shown in part). */
    CGFloat gap = 2, width = 9, height = 6;
    CGFloat total = kSteps * width + (kSteps - 1) * gap;
    CGFloat x = floor(NSMidX(b) - total / 2), y = 28;
    float lit = self.muted ? 0 : self.level * kSteps;
    for (int i = 0; i < kSteps; i++) {
        NSRect box = NSMakeRect(x + i * (width + gap), y, width, height);
        [[NSColor colorWithCalibratedWhite:1 alpha:0.25] setFill];
        NSRectFill(box);
        float part = lit - i;
        if (part > 0) {
            [[NSColor colorWithCalibratedWhite:1 alpha:0.95] setFill];
            NSRectFill(NSMakeRect(NSMinX(box), NSMinY(box), round(width * MIN(part, 1.0f)), height));
        }
    }
}

@end

@implementation VolumeKeys {
    NSPanel *_panel;
    VolumeLevelView *_view;
    NSTimer *_hide;
}

- (float)volumeOf:(AudioDeviceID)device
{
    AudioObjectPropertyAddress a = outputAddress(kAudioDevicePropertyVolumeScalar);
    Float32 volume = 0;
    UInt32 size = sizeof(volume);

    if (device == kAudioObjectUnknown || !AudioObjectHasProperty(device, &a))
        return 0;
    AudioObjectGetPropertyData(device, &a, 0, NULL, &size, &volume);
    return volume;
}

- (BOOL)mutedOf:(AudioDeviceID)device
{
    AudioObjectPropertyAddress a = outputAddress(kAudioDevicePropertyMute);
    UInt32 muted = 0, size = sizeof(muted);

    if (device == kAudioObjectUnknown || !AudioObjectHasProperty(device, &a))
        return NO;
    AudioObjectGetPropertyData(device, &a, 0, NULL, &size, &muted);
    return muted != 0;
}

- (void)setMuted:(BOOL)muted of:(AudioDeviceID)device
{
    AudioObjectPropertyAddress a = outputAddress(kAudioDevicePropertyMute);
    UInt32 value = muted ? 1 : 0;

    if (AudioObjectHasProperty(device, &a))
        AudioObjectSetPropertyData(device, &a, 0, NULL, sizeof(value), &value);
}

- (void)handleHotKey:(uint32_t)hotKey flags:(NSUInteger)flags
{
    AudioDeviceID device = defaultOutput();
    BOOL shift = (flags & NSEventModifierFlagShift) != 0, option = (flags & NSEventModifierFlagOption) != 0;

    if (device == kAudioObjectUnknown) {
        NSBeep();
        return;
    }
    float volume = [self volumeOf:device];
    BOOL muted = [self mutedOf:device];

    if (hotKey == kSLSHotKeyVolumeMute) {
        muted = !muted;
        [self setMuted:muted of:device];
    } else {
        /* A sixteenth, or a quarter of one with Shift-Option; to the
           next step from where the level is. */
        float step = 1.0f / kSteps / ((shift && option) ? 4 : 1);
        float steps = (hotKey == kSLSHotKeyVolumeUp) ? floorf(volume / step + 0.01f) + 1
                                                     : ceilf(volume / step - 0.01f) - 1;
        volume = MAX(0.0f, MIN(1.0f, steps * step));
        AudioObjectPropertyAddress a = outputAddress(kAudioDevicePropertyVolumeScalar);
        Float32 value = volume;
        AudioObjectSetPropertyData(device, &a, 0, NULL, sizeof(value), &value);
        if (muted) {
            muted = NO;
            [self setMuted:NO of:device];
        }
        [self feedback:shift];
    }
    os_log_info(OS_LOG_DEFAULT, "volume key %u: %.4f%{public}s", hotKey, volume, muted ? " muted" : "");
    [self showLevel:volume muted:muted];
}

/* The Sound pane's feedback, Shift turning it the other way. */
- (void)feedback:(BOOL)shift
{
    Boolean valid = false;
    CFPreferencesAppSynchronize(kCFPreferencesAnyApplication);
    BOOL on = CFPreferencesGetAppBooleanValue(CFSTR("com.apple.sound.beep.feedback"), kCFPreferencesAnyApplication,
                                              &valid) && valid;
    if (on == shift)
        return;
    static SystemSoundID pop;
    if (pop == 0)
        AudioServicesCreateSystemSoundID((__bridge CFURLRef)[NSURL fileURLWithPath:@"/System/Library/Sounds/Pop.aiff"],
                                         &pop);
    if (pop != 0)
        AudioServicesPlaySystemSound(pop);
}

/* A rounded square in the lower middle of the main screen, over every
   window, gone after a moment. */
- (void)showLevel:(float)level muted:(BOOL)muted
{
    NSRect screen = [[NSScreen mainScreen] frame];
    NSRect frame = NSMakeRect(floor(NSMidX(screen) - 100), NSMinY(screen) + 140, 200, 200);

    if (_panel == nil) {
        _panel = [[NSPanel alloc] initWithContentRect:frame styleMask:NSWindowStyleMaskBorderless
                                              backing:NSBackingStoreBuffered defer:NO];
        [_panel setOpaque:NO];
        [_panel setBackgroundColor:[NSColor clearColor]];
        [_panel setHasShadow:NO];
        [_panel setIgnoresMouseEvents:YES];
        [_panel setLevel:kCGOverlayWindowLevel];
        [_panel setReleasedWhenClosed:NO];
        _view = [[VolumeLevelView alloc] initWithFrame:NSMakeRect(0, 0, 200, 200)];
        [_panel setContentView:_view];
    }
    [_panel setFrame:frame display:NO];
    _view.level = level;
    _view.muted = muted;
    [_view setNeedsDisplay:YES];
    [_panel setAlphaValue:1];
    [_panel orderFrontRegardless];
    [_hide invalidate];
    _hide = [NSTimer scheduledTimerWithTimeInterval:1.5 target:self selector:@selector(hideLevel:) userInfo:nil
                                            repeats:NO];
}

- (void)hideLevel:(NSTimer *)timer
{
    [_panel orderOut:nil];
}

@end
