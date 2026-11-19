/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Annotations, as the reference has them (measured 19 Nov 2026): values
 * kept by PDF key; read from a page's Annots as the legacy subclass of
 * their subtype, made in code as PDFAnnotation with the type's defaults
 * (black and a 1-point border for shapes, ink and lines; yellow, no
 * border, for markup and free text); a note's bounds 24 square from its
 * top left; markup quadrilaterals and line ends relative to the bounds.
 */

#import "PDFKitPrivate.h"
#import <PDFKit/PDFAnnotation.h>
#import <PDFKit/PDFAnnotationSubclasses.h>
#import <PDFKit/PDFBorder.h>

@interface PDFAnnotation (PDFAnnotationLink)
- (PDFAction *)_linkAction;
@end

@implementation PDFAnnotation {
    NSMutableDictionary *_values;
}

static NSColor *defaultYellow(void) {
    return [NSColor colorWithDeviceRed:1 green:1 blue:0 alpha:1];
}

- (instancetype)initWithBounds:(PDFRect)bounds forType:(PDFAnnotationSubtype)type withProperties:(NSDictionary *)properties {
    if ((self = [super init])) {
        NSString *subtype = [type hasPrefix:@"/"] ? type : [@"/" stringByAppendingString:type ?: @"Text"];
        NSString *name = [subtype substringFromIndex:1];

        _values = [NSMutableDictionary dictionary];
        _values[PDFAnnotationKeySubtype] = subtype;
        _values[PDFAnnotationKeyRect] = [NSValue valueWithRect:NSRectFromCGRect(CGRectStandardize(bounds))];
        _values[PDFAnnotationKeyFlags] = @4;
        if ([@[ @"Square", @"Circle", @"Ink", @"Line" ] containsObject:name]) {
            _values[PDFAnnotationKeyColor] = [NSColor colorWithDeviceRed:0 green:0 blue:0 alpha:1];
            _values[PDFAnnotationKeyBorder] = [PDFBorder new];
        } else if ([@[ @"FreeText", @"Highlight", @"Underline", @"StrikeOut" ] containsObject:name])
            _values[PDFAnnotationKeyColor] = defaultYellow();
        else if ([@[ @"Text", @"Stamp" ] containsObject:name])
            _values[PDFAnnotationKeyColor] = [NSColor colorWithDeviceRed:0 green:0 blue:0 alpha:1];
        for (NSString *key in properties)
            [self setValue:properties[key] forAnnotationKey:key];
    }
    return self;
}

- (instancetype)init {
    return [self initWithBounds:CGRectZero forType:PDFAnnotationSubtypeText withProperties:nil];
}

- (id)copyWithZone:(NSZone *)zone {
    PDFAnnotation *copy = [[[self class] allocWithZone:zone] initWithBounds:self.bounds forType:self.type withProperties:nil];

    copy->_values = [_values mutableCopy];
    return copy;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [self initWithBounds:CGRectZero forType:PDFAnnotationSubtypeText withProperties:nil])) {
        NSDictionary *values = [coder decodeObjectForKey:@"values"];
        if (values != nil)
            _values = [values mutableCopy];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_values forKey:@"values"];
}

/* ---- values by key ---- */

- (NSDictionary *)annotationKeyValues {
    return [_values copy];
}

- (id)valueForAnnotationKey:(PDFAnnotationKey)key {
    return _values[key];
}

- (BOOL)setValue:(id)value forAnnotationKey:(PDFAnnotationKey)key {
    if (key == nil)
        return NO;
    if (value == nil) {
        [_values removeObjectForKey:key];
        return YES;
    }
    if ([key isEqual:PDFAnnotationKeyRect] && [value isKindOfClass:[NSValue class]])
        value = [NSValue valueWithRect:[value rectValue]];
    [self willChangeValueForKey:@"annotationKeyValues"];
    _values[key] = value;
    [self didChangeValueForKey:@"annotationKeyValues"];
    return YES;
}

- (BOOL)setBoolean:(BOOL)value forAnnotationKey:(PDFAnnotationKey)key {
    return [self setValue:@(value) forAnnotationKey:key];
}

- (BOOL)setRect:(PDFRect)value forAnnotationKey:(PDFAnnotationKey)key {
    return [self setValue:[NSValue valueWithRect:NSRectFromCGRect(value)] forAnnotationKey:key];
}

- (void)removeValueForAnnotationKey:(PDFAnnotationKey)key {
    [_values removeObjectForKey:key];
}

/* ---- common properties ---- */

- (NSString *)type {
    NSString *subtype = _values[PDFAnnotationKeySubtype];

    return [subtype hasPrefix:@"/"] ? [subtype substringFromIndex:1] : subtype;
}

- (void)setType:(NSString *)type {
    [self setValue:(type != nil) ? ([type hasPrefix:@"/"] ? type : [@"/" stringByAppendingString:type]) : nil forAnnotationKey:PDFAnnotationKeySubtype];
}

- (PDFRect)bounds {
    NSValue *rect = _values[PDFAnnotationKeyRect];

    return rect ? NSRectToCGRect(rect.rectValue) : CGRectZero;
}

- (void)setBounds:(PDFRect)bounds {
    [self setRect:CGRectStandardize(bounds) forAnnotationKey:PDFAnnotationKeyRect];
}

- (NSInteger)_flags {
    return [_values[PDFAnnotationKeyFlags] integerValue];
}

- (BOOL)shouldDisplay {
    return ([self _flags] & 2) == 0;         // Hidden, bit 2
}

- (void)setShouldDisplay:(BOOL)display {
    [self setValue:@(display ? ([self _flags] & ~2) : ([self _flags] | 2)) forAnnotationKey:PDFAnnotationKeyFlags];
}

- (BOOL)shouldPrint {
    return ([self _flags] & 4) != 0;         // Print, bit 3
}

