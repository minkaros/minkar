/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The volume Menu Extra (AU2), a menu as the reference's is: "Volume:",
 * a slider (an item's view), "Output Device:" and the output devices (the
 * default one checked, a choice making it the default), then Sound
 * Preferences. Its items are highlighted in the accent like any menu's.
 */

#import "VolumeExtra.h"

/* A .menu bundle's texts are its own (Resources/es.lproj). */
#undef NSLocalizedString
#define NSLocalizedString(key, comment) \
    [[NSBundle bundleForClass:[VolumeExtra class]] localizedStringForKey:(key) value:@"" table:nil]
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioServices.h>

static NSString *const kIcons = @"/System/Library/MenuBarIcons";

static AudioObjectPropertyAddress outputAddress(AudioObjectPropertySelector selector)
{
    return (AudioObjectPropertyAddress){ selector, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain };
}

static AudioObjectPropertyAddress globalAddress(AudioObjectPropertySelector selector)
{
    return (AudioObjectPropertyAddress){ selector, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
}

@interface VolumeExtra () <NSMenuDelegate>
@end

@implementation VolumeExtra {
    NSStatusItem *_item;
    NSMenu *_menu;
    NSSlider *_slider;
    AudioDeviceID _device;
}

- (AudioDeviceID)defaultOutput
{
    AudioObjectPropertyAddress a = globalAddress(kAudioHardwarePropertyDefaultOutputDevice);
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

/* The devices with output streams, by name. */
- (NSArray<NSArray *> *)outputDevices
{
    AudioObjectPropertyAddress a = globalAddress(kAudioHardwarePropertyDevices);
    UInt32 size = 0;
    NSMutableArray *found = [NSMutableArray array];

    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &a, 0, NULL, &size) != noErr || size == 0)
        return found;
    AudioDeviceID devices[size / sizeof(AudioDeviceID)];
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, devices) != noErr)
        return found;
    for (UInt32 i = 0; i < size / sizeof(AudioDeviceID); i++) {
        AudioObjectPropertyAddress streams = outputAddress(kAudioDevicePropertyStreams);
        UInt32 streamsSize = 0;
        if (AudioObjectGetPropertyDataSize(devices[i], &streams, 0, NULL, &streamsSize) != noErr || streamsSize == 0)
            continue;
        AudioObjectPropertyAddress n = globalAddress(kAudioObjectPropertyName);
        CFStringRef name = NULL;
        UInt32 nameSize = sizeof(name);
        AudioObjectGetPropertyData(devices[i], &n, 0, NULL, &nameSize, &name);
        [found addObject:@[ @(devices[i]), name != NULL ? (__bridge_transfer NSString *)name : @"?" ]];
    }
    return found;
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
    /* Black on clear: drawn in the menu bar's own color. */
    [image setTemplate:YES];
    [_item setImage:image];
    /* Changed from a notification, with no event to come: drawn now. */
    [[_item button] display];
    [_slider setFloatValue:volume];
    [_slider setEnabled:_device != kAudioObjectUnknown];
}

- (instancetype)initWithStatusItem:(NSStatusItem *)item
{
    if ((self = [super init]) == nil)
        return nil;
    _item = item;
    _device = [self defaultOutput];

    /* The slider, as the reference's menu has it: inset like the items'
       titles. */
    NSView *holder = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 220, 28)];
    _slider = [[NSSlider alloc] initWithFrame:NSMakeRect(18, 2, 184, 24)];
    [_slider setMinValue:0];
    [_slider setMaxValue:1];
    [_slider setContinuous:YES];
    [_slider setTarget:self];
    [_slider setAction:@selector(volumeChanged:)];
    [holder addSubview:_slider];

    _menu = [[NSMenu alloc] initWithTitle:@"Volume"];
    [_menu setDelegate:self];
    [_menu setAutoenablesItems:NO];
    NSMenuItem *sliderItem = [[NSMenuItem alloc] initWithTitle:@"" action:NULL keyEquivalent:@""];
    [sliderItem setView:holder];
    [_menu addItem:sliderItem];
    [_item setMenu:_menu];
    [self menuNeedsUpdate:_menu];
    [self showLevel];

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

static NSMenuItem *heading(NSString *title)
{
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:NULL keyEquivalent:@""];
    [item setEnabled:NO];
    return item;
}

/* Made again as it opens: the level, the devices, the default one. */
- (void)menuNeedsUpdate:(NSMenu *)menu
{
    NSMenuItem *sliderItem = nil;
    for (NSMenuItem *item in [menu itemArray])
        if ([item view] != nil)
            sliderItem = item;
    [menu removeAllItems];
    _device = [self defaultOutput];
    [self showLevel];

    [menu addItem:heading(NSLocalizedString(@"Volume:", @"the volume menu's first heading"))];
    if (sliderItem != nil)
        [menu addItem:sliderItem];
    NSArray *devices = [self outputDevices];
    if (devices.count > 0) {
        [menu addItem:heading(NSLocalizedString(@"Output Device:", @"the volume menu's devices"))];
        for (NSArray *device in devices) {
            NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:device[1] action:@selector(chooseDevice:)
                                                   keyEquivalent:@""];
            [item setTarget:self];
            [item setTag:[device[0] integerValue]];
            [item setState:[device[0] unsignedIntValue] == _device ? NSControlStateValueOn : NSControlStateValueOff];
            [menu addItem:item];
        }
    }
    [menu addItem:[NSMenuItem separatorItem]];
    NSMenuItem *prefs = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Sound Preferences…", nil)
                                                   action:@selector(openPreferences:) keyEquivalent:@""];
    [prefs setTarget:self];
    [menu addItem:prefs];
}

- (void)chooseDevice:(NSMenuItem *)sender
{
    AudioObjectPropertyAddress a = globalAddress(kAudioHardwarePropertyDefaultOutputDevice);
    AudioDeviceID device = (AudioDeviceID)[sender tag];

    AudioObjectSetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, sizeof(device), &device);
    _device = [self defaultOutput];
    [self showLevel];
}

- (void)volumeChanged:(id)sender
{
    AudioObjectPropertyAddress a = outputAddress(kAudioDevicePropertyVolumeScalar);
    Float32 volume = [_slider floatValue];

    AudioObjectSetPropertyData(_device, &a, 0, NULL, sizeof(volume), &volume);
    [self showLevel];
    /* The Sound pane's "Play feedback when volume is changed"
       (com.apple.sound.beep.feedback): a short sound at the new volume,
       once the slider is let go. */
    if ([[NSApp currentEvent] type] == NSEventTypeLeftMouseUp) {
        Boolean valid = false;
        CFPreferencesAppSynchronize(kCFPreferencesAnyApplication);
        if (CFPreferencesGetAppBooleanValue(CFSTR("com.apple.sound.beep.feedback"),
                                            kCFPreferencesAnyApplication, &valid) && valid) {
            static SystemSoundID pop;
            if (pop == 0)
                AudioServicesCreateSystemSoundID((__bridge CFURLRef)[NSURL fileURLWithPath:@"/System/Library/Sounds/Pop.aiff"], &pop);
            if (pop != 0)
                AudioServicesPlaySystemSound(pop);
        }
    }
}

- (void)openPreferences:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:
        [NSURL fileURLWithPath:@"/System/Library/PreferencePanes/Sound.prefPane"]];
}

@end
