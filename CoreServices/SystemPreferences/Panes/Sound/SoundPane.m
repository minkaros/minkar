/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Sound pane, laid out as the reference platform's: the Sound
 * Effects, Output and Input tabs, and below them the output volume, Mute
 * and "Show volume in menu bar". Sound Effects: the alert sound (the
 * sounds of /System/Library/Sounds and ~/Library/Sounds, the one chosen
 * played), the alert volume, "Play user interface sound effects" and
 * "Play feedback when volume is changed", in the reference's global keys
 * (measured on it, 22 Nov 2026): com.apple.sound.beep.sound (the file's
 * path), com.apple.sound.beep.volume (e^(s-1) of the slider's s, 0 at its
 * left end), com.apple.sound.uiaudio.enabled, com.apple.sound.beep.feedback;
 * AudioToolbox's system sounds follow them. Output and Input: the devices
 * of each kind, the default one selected and chosen by a click, through
 * CoreAudio (the volume and mute on the output scope).
 */

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioServices.h>
#include <math.h>
#include <os/log.h>
#import "../MenuExtraList.h"

@interface SoundPane : NSPreferencePane
@end

static const CGFloat kWidth = 668, kHeight = 420;

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

@interface SoundPane () <NSTableViewDataSource, NSTableViewDelegate>
@end

/* A global preference, written for the whole user (NSGlobalDomain). */
static void setGlobal(CFStringRef key, CFPropertyListRef value)
{
    CFPreferencesSetValue(key, value, kCFPreferencesAnyApplication, kCFPreferencesCurrentUser,
                          kCFPreferencesAnyHost);
    CFPreferencesSynchronize(kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
}

static CFPropertyListRef copyGlobal(CFStringRef key)
{
    CFPreferencesAppSynchronize(kCFPreferencesAnyApplication);
    return CFPreferencesCopyAppValue(key, kCFPreferencesAnyApplication);
}

static BOOL globalFlag(CFStringRef key, BOOL fallback)
{
    Boolean valid = false;
    Boolean on = CFPreferencesGetAppBooleanValue(key, kCFPreferencesAnyApplication, &valid);
    return valid ? on : fallback;
}

@implementation SoundPane {
    NSPopUpButton *_device;
    NSSlider *_volume, *_alertVolume;
    NSButton *_mute, *_uiSounds, *_feedback;
    NSTextField *_none;
    NSMutableArray<NSNumber *> *_devices;
    NSTableView *_alerts, *_outputs, *_inputs;
    NSArray<NSDictionary *> *_sounds;      /* name, path, type */
    NSMutableArray<NSDictionary *> *_outputList, *_inputList;  /* id, name */
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

/* The alert sounds: the system's and the user's, by name. */
- (NSArray<NSDictionary *> *)alertSounds
{
    NSMutableArray *sounds = [NSMutableArray array];
    NSArray *dirs = @[ @[ @"/System/Library/Sounds", [self text:@"Built-in"] ],
                       @[ [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Sounds"], [self text:@"Custom"] ] ];
    for (NSArray *dir in dirs) {
        NSArray *names = [[[NSFileManager defaultManager] contentsOfDirectoryAtPath:dir[0] error:NULL]
            sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
        for (NSString *name in names) {
            NSString *ext = [[name pathExtension] lowercaseString];
            if (![@[ @"aiff", @"aif", @"wav", @"caf" ] containsObject:ext])
                continue;
            [sounds addObject:@{ @"name": [name stringByDeletingPathExtension],
                                 @"path": [dir[0] stringByAppendingPathComponent:name], @"type": dir[1] }];
        }
    }
    return sounds;
}

/* The devices with streams in a scope (output or input), by name. */
- (NSMutableArray<NSDictionary *> *)devicesInScope:(AudioObjectPropertyScope)scope
{
    AudioObjectPropertyAddress a = address(kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal);
    UInt32 size = 0;
    AudioDeviceID ids[16];
    NSMutableArray *list = [NSMutableArray array];

    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &a, 0, NULL, &size) == noErr && size > 0) {
        if (size > sizeof(ids))
            size = sizeof(ids);
        AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, ids);
    } else
        size = 0;
    for (UInt32 i = 0; i < size / sizeof(AudioDeviceID); i++) {
        AudioObjectPropertyAddress st = address(kAudioDevicePropertyStreams, scope);
        UInt32 streams = 0;
        if (AudioObjectGetPropertyDataSize(ids[i], &st, 0, NULL, &streams) != noErr || streams == 0)
            continue;
        AudioObjectPropertyAddress n = address(kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal);
        CFStringRef name = NULL;
        UInt32 nameSize = sizeof(name);
        if (AudioObjectGetPropertyData(ids[i], &n, 0, NULL, &nameSize, &name) != noErr || name == NULL)
            continue;
        [list addObject:@{ @"id": @(ids[i]), @"name": (__bridge_transfer NSString *)name }];
    }
    return list;
}

- (AudioDeviceID)defaultDevice:(AudioObjectPropertySelector)which
{
    AudioObjectPropertyAddress a = address(which, kAudioObjectPropertyScopeGlobal);
    AudioDeviceID current = kAudioObjectUnknown;
    UInt32 size = sizeof(current);
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, &current);
    return current;
}

- (NSTableView *)table:(NSArray<NSArray *> *)columns frame:(NSRect)frame in:(NSView *)view
{
    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:frame];
    [scroll setBorderType:NSBezelBorder];
    [scroll setHasVerticalScroller:YES];
    NSTableView *table = [[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]];
    for (NSArray *c in columns) {
        NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:c[0]];
        [[column headerCell] setStringValue:[self text:c[1]]];
        [column setWidth:[c[2] doubleValue]];
        [column setEditable:NO];
        [table addTableColumn:column];
    }
    [table setDataSource:self];
    [table setDelegate:self];
    [scroll setDocumentView:table];
    [view addSubview:scroll];
    return table;
}

