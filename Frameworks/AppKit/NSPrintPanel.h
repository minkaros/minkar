#import <Foundation/NSObject.h>
#import <AppKit/AppKitExport.h>

#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSSet.h>
@class NSMutableDictionary, NSViewController;

enum {

    NSPrintPanelShowsCopies = 1 << 0,
    NSPrintPanelShowsPageRange = 1 << 1,
    NSPrintPanelShowsPaperSize = 1 << 2,
    NSPrintPanelShowsOrientation = 1 << 3,
    NSPrintPanelShowsScaling = 1 << 4,
    NSPrintPanelShowsPrintSelection = 1 << 5,
    NSPrintPanelShowsPageSetupAccessory = 1 << 8,
    NSPrintPanelShowsPreview = 1 << 17
};

typedef NSInteger NSPrintPanelOptions;

@interface NSPrintPanel : NSObject {
    NSMutableDictionary *_attributes;
    NSInteger _options;
}

+ (NSPrintPanel *)printPanel;

- (void)setOptions:(NSPrintPanelOptions)options;
- (NSPrintPanelOptions)options;

- (NSInteger)runModal;

- (void)updateFromPrintInfo;
- (void)finalWritePrintInfo;

@end

APPKIT_EXPORT NSString *const NSPrintPanelAccessorySummaryItemNameKey;
APPKIT_EXPORT NSString *const NSPrintPanelAccessorySummaryItemDescriptionKey;

/* Minkar: what a print panel's accessory view controller provides
   (API 10.5). */
@protocol NSPrintPanelAccessorizing
- (NSArray<NSDictionary<NSString *, NSString *> *> *)localizedSummaryItems;
@optional
- (NSSet<NSString *> *)keyPathsForValuesAffectingPreview;
@end

/* Minkar: accessory controllers (API 10.5), shown with the panel. */
@interface NSPrintPanel (MinkarAccessories)
- (void)addAccessoryController:(NSViewController<NSPrintPanelAccessorizing> *)accessoryController;
- (void)removeAccessoryController:(NSViewController<NSPrintPanelAccessorizing> *)accessoryController;
@property (readonly, copy) NSArray *accessoryControllers;
@end
