/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/Foundation.h>
#import <AppKit/AppKitExport.h>

@class NSScreen, NSMenu, NSMenuItem, NSWindow, NSEvent, NSView, NSFont;

@protocol NSMenuDelegate;

/* Posted with the menu as the object; the add, remove and change ones
 * give the item's index under NSMenuItemIndex in the user info. */
APPKIT_EXPORT NSString *const NSMenuWillSendActionNotification;
APPKIT_EXPORT NSString *const NSMenuDidSendActionNotification;
APPKIT_EXPORT NSString *const NSMenuDidAddItemNotification;
APPKIT_EXPORT NSString *const NSMenuDidRemoveItemNotification;
APPKIT_EXPORT NSString *const NSMenuDidChangeItemNotification;
APPKIT_EXPORT NSString *const NSMenuDidBeginTrackingNotification;
APPKIT_EXPORT NSString *const NSMenuDidEndTrackingNotification;

typedef NS_OPTIONS(NSUInteger, NSMenuProperties) {
    NSMenuPropertyItemTitle = 1 << 0,
    NSMenuPropertyItemAttributedTitle = 1 << 1,
    NSMenuPropertyItemKeyEquivalent = 1 << 2,
    NSMenuPropertyItemImage = 1 << 3,
    NSMenuPropertyItemEnabled = 1 << 4,
    NSMenuPropertyItemAccessibilityDescription = 1 << 5,
};

@interface NSMenu : NSObject <NSCopying> {
    NSMenu *_supermenu;
    NSString *_title;
    NSString *_name;
    NSMutableArray *_itemArray;
    BOOL _autoenablesItems;
    id<NSMenuDelegate> _delegate;
    int _DBusItemID;
}

+ (void)popUpContextMenu:(NSMenu *)menu withEvent:(NSEvent *)event forView:(NSView *)view;
+ (void)popUpContextMenu:(NSMenu *)menu withEvent:(NSEvent *)event forView:(NSView *)view withFont:(NSFont *)font;
- (BOOL)popUpMenuPositioningItem:(NSMenuItem *)item atLocation:(NSPoint)location inView:(NSView *)view;

- initWithTitle:(NSString *)title;

- (NSMenu *)supermenu;
- (NSString *)title;
/* The menu bar's height for the application's main menu, else 0. */
- (CGFloat)menuBarHeight;
- (NSInteger)numberOfItems;
- (NSArray<NSMenuItem *> *)itemArray;
- (BOOL)autoenablesItems;

- (NSMenuItem *)itemAtIndex:(NSInteger)index;
- (NSMenuItem *)itemWithTag:(NSInteger)tag;
- (NSMenuItem *)itemWithTitle:(NSString *)title;

- (NSInteger)indexOfItem:(NSMenuItem *)item;
- (NSInteger)indexOfItemWithTag:(NSInteger)tag;
- (NSInteger)indexOfItemWithTitle:(NSString *)title;
- (NSInteger)indexOfItemWithRepresentedObject:object;
- (NSInteger)indexOfItemWithTarget:(id)target andAction:(SEL)action;
- (NSInteger)indexOfItemWithSubmenu:(NSMenu *)menu;

- (void)setSupermenu:(NSMenu *)value;
- (void)setTitle:(NSString *)title;
- (void)setAutoenablesItems:(BOOL)flag;

- (void)addItem:(NSMenuItem *)item;
- (NSMenuItem *)addItemWithTitle:(NSString *)title action:(SEL)action keyEquivalent:(NSString *)keyEquivalent;

- (void)removeAllItems; // private, don't use outside framework
- (void)removeItem:(NSMenuItem *)item;
- (void)removeItemAtIndex:(NSInteger)index;

- (void)insertItem:(NSMenuItem *)item atIndex:(NSInteger)index;
- (NSMenuItem *)insertItemWithTitle:(NSString *)title action:(SEL)action keyEquivalent:(NSString *)keyEquivalent atIndex:(NSInteger)index;

- (void)setSubmenu:(NSMenu *)submenu forItem:(NSMenuItem *)item;

- (void)update;

- (void)itemChanged:(NSMenuItem *)item;

- (BOOL)performKeyEquivalent:(NSEvent *)event;

- (void)setDelegate:(id<NSMenuDelegate>)object;
- (id<NSMenuDelegate>)delegate;


- (void)cancelTracking;
- (void)cancelTrackingWithoutAnimation;
- (void)performActionForItemAtIndex:(NSInteger)index;
- (void)submenuAction:(id)sender;

/* Minkar: the menu's font, the item it highlights, the properties a
   delegate's menuNeedsUpdate: should update (API 10.6), and the size it
   would have. */
@property (strong) NSFont *font;
@property (readonly, assign) NSMenuItem *highlightedItem;
@property (readonly) NSMenuProperties propertiesToUpdate;
@property (readonly) NSSize size;
@property CGFloat minimumWidth;
@property BOOL showsStateColumn;
@property BOOL allowsContextMenuPlugIns;

@end

@interface NSObject (NSMenu_validateItem)
- (BOOL)validateMenuItem:(NSMenuItem *)item;
@end

@protocol NSMenuDelegate <NSObject>

@optional

- (NSRect)confinementRectForMenu:(NSMenu *)menu onScreen:(NSScreen *)screen;
- (BOOL)menuHasKeyEquivalent:(NSMenu *)menu forEvent:(NSEvent *)event target:(id *)target action:(SEL *)action;
- (void)menuNeedsUpdate:(NSMenu *)menu;
- (void)menuWillOpen:(NSMenu *)menu;
- (void)menuDidClose:(NSMenu *)menu;
- (NSInteger)numberOfItemsInMenu:(NSMenu *)menu;
- (void)menu:(NSMenu *)menu willHighlightItem:(NSMenuItem *)item;
- (BOOL)menu:(NSMenu *)menu updateItem:(NSMenuItem *)item atIndex:(NSInteger)index shouldCancel:(BOOL)shouldCancel;

@end
