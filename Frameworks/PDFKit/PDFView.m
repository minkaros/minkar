/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A document shown, as the reference lays it out (measured 19 Nov 2026):
 * a document view (PDFDocumentView) in page units, scaled by its scroll
 * view's magnification; each page in a cell its size and the page break
 * margins (4.75 above and below, 4 at the sides), one or two a row,
 * centered; but for single page continuous, one margin wider all round; the
 * scale from 0.1 (no upper limit set), zooming by the fourth root of 2.
 */

#import "PDFKitPrivate.h"
#import <PDFKit/PDFView.h>
#import <PDFKit/PDFAnnotation.h>

NSNotificationName const PDFViewDocumentChangedNotification = @"PDFViewChangedDocument";
NSNotificationName const PDFViewChangedHistoryNotification = @"PDFViewChangedHistory";
NSNotificationName const PDFViewPageChangedNotification = @"PDFViewChangedPage";
NSNotificationName const PDFViewScaleChangedNotification = @"PDFViewScaleChanged";
NSNotificationName const PDFViewAnnotationHitNotification = @"PDFViewAnnotationHit";
NSNotificationName const PDFViewCopyPermissionNotification = @"PDFViewCopyPermission";
NSNotificationName const PDFViewPrintPermissionNotification = @"PDFViewPrintPermission";
NSNotificationName const PDFViewAnnotationWillHitNotification = @"PDFViewAnnotationWillHit";
NSNotificationName const PDFViewSelectionChangedNotification = @"PDFViewSelectionChanged";
NSNotificationName const PDFViewDisplayModeChangedNotification = @"PDFViewDisplayModeChanged";
NSNotificationName const PDFViewDisplayBoxChangedNotification = @"PDFViewDisplayBoxChanged";
NSNotificationName const PDFViewVisiblePagesChangedNotification = @"PDFViewVisiblePagesChanged";

static const CGFloat zoomStep = 1.189207115002721;   // 2^(1/4)

@class PDFView;

/* The pages, laid out top down (flipped), in page units. */
@interface PDFDocumentView : NSView
@property (nonatomic, weak) PDFView *pdfView;
@end

@interface PDFView ()
- (NSArray<PDFPage *> *)_shownPages;
- (NSRect)_rectForPage:(PDFPage *)page;
- (void)_drawPagesInRect:(NSRect)rect;
- (void)_documentViewMouseDown:(NSEvent *)event;
- (void)_documentViewMouseDragged:(NSEvent *)event;
- (void)_documentViewMouseUp:(NSEvent *)event;
@end

@implementation PDFDocumentView

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)isOpaque {
    return NO;
}

- (void)drawRect:(NSRect)dirty {
    [_pdfView _drawPagesInRect:dirty];
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)mouseDown:(NSEvent *)event { [_pdfView _documentViewMouseDown:event]; }
- (void)mouseDragged:(NSEvent *)event { [_pdfView _documentViewMouseDragged:event]; }
- (void)mouseUp:(NSEvent *)event { [_pdfView _documentViewMouseUp:event]; }

@end

@implementation PDFView {
    NSScrollView *_scrollView;
    PDFDocumentView *_documentView;
    NSMapTable<PDFPage *, NSValue *> *_pageRects;  // in the document view
    NSMutableArray<PDFDestination *> *_back, *_forward;
    PDFPage *_lastCurrentPage;
    NSArray<PDFPage *> *_lastVisiblePages;
    NSPoint _dragStart;
    PDFPage *_dragPage;
    PDFSelection *_currentSelection;
}

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame]))
        [self _setUp];
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super initWithCoder:coder]))
        [self _setUp];
    return self;
}

- (void)_setUp {
    _displayMode = kPDFDisplaySinglePageContinuous;
    _displaysPageBreaks = YES;
    _pageBreakMargins = NSEdgeInsetsMake(4.75, 4, 4.75, 4);
    _displayBox = kPDFDisplayBoxCropBox;
    _interpolationQuality = kPDFInterpolationQualityHigh;
    _shouldAntiAlias = YES;
    _minScaleFactor = 0.1;
    _maxScaleFactor = 20;
    _back = [NSMutableArray array];
    _forward = [NSMutableArray array];
    _pageRects = [NSMapTable strongToStrongObjectsMapTable];
    _scrollView = [[NSScrollView alloc] initWithFrame:self.bounds];
    _scrollView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _scrollView.hasVerticalScroller = YES;
    _scrollView.hasHorizontalScroller = YES;
    _scrollView.autohidesScrollers = YES;
    _scrollView.drawsBackground = YES;
    _scrollView.backgroundColor = [NSColor colorWithDeviceWhite:0.5 alpha:1];
    _scrollView.minMagnification = 0.1;
    _scrollView.maxMagnification = 1000;
    _documentView = [[PDFDocumentView alloc] initWithFrame:NSZeroRect];
    _documentView.pdfView = self;
    _scrollView.documentView = _documentView;
    [self addSubview:_scrollView];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_boundsChanged:) name:NSViewBoundsDidChangeNotification object:_scrollView.contentView];
    _scrollView.contentView.postsBoundsChangedNotifications = YES;
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (BOOL)isFlipped {
    return NO;
}