- (void)setShouldPrint:(BOOL)print {
    [self setValue:@(print ? ([self _flags] | 4) : ([self _flags] & ~4)) forAnnotationKey:PDFAnnotationKeyFlags];
}

- (NSDate *)modificationDate { return _values[PDFAnnotationKeyDate]; }
- (void)setModificationDate:(NSDate *)date { [self setValue:date forAnnotationKey:PDFAnnotationKeyDate]; }
- (NSString *)userName { return [self.type isEqual:@"Widget"] ? nil : _values[PDFAnnotationKeyTextLabel]; }
- (void)setUserName:(NSString *)name { [self setValue:name forAnnotationKey:PDFAnnotationKeyTextLabel]; }
- (PDFAnnotation *)popup { return _values[PDFAnnotationKeyPopup]; }
- (void)setPopup:(PDFAnnotation *)popup { [self setValue:popup forAnnotationKey:PDFAnnotationKeyPopup]; }
- (PDFBorder *)border { return _values[PDFAnnotationKeyBorder]; }
- (void)setBorder:(PDFBorder *)border { [self setValue:border forAnnotationKey:PDFAnnotationKeyBorder]; }
- (NSColor *)color { return _values[PDFAnnotationKeyColor]; }
- (void)setColor:(NSColor *)color { [self setValue:color forAnnotationKey:PDFAnnotationKeyColor]; }
- (NSString *)contents { return _values[PDFAnnotationKeyContents]; }
- (void)setContents:(NSString *)contents { [self setValue:contents forAnnotationKey:PDFAnnotationKeyContents]; }
- (PDFAction *)action { return [self.type isEqual:@"Link"] ? [self _linkAction] : _values[PDFAnnotationKeyAction]; }
- (void)setAction:(PDFAction *)action { [self setValue:action forAnnotationKey:PDFAnnotationKeyAction]; }
- (BOOL)hasAppearanceStream { return _values[PDFAnnotationKeyAppearanceDictionary] != nil; }
- (NSString *)toolTip { return self.contents ?: self.fieldName; }
- (void)setToolTip:(NSString *)toolTip { [self setValue:toolTip forAnnotationKey:PDFAnnotationKeyWidgetTextLabelUI]; }

/* ---- free text ---- */

