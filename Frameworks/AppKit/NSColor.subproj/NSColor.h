/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/Foundation.h>
#import <AppKit/AppKitExport.h>
#import <ApplicationServices/ApplicationServices.h>

@class NSImage, NSColorSpace;
@class NSPasteboard;

typedef NS_ENUM(NSInteger, NSColorType) {
    NSColorTypeComponentBased = 0,
    NSColorTypePattern = 1,
    NSColorTypeCatalog = 2,
};

/* Posted when the system's colors (the appearance's) or the control tint change. */
APPKIT_EXPORT NSString *const NSSystemColorsDidChangeNotification;
APPKIT_EXPORT NSString *const NSControlTintDidChangeNotification;

@interface NSColor : NSObject <NSCopying, NSCoding>

+ (NSColor *)highlightColor;
+ (NSColor *)shadowColor;
+ (NSColor *)gridColor;

+ (NSColor *)alternateSelectedControlColor;
+ (NSColor *)alternateSelectedControlTextColor;
+ (NSColor *)controlColor;
+ (NSColor *)secondarySelectedControlColor;
+ (NSColor *)selectedControlColor;
+ (NSColor *)controlTextColor;
+ (NSColor *)selectedControlTextColor;
+ (NSColor *)disabledControlTextColor;
+ (NSColor *)controlBackgroundColor;
+ (NSColor *)controlDarkShadowColor;
+ (NSColor *)controlHighlightColor;
+ (NSColor *)controlLightHighlightColor;
+ (NSColor *)controlShadowColor;
+ (NSArray *)controlAlternatingRowBackgroundColors;

+ (NSColor *)keyboardFocusIndicatorColor;
+ (NSColor *)controlAccentColor;

/* Colors of the system palette (System.clr), API 19 and before. */
+ (NSColor *)labelColor;
+ (NSColor *)secondaryLabelColor;
+ (NSColor *)tertiaryLabelColor;
+ (NSColor *)quaternaryLabelColor;
+ (NSColor *)placeholderTextColor;
+ (NSColor *)linkColor;
+ (NSColor *)findHighlightColor;
+ (NSColor *)underPageBackgroundColor;
+ (NSColor *)windowFrameTextColor;
+ (NSColor *)separatorColor;
+ (NSColor *)selectedContentBackgroundColor;
+ (NSColor *)unemphasizedSelectedContentBackgroundColor;
+ (NSColor *)unemphasizedSelectedTextColor;
+ (NSColor *)unemphasizedSelectedTextBackgroundColor;
+ (NSColor *)systemRedColor;
+ (NSColor *)systemOrangeColor;
+ (NSColor *)systemYellowColor;
+ (NSColor *)systemGreenColor;
+ (NSColor *)systemTealColor;
+ (NSColor *)systemBlueColor;
+ (NSColor *)systemIndigoColor;
+ (NSColor *)systemPurpleColor;
+ (NSColor *)systemPinkColor;
+ (NSColor *)systemBrownColor;
+ (NSColor *)systemGrayColor;

+ (NSColor *)textColor;
+ (NSColor *)textBackgroundColor;
+ (NSColor *)selectedTextColor;
+ (NSColor *)selectedTextBackgroundColor;

+ (NSColor *)headerColor;
+ (NSColor *)headerTextColor;

+ (NSColor *)scrollBarColor;
+ (NSColor *)knobColor;
+ (NSColor *)selectedKnobColor;

+ (NSColor *)windowBackgroundColor;
+ (NSColor *)windowFrameColor;

+ (NSColor *)selectedMenuItemColor;
+ (NSColor *)selectedMenuItemTextColor;

// private
+ (NSColor *)mainMenuBarColor;
+ (NSColor *)menuBackgroundColor;
+ (NSColor *)menuItemTextColor;

+ (NSColor *)clearColor;

+ (NSColor *)blackColor;
+ (NSColor *)blueColor;
+ (NSColor *)brownColor;
+ (NSColor *)cyanColor;
+ (NSColor *)darkGrayColor;
+ (NSColor *)grayColor;
+ (NSColor *)greenColor;
+ (NSColor *)lightGrayColor;
+ (NSColor *)magentaColor;
+ (NSColor *)orangeColor;
+ (NSColor *)purpleColor;
+ (NSColor *)redColor;
+ (NSColor *)whiteColor;
+ (NSColor *)yellowColor;

