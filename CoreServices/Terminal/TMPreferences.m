/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "TMPreferences.h"

NSString *const TMPreferencesDidChangeNotification = @"TMPreferencesDidChangeNotification";

NSString *const TMFontSizeKey = @"FontSize";
NSString *const TMColumnsKey = @"Columns";
NSString *const TMRowsKey = @"Rows";
NSString *const TMScrollbackLinesKey = @"ScrollbackLines";
NSString *const TMShellKey = @"Shell";
NSString *const TMShellExitActionKey = @"ShellExitAction";
NSString *const TMBellKey = @"Bell";
NSString *const TMSchemeKey = @"Scheme";
NSString *const TMBackgroundColorKey = @"BackgroundColor";
NSString *const TMTextColorKey = @"TextColor";
NSString *const TMCursorColorKey = @"CursorColor";
NSString *const TMPaletteKey = @"Palette";

/* Atlas's two schemes: background, text, then the sixteen ANSI colors. */
static NSDictionary<NSString *, NSArray<NSString *> *> *schemes(void)
{
    return @{
        @"Light" : @[ @"#FFFFFF", @"#1E1E1E",
            @"#000000", @"#B42318", @"#2E7D32", @"#A06A00", @"#1F5FBF", @"#8E3BA8", @"#0F7C86", @"#BFBFBF",
            @"#6B6B6B", @"#E0473A", @"#43A047", @"#C9A000", @"#3D82E6", @"#B05CCB", @"#22A3AE", @"#FFFFFF" ],
        @"Dark" : @[ @"#1C2023", @"#D8DEE1",
            @"#2B3034", @"#E5605A", @"#7CC36E", @"#E3C062", @"#5D9CEC", @"#C17BD6", @"#4CC3C9", @"#D0D6D9",
            @"#6A7378", @"#FF8279", @"#9BDB8C", @"#F5D67E", @"#82B6F5", @"#D99BEA", @"#78DDE1", @"#FFFFFF" ],
    };
}

NSString *TMHexOfColor(NSColor *color)
{
    NSColor *rgb = [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];

    if (rgb == nil)
        return @"#000000";
    return [NSString stringWithFormat:@"#%02X%02X%02X", (int)lround([rgb redComponent] * 255),
        (int)lround([rgb greenComponent] * 255), (int)lround([rgb blueComponent] * 255)];
}

NSColor *TMColorOfHex(NSString *hex)
{
    unsigned value = 0;

    if (![hex isKindOfClass:[NSString class]] || [hex length] != 7 || ![hex hasPrefix:@"#"])
        return nil;
    if (![[NSScanner scannerWithString:[hex substringFromIndex:1]] scanHexInt:&value])
        return nil;
    return [NSColor colorWithCalibratedRed:((value >> 16) & 0xff) / 255.0 green:((value >> 8) & 0xff) / 255.0
        blue:(value & 0xff) / 255.0 alpha:1];
}

@implementation TMPreferences {
    NSUserDefaults *_defaults;
}

+ (TMPreferences *)sharedPreferences
{
    static TMPreferences *shared;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        shared = [[TMPreferences alloc] init];
    });
    return shared;
}

+ (NSArray<NSString *> *)schemeNames
{
    return @[ @"Light", @"Dark" ];
}

+ (NSDictionary *)factoryValues
{
    NSArray *light = schemes()[@"Light"];

    return @{
        TMFontSizeKey : @13,
        TMColumnsKey : @80,
        TMRowsKey : @24,
        TMScrollbackLinesKey : @10000,
        TMShellKey : @"",
        TMShellExitActionKey : @(TMShellExitClose),
        TMBellKey : @(TMBellSound),
        TMSchemeKey : @"Light",
        TMBackgroundColorKey : light[0],
        TMTextColorKey : light[1],
        TMCursorColorKey : @"",
        TMPaletteKey : [light subarrayWithRange:NSMakeRange(2, 16)],
    };
}

- (instancetype)init
{
    if ((self = [super init]) != nil) {
        _defaults = [NSUserDefaults standardUserDefaults];
        [_defaults registerDefaults:[TMPreferences factoryValues]];
    }
    return self;
}

- (void)changed
{
    [[NSNotificationCenter defaultCenter] postNotificationName:TMPreferencesDidChangeNotification object:self];
}

- (void)setValue:(id)value forPreference:(NSString *)key
{
    if ([[_defaults objectForKey:key] isEqual:value])
        return;
    [_defaults setObject:value forKey:key];
    [self changed];
}

static double clamp(double value, double low, double high)
{
    return value < low ? low : (value > high ? high : value);
}

- (CGFloat)fontSize
{
    return clamp([_defaults doubleForKey:TMFontSizeKey], 8, 36);
}

- (void)setFontSize:(CGFloat)size
{
    [self setValue:@(clamp(size, 8, 36)) forPreference:TMFontSizeKey];
}