/* The default appearance's font and color ("/Name size Tf r g b rg"). */
- (NSFont *)font {
    NSString *da = _values[PDFAnnotationKeyDefaultAppearance];
    NSArray *words = [da componentsSeparatedByCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    NSUInteger tf = [words indexOfObject:@"Tf"];

    if (tf != NSNotFound && tf >= 2) {
        NSString *name = [words[tf - 2] hasPrefix:@"/"] ? [words[tf - 2] substringFromIndex:1] : words[tf - 2];
        CGFloat size = [words[tf - 1] doubleValue];

        return [NSFont fontWithName:name size:size ?: 12] ?: [NSFont userFontOfSize:size ?: 12];
    }
    return [NSFont userFontOfSize:12];
}

- (void)_setDefaultAppearanceFont:(NSFont *)font color:(NSColor *)color {
    NSColor *rgb = [color colorUsingColorSpaceName:NSDeviceRGBColorSpace] ?: [NSColor blackColor];

    [self setValue:[NSString stringWithFormat:@"/%@ %g Tf %g %g %g rg", font.fontName ?: @"Helvetica", font.pointSize ?: 12,
                                              rgb.redComponent, rgb.greenComponent, rgb.blueComponent]
    forAnnotationKey:PDFAnnotationKeyDefaultAppearance];
}

- (void)setFont:(NSFont *)font { [self _setDefaultAppearanceFont:font color:self.fontColor]; }

- (NSColor *)fontColor {
    NSString *da = _values[PDFAnnotationKeyDefaultAppearance];
    NSArray *words = [da componentsSeparatedByCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    NSUInteger rg = [words indexOfObject:@"rg"], g = [words indexOfObject:@"g"];

    if (rg != NSNotFound && rg >= 3)
        return [NSColor colorWithDeviceRed:[words[rg - 3] doubleValue] green:[words[rg - 2] doubleValue] blue:[words[rg - 1] doubleValue] alpha:1];
    if (g != NSNotFound && g >= 1)
        return [NSColor colorWithDeviceWhite:[words[g - 1] doubleValue] alpha:1];
    return [NSColor blackColor];
}

- (void)setFontColor:(NSColor *)color { [self _setDefaultAppearanceFont:self.font color:color]; }

- (NSTextAlignment)alignment {
    switch ([_values[PDFAnnotationKeyQuadding] integerValue]) {
    case 1: return NSTextAlignmentCenter;
    case 2: return NSTextAlignmentRight;
    default: return NSTextAlignmentLeft;
    }
}

- (void)setAlignment:(NSTextAlignment)alignment {
    [self setValue:@(alignment == NSTextAlignmentCenter ? 1 : alignment == NSTextAlignmentRight ? 2 : 0) forAnnotationKey:PDFAnnotationKeyQuadding];
}

/* ---- shapes and lines ---- */

- (NSColor *)interiorColor { return _values[PDFAnnotationKeyInteriorColor]; }
- (void)setInteriorColor:(NSColor *)color { [self setValue:color forAnnotationKey:PDFAnnotationKeyInteriorColor]; }

- (PDFPoint)_linePoint:(NSUInteger)index {
    NSArray *points = _values[PDFAnnotationKeyLinePoints];

    return (points.count > index) ? NSPointToCGPoint([points[index] pointValue]) : CGPointZero;
}

- (void)_setLinePoint:(PDFPoint)point index:(NSUInteger)index {
    NSMutableArray *points = [NSMutableArray arrayWithArray:_values[PDFAnnotationKeyLinePoints] ?: @[]];

    while (points.count < 2)
        [points addObject:[NSValue valueWithPoint:NSZeroPoint]];
    points[index] = [NSValue valueWithPoint:NSPointFromCGPoint(point)];
    [self setValue:points forAnnotationKey:PDFAnnotationKeyLinePoints];
}

- (PDFPoint)startPoint { return [self _linePoint:0]; }
- (void)setStartPoint:(PDFPoint)point { [self _setLinePoint:point index:0]; }
- (PDFPoint)endPoint { return [self _linePoint:1]; }
- (void)setEndPoint:(PDFPoint)point { [self _setLinePoint:point index:1]; }

static NSString *lineStyleNames[] = { @"/None", @"/Square", @"/Circle", @"/Diamond", @"/OpenArrow", @"/ClosedArrow" };

+ (PDFLineStyle)lineStyleFromName:(NSString *)name {
    if (![name hasPrefix:@"/"])
        name = [@"/" stringByAppendingString:name ?: @""];
    for (NSInteger i = 0; i < 6; i++)
        if ([name isEqualToString:lineStyleNames[i]])
            return i;
    return kPDFLineStyleNone;
}

+ (NSString *)nameForLineStyle:(PDFLineStyle)style {
    return [lineStyleNames[(style >= 0 && style < 6) ? style : 0] substringFromIndex:1];
}

- (PDFLineStyle)_lineStyle:(NSUInteger)index {
    NSArray *styles = _values[PDFAnnotationKeyLineEndingStyles];

    return (styles.count > index) ? [PDFAnnotation lineStyleFromName:styles[index]] : kPDFLineStyleNone;
}

- (void)_setLineStyle:(PDFLineStyle)style index:(NSUInteger)index {
    NSMutableArray *styles = [NSMutableArray arrayWithArray:_values[PDFAnnotationKeyLineEndingStyles] ?: @[ @"/None", @"/None" ]];

    while (styles.count < 2)
        [styles addObject:@"/None"];
    styles[index] = lineStyleNames[(style >= 0 && style < 6) ? style : 0];
    [self setValue:styles forAnnotationKey:PDFAnnotationKeyLineEndingStyles];
}

- (PDFLineStyle)startLineStyle { return [self _lineStyle:0]; }
- (void)setStartLineStyle:(PDFLineStyle)style { [self _setLineStyle:style index:0]; }
- (PDFLineStyle)endLineStyle { return [self _lineStyle:1]; }
- (void)setEndLineStyle:(PDFLineStyle)style { [self _setLineStyle:style index:1]; }

/* ---- notes ---- */

static NSString *iconNames[] = { @"/Comment", @"/Key", @"/Note", @"/Help", @"/NewParagraph", @"/Paragraph", @"/Insert" };

- (PDFTextAnnotationIconType)iconType {
    NSString *name = _values[PDFAnnotationKeyIconName];

    for (NSInteger i = 0; i < 7; i++)
        if ([name isEqualToString:iconNames[i]])
            return i;
    return kPDFTextAnnotationIconNote;
}

- (void)setIconType:(PDFTextAnnotationIconType)type {
    [self setValue:iconNames[(type >= 0 && type < 7) ? type : 2] forAnnotationKey:PDFAnnotationKeyIconName];
}

- (BOOL)isOpen { return [_values[PDFAnnotationKeyOpen] boolValue]; }
- (void)setOpen:(BOOL)open { [self setBoolean:open forAnnotationKey:PDFAnnotationKeyOpen]; }

/* ---- markup ---- */

- (NSArray<NSValue *> *)quadrilateralPoints { return _values[PDFAnnotationKeyQuadPoints]; }
- (void)setQuadrilateralPoints:(NSArray<NSValue *> *)points { [self setValue:points forAnnotationKey:PDFAnnotationKeyQuadPoints]; }

- (PDFMarkupType)markupType {
    NSString *type = self.type;

    return [type isEqual:@"StrikeOut"] ? kPDFMarkupTypeStrikeOut : [type isEqual:@"Underline"] ? kPDFMarkupTypeUnderline :
           [type isEqual:@"Redact"] ? kPDFMarkupTypeRedact : kPDFMarkupTypeHighlight;
}

- (void)setMarkupType:(PDFMarkupType)type {
    self.type = (type == kPDFMarkupTypeStrikeOut) ? @"StrikeOut" : (type == kPDFMarkupTypeUnderline) ? @"Underline" :
                (type == kPDFMarkupTypeRedact) ? @"Redact" : @"Highlight";
}

/* ---- ink ---- */

- (NSArray<NSBezierPath *> *)paths { return _values[PDFAnnotationKeyInklist]; }

- (void)addBezierPath:(NSBezierPath *)path {
    if (path != nil)
        [self setValue:[(self.paths ?: @[]) arrayByAddingObject:path] forAnnotationKey:PDFAnnotationKeyInklist];
}

- (void)removeBezierPath:(NSBezierPath *)path {
    NSMutableArray *paths = [NSMutableArray arrayWithArray:self.paths ?: @[]];

    [paths removeObjectIdenticalTo:path];
    [self setValue:paths forAnnotationKey:PDFAnnotationKeyInklist];
}

/* ---- links ---- */

- (PDFDestination *)destination {
    PDFDestination *destination = _values[PDFAnnotationKeyDestination];

    if (destination == nil && [self.action isKindOfClass:[PDFActionGoTo class]])
        destination = [(PDFActionGoTo *)self.action destination];
    return destination;
}

- (void)setDestination:(PDFDestination *)destination { [self setValue:destination forAnnotationKey:PDFAnnotationKeyDestination]; }

/* A link with a destination and no action goes to it: its action a GoTo,
   as on the reference. */
- (PDFAction *)_linkAction {
    PDFAction *action = _values[PDFAnnotationKeyAction];
    PDFDestination *destination = _values[PDFAnnotationKeyDestination];

    return action ?: (destination ? [[PDFActionGoTo alloc] initWithDestination:destination] : nil);
}

- (NSURL *)URL {
    return [self.action isKindOfClass:[PDFActionURL class]] ? [(PDFActionURL *)self.action URL] : nil;
}

- (void)setURL:(NSURL *)url {
    self.action = url ? [[PDFActionURL alloc] initWithURL:url] : nil;
}

- (PDFAnnotationHighlightingMode)highlightingMode { return _values[PDFAnnotationKeyHighlightingMode] ?: PDFAnnotationHighlightingModeInvert; }
- (void)setHighlightingMode:(PDFAnnotationHighlightingMode)mode { [self setValue:mode forAnnotationKey:PDFAnnotationKeyHighlightingMode]; }

/* ---- stamps ---- */

- (NSString *)stampName {
    NSString *name = _values[PDFAnnotationKeyIconName];

    return [name hasPrefix:@"/"] ? [name substringFromIndex:1] : name;
}

- (void)setStampName:(NSString *)name {
    [self setValue:name ? [@"/" stringByAppendingString:name] : nil forAnnotationKey:PDFAnnotationKeyIconName];
}

/* ---- widgets ---- */

- (PDFAnnotationWidgetSubtype)widgetFieldType { return _values[PDFAnnotationKeyWidgetFieldType] ?: PDFAnnotationWidgetSubtypeText; }
- (void)setWidgetFieldType:(PDFAnnotationWidgetSubtype)type { [self setValue:type forAnnotationKey:PDFAnnotationKeyWidgetFieldType]; }
- (NSInteger)_fieldFlags { return [_values[PDFAnnotationKeyWidgetFieldFlags] integerValue]; }

- (void)_setFieldFlag:(NSInteger)flag on:(BOOL)on {
    [self setValue:@(on ? ([self _fieldFlags] | flag) : ([self _fieldFlags] & ~flag)) forAnnotationKey:PDFAnnotationKeyWidgetFieldFlags];
}

/* A button's flags (PDF 1.7, table 226): push button bit 17, radio 16. */
- (PDFWidgetControlType)widgetControlType {
    if (![self.widgetFieldType isEqual:PDFAnnotationWidgetSubtypeButton])
        return kPDFWidgetUnknownControl;
    if ([self _fieldFlags] & (1 << 16))
        return kPDFWidgetPushButtonControl;
    if ([self _fieldFlags] & (1 << 15))
        return kPDFWidgetRadioButtonControl;
    return kPDFWidgetCheckBoxControl;
}

- (void)setWidgetControlType:(PDFWidgetControlType)type {
    [self _setFieldFlag:(1 << 16) on:type == kPDFWidgetPushButtonControl];
    [self _setFieldFlag:(1 << 15) on:type == kPDFWidgetRadioButtonControl];
}

- (NSString *)fieldName { return [self.type isEqual:@"Widget"] ? _values[PDFAnnotationKeyTextLabel] : nil; }
- (void)setFieldName:(NSString *)name { [self setValue:name forAnnotationKey:PDFAnnotationKeyTextLabel]; }
- (NSString *)widgetStringValue { id v = _values[PDFAnnotationKeyWidgetValue]; return [v isKindOfClass:[NSString class]] ? v : nil; }
- (void)setWidgetStringValue:(NSString *)value { [self setValue:value forAnnotationKey:PDFAnnotationKeyWidgetValue]; }
- (NSString *)widgetDefaultStringValue { id v = _values[PDFAnnotationKeyWidgetDefaultValue]; return [v isKindOfClass:[NSString class]] ? v : nil; }
- (void)setWidgetDefaultStringValue:(NSString *)value { [self setValue:value forAnnotationKey:PDFAnnotationKeyWidgetDefaultValue]; }

/* A check box or radio button's state: its appearance state, Off or not. */
- (PDFWidgetCellState)buttonWidgetState {
    NSString *state = _values[PDFAnnotationKeyAppearanceState] ?: _values[PDFAnnotationKeyWidgetValue];

    return (state == nil || [state isEqual:@"/Off"]) ? kPDFWidgetOffState : kPDFWidgetOnState;
}

- (void)setButtonWidgetState:(PDFWidgetCellState)state {
    NSString *on = self.buttonWidgetStateString ?: @"Yes";

    [self setValue:(state == kPDFWidgetOnState) ? [@"/" stringByAppendingString:on] : @"/Off" forAnnotationKey:PDFAnnotationKeyAppearanceState];
}

- (NSString *)buttonWidgetStateString { return _values[@"_onState"]; }
- (void)setButtonWidgetStateString:(NSString *)string { [self setValue:string forAnnotationKey:@"_onState"]; }
- (NSArray<NSString *> *)choices { return _values[PDFAnnotationKeyWidgetOptions]; }
- (void)setChoices:(NSArray<NSString *> *)choices { [self setValue:choices forAnnotationKey:PDFAnnotationKeyWidgetOptions]; }
/* A choice field's flags: combo box bit 18. */
- (BOOL)isListChoice { return ([self _fieldFlags] & (1 << 17)) == 0; }
- (void)setListChoice:(BOOL)list { [self _setFieldFlag:(1 << 17) on:!list]; }
- (BOOL)isMultiline { return ([self _fieldFlags] & (1 << 12)) != 0; }
- (void)setMultiline:(BOOL)multiline { [self _setFieldFlag:(1 << 12) on:multiline]; }
- (BOOL)isReadOnly { return ([self _fieldFlags] & 1) != 0; }
- (void)setReadOnly:(BOOL)readOnly { [self _setFieldFlag:1 on:readOnly]; }
- (NSInteger)maximumLength { return [_values[PDFAnnotationKeyWidgetMaxLen] integerValue]; }
- (void)setMaximumLength:(NSInteger)length { [self setValue:@(length) forAnnotationKey:PDFAnnotationKeyWidgetMaxLen]; }
- (NSString *)caption { return _values[PDFAnnotationKeyWidgetCaption]; }
- (void)setCaption:(NSString *)caption { [self setValue:caption forAnnotationKey:PDFAnnotationKeyWidgetCaption]; }
- (NSColor *)backgroundColor { return _values[PDFAnnotationKeyWidgetBackgroundColor]; }
- (void)setBackgroundColor:(NSColor *)color { [self setValue:color forAnnotationKey:PDFAnnotationKeyWidgetBackgroundColor]; }

/* ---- drawing ---- */

/* Drawn from its values (an appearance stream is not drawn yet), in page
   space. */
- (void)drawWithBox:(PDFDisplayBox)box inContext:(CGContextRef)context {
    NSGraphicsContext *saved = [NSGraphicsContext currentContext];

    if (!self.shouldDisplay || context == NULL)
        return;
    CGContextSaveGState(context);
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithCGContext:context flipped:NO]];
    [self _drawContent];
    [NSGraphicsContext setCurrentContext:saved];
    CGContextRestoreGState(context);
}

