/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <PDFKit/PDFAnnotationSubclasses.h>

@implementation PDFAnnotationText
@end

@implementation PDFAnnotationLink
@end

@implementation PDFAnnotationMarkup
@end

@implementation PDFAnnotationFreeText
@end

@implementation PDFAnnotationSquare
@end

@implementation PDFAnnotationCircle
@end

@implementation PDFAnnotationLine
@end

@implementation PDFAnnotationInk
@end

@implementation PDFAnnotationStamp
@end

@implementation PDFAnnotationPopup
@end

@implementation PDFAnnotationButtonWidget
- (PDFWidgetControlType)controlType { return self.widgetControlType; }
- (void)setControlType:(PDFWidgetControlType)type { self.widgetControlType = type; }
- (NSInteger)state { return self.buttonWidgetState; }
- (void)setState:(NSInteger)state { self.buttonWidgetState = state; }
- (NSString *)onStateValue { return self.buttonWidgetStateString; }
- (void)setOnStateValue:(NSString *)value { self.buttonWidgetStateString = value; }
/* A radio button's flags: no toggle to off bit 15, in unison bit 26. */
- (BOOL)allowsToggleToOff { return ([[self valueForAnnotationKey:PDFAnnotationKeyWidgetFieldFlags] integerValue] & (1 << 14)) == 0; }
- (void)setAllowsToggleToOff:(BOOL)allows {
    NSInteger flags = [[self valueForAnnotationKey:PDFAnnotationKeyWidgetFieldFlags] integerValue];
    [self setValue:@(allows ? (flags & ~(1 << 14)) : (flags | (1 << 14))) forAnnotationKey:PDFAnnotationKeyWidgetFieldFlags];
}
- (BOOL)radiosInUnison { return ([[self valueForAnnotationKey:PDFAnnotationKeyWidgetFieldFlags] integerValue] & (1 << 25)) != 0; }
- (void)setRadiosInUnison:(BOOL)unison {
    NSInteger flags = [[self valueForAnnotationKey:PDFAnnotationKeyWidgetFieldFlags] integerValue];
    [self setValue:@(unison ? (flags | (1 << 25)) : (flags & ~(1 << 25))) forAnnotationKey:PDFAnnotationKeyWidgetFieldFlags];
}
@end

@implementation PDFAnnotationChoiceWidget
- (NSString *)stringValue { return self.widgetStringValue; }
- (void)setStringValue:(NSString *)value { self.widgetStringValue = value; }
@end

@implementation PDFAnnotationTextWidget
- (NSString *)stringValue { return self.widgetStringValue; }
- (void)setStringValue:(NSString *)value { self.widgetStringValue = value; }
- (NSInteger)rotation { return [[self valueForAnnotationKey:PDFAnnotationKeyWidgetRotation] integerValue]; }
- (void)setRotation:(NSInteger)rotation { [self setValue:@(rotation) forAnnotationKey:PDFAnnotationKeyWidgetRotation]; }
@end

