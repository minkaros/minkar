/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Sound pane: the output device, its volume and mute, through
 * CoreAudio (kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyMute on
 * the output scope). Atlas's provisional look.
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#include <CoreAudio/CoreAudio.h>
#include <os/log.h>

@interface SoundPane : NSPreferencePane
@end

static const CGFloat kWidth = 668, kHeight = 200;

static NSTextField *label(NSString *text, NSRect frame, CGFloat size, NSTextAlignment alignment)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:frame];
    [field setStringValue:text];
    [field setEditable:NO];
    [field setSelectable:NO];
    [field setBordered:NO];
    [field setDrawsBackground:NO];
    [field setAlignment:alignment];
    [field setFont:[NSFont systemFontOfSize:size]];
    return field;
}

static AudioObjectPropertyAddress address(AudioObjectPropertySelector selector, AudioObjectPropertyScope scope)
{
    return (AudioObjectPropertyAddress){ selector, scope, kAudioObjectPropertyElementMain };
}

@implementation SoundPane {
    NSPopUpButton *_device;
    NSSlider *_volume;
    NSButton *_mute;
    NSTextField *_none;
    NSMutableArray<NSNumber *> *_devices;
}

- (NSString *)text:(NSString *)english
{
    return [[self bundle] localizedStringForKey:english value:english table:nil];
}

/* The output devices, by name, the default one chosen. */
- (void)loadDevices
{
    AudioObjectPropertyAddress a = address(kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal);
    UInt32 size = 0;
    AudioDeviceID ids[16];
    AudioDeviceID current = kAudioObjectUnknown;

    _devices = [NSMutableArray array];
    [_device removeAllItems];
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &a, 0, NULL, &size) == noErr && size > 0) {
        if (size > sizeof(ids))
            size = sizeof(ids);
        AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, ids);
    }
    for (UInt32 i = 0; i < size / sizeof(AudioDeviceID); i++) {
        AudioObjectPropertyAddress n = address(kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal);
        CFStringRef name = NULL;
        UInt32 nameSize = sizeof(name);
        if (AudioObjectGetPropertyData(ids[i], &n, 0, NULL, &nameSize, &name) != noErr || name == NULL)
            continue;
        [_device addItemWithTitle:(__bridge_transfer NSString *)name];
        [_devices addObject:@(ids[i])];
    }
    a = address(kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal);
    size = sizeof(current);
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, &current);
    NSUInteger index = [_devices indexOfObject:@(current)];
    if (index != NSNotFound)
        [_device selectItemAtIndex:(NSInteger)index];
}

- (AudioDeviceID)selectedDevice
{
    NSInteger index = [_device indexOfSelectedItem];
    return index >= 0 && (NSUInteger)index < [_devices count] ? [_devices[(NSUInteger)index] unsignedIntValue]
                                                             : kAudioObjectUnknown;
}

/* The controls show the selected device's volume and mute; without a
   device, or a volume, they are off. */
- (void)showDevice
{
    AudioDeviceID device = [self selectedDevice];
    AudioObjectPropertyAddress v = address(kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput);
    AudioObjectPropertyAddress m = address(kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput);
    Float32 volume = 0;
    UInt32 muted = 0, size;
    BOOL hasVolume = device != kAudioObjectUnknown && AudioObjectHasProperty(device, &v);
    BOOL hasMute = device != kAudioObjectUnknown && AudioObjectHasProperty(device, &m);

    if (hasVolume) {
        size = sizeof(volume);
        AudioObjectGetPropertyData(device, &v, 0, NULL, &size, &volume);
    }
    if (hasMute) {
        size = sizeof(muted);
        AudioObjectGetPropertyData(device, &m, 0, NULL, &size, &muted);
    }
    [_volume setEnabled:hasVolume];
    [_volume setFloatValue:volume];
    [_mute setEnabled:hasMute];
    [_mute setState:muted ? NSControlStateValueOn : NSControlStateValueOff];
    [_device setEnabled:[_devices count] > 0];
    [_none setHidden:[_devices count] > 0];
}

- (NSView *)loadMainView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];

    [view addSubview:label([self text:@"Output device:"], NSMakeRect(40, 146, 220, 20), 13, NSRightTextAlignment)];
    _device = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(270, 142, 300, 26) pullsDown:NO];
    [_device setTarget:self];
    [_device setAction:@selector(deviceChanged:)];
    [view addSubview:_device];

    [view addSubview:label([self text:@"Output volume:"], NSMakeRect(40, 96, 220, 20), 13, NSRightTextAlignment)];
    _volume = [[NSSlider alloc] initWithFrame:NSMakeRect(270, 94, 240, 24)];
    [_volume setMinValue:0];
    [_volume setMaxValue:1];
    [_volume setContinuous:YES];
    [_volume setTarget:self];
    [_volume setAction:@selector(volumeChanged:)];
    [view addSubview:_volume];
    [view addSubview:label([self text:@"Low"], NSMakeRect(270, 74, 100, 16), 11, NSLeftTextAlignment)];
    [view addSubview:label([self text:@"High"], NSMakeRect(410, 74, 100, 16), 11, NSRightTextAlignment)];

    _mute = [[NSButton alloc] initWithFrame:NSMakeRect(526, 96, 120, 20)];
    [_mute setButtonType:NSButtonTypeSwitch];
    [_mute setTitle:[self text:@"Mute"]];
    [_mute setTarget:self];
    [_mute setAction:@selector(muteChanged:)];
    [view addSubview:_mute];

    _none = label([self text:@"No output device was found."], NSMakeRect(40, 30, 588, 20), 12, NSCenterTextAlignment);
    [view addSubview:_none];

    [self loadDevices];
    [self showDevice];
    [self setMainView:view];
    return view;
}

- (void)deviceChanged:(id)sender
{
    [self showDevice];
}

- (void)volumeChanged:(id)sender
{
    AudioDeviceID device = [self selectedDevice];
    AudioObjectPropertyAddress v = address(kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput);
    Float32 volume = [_volume floatValue];
    OSStatus err = AudioObjectSetPropertyData(device, &v, 0, NULL, sizeof(volume), &volume);

    if (err != noErr)
        os_log_error(OS_LOG_DEFAULT, "output volume %.2f: %d", volume, (int)err);
}

- (void)muteChanged:(id)sender
{
    AudioDeviceID device = [self selectedDevice];
    AudioObjectPropertyAddress m = address(kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput);
    UInt32 muted = [_mute state] == NSControlStateValueOn;
    OSStatus err = AudioObjectSetPropertyData(device, &m, 0, NULL, sizeof(muted), &muted);

    if (err != noErr)
        os_log_error(OS_LOG_DEFAULT, "output mute %u: %d", muted, (int)err);
    [self showDevice];
}

@end