- (void)drawWithBox:(PDFDisplayBox)box {
    [self drawWithBox:box inContext:[[NSGraphicsContext currentContext] CGContext]];
}

- (void)_drawContent {
    NSRect b = NSRectFromCGRect(self.bounds);
    NSString *type = self.type;
    NSColor *color = self.color;
    CGFloat width = self.border ? self.border.lineWidth : 0;

    if ([type isEqual:@"Square"] || [type isEqual:@"Circle"]) {
        NSRect r = NSInsetRect(b, width / 2, width / 2);
        NSBezierPath *path = [type isEqual:@"Square"] ? [NSBezierPath bezierPathWithRect:r] : [NSBezierPath bezierPathWithOvalInRect:r];

        if (self.interiorColor) { [self.interiorColor setFill]; [path fill]; }
        if (color && width > 0) { [color setStroke]; path.lineWidth = width; [path stroke]; }
    } else if ([type isEqual:@"Line"]) {
        NSBezierPath *path = [NSBezierPath bezierPath];

        [path moveToPoint:NSMakePoint(NSMinX(b) + self.startPoint.x, NSMinY(b) + self.startPoint.y)];
        [path lineToPoint:NSMakePoint(NSMinX(b) + self.endPoint.x, NSMinY(b) + self.endPoint.y)];
        path.lineWidth = MAX(width, 1);
        [color ?: [NSColor blackColor] setStroke];
        [path stroke];
    } else if ([type isEqual:@"Ink"]) {
        NSAffineTransform *move = [NSAffineTransform transform];

        [move translateXBy:NSMinX(b) yBy:NSMinY(b)];
        [color ?: [NSColor blackColor] setStroke];
        for (NSBezierPath *path in self.paths) {
            NSBezierPath *placed = [move transformBezierPath:path];
            placed.lineWidth = MAX(width, 1);
            placed.lineCapStyle = NSLineCapStyleRound;
            placed.lineJoinStyle = NSLineJoinStyleRound;
            [placed stroke];
        }
    } else if ([type isEqual:@"Highlight"] || [type isEqual:@"Underline"] || [type isEqual:@"StrikeOut"]) {
        NSArray *points = self.quadrilateralPoints;
        CGContextRef context = [[NSGraphicsContext currentContext] CGContext];

        for (NSUInteger i = 0; i + 3 < points.count; i += 4) {
            NSPoint p[4];
            for (int k = 0; k < 4; k++) {
                p[k] = [points[i + k] pointValue];
                p[k].x += NSMinX(b);
                p[k].y += NSMinY(b);
            }
            /* Its points: top left, top right, bottom left, bottom right. */
            CGFloat minX = MIN(MIN(p[0].x, p[1].x), MIN(p[2].x, p[3].x)), maxX = MAX(MAX(p[0].x, p[1].x), MAX(p[2].x, p[3].x));
            CGFloat minY = MIN(MIN(p[0].y, p[1].y), MIN(p[2].y, p[3].y)), maxY = MAX(MAX(p[0].y, p[1].y), MAX(p[2].y, p[3].y));
            CGFloat height = maxY - minY;

            if ([type isEqual:@"Highlight"]) {
                CGContextSaveGState(context);
                CGContextSetBlendMode(context, kCGBlendModeMultiply);
                [color ?: defaultYellow() setFill];
                NSRectFill(NSMakeRect(minX, minY, maxX - minX, height));
                CGContextRestoreGState(context);
            } else {
                CGFloat y = [type isEqual:@"Underline"] ? minY + height * 0.1 : minY + height * 0.45;
                [color ?: [NSColor blackColor] setFill];
                NSRectFill(NSMakeRect(minX, y, maxX - minX, MAX(1, height / 14)));
            }
        }
    } else if ([type isEqual:@"Text"]) {
        /* A note: a page with a folded corner, in its color. */
        NSRect r = NSInsetRect(NSMakeRect(NSMinX(b), NSMaxY(b) - 24, 24, 24), 2, 2);
        NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:r xRadius:2 yRadius:2];

        [color ?: defaultYellow() setFill];
        [path fill];
        [[NSColor colorWithDeviceWhite:0 alpha:0.6] setStroke];
        path.lineWidth = 1;
        [path stroke];
        [[NSColor colorWithDeviceWhite:0 alpha:0.6] setFill];
        for (int line = 0; line < 3; line++) {
            NSRectFill(NSMakeRect(NSMinX(r) + 4, NSMaxY(r) - 6 - line * 5, NSWidth(r) - 8, 1));
        }
    } else if ([type isEqual:@"FreeText"]) {
        NSMutableParagraphStyle *style = [NSMutableParagraphStyle new];

        style.alignment = self.alignment;
        if (color && ![color isEqual:defaultYellow()]) { [color setFill]; NSRectFill(b); }
        [self.contents ?: @"" drawInRect:NSInsetRect(b, 2, 2) withAttributes:@{ NSFontAttributeName: self.font, NSForegroundColorAttributeName: self.fontColor, NSParagraphStyleAttributeName: style }];
        if (width > 0) { [color ?: [NSColor blackColor] setStroke]; [self.border drawInRect:b]; }
    } else if ([type isEqual:@"Stamp"]) {
        NSDictionary *attributes = @{ NSFontAttributeName: [NSFont boldSystemFontOfSize:MAX(8, NSHeight(b) * 0.4)], NSForegroundColorAttributeName: color ?: [NSColor redColor] };
        NSString *name = self.stampName ?: @"Draft";

        [color ?: [NSColor redColor] setStroke];
        NSBezierPath *frame = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(b, 1, 1) xRadius:4 yRadius:4];
        frame.lineWidth = 2;
        [frame stroke];
        NSSize size = [name sizeWithAttributes:attributes];
        [name drawAtPoint:NSMakePoint(NSMidX(b) - size.width / 2, NSMidY(b) - size.height / 2) withAttributes:attributes];
    } else if ([type isEqual:@"Widget"]) {
        if (self.backgroundColor) { [self.backgroundColor setFill]; NSRectFill(b); }
        if (self.widgetControlType == kPDFWidgetCheckBoxControl || self.widgetControlType == kPDFWidgetRadioButtonControl) {
            if (self.buttonWidgetState == kPDFWidgetOnState) {
                [[NSColor blackColor] setFill];
                NSRect mark = NSInsetRect(b, NSWidth(b) * 0.25, NSHeight(b) * 0.25);
                (self.widgetControlType == kPDFWidgetRadioButtonControl) ? [[NSBezierPath bezierPathWithOvalInRect:mark] fill] : NSRectFill(mark);
            }
        } else
            [self.widgetStringValue ?: @"" drawInRect:NSInsetRect(b, 2, 2) withAttributes:@{ NSFontAttributeName: self.font, NSForegroundColorAttributeName: self.fontColor }];
        if (width > 0) { [[NSColor blackColor] setStroke]; [self.border drawInRect:b]; }
    } else if ([type isEqual:@"Link"]) {
        if (width > 0) { [color ?: [NSColor blueColor] setStroke]; [self.border drawInRect:b]; }
    }
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p %@ %@>", [self class], self, self.type, NSStringFromRect(NSRectFromCGRect(self.bounds))];
}