- (void)_post:(NSNotificationName)name {
    [[NSNotificationCenter defaultCenter] postNotificationName:name object:self];
}

/* ---- document ---- */

- (void)setDocument:(PDFDocument *)document {
    if (document == _document)
        return;
    _document = document;
    _currentSelection = nil;
    [_back removeAllObjects];
    [_forward removeAllObjects];
    [self layoutDocumentView];
    [self _scrollToTop];
    [self _post:PDFViewDocumentChangedNotification];
    [self _checkCurrentPage];
}

- (NSView *)documentView {
    return _documentView;
}

/* ---- layout ---- */

- (BOOL)_isContinuous {
    return _displayMode == kPDFDisplaySinglePageContinuous || _displayMode == kPDFDisplayTwoUpContinuous;
}

- (BOOL)_isTwoUp {
    return _displayMode == kPDFDisplayTwoUp || _displayMode == kPDFDisplayTwoUpContinuous;
}

/* A page's size on screen: its display box turned by its rotation. */
- (NSSize)_pageSize:(PDFPage *)page {
    CGRect box = [page boundsForBox:_displayBox];

    return (page.rotation % 180) ? NSMakeSize(box.size.height, box.size.width) : NSMakeSize(box.size.width, box.size.height);
}

/* The pages on screen: all, or (not continuous) the current one's row. */
- (NSArray<PDFPage *> *)_shownPages {
    NSMutableArray *pages = [NSMutableArray array];
    NSUInteger count = _document.pageCount;

    if ([self _isContinuous]) {
        for (NSUInteger i = 0; i < count; i++)
            [pages addObject:[_document pageAtIndex:i]];
    } else if (count > 0) {
        NSUInteger current = _lastCurrentPage ? [_document indexForPage:_lastCurrentPage] : 0;

        if (current == NSNotFound) current = 0;
        if ([self _isTwoUp]) {
            NSUInteger first = (_displaysAsBook ? ((current + 1) / 2) * 2 - 1 : (current / 2) * 2);
            if ((NSInteger)first < 0) first = 0;
            for (NSUInteger i = first; i < MIN(first + ((_displaysAsBook && first == 0 && current == 0) ? 1 : 2), count); i++)
                [pages addObject:[_document pageAtIndex:i]];
        } else
            [pages addObject:[_document pageAtIndex:current]];
    }
    return pages;
}

- (void)layoutDocumentView {
    NSArray<PDFPage *> *pages = [self _shownPages];
    PDFEdgeInsets m = _displaysPageBreaks ? _pageBreakMargins : NSEdgeInsetsMake(0, 0, 0, 0);
    /* One margin more all round but for a single page continuous, as on
       the reference (two up continuous has it too). */
    BOOL extra = !(_displayMode == kPDFDisplaySinglePageContinuous);
    CGFloat extraW = extra ? m.left + m.right : 0, extraH = extra ? m.top + m.bottom : 0;
    NSMutableArray<NSArray<PDFPage *> *> *rows = [NSMutableArray array];
    CGFloat width = 0, height = 0;

    [_pageRects removeAllObjects];
    /* Rows: one page, or two (a book's first page alone). */
    for (NSUInteger i = 0; i < pages.count;) {
        NSUInteger take = [self _isTwoUp] ? ((_displaysAsBook && [_document indexForPage:pages[i]] == 0) ? 1 : 2) : 1;
        take = MIN(take, pages.count - i);
        [rows addObject:[pages subarrayWithRange:NSMakeRange(i, take)]];
        i += take;
    }
    for (NSArray<PDFPage *> *row in rows) {
        CGFloat rowWidth = 0, rowHeight = 0;
        for (PDFPage *page in row) {
            NSSize size = [self _pageSize:page];
            rowWidth += size.width + m.left + m.right;
            rowHeight = MAX(rowHeight, size.height + m.top + m.bottom);
        }
        width = MAX(width, rowWidth);
        height += rowHeight;
    }
    width += extraW;
    height += extraH;
    /* Each row centered across, its pages side by side. */
    CGFloat y = extraH / 2;
    for (NSArray<PDFPage *> *row in rows) {
        CGFloat rowWidth = 0, rowHeight = 0, x;
        for (PDFPage *page in row) {
            NSSize size = [self _pageSize:page];
            rowWidth += size.width + m.left + m.right;
            rowHeight = MAX(rowHeight, size.height + m.top + m.bottom);
        }
        x = floor((width - rowWidth) / 2);
        for (PDFPage *page in row) {
            NSSize size = [self _pageSize:page];
            [_pageRects setObject:[NSValue valueWithRect:NSMakeRect(x + m.left, y + m.top + floor((rowHeight - m.top - m.bottom - size.height) / 2), size.width, size.height)]
                           forKey:page];
            x += size.width + m.left + m.right;
        }
        y += rowHeight;
    }
    [_documentView setFrameSize:NSMakeSize(width, height)];
    [_documentView setNeedsDisplay:YES];
    if (_autoScales)
        [self _applyScale:self.scaleFactorForSizeToFit notify:YES];
}

