/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSVisualEffectView (API 19): a view that shows a material (sidebar,
 * menu, title bar...) behind its subviews. The compositor has no blur
 * yet: each material draws as the translucent color of its Atlas
 * surface.
 */

#import <AppKit/NSView.h>
#import <AppKit/NSCell.h>

@class NSImage;

typedef NS_ENUM(NSInteger, NSVisualEffectMaterial) {
    NSVisualEffectMaterialTitlebar = 3,
    NSVisualEffectMaterialSelection = 4,
    NSVisualEffectMaterialMenu = 5,
    NSVisualEffectMaterialPopover = 6,
    NSVisualEffectMaterialSidebar = 7,
    NSVisualEffectMaterialHeaderView = 10,
    NSVisualEffectMaterialSheet = 11,
    NSVisualEffectMaterialWindowBackground = 12,
    NSVisualEffectMaterialHUDWindow = 13,
    NSVisualEffectMaterialFullScreenUI = 15,
    NSVisualEffectMaterialToolTip = 17,
    NSVisualEffectMaterialContentBackground = 18,
    NSVisualEffectMaterialUnderWindowBackground = 21,
    NSVisualEffectMaterialUnderPageBackground = 22,
    NSVisualEffectMaterialAppearanceBased = 0,
    NSVisualEffectMaterialLight = 1,
    NSVisualEffectMaterialDark = 2,
    NSVisualEffectMaterialMediumLight = 8,
    NSVisualEffectMaterialUltraDark = 9,
};

typedef NS_ENUM(NSInteger, NSVisualEffectBlendingMode) {
    NSVisualEffectBlendingModeBehindWindow = 0,
    NSVisualEffectBlendingModeWithinWindow = 1,
};

typedef NS_ENUM(NSInteger, NSVisualEffectState) {
    NSVisualEffectStateFollowsWindowActiveState = 0,
    NSVisualEffectStateActive = 1,
    NSVisualEffectStateInactive = 2,
};

@interface NSVisualEffectView : NSView {
    NSVisualEffectMaterial _material;
    NSVisualEffectBlendingMode _blendingMode;
    NSVisualEffectState _state;
    NSImage *_maskImage;
    BOOL _emphasized;
}

@property NSVisualEffectMaterial material;
@property (readonly) NSBackgroundStyle interiorBackgroundStyle;
@property NSVisualEffectBlendingMode blendingMode;
@property NSVisualEffectState state;
@property (strong) NSImage *maskImage;
@property (getter=isEmphasized) BOOL emphasized;

@end