@end

/* ---- read from a PDF ---- */

static NSColor *colorFromArray(CGPDFArrayRef array) {
    CGPDFReal c[4] = {0};
    size_t count = CGPDFArrayGetCount(array);

    for (size_t i = 0; i < count && i < 4; i++)
        CGPDFArrayGetNumber(array, i, &c[i]);
    switch (count) {
    case 1: return [NSColor colorWithDeviceWhite:c[0] alpha:1];
    case 3: return [NSColor colorWithDeviceRed:c[0] green:c[1] blue:c[2] alpha:1];
    case 4: return [NSColor colorWithDeviceCyan:c[0] magenta:c[1] yellow:c[2] black:c[3] alpha:1];
    }
    return nil;
}

static id plainValue(CGPDFObjectRef object, int depth);

/* A PDF value as Cocoa's: a name as "/Name", text as a string, numbers,
   arrays and dictionaries of the same (keys as names); nil for a stream. */
typedef struct { NSMutableDictionary *result; int depth; } PlainContext;

static void plainEntry(const char *key, CGPDFObjectRef value, void *info) {
    PlainContext *context = info;
    id plain;

    if (strcmp(key, "P") == 0 || strcmp(key, "Parent") == 0 || strcmp(key, "Popup") == 0)
        return;
    if ((plain = plainValue(value, context->depth + 1)) != nil)
        context->result[[NSString stringWithFormat:@"/%s", key]] = plain;
}