- (int)columns
{
    return (int)clamp([_defaults integerForKey:TMColumnsKey], 20, 400);
}

- (void)setColumns:(int)columns
{
    [self setValue:@((int)clamp(columns, 20, 400)) forPreference:TMColumnsKey];
}

- (int)rows
{
    return (int)clamp([_defaults integerForKey:TMRowsKey], 5, 200);
}

- (void)setRows:(int)rows
{
    [self setValue:@((int)clamp(rows, 5, 200)) forPreference:TMRowsKey];
}

- (NSInteger)scrollbackLines
{
    return (NSInteger)clamp([_defaults integerForKey:TMScrollbackLinesKey], 0, 100000);
}

- (void)setScrollbackLines:(NSInteger)lines
{
    [self setValue:@((NSInteger)clamp(lines, 0, 100000)) forPreference:TMScrollbackLinesKey];
}

- (NSString *)shell
{
    return [_defaults stringForKey:TMShellKey] ?: @"";
}

- (void)setShell:(NSString *)shell
{
    [self setValue:[shell stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] ?: @""
        forPreference:TMShellKey];
}

- (TMShellExitAction)shellExitAction
{
    return (TMShellExitAction)clamp([_defaults integerForKey:TMShellExitActionKey], 0, 2);
}

- (void)setShellExitAction:(TMShellExitAction)action
{
    [self setValue:@(action) forPreference:TMShellExitActionKey];
}

- (TMBell)bell
{
    return (TMBell)clamp([_defaults integerForKey:TMBellKey], 0, 2);
}

- (void)setBell:(TMBell)bell
{
    [self setValue:@(bell) forPreference:TMBellKey];
}

- (NSString *)scheme
{
    return [_defaults stringForKey:TMSchemeKey] ?: @"Light";
}

- (void)setScheme:(NSString *)scheme
{
    [self setValue:scheme forPreference:TMSchemeKey];
}

- (NSColor *)colorForKey:(NSString *)key
{
    return TMColorOfHex([_defaults stringForKey:key]) ?:
        TMColorOfHex([TMPreferences factoryValues][key]);
}

/* A color picked by hand makes the scheme "Custom" (the cursor's is apart). */
- (void)setColor:(NSColor *)color forKey:(NSString *)key
{
    NSString *hex = color ? TMHexOfColor(color) : @"";

    if ([[_defaults stringForKey:key] isEqual:hex])
        return;
    [_defaults setObject:hex forKey:key];
    if (![key isEqual:TMCursorColorKey])
        [_defaults setObject:@"Custom" forKey:TMSchemeKey];
    [self changed];
}

- (NSColor *)backgroundColor
{
    return [self colorForKey:TMBackgroundColorKey];
}

- (void)setBackgroundColor:(NSColor *)color
{
    [self setColor:color forKey:TMBackgroundColorKey];
}

- (NSColor *)textColor
{
    return [self colorForKey:TMTextColorKey];
}

- (void)setTextColor:(NSColor *)color
{
    [self setColor:color forKey:TMTextColorKey];
}

- (NSColor *)cursorColor
{
    return TMColorOfHex([_defaults stringForKey:TMCursorColorKey]);
}

- (void)setCursorColor:(NSColor *)color
{
    [self setColor:color forKey:TMCursorColorKey];
}

- (NSArray<NSString *> *)palette
{
    NSArray *palette = [_defaults arrayForKey:TMPaletteKey];

    if ([palette count] != 16)
        palette = [TMPreferences factoryValues][TMPaletteKey];
    return palette;
}

- (NSColor *)paletteColorAtIndex:(NSUInteger)index
{
    if (index >= 16)
        return nil;
    return TMColorOfHex([self palette][index]) ?: TMColorOfHex([TMPreferences factoryValues][TMPaletteKey][index]);
}

- (void)setPaletteColor:(NSColor *)color atIndex:(NSUInteger)index
{
    NSMutableArray *palette = [[self palette] mutableCopy];
    NSString *hex = TMHexOfColor(color);

    if (index >= 16 || [palette[index] isEqual:hex])
        return;
    palette[index] = hex;
    [_defaults setObject:palette forKey:TMPaletteKey];
    [_defaults setObject:@"Custom" forKey:TMSchemeKey];
    [self changed];
}

- (void)applyScheme:(NSString *)name
{
    NSArray *colors = schemes()[name];

    if (colors == nil)
        return;
    [_defaults setObject:name forKey:TMSchemeKey];
    [_defaults setObject:colors[0] forKey:TMBackgroundColorKey];
    [_defaults setObject:colors[1] forKey:TMTextColorKey];
    [_defaults setObject:[colors subarrayWithRange:NSMakeRange(2, 16)] forKey:TMPaletteKey];
    [self changed];
}

- (void)restoreDefaults
{
    for (NSString *key in [TMPreferences factoryValues])
        [_defaults removeObjectForKey:key];
    [self changed];
}

@end