- (NSRect)_rectForPage:(PDFPage *)page {
    NSValue *rect = page ? [_pageRects objectForKey:page] : nil;

    return rect ? rect.rectValue : NSZeroRect;
}

- (PDFSize)rowSizeForPage:(PDFPage *)page {
    NSRect rect = [self _rectForPage:page];
    PDFEdgeInsets m = _displaysPageBreaks ? _pageBreakMargins : NSEdgeInsetsMake(0, 0, 0, 0);

    return CGSizeMake(_documentView.frame.size.width, rect.size.height + m.top + m.bottom);
}

- (void)setFrameSize:(NSSize)size {
    [super setFrameSize:size];
    if (_autoScales)
        [self _applyScale:self.scaleFactorForSizeToFit notify:YES];
}

#define RELAYOUT(property, value, note) \
    if (_##property == (value)) return; \
    _##property = (value); \
    [self layoutDocumentView]; \
    if (note) [self _post:note];

- (void)setDisplayMode:(PDFDisplayMode)mode { RELAYOUT(displayMode, mode, PDFViewDisplayModeChangedNotification) }
- (void)setDisplaysPageBreaks:(BOOL)breaks { RELAYOUT(displaysPageBreaks, breaks, nil) }
- (void)setDisplayBox:(PDFDisplayBox)box { RELAYOUT(displayBox, box, PDFViewDisplayBoxChangedNotification) }
- (void)setDisplaysAsBook:(BOOL)book { RELAYOUT(displaysAsBook, book, nil) }
- (void)setDisplaysRTL:(BOOL)rtl { RELAYOUT(displaysRTL, rtl, nil) }
- (void)setDisplayDirection:(PDFDisplayDirection)direction { RELAYOUT(displayDirection, direction, nil) }

- (void)setPageBreakMargins:(PDFEdgeInsets)margins {
    _pageBreakMargins = margins;
    [self layoutDocumentView];
}

- (void)setBackgroundColor:(NSColor *)color {
    _backgroundColor = [color copy];
    _scrollView.backgroundColor = color ?: [NSColor colorWithDeviceWhite:0.5 alpha:1];
    [_scrollView setNeedsDisplay:YES];
}

- (void)annotationsChangedOnPage:(PDFPage *)page {
    NSRect rect = [self _rectForPage:page];

    if (!NSIsEmptyRect(rect))
        [_documentView setNeedsDisplayInRect:rect];
}

/* ---- scale ---- */

- (CGFloat)scaleFactor {
    return _scrollView.magnification;
}

- (void)_applyScale:(CGFloat)scale notify:(BOOL)notify {
    if (scale < _minScaleFactor)
        scale = _minScaleFactor;
    if ([_delegate respondsToSelector:@selector(PDFViewWillChangeScaleFactor:toScale:)])
        scale = [_delegate PDFViewWillChangeScaleFactor:self toScale:scale];
    if (fabs(scale - _scrollView.magnification) < 1e-9)
        return;
    _scrollView.magnification = scale;
    if (notify)
        [self _post:PDFViewScaleChangedNotification];
}

/* Setting a scale ends scaling to fit, as on the reference. */
- (void)setScaleFactor:(CGFloat)scale {
    _autoScales = NO;
    [self _applyScale:scale notify:YES];
}

- (void)setMinScaleFactor:(CGFloat)scale {
    _minScaleFactor = scale;
}

- (void)setAutoScales:(BOOL)autoScales {
    _autoScales = autoScales;
    if (autoScales)
        [self _applyScale:self.scaleFactorForSizeToFit notify:YES];
}

/* The scale at which the document's width fills the view. */
- (CGFloat)scaleFactorForSizeToFit {
    CGFloat width = _documentView.frame.size.width;

    return (width > 0) ? self.bounds.size.width / width : 1;
}

- (BOOL)canZoomIn { return self.scaleFactor < _maxScaleFactor; }
- (BOOL)canZoomOut { return self.scaleFactor > _minScaleFactor; }
- (IBAction)zoomIn:(id)sender { self.scaleFactor = self.scaleFactor * zoomStep; }
- (IBAction)zoomOut:(id)sender { self.scaleFactor = self.scaleFactor / zoomStep; }

/* ---- geometry ---- */

/* A point on a page in the document view, and back: the page's display
   box turned by its rotation, the document view flipped. */
- (NSPoint)_documentPoint:(PDFPoint)point onPage:(PDFPage *)page {
    NSRect rect = [self _rectForPage:page];
    CGPoint rotated = CGPointApplyAffineTransform(point, [page transformForBox:_displayBox]);

    return NSMakePoint(NSMinX(rect) + rotated.x, NSMaxY(rect) - rotated.y);
}

- (PDFPoint)_pagePoint:(NSPoint)point onPage:(PDFPage *)page {
    NSRect rect = [self _rectForPage:page];
    CGPoint rotated = CGPointMake(point.x - NSMinX(rect), NSMaxY(rect) - point.y);

    return CGPointApplyAffineTransform(rotated, CGAffineTransformInvert([page transformForBox:_displayBox]));
}

- (PDFPoint)convertPoint:(PDFPoint)point fromPage:(PDFPage *)page {
    return NSPointToCGPoint([self convertPoint:[self _documentPoint:point onPage:page] fromView:_documentView]);
}

- (PDFPoint)convertPoint:(PDFPoint)point toPage:(PDFPage *)page {
    return [self _pagePoint:[self convertPoint:NSPointFromCGPoint(point) toView:_documentView] onPage:page];
}

- (PDFRect)convertRect:(PDFRect)rect fromPage:(PDFPage *)page {
    CGPoint a = [self convertPoint:rect.origin fromPage:page];
    CGPoint b = [self convertPoint:CGPointMake(CGRectGetMaxX(rect), CGRectGetMaxY(rect)) fromPage:page];

    return CGRectStandardize(CGRectMake(a.x, a.y, b.x - a.x, b.y - a.y));
}

- (PDFRect)convertRect:(PDFRect)rect toPage:(PDFPage *)page {
    CGPoint a = [self convertPoint:rect.origin toPage:page];
    CGPoint b = [self convertPoint:CGPointMake(CGRectGetMaxX(rect), CGRectGetMaxY(rect)) toPage:page];

    return CGRectStandardize(CGRectMake(a.x, a.y, b.x - a.x, b.y - a.y));
}

/* The page under point (in the view), or the nearest one. */
- (PDFPage *)pageForPoint:(PDFPoint)point nearest:(BOOL)nearest {
    NSPoint p = [self convertPoint:NSPointFromCGPoint(point) toView:_documentView];
    PDFPage *best = nil;
    CGFloat bestDistance = CGFLOAT_MAX;

    for (PDFPage *page in [self _shownPages]) {
        NSRect rect = [self _rectForPage:page];
        CGFloat dx = MAX(0, MAX(NSMinX(rect) - p.x, p.x - NSMaxX(rect))), dy = MAX(0, MAX(NSMinY(rect) - p.y, p.y - NSMaxY(rect)));

        if (dx == 0 && dy == 0)
            return page;
        if (dx * dx + dy * dy < bestDistance) {
            bestDistance = dx * dx + dy * dy;
            best = page;
        }
    }
    return nearest ? best : nil;
}

- (NSArray<PDFPage *> *)visiblePages {
    NSRect visible = _scrollView.documentVisibleRect;
    NSMutableArray *pages = [NSMutableArray array];

    for (PDFPage *page in [self _shownPages])
        if (NSIntersectsRect([self _rectForPage:page], visible))
            [pages addObject:page];
    return pages;
}

- (PDFAreaOfInterest)areaOfInterestForPoint:(PDFPoint)point {
    PDFPage *page = [self pageForPoint:point nearest:NO];
    PDFAreaOfInterest area = kPDFNoArea;
    PDFAnnotation *annotation;
    CGPoint onPage;

    if (page == nil)
        return kPDFNoArea;
    area |= kPDFPageArea;
    onPage = [self convertPoint:point toPage:page];
    if ([page characterIndexAtPoint:onPage] != NSNotFound)
        area |= kPDFTextArea;
    if ((annotation = [page annotationAtPoint:onPage]) != nil) {
        area |= kPDFAnnotationArea;
        if ([annotation.type isEqual:@"Link"])
            area |= kPDFLinkArea;
        if ([annotation.type isEqual:@"Widget"])
            area |= [annotation.widgetFieldType isEqual:PDFAnnotationWidgetSubtypeText] ? kPDFTextFieldArea : kPDFControlArea;
        if ([annotation.type isEqual:@"Text"])
            area |= kPDFIconArea;
    }
    return area;
}

- (PDFAreaOfInterest)areaOfInterestForMouse:(NSEvent *)event {
    return [self areaOfInterestForPoint:NSPointToCGPoint([self convertPoint:event.locationInWindow fromView:nil])];
}

- (void)setCursorForAreaOfInterest:(PDFAreaOfInterest)area {
    if (area & kPDFLinkArea)
        [[NSCursor pointingHandCursor] set];
    else if (area & (kPDFTextArea | kPDFTextFieldArea))
        [[NSCursor IBeamCursor] set];
    else
        [[NSCursor arrowCursor] set];
}

/* ---- pages and history ---- */

/* The page whose top is nearest the top of the visible area (the first
   one shown when not continuous). */
- (PDFPage *)currentPage {
    NSRect visible = _scrollView.documentVisibleRect;
    PDFPage *best = nil;

    for (PDFPage *page in [self _shownPages]) {
        NSRect rect = [self _rectForPage:page];

        if (NSMaxY(rect) > NSMinY(visible) + 1) {
            best = page;
            break;
        }
    }
    return best ?: [self _shownPages].lastObject;
}

- (PDFDestination *)currentDestination {
    PDFPage *page = self.currentPage;
    NSRect visible = _scrollView.documentVisibleRect;

    if (page == nil)
        return nil;
    return [[PDFDestination alloc] initWithPage:page atPoint:[self _pagePoint:visible.origin onPage:page]];
}

- (void)_checkCurrentPage {
    PDFPage *page = self.currentPage;
    NSArray *visible = self.visiblePages;

    if (page != _lastCurrentPage) {
        _lastCurrentPage = page;
        [self _post:PDFViewPageChangedNotification];
    }
    if (![visible isEqualToArray:_lastVisiblePages ?: @[]]) {
        _lastVisiblePages = visible;
        [self _post:PDFViewVisiblePagesChangedNotification];
    }
}

- (void)_boundsChanged:(NSNotification *)note {
    [self _checkCurrentPage];
}

- (void)_scrollToTop {
    [_scrollView.contentView scrollToPoint:NSMakePoint(0, 0)];
    [_scrollView reflectScrolledClipView:_scrollView.contentView];
}

/* Where the view is, remembered for going back. */
- (void)_pushHistory {
    PDFDestination *here = self.currentDestination;

    if (here == nil)
        return;
    [_back addObject:here];
    [_forward removeAllObjects];
    [self _post:PDFViewChangedHistoryNotification];
}

- (void)_showPage:(PDFPage *)page atDocumentPoint:(NSPoint)point {
    if (page == nil)
        return;
    if (![self _isContinuous] && ![[self _shownPages] containsObject:page]) {
        _lastCurrentPage = page;
        [self layoutDocumentView];
    }
    [_scrollView.contentView scrollToPoint:[_scrollView.contentView constrainScrollPoint:point]];
    [_scrollView reflectScrolledClipView:_scrollView.contentView];
    [self _checkCurrentPage];
}

- (void)goToPage:(PDFPage *)page {
    NSRect rect;

    if (page == nil || [_document indexForPage:page] == NSNotFound)
        return;
    [self _pushHistory];
    if (![self _isContinuous]) {
        _lastCurrentPage = page;
        [self layoutDocumentView];
    }
    rect = [self _rectForPage:page];
    [self _showPage:page atDocumentPoint:NSMakePoint(_scrollView.documentVisibleRect.origin.x, NSMinY(rect) - (_displaysPageBreaks ? _pageBreakMargins.top : 0))];
}

- (void)goToDestination:(PDFDestination *)destination {
    PDFPage *page = destination.page;
    NSPoint point;

    if (page == nil)
        return;
    [self _pushHistory];
    if (![self _isContinuous]) {
        _lastCurrentPage = page;
        [self layoutDocumentView];
    }
    if (destination.zoom != kPDFDestinationUnspecifiedValue && destination.zoom > 0)
        [self setScaleFactor:destination.zoom];
    CGPoint target = destination.point;
    CGRect box = [page boundsForBox:_displayBox];
    if (target.x == kPDFDestinationUnspecifiedValue) target.x = CGRectGetMinX(box);
    if (target.y == kPDFDestinationUnspecifiedValue) target.y = CGRectGetMaxY(box);
    point = [self _documentPoint:target onPage:page];
    [self _showPage:page atDocumentPoint:point];
}

- (void)goToRect:(PDFRect)rect onPage:(PDFPage *)page {
    [self goToDestination:[[PDFDestination alloc] initWithPage:page atPoint:CGPointMake(CGRectGetMinX(rect), CGRectGetMaxY(rect))]];
}

- (void)goToSelection:(PDFSelection *)selection {
    PDFPage *page = selection.pages.firstObject;

    if (page != nil)
        [self goToRect:[selection boundsForPage:page] onPage:page];
}

- (NSUInteger)_currentIndex {
    NSUInteger index = self.currentPage ? [_document indexForPage:self.currentPage] : NSNotFound;
    return index;
}

- (NSUInteger)_step {
    return [self _isTwoUp] ? 2 : 1;
}

- (BOOL)canGoToFirstPage { return _document.pageCount > 0 && [self _currentIndex] > 0; }
- (BOOL)canGoToLastPage { return _document.pageCount > 0 && [self _currentIndex] + [self _step] < _document.pageCount; }
- (BOOL)canGoToNextPage { return [self canGoToLastPage]; }
- (BOOL)canGoToPreviousPage { return [self canGoToFirstPage]; }
- (BOOL)canGoBack { return _back.count > 0; }
- (BOOL)canGoForward { return _forward.count > 0; }

- (IBAction)goToFirstPage:(id)sender { if (_document.pageCount) [self goToPage:[_document pageAtIndex:0]]; }
- (IBAction)goToLastPage:(id)sender { if (_document.pageCount) [self goToPage:[_document pageAtIndex:_document.pageCount - 1]]; }

- (IBAction)goToNextPage:(id)sender {
    NSUInteger index = [self _currentIndex];

    if (index != NSNotFound && index + [self _step] < _document.pageCount)
        [self goToPage:[_document pageAtIndex:index + [self _step]]];
}

- (IBAction)goToPreviousPage:(id)sender {
    NSUInteger index = [self _currentIndex];

    if (index != NSNotFound && index > 0)
        [self goToPage:[_document pageAtIndex:(index >= [self _step]) ? index - [self _step] : 0]];
}

- (void)_goToRemembered:(PDFDestination *)destination {
    NSPoint point = [self _documentPoint:destination.point onPage:destination.page];

    if (![self _isContinuous]) {
        _lastCurrentPage = destination.page;
        [self layoutDocumentView];
    }
    [self _showPage:destination.page atDocumentPoint:point];
}

- (IBAction)goBack:(id)sender {
    PDFDestination *destination = _back.lastObject, *here = self.currentDestination;

    if (destination == nil)
        return;
    [_back removeLastObject];
    if (here) [_forward addObject:here];
    [self _goToRemembered:destination];
    [self _post:PDFViewChangedHistoryNotification];
}

- (IBAction)goForward:(id)sender {
    PDFDestination *destination = _forward.lastObject, *here = self.currentDestination;

    if (destination == nil)
        return;
    [_forward removeLastObject];
    if (here) [_back addObject:here];
    [self _goToRemembered:destination];
    [self _post:PDFViewChangedHistoryNotification];
}

/* ---- drawing ---- */

- (void)drawPage:(PDFPage *)page toContext:(CGContextRef)context {
    [page drawWithBox:_displayBox toContext:context];
}

- (void)drawPagePost:(PDFPage *)page toContext:(CGContextRef)context {
}

- (void)drawPage:(PDFPage *)page {
    [self drawPage:page toContext:[[NSGraphicsContext currentContext] CGContext]];
}

- (void)drawPagePost:(PDFPage *)page {
    [self drawPagePost:page toContext:[[NSGraphicsContext currentContext] CGContext]];
}

/* Each page in its rect: a shadow, the page (its box turned, its origin at
   the rect's lower left), the selection over it. */
- (void)_drawPagesInRect:(NSRect)dirty {
    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];

    for (PDFPage *page in [self _shownPages]) {
        NSRect rect = [self _rectForPage:page];

        if (!NSIntersectsRect(NSInsetRect(rect, -4, -4), dirty))
            continue;
        [[NSColor colorWithDeviceWhite:0 alpha:0.25] setFill];
        NSRectFill(NSOffsetRect(rect, 1, 2));
        CGContextSaveGState(context);
        CGContextTranslateCTM(context, NSMinX(rect), NSMaxY(rect));
        CGContextScaleCTM(context, 1, -1);
        CGContextClipToRect(context, CGRectMake(0, 0, rect.size.width, rect.size.height));
        [self drawPage:page toContext:context];
        /* The selection and highlights, in page space. */
        CGContextConcatCTM(context, [page transformForBox:_displayBox]);
        NSGraphicsContext *saved = [NSGraphicsContext currentContext];
        [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithCGContext:context flipped:NO]];
        for (PDFSelection *highlight in _highlightedSelections)
            [highlight drawForPage:page active:NO];
        [_currentSelection drawForPage:page active:(self.window.firstResponder == _documentView)];
        [NSGraphicsContext setCurrentContext:saved];
        CGContextConcatCTM(context, CGAffineTransformInvert([page transformForBox:_displayBox]));
        [self drawPagePost:page toContext:context];
        CGContextRestoreGState(context);
    }
}