static id plainValue(CGPDFObjectRef object, int depth) {
    CGPDFBoolean boolean;
    CGPDFInteger integer;
    CGPDFReal real;
    const char *name;
    CGPDFStringRef string;
    CGPDFArrayRef array;
    CGPDFDictionaryRef dictionary;

    if (depth > 8)
        return nil;
    switch (CGPDFObjectGetType(object)) {
    case kCGPDFObjectTypeBoolean: CGPDFObjectGetValue(object, kCGPDFObjectTypeBoolean, &boolean); return @(boolean != 0);
    case kCGPDFObjectTypeInteger: CGPDFObjectGetValue(object, kCGPDFObjectTypeInteger, &integer); return @(integer);
    case kCGPDFObjectTypeReal: CGPDFObjectGetValue(object, kCGPDFObjectTypeReal, &real); return @(real);
    case kCGPDFObjectTypeName: CGPDFObjectGetValue(object, kCGPDFObjectTypeName, &name); return [NSString stringWithFormat:@"/%s", name];
    case kCGPDFObjectTypeString: CGPDFObjectGetValue(object, kCGPDFObjectTypeString, &string); return PDFKitString(string);
    case kCGPDFObjectTypeArray: {
        NSMutableArray *result = [NSMutableArray array];

        CGPDFObjectGetValue(object, kCGPDFObjectTypeArray, &array);
        for (size_t i = 0; i < CGPDFArrayGetCount(array); i++) {
            CGPDFObjectRef item;
            id plain;

            if (CGPDFArrayGetObject(array, i, &item) && (plain = plainValue(item, depth + 1)) != nil)
                [result addObject:plain];
        }
        return result;
    }
    case kCGPDFObjectTypeDictionary: {
        PlainContext context = { [NSMutableDictionary dictionary], depth };

        CGPDFObjectGetValue(object, kCGPDFObjectTypeDictionary, &dictionary);
        CGPDFDictionaryApplyFunction(dictionary, plainEntry, &context);
        return context.result;
    }
    default:
        return nil;
    }
}

