/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The legacy annotation subclasses: what an annotation read from a PDF
 * is, by its subtype, on the reference; each property is PDFAnnotation's.
 */

#import <PDFKit/PDFAnnotation.h>

NS_ASSUME_NONNULL_BEGIN

PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFAnnotationText : PDFAnnotation
@end

PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFAnnotationLink : PDFAnnotation
@end

PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFAnnotationMarkup : PDFAnnotation
@end

PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFAnnotationFreeText : PDFAnnotation
@end

PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFAnnotationSquare : PDFAnnotation
@end

PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFAnnotationCircle : PDFAnnotation
@end

PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFAnnotationLine : PDFAnnotation
@end

PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFAnnotationInk : PDFAnnotation
@end

PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFAnnotationStamp : PDFAnnotation
@end

PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFAnnotationPopup : PDFAnnotation
@property (nonatomic, getter=isOpen) BOOL open;
@end

PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFAnnotationButtonWidget : PDFAnnotation
@property (nonatomic) PDFWidgetControlType controlType;
@property (nonatomic) NSInteger state;
@property (nonatomic, copy, nullable) NSString *onStateValue;
@property (nonatomic) BOOL allowsToggleToOff;
@property (nonatomic) BOOL radiosInUnison;
@end

PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFAnnotationChoiceWidget : PDFAnnotation
@property (nonatomic, copy, nullable) NSString *stringValue;
@end

PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFAnnotationTextWidget : PDFAnnotation
@property (nonatomic, copy, nullable) NSString *stringValue;
@property (nonatomic) NSInteger rotation;
@end

NS_ASSUME_NONNULL_END
