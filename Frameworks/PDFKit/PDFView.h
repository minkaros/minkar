/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <PDFKit/PDFKitPlatform.h>
#import <PDFKit/PDFPage.h>

@class PDFDocument, PDFSelection, PDFDestination, PDFAnnotation, PDFAction;
@protocol PDFViewDelegate;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, PDFDisplayMode) {
    kPDFDisplaySinglePage = 0,
    kPDFDisplaySinglePageContinuous = 1,
    kPDFDisplayTwoUp = 2,
    kPDFDisplayTwoUpContinuous = 3
};

typedef NS_ENUM(NSInteger, PDFDisplayDirection) {
    kPDFDisplayDirectionVertical = 0,
    kPDFDisplayDirectionHorizontal = 1
};

typedef NS_OPTIONS(NSInteger, PDFAreaOfInterest) {
    kPDFNoArea = 0,
    kPDFPageArea = (1UL << 0),
    kPDFTextArea = (1UL << 1),
    kPDFAnnotationArea = (1UL << 2),
    kPDFLinkArea = (1UL << 3),
    kPDFControlArea = (1UL << 4),
    kPDFTextFieldArea = (1UL << 5),
    kPDFIconArea = (1UL << 6),
    kPDFPopupArea = (1UL << 7),
    kPDFImageArea = (1UL << 8),
    kPDFAnyArea = NSIntegerMax
};

typedef NS_ENUM(NSInteger, PDFInterpolationQuality) {
    kPDFInterpolationQualityNone = 0,
    kPDFInterpolationQualityLow = 1,
    kPDFInterpolationQualityHigh = 2
};

PDFKIT_EXTERN NSNotificationName const PDFViewDocumentChangedNotification;
PDFKIT_EXTERN NSNotificationName const PDFViewChangedHistoryNotification;
PDFKIT_EXTERN NSNotificationName const PDFViewPageChangedNotification;
PDFKIT_EXTERN NSNotificationName const PDFViewScaleChangedNotification;
PDFKIT_EXTERN NSNotificationName const PDFViewAnnotationHitNotification;
PDFKIT_EXTERN NSNotificationName const PDFViewCopyPermissionNotification;
PDFKIT_EXTERN NSNotificationName const PDFViewPrintPermissionNotification;
PDFKIT_EXTERN NSNotificationName const PDFViewAnnotationWillHitNotification;
PDFKIT_EXTERN NSNotificationName const PDFViewSelectionChangedNotification;
PDFKIT_EXTERN NSNotificationName const PDFViewDisplayModeChangedNotification;
PDFKIT_EXTERN NSNotificationName const PDFViewDisplayBoxChangedNotification;
PDFKIT_EXTERN NSNotificationName const PDFViewVisiblePagesChangedNotification;

/* A document shown: its pages laid out in a scroll view (one or two a row,
   continuous or not), scaled, selectable, its links followed. */
PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFView : NSView <NSMenuDelegate>

@property (nonatomic, strong, nullable) PDFDocument *document;
@property (nonatomic, weak, nullable) id<PDFViewDelegate> delegate;

/* Pages */
@property (nonatomic, readonly) BOOL canGoToFirstPage;
@property (nonatomic, readonly) BOOL canGoToLastPage;
@property (nonatomic, readonly) BOOL canGoToNextPage;
@property (nonatomic, readonly) BOOL canGoToPreviousPage;
@property (nonatomic, readonly) BOOL canGoBack;
@property (nonatomic, readonly) BOOL canGoForward;
- (IBAction)goToFirstPage:(nullable id)sender;
- (IBAction)goToLastPage:(nullable id)sender;
- (IBAction)goToNextPage:(nullable id)sender;
- (IBAction)goToPreviousPage:(nullable id)sender;
- (IBAction)goBack:(nullable id)sender;
- (IBAction)goForward:(nullable id)sender;
@property (nonatomic, readonly, nullable) PDFPage *currentPage;
- (void)goToPage:(PDFPage *)page;
@property (nonatomic, readonly, nullable) PDFDestination *currentDestination;
- (void)goToDestination:(PDFDestination *)destination;
- (void)goToSelection:(PDFSelection *)selection;
- (void)goToRect:(PDFRect)rect onPage:(PDFPage *)page;