/* A field's value, its own or its parent field's (inheritable). */
static CGPDFObjectRef inheritedObject(CGPDFDictionaryRef dictionary, const char *key) {
    CGPDFObjectRef object = NULL;

    for (int depth = 0; dictionary != NULL && depth < 16; depth++) {
        if (CGPDFDictionaryGetObject(dictionary, key, &object))
            return object;
        if (!CGPDFDictionaryGetDictionary(dictionary, "Parent", &dictionary))
            break;
    }
    return NULL;
}

@implementation PDFAnnotation (PDFKitReading)

+ (Class)_classForSubtype:(NSString *)subtype fieldType:(NSString *)fieldType {
    NSDictionary *classes = @{ @"Text": [PDFAnnotationText class], @"Link": [PDFAnnotationLink class], @"Highlight": [PDFAnnotationMarkup class],
                               @"Underline": [PDFAnnotationMarkup class], @"StrikeOut": [PDFAnnotationMarkup class], @"FreeText": [PDFAnnotationFreeText class],
                               @"Square": [PDFAnnotationSquare class], @"Circle": [PDFAnnotationCircle class], @"Line": [PDFAnnotationLine class],
                               @"Ink": [PDFAnnotationInk class], @"Stamp": [PDFAnnotationStamp class], @"Popup": [PDFAnnotationPopup class] };

    if ([subtype isEqual:@"Widget"]) {
        if ([fieldType isEqual:@"/Btn"]) return [PDFAnnotationButtonWidget class];
        if ([fieldType isEqual:@"/Ch"]) return [PDFAnnotationChoiceWidget class];
        return [PDFAnnotationTextWidget class];
    }
    return classes[subtype] ?: [PDFAnnotation class];
}