+ (NSColor *)colorWithDeviceWhite:(CGFloat)white alpha:(CGFloat)alpha;
+ (NSColor *)colorWithDeviceRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha;
+ (NSColor *)colorWithDeviceHue:(CGFloat)hue saturation:(CGFloat)saturation brightness:(CGFloat)brightness alpha:(CGFloat)alpha;
+ (NSColor *)colorWithDeviceCyan:(CGFloat)cyan magenta:(CGFloat)magenta yellow:(CGFloat)yellow black:(CGFloat)black alpha:(CGFloat)alpha;

+ (NSColor *)colorWithCalibratedWhite:(CGFloat)white alpha:(CGFloat)alpha;
+ (NSColor *)colorWithCalibratedRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha;
+ (NSColor *)colorWithCalibratedHue:(CGFloat)hue saturation:(CGFloat)saturation brightness:(CGFloat)brightness alpha:(CGFloat)alpha;

/* In sRGB (RGB and HSB), generic gray with a gamma of 2.2 (white), or the
   space given. */
+ (NSColor *)colorWithRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha;
+ (NSColor *)colorWithSRGBRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha;
+ (NSColor *)colorWithDisplayP3Red:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha;
+ (NSColor *)colorWithHue:(CGFloat)hue saturation:(CGFloat)saturation brightness:(CGFloat)brightness alpha:(CGFloat)alpha;
+ (NSColor *)colorWithColorSpace:(NSColorSpace *)space hue:(CGFloat)hue saturation:(CGFloat)saturation brightness:(CGFloat)brightness alpha:(CGFloat)alpha;
+ (NSColor *)colorWithWhite:(CGFloat)white alpha:(CGFloat)alpha;
+ (NSColor *)colorWithGenericGamma22White:(CGFloat)white alpha:(CGFloat)alpha;
+ (NSColor *)colorWithColorSpace:(NSColorSpace *)space components:(const CGFloat *)components count:(NSInteger)numberOfComponents;
+ (NSColor *)colorWithCGColor:(CGColorRef)cgColor;

+ (NSColor *)colorWithCatalogName:(NSString *)catalogName colorName:(NSString *)colorName;

+ (NSColor *)colorFromPasteboard:(NSPasteboard *)pasteboard;

+ (NSColor *)colorWithPatternImage:(NSImage *)image;

@property (readonly, copy) NSString *colorSpaceName;

@property (readonly) NSInteger numberOfComponents;
- (void)getComponents:(CGFloat *)components;

- (void)getWhite:(CGFloat *)white alpha:(CGFloat *)alpha;
- (void)getRed:(CGFloat *)red green:(CGFloat *)green blue:(CGFloat *)blue alpha:(CGFloat *)alpha;
- (void)getHue:(CGFloat *)hue saturation:(CGFloat *)saturation brightness:(CGFloat *)brightness alpha:(CGFloat *)alpha;
- (void)getCyan:(CGFloat *)cyan magenta:(CGFloat *)magenta yellow:(CGFloat *)yellow black:(CGFloat *)black alpha:(CGFloat *)alpha;

@property (readonly) CGFloat whiteComponent;

@property (readonly) CGFloat redComponent;
@property (readonly) CGFloat greenComponent;
@property (readonly) CGFloat blueComponent;

@property (readonly) CGFloat hueComponent;
@property (readonly) CGFloat saturationComponent;
@property (readonly) CGFloat brightnessComponent;

@property (readonly) CGFloat cyanComponent;
@property (readonly) CGFloat magentaComponent;
@property (readonly) CGFloat yellowComponent;
@property (readonly) CGFloat blackComponent;

@property (readonly) CGFloat alphaComponent;

- (NSColor *)colorWithAlphaComponent:(CGFloat)alpha;

@property (readonly) NSColorType type;
- (NSColor *)colorUsingType:(NSColorType)type;
/* Nil for colors that have no components (patterns, catalog colors whose
   color has none). */
@property (readonly, strong) NSColorSpace *colorSpace;
- (NSColor *)colorUsingColorSpace:(NSColorSpace *)space;
/* Valid as long as the color, or the current autorelease pool for colors
   that are converted to get one. */
@property (readonly) CGColorRef CGColor;

- (NSColor *)colorUsingColorSpaceName:(NSString *)colorSpace;
- (NSColor *)colorUsingColorSpaceName:(NSString *)colorSpace device:(NSDictionary *)device;

- (NSColor *)blendedColorWithFraction:(CGFloat)fraction ofColor:(NSColor *)color;
- (NSColor *)shadowWithLevel:(CGFloat)level;
- (NSColor *)highlightWithLevel:(CGFloat)level;

- (void)set;
- (void)setStroke;
- (void)setFill;

- (void)drawSwatchInRect:(NSRect)rect;

- (void)writeToPasteboard:(NSPasteboard *)pasteboard;

@end
