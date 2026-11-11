#import <AppKit/NSPrintPanel.h>
#import <AppKit/NSPrintOperation.h>
#import <AppKit/NSPrintInfo.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSDisplay.h>
#import <Foundation/NSDictionary.h>

@implementation NSPrintPanel

+(NSPrintPanel *)printPanel {
   return [[[self alloc] init] autorelease];
}

- (void)setOptions:(NSPrintPanelOptions)options {
    _options = options;
}

- (NSPrintPanelOptions)options {
    return _options;
}

-(NSInteger)runModal {
   int result;
   
   [self updateFromPrintInfo];
   result=[[NSDisplay currentDisplay] runModalPrintPanelWithPrintInfoDictionary:_attributes];
   if(result==NSOKButton)
    [self finalWritePrintInfo];
    
   return result;
}

-(void)updateFromPrintInfo {
   NSDictionary *source=[[[NSPrintOperation currentOperation] printInfo] dictionary];
   
   [_attributes release];
   _attributes=[source mutableCopy];
}

-(void)finalWritePrintInfo {
   NSMutableDictionary *destination=[[[NSPrintOperation currentOperation] printInfo] dictionary];
   
   [destination addEntriesFromDictionary:_attributes];
}

@end

NSString *const NSPrintPanelAccessorySummaryItemNameKey = @"NSPrintPanelAccessorySummaryItemName";
NSString *const NSPrintPanelAccessorySummaryItemDescriptionKey = @"NSPrintPanelAccessorySummaryItemDescription";

#import <objc/runtime.h>
static char accessoriesKey;

@implementation NSPrintPanel (MinkarAccessories)
-(NSMutableArray *)_accessories {
   NSMutableArray *list=objc_getAssociatedObject(self,&accessoriesKey);
   if(list==nil){
    list=[NSMutableArray array];
    objc_setAssociatedObject(self,&accessoriesKey,list,OBJC_ASSOCIATION_RETAIN);
   }
   return list;
}
-(void)addAccessoryController:(NSViewController *)controller { if(controller) [[self _accessories] addObject:controller]; }
-(void)removeAccessoryController:(NSViewController *)controller { [[self _accessories] removeObjectIdenticalTo:controller]; }
-(NSArray *)accessoryControllers { return [NSArray arrayWithArray:[self _accessories]]; }
@end