+ (PDFAnnotation *)_annotationWithDictionary:(CGPDFDictionaryRef)dictionary page:(PDFPage *)page {
    const char *subtypeName = NULL, *fieldTypeName = NULL;
    CGPDFArrayRef rectArray, array;
    CGPDFObjectRef object;
    CGPDFDictionaryRef style, action, mk;
    CGPDFStringRef date;
    CGPDFReal r[4] = {0};
    PDFDocument *document = page.document;

    if (!CGPDFDictionaryGetName(dictionary, "Subtype", &subtypeName) || !CGPDFDictionaryGetArray(dictionary, "Rect", &rectArray))
        return nil;
    for (size_t i = 0; i < 4; i++)
        CGPDFArrayGetNumber(rectArray, i, &r[i]);
    CGRect bounds = CGRectStandardize(CGRectMake(r[0], r[1], r[2] - r[0], r[3] - r[1]));
    NSString *subtype = @(subtypeName);
    if ((object = inheritedObject(dictionary, "FT")) != NULL)
        CGPDFObjectGetValue(object, kCGPDFObjectTypeName, &fieldTypeName);

    /* A note is 24 points square, from its top left. */
    if ([subtype isEqual:@"Text"])
        bounds = CGRectMake(CGRectGetMinX(bounds), CGRectGetMaxY(bounds) - 24, 24, 24);

    Class cls = [self _classForSubtype:subtype fieldType:fieldTypeName ? [NSString stringWithFormat:@"/%s", fieldTypeName] : nil];
    PDFAnnotation *annotation = [[cls alloc] initWithBounds:bounds forType:subtype withProperties:nil];
    NSMutableDictionary *values = annotation->_values;
    PlainContext context = { [NSMutableDictionary dictionary], 0 };

    [values removeObjectForKey:PDFAnnotationKeyColor];
    [values removeObjectForKey:PDFAnnotationKeyBorder];
    CGPDFDictionaryApplyFunction(dictionary, plainEntry, &context);
    for (NSString *key in context.result)
        if (![@[ @"/Rect", @"/Subtype", @"/Type" ] containsObject:key])
            values[key] = context.result[key];
    values[@"/Type"] = @"/Annot";
    if (values[PDFAnnotationKeyFlags] == nil)
        values[PDFAnnotationKeyFlags] = @4;
    /* The keys with values of their own. */
    if (CGPDFDictionaryGetArray(dictionary, "C", &array)) values[PDFAnnotationKeyColor] = colorFromArray(array) ?: [NSNull null];
    else [values removeObjectForKey:PDFAnnotationKeyColor];
    if (CGPDFDictionaryGetArray(dictionary, "IC", &array)) values[PDFAnnotationKeyInteriorColor] = colorFromArray(array);
    if (CGPDFDictionaryGetString(dictionary, "M", &date)) {
        NSDate *when = PDFKitDate(date);
        if (when) values[PDFAnnotationKeyDate] = when; else [values removeObjectForKey:PDFAnnotationKeyDate];
    }
    if (CGPDFDictionaryGetDictionary(dictionary, "BS", &style) || CGPDFDictionaryGetArray(dictionary, "Border", &array)) {
        PDFBorder *border = [PDFBorder new];
        CGPDFReal width = 1;
        const char *styleName = NULL;

        if (style != NULL && CGPDFDictionaryGetDictionary(dictionary, "BS", &style)) {
            CGPDFDictionaryGetNumber(style, "W", &width);
            if (CGPDFDictionaryGetName(style, "S", &styleName))
                border.style = (styleName[0] == 'D') ? kPDFBorderStyleDashed : (styleName[0] == 'B') ? kPDFBorderStyleBeveled :
                               (styleName[0] == 'I') ? kPDFBorderStyleInset : (styleName[0] == 'U') ? kPDFBorderStyleUnderline : kPDFBorderStyleSolid;
        } else
            CGPDFArrayGetNumber(array, 2, &width);
        border.lineWidth = width;
        values[PDFAnnotationKeyBorder] = border;
    }
    [values removeObjectForKey:PDFAnnotationKeyBorderStyle];
    if (CGPDFDictionaryGetObject(dictionary, "Dest", &object)) {
        PDFDestination *destination = [document _destinationForObject:object];
        if (destination) values[PDFAnnotationKeyDestination] = destination; else [values removeObjectForKey:PDFAnnotationKeyDestination];
    }
    if (CGPDFDictionaryGetDictionary(dictionary, "A", &action)) {
        PDFAction *read = [document _actionForDictionary:action];
        if (read) values[PDFAnnotationKeyAction] = read; else [values removeObjectForKey:PDFAnnotationKeyAction];
    }
    if (CGPDFDictionaryGetArray(dictionary, "QuadPoints", &array)) {
        NSMutableArray *points = [NSMutableArray array];

        for (size_t i = 0; i + 1 < CGPDFArrayGetCount(array); i += 2) {
            CGPDFReal x = 0, y = 0;
            CGPDFArrayGetNumber(array, i, &x);
            CGPDFArrayGetNumber(array, i + 1, &y);
            [points addObject:[NSValue valueWithPoint:NSMakePoint(x - bounds.origin.x, y - bounds.origin.y)]];
        }
        values[PDFAnnotationKeyQuadPoints] = points;
    }
    if (CGPDFDictionaryGetArray(dictionary, "L", &array) && CGPDFArrayGetCount(array) >= 4) {
        CGPDFReal l[4] = {0};
        for (size_t i = 0; i < 4; i++)
            CGPDFArrayGetNumber(array, i, &l[i]);
        values[PDFAnnotationKeyLinePoints] = @[ [NSValue valueWithPoint:NSMakePoint(l[0] - bounds.origin.x, l[1] - bounds.origin.y)],
                                                [NSValue valueWithPoint:NSMakePoint(l[2] - bounds.origin.x, l[3] - bounds.origin.y)] ];
    }
    if (CGPDFDictionaryGetArray(dictionary, "InkList", &array)) {
        NSMutableArray *paths = [NSMutableArray array];

        for (size_t i = 0; i < CGPDFArrayGetCount(array); i++) {
            CGPDFArrayRef stroke;
            NSBezierPath *path = [NSBezierPath bezierPath];

            if (!CGPDFArrayGetArray(array, i, &stroke))
                continue;
            for (size_t k = 0; k + 1 < CGPDFArrayGetCount(stroke); k += 2) {
                CGPDFReal x = 0, y = 0;
                CGPDFArrayGetNumber(stroke, k, &x);
                CGPDFArrayGetNumber(stroke, k + 1, &y);
                NSPoint p = NSMakePoint(x - bounds.origin.x, y - bounds.origin.y);
                (k == 0) ? [path moveToPoint:p] : [path lineToPoint:p];
            }
            [paths addObject:path];
        }
        values[PDFAnnotationKeyInklist] = paths;
    }
    if (CGPDFDictionaryGetDictionary(dictionary, "AP", &style))
        values[PDFAnnotationKeyAppearanceDictionary] = @YES;
    /* A widget's field values, inherited from its parent fields. */
    if ([subtype isEqual:@"Widget"]) {
        for (NSString *key in @[ @"FT", @"T", @"V", @"DV", @"Ff", @"Opt", @"MaxLen", @"DA", @"Q" ])
            if (values[[@"/" stringByAppendingString:key]] == nil && (object = inheritedObject(dictionary, key.UTF8String)) != NULL) {
                id plain = plainValue(object, 0);
                if (plain) values[[@"/" stringByAppendingString:key]] = plain;
            }
        if (CGPDFDictionaryGetDictionary(dictionary, "MK", &mk)) {
            CGPDFStringRef caption;
            if (CGPDFDictionaryGetArray(mk, "BG", &array)) values[PDFAnnotationKeyWidgetBackgroundColor] = colorFromArray(array);
            if (CGPDFDictionaryGetArray(mk, "BC", &array)) values[PDFAnnotationKeyWidgetBorderColor] = colorFromArray(array);
            if (CGPDFDictionaryGetString(mk, "CA", &caption)) values[PDFAnnotationKeyWidgetCaption] = PDFKitString(caption);
        }
        /* A check box's on state: its normal appearance's name other than
           Off. */
        CGPDFDictionaryRef ap, normal;
        if (CGPDFDictionaryGetDictionary(dictionary, "AP", &ap) && CGPDFDictionaryGetDictionary(ap, "N", &normal)) {
            PlainContext names = { [NSMutableDictionary dictionary], 7 };

            CGPDFDictionaryApplyFunction(normal, plainEntry, &names);
            for (NSString *name in names.result)
                if (![name isEqual:@"/Off"])
                    values[@"_onState"] = [name substringFromIndex:1];
        }
    }
    /* As the reference reads them: a link's highlighting mode is Invert by
       default; a note has a popup (for its text), beside it. */
    if ([subtype isEqual:@"Link"] && values[PDFAnnotationKeyHighlightingMode] == nil)
        values[PDFAnnotationKeyHighlightingMode] = PDFAnnotationHighlightingModeInvert;
    if ([subtype isEqual:@"Text"] && values[PDFAnnotationKeyPopup] == nil) {
        PDFAnnotation *popup = [[PDFAnnotationPopup alloc] initWithBounds:CGRectMake(CGRectGetMaxX(bounds) + 4, CGRectGetMaxY(bounds) - 120, 180, 120)
                                                                 forType:PDFAnnotationSubtypePopup withProperties:nil];
        values[PDFAnnotationKeyPopup] = popup;
    }
    for (id key in [values allKeys])
        if (values[key] == [NSNull null])
            [values removeObjectForKey:key];
    annotation.page = page;
    return annotation;
}

@end