- (NSView *)loadMainView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth, kHeight)];
    NSTabView *tabs = [[NSTabView alloc] initWithFrame:NSMakeRect(20, 96, kWidth - 40, 314)];

    /* Sound Effects. */
    NSTabViewItem *effects = [[NSTabViewItem alloc] initWithIdentifier:@"effects"];
    [effects setLabel:[self text:@"Sound Effects"]];
    NSView *e = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth - 60, 270)];
    [e addSubview:label([self text:@"Select an alert sound:"], NSMakeRect(10, 244, 300, 18), 13, NSLeftTextAlignment)];
    _sounds = [self alertSounds];
    _alerts = [self table:@[ @[ @"name", @"Name", @360 ], @[ @"type", @"Type", @200 ] ]
                    frame:NSMakeRect(10, 120, kWidth - 80, 120) in:e];
    [e addSubview:label([self text:@"Alert volume:"], NSMakeRect(10, 86, 180, 20), 13, NSRightTextAlignment)];
    _alertVolume = [[NSSlider alloc] initWithFrame:NSMakeRect(200, 84, 240, 24)];
    [_alertVolume setMinValue:0];
    [_alertVolume setMaxValue:1];
    [_alertVolume setContinuous:NO];
    [_alertVolume setTarget:self];
    [_alertVolume setAction:@selector(alertVolumeChanged:)];
    [e addSubview:_alertVolume];
    _uiSounds = [[NSButton alloc] initWithFrame:NSMakeRect(196, 50, 400, 20)];
    [_uiSounds setButtonType:NSButtonTypeSwitch];
    [_uiSounds setTitle:[self text:@"Play user interface sound effects"]];
    [_uiSounds setTarget:self];
    [_uiSounds setAction:@selector(effectsChanged:)];
    [e addSubview:_uiSounds];
    _feedback = [[NSButton alloc] initWithFrame:NSMakeRect(196, 26, 400, 20)];
    [_feedback setButtonType:NSButtonTypeSwitch];
    [_feedback setTitle:[self text:@"Play feedback when volume is changed"]];
    [_feedback setTarget:self];
    [_feedback setAction:@selector(effectsChanged:)];
    [e addSubview:_feedback];
    [effects setView:e];
    [tabs addTabViewItem:effects];

    /* Output and Input: the devices. */
    NSTabViewItem *output = [[NSTabViewItem alloc] initWithIdentifier:@"output"];
    [output setLabel:[self text:@"Output"]];
    NSView *o = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth - 60, 270)];
    [o addSubview:label([self text:@"Select a device for sound output:"], NSMakeRect(10, 244, 400, 18), 13, NSLeftTextAlignment)];
    _outputs = [self table:@[ @[ @"name", @"Name", @560 ] ] frame:NSMakeRect(10, 60, kWidth - 80, 180) in:o];
    _none = label([self text:@"No output device was found."], NSMakeRect(10, 30, kWidth - 80, 20), 12, NSCenterTextAlignment);
    [o addSubview:_none];
    [output setView:o];
    [tabs addTabViewItem:output];

    NSTabViewItem *input = [[NSTabViewItem alloc] initWithIdentifier:@"input"];
    [input setLabel:[self text:@"Input"]];
    NSView *i = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kWidth - 60, 270)];
    [i addSubview:label([self text:@"Select a device for sound input:"], NSMakeRect(10, 244, 400, 18), 13, NSLeftTextAlignment)];
    _inputs = [self table:@[ @[ @"name", @"Name", @560 ] ] frame:NSMakeRect(10, 60, kWidth - 80, 180) in:i];
    [input setView:i];
    [tabs addTabViewItem:input];
    [view addSubview:tabs];

    /* Below the tabs: the output volume, Mute, the menu bar. The output
       device is chosen in Output; this popup is kept hidden for it. */
    _device = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [view addSubview:label([self text:@"Output volume:"], NSMakeRect(40, 58, 180, 20), 13, NSRightTextAlignment)];
    _volume = [[NSSlider alloc] initWithFrame:NSMakeRect(230, 56, 240, 24)];
    [_volume setMinValue:0];
    [_volume setMaxValue:1];
    [_volume setContinuous:YES];
    [_volume setTarget:self];
    [_volume setAction:@selector(volumeChanged:)];
    [view addSubview:_volume];
    _mute = [[NSButton alloc] initWithFrame:NSMakeRect(490, 58, 120, 20)];
    [_mute setButtonType:NSButtonTypeSwitch];
    [_mute setTitle:[self text:@"Mute"]];
    [_mute setTarget:self];
    [_mute setAction:@selector(muteChanged:)];
    [view addSubview:_mute];

    /* As on the reference: the Volume Menu Extra in the menu bar or not. */
    NSButton *show = [[NSButton alloc] initWithFrame:NSMakeRect(226, 20, 340, 22)];
    [show setButtonType:NSButtonTypeSwitch];
    [show setTitle:[self text:@"Show volume in menu bar"]];
    [show setState:MenuExtraIsShown(@"Volume") ? NSOnState : NSOffState];
    [show setTarget:self];
    [show setAction:@selector(showInMenuBar:)];
    [view addSubview:show];

    [self loadDevices];
    [self reloadDeviceLists];
    [self showEffects];
    [self showDevice];
    [self setMainView:view];
    return view;
}