/* Display */
@property (nonatomic) PDFDisplayMode displayMode;
@property (nonatomic) PDFDisplayDirection displayDirection;
@property (nonatomic) BOOL displaysPageBreaks;
@property (nonatomic) PDFEdgeInsets pageBreakMargins;
@property (nonatomic) PDFDisplayBox displayBox;
@property (nonatomic) BOOL displaysAsBook;
@property (nonatomic) BOOL displaysRTL;
@property (nonatomic, copy, nullable) NSColor *backgroundColor;
@property (nonatomic) PDFInterpolationQuality interpolationQuality;
@property (nonatomic) CGFloat greekingThreshold;
@property (nonatomic) BOOL shouldAntiAlias;
@property (nonatomic) BOOL enableDataDetectors;
@property (nonatomic) BOOL acceptsDraggedFiles;
@property (nonatomic, readonly, nullable) NSView *documentView;
- (void)layoutDocumentView;
- (void)annotationsChangedOnPage:(PDFPage *)page;

/* Scale */
@property (nonatomic) CGFloat scaleFactor;
@property (nonatomic) CGFloat minScaleFactor;
@property (nonatomic) CGFloat maxScaleFactor;
@property (nonatomic) BOOL autoScales;
@property (nonatomic, readonly) CGFloat scaleFactorForSizeToFit;
- (IBAction)zoomIn:(nullable id)sender;
- (IBAction)zoomOut:(nullable id)sender;
@property (nonatomic, readonly) BOOL canZoomIn;
@property (nonatomic, readonly) BOOL canZoomOut;

/* Geometry */
- (PDFAreaOfInterest)areaOfInterestForMouse:(NSEvent *)event;
- (PDFAreaOfInterest)areaOfInterestForPoint:(PDFPoint)point;
- (void)setCursorForAreaOfInterest:(PDFAreaOfInterest)area;
- (nullable PDFPage *)pageForPoint:(PDFPoint)point nearest:(BOOL)nearest;
- (PDFPoint)convertPoint:(PDFPoint)point toPage:(PDFPage *)page;
- (PDFRect)convertRect:(PDFRect)rect toPage:(PDFPage *)page;
- (PDFPoint)convertPoint:(PDFPoint)point fromPage:(PDFPage *)page;
- (PDFRect)convertRect:(PDFRect)rect fromPage:(PDFPage *)page;
- (PDFSize)rowSizeForPage:(PDFPage *)page;
@property (nonatomic, readonly) NSArray<PDFPage *> *visiblePages;

/* Selection */
@property (nonatomic, copy, nullable) PDFSelection *currentSelection;
- (void)setCurrentSelection:(nullable PDFSelection *)selection animate:(BOOL)animate;
- (void)clearSelection;
- (IBAction)selectAll:(nullable id)sender;
- (void)scrollSelectionToVisible:(nullable id)sender;
@property (nonatomic, copy, nullable) NSArray<PDFSelection *> *highlightedSelections;
@property (nonatomic, readonly) BOOL canCopy;
- (IBAction)copy:(nullable id)sender;
- (void)performAction:(PDFAction *)action;

/* Drawing and printing */
- (void)drawPage:(PDFPage *)page toContext:(CGContextRef)context;
- (void)drawPagePost:(PDFPage *)page toContext:(CGContextRef)context;
- (void)drawPage:(PDFPage *)page;
- (void)drawPagePost:(PDFPage *)page;
- (void)printWithInfo:(NSPrintInfo *)printInfo autoRotate:(BOOL)doRotate;
- (void)printWithInfo:(NSPrintInfo *)printInfo autoRotate:(BOOL)doRotate pageScaling:(NSInteger)scale;
@end

@protocol PDFViewDelegate <NSObject>
@optional
- (CGFloat)PDFViewWillChangeScaleFactor:(PDFView *)sender toScale:(CGFloat)scaler;
- (void)PDFViewWillClickOnLink:(PDFView *)sender withURL:(NSURL *)url;
- (NSString *)PDFViewPrintJobTitle:(PDFView *)sender;
- (void)PDFViewPerformFind:(PDFView *)sender;
- (void)PDFViewPerformGoToPage:(PDFView *)sender;
- (void)PDFViewPerformPrint:(PDFView *)sender;
- (void)PDFViewOpenPDF:(PDFView *)sender forRemoteGoToAction:(PDFAction *)action;
- (nullable PDFDestination *)PDFViewParentForDestination:(PDFDestination *)destination;
@end

NS_ASSUME_NONNULL_END