/* ---- selection ---- */

- (PDFSelection *)currentSelection {
    return _currentSelection;
}

- (void)setCurrentSelection:(PDFSelection *)selection {
    [self setCurrentSelection:selection animate:NO];
}

- (void)setCurrentSelection:(PDFSelection *)selection animate:(BOOL)animate {
    _currentSelection = [selection copy];
    [_documentView setNeedsDisplay:YES];
    [self _post:PDFViewSelectionChangedNotification];
}

- (void)setHighlightedSelections:(NSArray<PDFSelection *> *)selections {
    _highlightedSelections = [selections copy];
    [_documentView setNeedsDisplay:YES];
}

- (void)clearSelection {
    self.currentSelection = nil;
}

- (IBAction)selectAll:(id)sender {
    self.currentSelection = _document.selectionForEntireDocument;
}

- (void)scrollSelectionToVisible:(id)sender {
    if (_currentSelection != nil)
        [self goToSelection:_currentSelection];
}

- (BOOL)canCopy {
    return _currentSelection.string.length > 0 && (_document == nil || _document.allowsCopying);
}

- (IBAction)copy:(id)sender {
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];

    if (!_document.allowsCopying) {
        [self _post:PDFViewCopyPermissionNotification];
        return;
    }
    if (_currentSelection.string == nil)
        return;
    [pasteboard clearContents];
    [pasteboard setString:_currentSelection.string forType:NSPasteboardTypeString];
}

