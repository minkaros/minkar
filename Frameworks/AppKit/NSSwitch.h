/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSSwitch (API 19): an on/off switch, a pill whose knob slides to the
 * side of its state.
 */

#import <AppKit/NSControl.h>
#import <AppKit/NSCell.h>

@interface NSSwitch : NSControl {
    NSControlStateValue _switchState;
    BOOL _tracking;
}

/* NSControlStateValueOn or NSControlStateValueOff. */
@property NSControlStateValue state;

@end
