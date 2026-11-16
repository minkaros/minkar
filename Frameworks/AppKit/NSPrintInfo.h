/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSGeometry.h>
#import <AppKit/AppKitExport.h>

@class NSDictionary, NSMutableDictionary, NSPrinter;

typedef NSUInteger NSPrintingPaginationMode;
enum {
    NSAutoPagination = 0,
    NSFitPagination = 1,
    NSClipPagination = 2,
};

typedef enum {
    NSPortraitOrientation,
    NSLandscapeOrientation,
} NSPrintingOrientation;

typedef NSInteger NSPaperOrientation;
enum {
    NSPaperOrientationPortrait = 0,
    NSPaperOrientationLandscape = 1,
};

APPKIT_EXPORT NSString *const NSPrintPrinter;
APPKIT_EXPORT NSString *const NSPrintPrinterName;
APPKIT_EXPORT NSString *const NSPrintJobDisposition;
APPKIT_EXPORT NSString *const NSPrintDetailedErrorReporting;

APPKIT_EXPORT NSString *const NSPrintSpoolJob;
APPKIT_EXPORT NSString *const NSPrintPreviewJob;
APPKIT_EXPORT NSString *const NSPrintSaveJob;
APPKIT_EXPORT NSString *const NSPrintCancelJob;

APPKIT_EXPORT NSString *const NSPrintSavePath;

APPKIT_EXPORT NSString *const NSPrintCopies;
APPKIT_EXPORT NSString *const NSPrintAllPages;
APPKIT_EXPORT NSString *const NSPrintFirstPage;
APPKIT_EXPORT NSString *const NSPrintLastPage;

APPKIT_EXPORT NSString *const NSPrintPaperName;
APPKIT_EXPORT NSString *const NSPrintPaperSize;
APPKIT_EXPORT NSString *const NSPrintOrientation;

APPKIT_EXPORT NSString *const NSPrintHorizontalPagination;
APPKIT_EXPORT NSString *const NSPrintVerticalPagination;

APPKIT_EXPORT NSString *const NSPrintTopMargin;
APPKIT_EXPORT NSString *const NSPrintBottomMargin;
APPKIT_EXPORT NSString *const NSPrintLeftMargin;
APPKIT_EXPORT NSString *const NSPrintRightMargin;
APPKIT_EXPORT NSString *const NSPrintHorizontallyCentered;
APPKIT_EXPORT NSString *const NSPrintVerticallyCentered;

APPKIT_EXPORT NSString *const NSPrintHeaderAndFooter;
APPKIT_EXPORT NSString *const NSPrintScalingFactor;
APPKIT_EXPORT NSString *const NSPrintMustCollate;
APPKIT_EXPORT NSString *const NSPrintReversePageOrder;
APPKIT_EXPORT NSString *const NSPrintSelectionOnly;
APPKIT_EXPORT NSString *const NSPrintPagesAcross;
APPKIT_EXPORT NSString *const NSPrintPagesDown;
APPKIT_EXPORT NSString *const NSPrintTime;
APPKIT_EXPORT NSString *const NSPrintFaxNumber;
APPKIT_EXPORT NSString *const NSPrintJobSavingURL;
APPKIT_EXPORT NSString *const NSPrintJobSavingFileNameExtensionHidden;

@interface NSPrintInfo : NSObject <NSCopying, NSCoding> {
    NSMutableDictionary *_attributes;
    /* PrintCore's objects (PMPrintSession, PMPageFormat, PMPrintSettings),
       made when first asked for, kept in step with the dictionary. */
    void *_session;
    void *_pageFormat;
    void *_printSettings;
    NSMutableDictionary *_printSettingsDictionary;
}

+ (void)setSharedPrintInfo:(NSPrintInfo *)printInfo;
+ (NSPrinter *)defaultPrinter;

+ (NSPrintInfo *)sharedPrintInfo;

- initWithDictionary:(NSDictionary *)dictionary;

- (NSMutableDictionary *)dictionary;

- (NSPrinter *)printer;
- (NSString *)jobDisposition;

- (NSString *)paperName;
- (NSSize)paperSize;
- (NSPrintingOrientation)orientation;

- (NSPrintingPaginationMode)horizontalPagination;
- (NSPrintingPaginationMode)verticalPagination;

- (CGFloat)topMargin;
- (CGFloat)bottomMargin;
- (CGFloat)leftMargin;
- (CGFloat)rightMargin;

- (BOOL)isHorizontallyCentered;
- (BOOL)isVerticallyCentered;

- (NSString *)localizedPaperName;
- (NSRect)imageablePageBounds;

- (void)setPrinter:(NSPrinter *)printer;
- (void)setJobDisposition:(NSString *)value;

- (void)setPaperName:(NSString *)value;
- (void)setPaperSize:(NSSize)value;
- (void)setOrientation:(NSPrintingOrientation)value;

- (void)setHorizontalPagination:(NSPrintingPaginationMode)value;
- (void)setVerticalPagination:(NSPrintingPaginationMode)value;

- (void)setTopMargin:(CGFloat)value;
- (void)setBottomMargin:(CGFloat)value;
- (void)setLeftMargin:(CGFloat)value;
- (void)setRightMargin:(CGFloat)value;

- (void)setHorizontallyCentered:(BOOL)value;
- (void)setVerticallyCentered:(BOOL)value;

- (void)setUpPrintOperationDefaultValues;

@property CGFloat scalingFactor;
@property (getter=isSelectionOnly) BOOL selectionOnly;
@property (readonly, strong) NSMutableDictionary *printSettings;

- (void *)PMPrintSession;
- (void *)PMPageFormat;
- (void *)PMPrintSettings;
- (void)updateFromPMPageFormat;
- (void)updateFromPMPrintSettings;

/* Minkar: as properties too, for dot syntax. */
@property (getter=isHorizontallyCentered) BOOL horizontallyCentered;
@property (getter=isVerticallyCentered) BOOL verticallyCentered;
@end