- (BOOL)validateMenuItem:(NSMenuItem *)item {
    if (item.action == @selector(copy:))
        return self.canCopy;
    return YES;
}

/* ---- actions and the mouse ---- */

- (void)performAction:(PDFAction *)action {
    if ([action isKindOfClass:[PDFActionGoTo class]])
        [self goToDestination:[(PDFActionGoTo *)action destination]];
    else if ([action isKindOfClass:[PDFActionURL class]]) {
        NSURL *url = [(PDFActionURL *)action URL];

        if ([_delegate respondsToSelector:@selector(PDFViewWillClickOnLink:withURL:)])
            [_delegate PDFViewWillClickOnLink:self withURL:url];
        else if (url)
            [[NSWorkspace sharedWorkspace] openURL:url];
    } else if ([action isKindOfClass:[PDFActionNamed class]]) {
        switch ([(PDFActionNamed *)action name]) {
        case kPDFActionNamedNextPage: [self goToNextPage:nil]; break;
        case kPDFActionNamedPreviousPage: [self goToPreviousPage:nil]; break;
        case kPDFActionNamedFirstPage: [self goToFirstPage:nil]; break;
        case kPDFActionNamedLastPage: [self goToLastPage:nil]; break;
        case kPDFActionNamedGoBack: [self goBack:nil]; break;
        case kPDFActionNamedGoForward: [self goForward:nil]; break;
        case kPDFActionNamedZoomIn: [self zoomIn:nil]; break;
        case kPDFActionNamedZoomOut: [self zoomOut:nil]; break;
        case kPDFActionNamedFind: if ([_delegate respondsToSelector:@selector(PDFViewPerformFind:)]) [_delegate PDFViewPerformFind:self]; break;
        case kPDFActionNamedGoToPage: if ([_delegate respondsToSelector:@selector(PDFViewPerformGoToPage:)]) [_delegate PDFViewPerformGoToPage:self]; break;
        case kPDFActionNamedPrint: if ([_delegate respondsToSelector:@selector(PDFViewPerformPrint:)]) [_delegate PDFViewPerformPrint:self]; break;
        default: break;
        }
    }
}

