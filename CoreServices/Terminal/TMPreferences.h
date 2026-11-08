/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Terminal's settings, kept in its defaults (org.minkar.Terminal). Each
 * change is posted as TMPreferencesDidChangeNotification, and the open
 * windows take it at once.
 */

#import <Cocoa/Cocoa.h>

extern NSString *const TMPreferencesDidChangeNotification;

/* The defaults' keys. */
extern NSString *const TMFontSizeKey;           /* number, 13 */
extern NSString *const TMColumnsKey;            /* number, 80 */
extern NSString *const TMRowsKey;               /* number, 24 */
extern NSString *const TMScrollbackLinesKey;    /* number, 10000 */
extern NSString *const TMShellKey;              /* path; empty: the user's login shell */
extern NSString *const TMShellExitActionKey;    /* TMShellExitAction */
extern NSString *const TMBellKey;               /* TMBell */
extern NSString *const TMSchemeKey;             /* "Light", "Dark" or "Custom" */
extern NSString *const TMBackgroundColorKey;    /* "#RRGGBB" */
extern NSString *const TMTextColorKey;
extern NSString *const TMCursorColorKey;        /* empty: the accent color */
extern NSString *const TMPaletteKey;            /* sixteen "#RRGGBB" */

typedef NS_ENUM(NSInteger, TMShellExitAction) {
    TMShellExitClose = 0,
    TMShellExitCloseIfClean = 1,    /* only when it exited with status 0 */
    TMShellExitKeepOpen = 2,
};

typedef NS_ENUM(NSInteger, TMBell) {
    TMBellSound = 0,
    TMBellFlash = 1,
    TMBellNone = 2,
};

@interface TMPreferences : NSObject
+ (TMPreferences *)sharedPreferences;
/* The names of the built-in schemes. */
+ (NSArray<NSString *> *)schemeNames;

@property (nonatomic) CGFloat fontSize;          /* 8 to 36 */
@property (nonatomic) int columns;               /* 20 to 400 */
@property (nonatomic) int rows;                  /* 5 to 200 */
@property (nonatomic) NSInteger scrollbackLines; /* 0 to 100000 */
@property (nonatomic, copy) NSString *shell;     /* empty: the login shell */
@property (nonatomic) TMShellExitAction shellExitAction;
@property (nonatomic) TMBell bell;

@property (nonatomic, copy) NSString *scheme;
@property (nonatomic, strong) NSColor *backgroundColor;
@property (nonatomic, strong) NSColor *textColor;
@property (nonatomic, strong) NSColor *cursorColor;     /* nil: the accent color */
- (NSColor *)paletteColorAtIndex:(NSUInteger)index;     /* 0–15 */
- (void)setPaletteColor:(NSColor *)color atIndex:(NSUInteger)index;
/* A built-in scheme's colors, all at once. */
- (void)applyScheme:(NSString *)name;

/* Back to the values Terminal ships with. */
- (void)restoreDefaults;
@end

/* "#RRGGBB" and back. */
NSString *TMHexOfColor(NSColor *color);
NSColor *TMColorOfHex(NSString *hex);
