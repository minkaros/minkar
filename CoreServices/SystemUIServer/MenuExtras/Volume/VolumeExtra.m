/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The volume Menu Extra (AU2). The panel is a small window of its own (a
 * menu with a slider in it comes with menu items' views).
 */

#import "VolumeExtra.h"

/* A .menu bundle's texts are its own (Resources/es.lproj). */
#undef NSLocalizedString
#define NSLocalizedString(key, comment) \
    [[NSBundle bundleForClass:[VolumeExtra class]] localizedStringForKey:(key) value:@"" table:nil]
#include <CoreAudio/CoreAudio.h>

static NSString *const kIcons = @"/System/Library/Icons/menubar";

/* A borderless panel that takes the keyboard, and goes when it loses it
   (a click anywhere else). */
@interface VolumePanel : NSPanel
@end

@implementation VolumePanel
- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (void)resignKeyWindow
{
    [super resignKeyWindow];
    [self orderOut:nil];
}
@end

static AudioObjectPropertyAddress outputAddress(AudioObjectPropertySelector selector)
{
    return (AudioObjectPropertyAddress){ selector, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain };
}

@implementation VolumeExtra {
    NSStatusItem *_item;
    NSRunningApplication *_previous;   /* in front before the panel */
    VolumePanel *_panel;
    NSSlider *_slider;
    NSButton *_mute;
    AudioDeviceID _device;
}

- (AudioDeviceID)defaultOutput
{
    AudioObjectPropertyAddress a = { kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal,
                                     kAudioObjectPropertyElementMain };
    AudioDeviceID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);

    AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, &device);
    return device;
}

- (Float32)volume
{
    AudioObjectPropertyAddress a = outputAddress(kAudioDevicePropertyVolumeScalar);
    Float32 volume = 0;
    UInt32 size = sizeof(volume);

    if (_device == kAudioObjectUnknown || !AudioObjectHasProperty(_device, &a))
        return 0;
    AudioObjectGetPropertyData(_device, &a, 0, NULL, &size, &volume);
    return volume;
}

- (BOOL)muted
{
    AudioObjectPropertyAddress a = outputAddress(kAudioDevicePropertyMute);
    UInt32 muted = 0, size = sizeof(muted);

    if (_device == kAudioObjectUnknown || !AudioObjectHasProperty(_device, &a))
        return NO;
    AudioObjectGetPropertyData(_device, &a, 0, NULL, &size, &muted);
    return muted != 0;
}

/* The speaker for the level: muted (or no device), low, medium, high. */
- (void)showLevel
{
    Float32 volume = [self volume];
    NSString *name = _device == kAudioObjectUnknown || [self muted] || volume <= 0.001 ? @"audio-volume-muted"
                   : volume < 0.34 ? @"audio-volume-low"
                   : volume < 0.67 ? @"audio-volume-medium" : @"audio-volume-high";
    NSImage *image = [[NSImage alloc] initWithContentsOfFile:
        [kIcons stringByAppendingPathComponent:[name stringByAppendingPathExtension:@"png"]]];

    [image setSize:NSMakeSize(18, 18)];
    [_item setImage:image];
    /* Changed from a notification, with no event to come: drawn now. */
    [[_item button] display];
    [_slider setFloatValue:volume];
    [_slider setEnabled:_device != kAudioObjectUnknown];
    [_mute setState:[self muted] ? NSControlStateValueOn : NSControlStateValueOff];
    [_mute setEnabled:_device != kAudioObjectUnknown];
}

- (instancetype)initWithStatusItem:(NSStatusItem *)item
{
    if ((self = [super init]) == nil)
        return nil;
    _item = item;
    _device = [self defaultOutput];
    [_item setTarget:self];
    [_item setAction:@selector(toggle:)];
    [_item sendActionOn:NSEventMaskLeftMouseDown];
    [self buildPanel];
    [self showLevel];
    /* A click in another app (this one no longer active) closes it. */
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(deactivated:)
                                                 name:NSApplicationDidResignActiveNotification object:nil];

    /* Changes from anywhere (the Sound pane, an app) show here. */
    __weak VolumeExtra *weakSelf = self;
    AudioObjectPropertyListenerBlock changed = ^(UInt32 count, const AudioObjectPropertyAddress *addresses) {
        [weakSelf showLevel];
    };
    if (_device != kAudioObjectUnknown) {
        AudioObjectPropertyAddress v = outputAddress(kAudioDevicePropertyVolumeScalar);
        AudioObjectPropertyAddress m = outputAddress(kAudioDevicePropertyMute);
        AudioObjectAddPropertyListenerBlock(_device, &v, (__bridge void *)dispatch_get_main_queue(), changed);
        AudioObjectAddPropertyListenerBlock(_device, &m, (__bridge void *)dispatch_get_main_queue(), changed);
    }
    return self;
}