/* ---- Sound Effects -------------------------------------------------------- */

static double alertSlider(double volume)
{
    return volume <= 0 ? 0 : 1 + log(volume);
}

- (void)showEffects
{
    NSString *path = (__bridge_transfer NSString *)copyGlobal(CFSTR("com.apple.sound.beep.sound"));
    NSUInteger row = 0;
    for (NSUInteger k = 0; k < [_sounds count]; k++)
        if ([path isKindOfClass:[NSString class]] ? [_sounds[k][@"path"] isEqualToString:path]
                                                  : [_sounds[k][@"name"] isEqualToString:@"Chime"])
            row = k;
    [_alerts reloadData];
    if ([_sounds count] > 0)
        [_alerts selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];
    NSNumber *volume = (__bridge_transfer NSNumber *)copyGlobal(CFSTR("com.apple.sound.beep.volume"));
    [_alertVolume setDoubleValue:alertSlider([volume isKindOfClass:[NSNumber class]] ? [volume doubleValue] : 1)];
    [_uiSounds setState:globalFlag(CFSTR("com.apple.sound.uiaudio.enabled"), YES) ? NSOnState : NSOffState];
    [_feedback setState:globalFlag(CFSTR("com.apple.sound.beep.feedback"), NO) ? NSOnState : NSOffState];
}

