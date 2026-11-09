/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSTouchBar and its items (API 19). No Minkar machine has a Touch Bar:
 * applications make their bars and items as they would, and nothing shows
 * them.
 */

#import <Foundation/Foundation.h>
#import <AppKit/AppKitExport.h>
#import <AppKit/NSResponder.h>
#import <AppKit/NSApplication.h>

@class NSTouchBar, NSView, NSViewController, NSImage, NSString;

typedef NSString *NSTouchBarItemIdentifier;
typedef NSString *NSTouchBarCustomizationIdentifier;
typedef float NSTouchBarItemPriority;

APPKIT_EXPORT const NSTouchBarItemPriority NSTouchBarItemPriorityHigh;
APPKIT_EXPORT const NSTouchBarItemPriority NSTouchBarItemPriorityNormal;
APPKIT_EXPORT const NSTouchBarItemPriority NSTouchBarItemPriorityLow;

APPKIT_EXPORT NSTouchBarItemIdentifier const NSTouchBarItemIdentifierFixedSpaceSmall;
APPKIT_EXPORT NSTouchBarItemIdentifier const NSTouchBarItemIdentifierFixedSpaceLarge;
APPKIT_EXPORT NSTouchBarItemIdentifier const NSTouchBarItemIdentifierFlexibleSpace;
APPKIT_EXPORT NSTouchBarItemIdentifier const NSTouchBarItemIdentifierOtherItemsProxy;
APPKIT_EXPORT NSTouchBarItemIdentifier const NSTouchBarItemIdentifierCharacterPicker;
APPKIT_EXPORT NSTouchBarItemIdentifier const NSTouchBarItemIdentifierTextColorPicker;
APPKIT_EXPORT NSTouchBarItemIdentifier const NSTouchBarItemIdentifierTextStyle;
APPKIT_EXPORT NSTouchBarItemIdentifier const NSTouchBarItemIdentifierTextAlignment;
APPKIT_EXPORT NSTouchBarItemIdentifier const NSTouchBarItemIdentifierTextList;
APPKIT_EXPORT NSTouchBarItemIdentifier const NSTouchBarItemIdentifierTextFormat;
APPKIT_EXPORT NSTouchBarItemIdentifier const NSTouchBarItemIdentifierCandidateList;

@interface NSTouchBarItem : NSObject <NSCoding> {
    NSTouchBarItemIdentifier _identifier;
    NSTouchBarItemPriority _visibilityPriority;
}
- (instancetype)initWithIdentifier:(NSTouchBarItemIdentifier)identifier;
@property (readonly, copy) NSTouchBarItemIdentifier identifier;
@property NSTouchBarItemPriority visibilityPriority;
@property (readonly, strong) NSView *view;
@property (readonly, strong) NSViewController *viewController;
@property (readonly, copy) NSString *customizationLabel;
/* Never: there is no Touch Bar to show it on. */
@property (readonly, getter=isVisible) BOOL visible;
@end

@interface NSCustomTouchBarItem : NSTouchBarItem {
    NSView *_view;
    NSViewController *_viewController;
    NSString *_customizationLabel;
}
@property (readwrite, strong) NSView *view;
@property (readwrite, strong) NSViewController *viewController;
@property (readwrite, copy) NSString *customizationLabel;
@end

@interface NSGroupTouchBarItem : NSTouchBarItem {
    NSTouchBar *_groupTouchBar;
    NSString *_customizationLabel;
}
+ (instancetype)groupItemWithIdentifier:(NSTouchBarItemIdentifier)identifier items:(NSArray<NSTouchBarItem *> *)items;
@property (strong) NSTouchBar *groupTouchBar;
@property (readwrite, copy) NSString *customizationLabel;
@end

@protocol NSTouchBarDelegate <NSObject>
@optional
- (NSTouchBarItem *)touchBar:(NSTouchBar *)touchBar makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier;
@end

@interface NSTouchBar : NSObject <NSCoding> {
    NSTouchBarCustomizationIdentifier _customizationIdentifier;
    NSArray *_customizationAllowedItemIdentifiers;
    NSArray *_customizationRequiredItemIdentifiers;
    NSArray *_defaultItemIdentifiers;
    NSTouchBarItemIdentifier _principalItemIdentifier;
    NSTouchBarItemIdentifier _escapeKeyReplacementItemIdentifier;
    NSSet *_templateItems;
    id _delegate;
    NSMutableDictionary *_items;
}
@property (copy) NSTouchBarCustomizationIdentifier customizationIdentifier;
@property (copy) NSArray<NSTouchBarItemIdentifier> *customizationAllowedItemIdentifiers;
@property (copy) NSArray<NSTouchBarItemIdentifier> *customizationRequiredItemIdentifiers;
@property (copy) NSArray<NSTouchBarItemIdentifier> *defaultItemIdentifiers;
@property (readonly, copy) NSArray<NSTouchBarItemIdentifier> *itemIdentifiers;
@property (copy) NSTouchBarItemIdentifier principalItemIdentifier;
@property (copy) NSTouchBarItemIdentifier escapeKeyReplacementItemIdentifier;
@property (copy) NSSet<NSTouchBarItem *> *templateItems;
@property (weak) id<NSTouchBarDelegate> delegate;
/* The item for an identifier: a template item's, or the delegate's. */
- (NSTouchBarItem *)itemForIdentifier:(NSTouchBarItemIdentifier)identifier;
@property (readonly, getter=isVisible) BOOL visible;
@property (class, readonly, getter=isAutomaticCustomizeTouchBarMenuItemEnabled) BOOL automaticCustomizeTouchBarMenuItemEnabled;
+ (void)setAutomaticCustomizeTouchBarMenuItemEnabled:(BOOL)flag;
@end

@protocol NSTouchBarProvider <NSObject>
@property (readonly, strong) NSTouchBar *touchBar;
@end

@interface NSResponder (NSTouchBarProvider) <NSTouchBarProvider>
@property (strong) NSTouchBar *touchBar;
- (NSTouchBar *)makeTouchBar;
@end

@interface NSApplication (NSTouchBarCustomization)
@property (getter=isAutomaticCustomizeTouchBarMenuItemEnabled) BOOL automaticCustomizeTouchBarMenuItemEnabled;
- (IBAction)toggleTouchBarCustomizationPalette:(id)sender;
@end