- (PDFPage *)_pageAtDocumentPoint:(NSPoint)point {
    for (PDFPage *page in [self _shownPages])
        if (NSPointInRect(point, [self _rectForPage:page]))
            return page;
    return nil;
}

/* A click on a link follows it; on another annotation tells it (annotation
   hit); otherwise a selection starts: a word on a double click, a line on
   a triple click. */
- (void)_documentViewMouseDown:(NSEvent *)event {
    NSPoint point = [_documentView convertPoint:event.locationInWindow fromView:nil];
    PDFPage *page = [self _pageAtDocumentPoint:point];
    CGPoint onPage;
    PDFAnnotation *annotation;

    [self.window makeFirstResponder:_documentView];
    _dragPage = nil;
    if (page == nil) {
        [self clearSelection];
        return;
    }
    onPage = [self _pagePoint:point onPage:page];
    if ((annotation = [page annotationAtPoint:onPage]) != nil) {
        if ([annotation.type isEqual:@"Link"] && annotation.action != nil) {
            [self performAction:annotation.action];
            return;
        }
        if ([annotation.type isEqual:@"Widget"]) {
            [[NSNotificationCenter defaultCenter] postNotificationName:PDFViewAnnotationHitNotification object:self userInfo:@{ @"PDFAnnotationHit": annotation }];
            return;
        }
    }
    if (event.clickCount == 2)
        self.currentSelection = [page selectionForWordAtPoint:onPage];
    else if (event.clickCount >= 3)
        self.currentSelection = [page selectionForLineAtPoint:onPage];
    else {
        _dragPage = page;
        _dragStart = onPage;
        if (_currentSelection != nil)
            [self clearSelection];
    }
}