- (void)alertVolumeChanged:(id)sender
{
    double s = [_alertVolume doubleValue];
    float volume = s <= 0.001 ? 0 : (float)exp(s - 1);
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberFloatType, &volume);
    setGlobal(CFSTR("com.apple.sound.beep.volume"), n);
    CFRelease(n);
    AudioServicesPlayAlertSound(kSystemSoundID_UserPreferredAlert);
    os_log_info(OS_LOG_DEFAULT, "sound alert volume %.4f", volume);
}

- (void)effectsChanged:(NSButton *)sender
{
    CFStringRef key = sender == _uiSounds ? CFSTR("com.apple.sound.uiaudio.enabled") : CFSTR("com.apple.sound.beep.feedback");
    int on = [sender state] == NSOnState;
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberIntType, &on);
    setGlobal(key, n);
    CFRelease(n);
    os_log_info(OS_LOG_DEFAULT, "sound %{public}s %d", [(__bridge NSString *)key UTF8String], on);
}

/* ---- The tables ----------------------------------------------------------- */

- (void)reloadDeviceLists
{
    _outputList = [self devicesInScope:kAudioObjectPropertyScopeOutput];
    _inputList = [self devicesInScope:kAudioObjectPropertyScopeInput];
    [_outputs reloadData];
    [_inputs reloadData];
    AudioDeviceID out = [self defaultDevice:kAudioHardwarePropertyDefaultOutputDevice];
    AudioDeviceID in = [self defaultDevice:kAudioHardwarePropertyDefaultInputDevice];
    for (NSUInteger k = 0; k < [_outputList count]; k++)
        if ([_outputList[k][@"id"] unsignedIntValue] == out)
            [_outputs selectRowIndexes:[NSIndexSet indexSetWithIndex:k] byExtendingSelection:NO];
    for (NSUInteger k = 0; k < [_inputList count]; k++)
        if ([_inputList[k][@"id"] unsignedIntValue] == in)
            [_inputs selectRowIndexes:[NSIndexSet indexSetWithIndex:k] byExtendingSelection:NO];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)table
{
    if (table == _alerts)
        return (NSInteger)[_sounds count];
    return (NSInteger)[(table == _outputs ? _outputList : _inputList) count];
}

- (id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    NSArray *list = table == _alerts ? _sounds : table == _outputs ? _outputList : _inputList;
    return list[(NSUInteger)row][[column identifier]];
}

- (void)tableViewSelectionDidChange:(NSNotification *)note
{
    NSTableView *table = [note object];
    NSInteger row = [table selectedRow];
    if (row < 0)
        return;
    if (table == _alerts) {
        NSString *path = _sounds[(NSUInteger)row][@"path"];
        NSString *current = (__bridge_transfer NSString *)copyGlobal(CFSTR("com.apple.sound.beep.sound"));
        if ([current isKindOfClass:[NSString class]] && [current isEqualToString:path])
            return;
        setGlobal(CFSTR("com.apple.sound.beep.sound"), (__bridge CFStringRef)path);
        /* The sound chosen, heard (at the alert volume). */
        AudioServicesPlayAlertSound(kSystemSoundID_UserPreferredAlert);
        os_log_info(OS_LOG_DEFAULT, "sound alert %{public}s", [path UTF8String]);
        return;
    }
    AudioDeviceID device = [(table == _outputs ? _outputList : _inputList)[(NSUInteger)row][@"id"] unsignedIntValue];
    AudioObjectPropertyAddress a = address(table == _outputs ? kAudioHardwarePropertyDefaultOutputDevice
                                                             : kAudioHardwarePropertyDefaultInputDevice,
                                           kAudioObjectPropertyScopeGlobal);
    if ([self defaultDevice:a.mSelector] != device) {
        AudioObjectSetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, sizeof(device), &device);
        if (table == _outputs)
            [self loadDevices];
        [self showDevice];
    }
}

- (void)showInMenuBar:(NSButton *)sender
{
    MenuExtraSetShown(@"Volume", [sender state] == NSOnState);
    os_log_info(OS_LOG_DEFAULT, "volume menu extra shown=%d", [sender state] == NSOnState);
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
