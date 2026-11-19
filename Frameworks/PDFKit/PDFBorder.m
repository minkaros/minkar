/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <PDFKit/PDFBorder.h>

PDFBorderKey const PDFBorderKeyLineWidth = @"W";
PDFBorderKey const PDFBorderKeyStyle = @"S";
PDFBorderKey const PDFBorderKeyDashPattern = @"D";

@implementation PDFBorder

- (instancetype)init {
    if ((self = [super init]))
        _lineWidth = 1;
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    PDFBorder *copy = [[PDFBorder allocWithZone:zone] init];

    copy.style = _style;
    copy.lineWidth = _lineWidth;
    copy.dashPattern = _dashPattern;
    return copy;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [self init])) {
        _style = [coder decodeIntegerForKey:@"style"];
        _lineWidth = [coder decodeDoubleForKey:@"lineWidth"];
        _dashPattern = [coder decodeObjectForKey:@"dashPattern"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeInteger:_style forKey:@"style"];
    [coder encodeDouble:_lineWidth forKey:@"lineWidth"];
    [coder encodeObject:_dashPattern forKey:@"dashPattern"];
}

/* As a border style dictionary (BS): its width, style name, dashes. */
- (NSDictionary *)borderKeyValues {
    static NSString *styles[] = { @"S", @"D", @"B", @"I", @"U" };
    NSMutableDictionary *values = [NSMutableDictionary dictionary];

    values[PDFBorderKeyLineWidth] = @(_lineWidth);
    values[PDFBorderKeyStyle] = styles[(_style >= 0 && _style <= 4) ? _style : 0];
    if (_dashPattern.count > 0)
        values[PDFBorderKeyDashPattern] = _dashPattern;
    return values;
}

- (void)drawInRect:(PDFRect)rect {
    NSBezierPath *path = [NSBezierPath bezierPathWithRect:NSInsetRect(rect, _lineWidth / 2, _lineWidth / 2)];

    if (_lineWidth <= 0)
        return;
    path.lineWidth = _lineWidth;
    if (_style == kPDFBorderStyleDashed && _dashPattern.count > 0) {
        CGFloat dashes[_dashPattern.count];
        for (NSUInteger i = 0; i < _dashPattern.count; i++)
            dashes[i] = [_dashPattern[i] doubleValue];
        [path setLineDash:dashes count:_dashPattern.count phase:0];
    }
    [path stroke];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p width %g style %ld>", [self class], self, _lineWidth, (long)_style];
}

@end