- (void)_documentViewMouseDragged:(NSEvent *)event {
    NSPoint point = [_documentView convertPoint:event.locationInWindow fromView:nil];
    PDFPage *page = [self _pageAtDocumentPoint:point] ?: [self pageForPoint:[self convertPoint:event.locationInWindow fromView:nil] nearest:YES];

    if (_dragPage == nil || page == nil)
        return;
    [_documentView autoscroll:event];
    self.currentSelection = [_document selectionFromPage:_dragPage atPoint:_dragStart toPage:page atPoint:[self _pagePoint:point onPage:page]];
}

- (void)_documentViewMouseUp:(NSEvent *)event {
    _dragPage = nil;
}

- (void)keyDown:(NSEvent *)event {
    NSString *characters = event.charactersIgnoringModifiers;
    unichar c = characters.length ? [characters characterAtIndex:0] : 0;

    switch (c) {
    case NSPageDownFunctionKey: case ' ': [_scrollView pageDown:nil]; break;
    case NSPageUpFunctionKey: [_scrollView pageUp:nil]; break;
    case NSHomeFunctionKey: [self goToFirstPage:nil]; break;
    case NSEndFunctionKey: [self goToLastPage:nil]; break;
    case NSRightArrowFunctionKey: if (![self _isContinuous]) [self goToNextPage:nil]; else [super keyDown:event]; break;
    case NSLeftArrowFunctionKey: if (![self _isContinuous]) [self goToPreviousPage:nil]; else [super keyDown:event]; break;
    default: [super keyDown:event]; break;
    }
}

/* ---- printing ---- */

- (void)printWithInfo:(NSPrintInfo *)printInfo autoRotate:(BOOL)doRotate {
    [self printWithInfo:printInfo autoRotate:doRotate pageScaling:0];
}

- (void)printWithInfo:(NSPrintInfo *)printInfo autoRotate:(BOOL)doRotate pageScaling:(NSInteger)scale {
    if (_document != nil && !_document.allowsPrinting) {
        [self _post:PDFViewPrintPermissionNotification];
        return;
    }
    NSPrintOperation *operation = [NSPrintOperation printOperationWithView:_documentView printInfo:printInfo ?: [NSPrintInfo sharedPrintInfo]];
    if ([_delegate respondsToSelector:@selector(PDFViewPrintJobTitle:)])
        operation.jobTitle = [_delegate PDFViewPrintJobTitle:self];
    [operation runOperation];
}

@end
