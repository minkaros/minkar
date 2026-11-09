/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSAppearance (API 19): the look views draw with. Atlas draws its light
 * look (Aqua); the other appearances can be asked for and set, and views
 * report the one they have.
 */

#import <Foundation/Foundation.h>
#import <AppKit/AppKitExport.h>

typedef NSString *NSAppearanceName;

APPKIT_EXPORT NSAppearanceName const NSAppearanceNameAqua;
APPKIT_EXPORT NSAppearanceName const NSAppearanceNameDarkAqua;
APPKIT_EXPORT NSAppearanceName const NSAppearanceNameLightContent;
APPKIT_EXPORT NSAppearanceName const NSAppearanceNameVibrantDark;
APPKIT_EXPORT NSAppearanceName const NSAppearanceNameVibrantLight;
APPKIT_EXPORT NSAppearanceName const NSAppearanceNameAccessibilityHighContrastAqua;
APPKIT_EXPORT NSAppearanceName const NSAppearanceNameAccessibilityHighContrastDarkAqua;
APPKIT_EXPORT NSAppearanceName const NSAppearanceNameAccessibilityHighContrastVibrantLight;
APPKIT_EXPORT NSAppearanceName const NSAppearanceNameAccessibilityHighContrastVibrantDark;

@interface NSAppearance : NSObject <NSSecureCoding> {
    NSAppearanceName _name;
}

/* nil for a name that is not one of the system's. */
+ (NSAppearance *)appearanceNamed:(NSAppearanceName)name;
- (instancetype)initWithAppearanceNamed:(NSAppearanceName)name bundle:(NSBundle *)bundle;

/* The appearance drawing uses now (the application's, or the one set). */
@property (class, strong) NSAppearance *currentAppearance;

@property (readonly, copy) NSAppearanceName name;
@property (readonly) BOOL allowsVibrancy;
/* Of names, the one that best matches this appearance, or nil. */
- (NSAppearanceName)bestMatchFromAppearancesWithNames:(NSArray<NSAppearanceName> *)appearances;

@end

@protocol NSAppearanceCustomization <NSObject>
@property (strong) NSAppearance *appearance;
@property (readonly, strong) NSAppearance *effectiveAppearance;
@end
