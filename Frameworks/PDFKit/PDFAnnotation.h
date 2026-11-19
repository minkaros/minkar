/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <PDFKit/PDFKitPlatform.h>
#import <PDFKit/PDFPage.h>
#import <PDFKit/PDFAnnotationUtilities.h>

@class PDFBorder, PDFAction, PDFDestination;

NS_ASSUME_NONNULL_BEGIN

/* An annotation on a page: its values kept by their PDF keys ("/Rect",
   "/Contents"...), each property one of them. */
PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFAnnotation : NSObject <NSCopying, NSCoding>
- (instancetype)initWithBounds:(PDFRect)bounds forType:(PDFAnnotationSubtype)annotationType withProperties:(nullable NSDictionary *)properties NS_DESIGNATED_INITIALIZER;

@property (nonatomic, weak, nullable) PDFPage *page;
/* The subtype without its slash ("Text", "Link"...). */
@property (nonatomic, copy, nullable) NSString *type;
@property (nonatomic) PDFRect bounds;
@property (nonatomic) BOOL shouldDisplay;
@property (nonatomic) BOOL shouldPrint;
@property (nonatomic, copy, nullable) NSDate *modificationDate;
@property (nonatomic, copy, nullable) NSString *userName;
@property (nonatomic, strong, nullable) PDFAnnotation *popup;
@property (nonatomic, strong, nullable) PDFBorder *border;
@property (nonatomic, copy) NSColor *color;
@property (nonatomic, copy, nullable) NSString *contents;
@property (nonatomic, strong, nullable) PDFAction *action;
@property (nonatomic, readonly) BOOL hasAppearanceStream;
@property (nonatomic, getter=isHighlighted) BOOL highlighted;
@property (nonatomic, copy, nullable) NSString *toolTip;

@property (nonatomic, readonly, copy) NSDictionary *annotationKeyValues;
- (BOOL)setValue:(id)value forAnnotationKey:(PDFAnnotationKey)key;
- (BOOL)setBoolean:(BOOL)value forAnnotationKey:(PDFAnnotationKey)key;
- (BOOL)setRect:(PDFRect)value forAnnotationKey:(PDFAnnotationKey)key;
- (nullable id)valueForAnnotationKey:(PDFAnnotationKey)key;
- (void)removeValueForAnnotationKey:(PDFAnnotationKey)key;

- (void)drawWithBox:(PDFDisplayBox)box inContext:(CGContextRef)context;
- (void)drawWithBox:(PDFDisplayBox)box;

/* Free text */
@property (nonatomic, copy, nullable) NSFont *font;
@property (nonatomic, copy, nullable) NSColor *fontColor;
@property (nonatomic) NSTextAlignment alignment;
/* Square, circle, line */
@property (nonatomic, copy, nullable) NSColor *interiorColor;
/* Line: its ends relative to the bounds' origin */
@property (nonatomic) PDFPoint startPoint;
@property (nonatomic) PDFPoint endPoint;
@property (nonatomic) PDFLineStyle startLineStyle;
@property (nonatomic) PDFLineStyle endLineStyle;
+ (PDFLineStyle)lineStyleFromName:(NSString *)name;
+ (NSString *)nameForLineStyle:(PDFLineStyle)style;
/* Text (a note) */
@property (nonatomic) PDFTextAnnotationIconType iconType;
@property (nonatomic, getter=isOpen) BOOL open;
/* Markup: the quadrilaterals' points, relative to the bounds' origin */
@property (nonatomic, copy, nullable) NSArray<NSValue *> *quadrilateralPoints;
@property (nonatomic) PDFMarkupType markupType;
/* Ink: its paths, relative to the bounds' origin */
@property (nonatomic, readonly, nullable) NSArray<NSBezierPath *> *paths;
- (void)addBezierPath:(NSBezierPath *)path;
- (void)removeBezierPath:(NSBezierPath *)path;
/* Link */
@property (nonatomic, strong, nullable) PDFDestination *destination;
@property (nonatomic, copy, nullable) NSURL *URL;
@property (nonatomic) PDFAnnotationHighlightingMode highlightingMode;
/* Stamp */
@property (nonatomic, copy, nullable) NSString *stampName;
/* Widget */
@property (nonatomic, copy) PDFAnnotationWidgetSubtype widgetFieldType;
@property (nonatomic) PDFWidgetControlType widgetControlType;
@property (nonatomic, copy, nullable) NSString *fieldName;
@property (nonatomic, copy, nullable) NSString *widgetStringValue;
@property (nonatomic, copy, nullable) NSString *widgetDefaultStringValue;
@property (nonatomic) PDFWidgetCellState buttonWidgetState;
@property (nonatomic, copy, nullable) NSString *buttonWidgetStateString;
@property (nonatomic, copy, nullable) NSArray<NSString *> *choices;
@property (nonatomic, getter=isListChoice) BOOL listChoice;
@property (nonatomic, getter=isMultiline) BOOL multiline;
@property (nonatomic, getter=isReadOnly) BOOL readOnly;
@property (nonatomic) NSInteger maximumLength;
@property (nonatomic, copy, nullable) NSString *caption;
@property (nonatomic, copy, nullable) NSColor *backgroundColor;
@end

NS_ASSUME_NONNULL_END