- (void)buildPanel
{
    NSRect frame = NSMakeRect(0, 0, 240, 128);
    NSView *content;
    NSTextField *title;
    NSBox *line;
    NSButton *prefs;

    _panel = [[VolumePanel alloc] initWithContentRect:frame styleMask:NSWindowStyleMaskBorderless
                                              backing:NSBackingStoreBuffered defer:NO];
    [_panel setLevel:NSPopUpMenuWindowLevel];
    [_panel setHidesOnDeactivate:NO];
    content = [_panel contentView];

    title = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 96, 208, 18)];
    [title setStringValue:NSLocalizedString(@"Sound", @"the volume panel's title")];
    [title setFont:[NSFont boldSystemFontOfSize:13]];
    [title setEditable:NO];
    [title setBordered:NO];
    [title setDrawsBackground:NO];
    [content addSubview:title];

    _slider = [[NSSlider alloc] initWithFrame:NSMakeRect(16, 66, 208, 24)];
    [_slider setMinValue:0];
    [_slider setMaxValue:1];
    [_slider setContinuous:YES];
    [_slider setTarget:self];
    [_slider setAction:@selector(volumeChanged:)];
    [content addSubview:_slider];

    _mute = [[NSButton alloc] initWithFrame:NSMakeRect(16, 42, 208, 20)];
    [_mute setButtonType:NSButtonTypeSwitch];
    [_mute setTitle:NSLocalizedString(@"Mute", nil)];
    [_mute setTarget:self];
    [_mute setAction:@selector(muteChanged:)];
    [content addSubview:_mute];

    line = [[NSBox alloc] initWithFrame:NSMakeRect(12, 36, 216, 1)];
    [line setBoxType:NSBoxSeparator];
    [content addSubview:line];

    prefs = [[NSButton alloc] initWithFrame:NSMakeRect(10, 8, 220, 24)];
    [prefs setTitle:NSLocalizedString(@"Sound Preferences…", nil)];
    [prefs setBordered:NO];
    [prefs setAlignment:NSTextAlignmentLeft];
    [prefs setTarget:self];
    [prefs setAction:@selector(openPreferences:)];
    [content addSubview:prefs];
}

/* Below the speaker, inside the screen. */
- (void)deactivated:(NSNotification *)note
{
    _previous = nil;
    [_panel orderOut:nil];
}

/* Closed from here: the app that was in front is again. */
- (void)close
{
    [_panel orderOut:nil];
    [_previous activateWithOptions:0];
    _previous = nil;
}

- (void)toggle:(id)sender
{
    if ([_panel isVisible]) {
        [self close];
        return;
    }
    NSRunningApplication *front = [[NSWorkspace sharedWorkspace] frontmostApplication];
    if (![front isEqual:[NSRunningApplication currentApplication]])
        _previous = front;
    _device = [self defaultOutput];
    [self showLevel];
    NSRect itemFrame = [[[_item button] window] frame];
    NSRect visible = [[NSScreen mainScreen] visibleFrame];
    NSRect frame = [_panel frame];
    frame.origin.x = MIN(NSMinX(itemFrame), NSMaxX(visible) - frame.size.width);
    frame.origin.y = NSMinY(itemFrame) - frame.size.height;
    [_panel setFrame:frame display:NO];
    [NSApp activateIgnoringOtherApps:YES];
    [_panel makeKeyAndOrderFront:nil];
}

- (void)volumeChanged:(id)sender
{
    AudioObjectPropertyAddress a = outputAddress(kAudioDevicePropertyVolumeScalar);
    Float32 volume = [_slider floatValue];

    AudioObjectSetPropertyData(_device, &a, 0, NULL, sizeof(volume), &volume);
    [self showLevel];
}

- (void)muteChanged:(id)sender
{
    AudioObjectPropertyAddress a = outputAddress(kAudioDevicePropertyMute);
    UInt32 muted = [_mute state] == NSControlStateValueOn;

    AudioObjectSetPropertyData(_device, &a, 0, NULL, sizeof(muted), &muted);
    [self showLevel];
}

- (void)openPreferences:(id)sender
{
    _previous = nil;
    [_panel orderOut:nil];
    [[NSWorkspace sharedWorkspace] openURL:
        [NSURL fileURLWithPath:@"/System/Library/PreferencePanes/Sound.prefPane"]];
}

@end
