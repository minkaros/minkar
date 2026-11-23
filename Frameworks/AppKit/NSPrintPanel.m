/*
 * Minkar: the print panel (Atlas.md, A2; the maintainer's choice of 20 Nov
 * 2026: a minimal panel over PrintCore). The printer (those PrintCore
 * knows), the copies and the pages (all, or from and to); Print, Cancel,
 * and PDF, which saves the document as a PDF where a save panel says. It
 * writes the print info's keys, as the reference's does: NSPrintPrinter,
 * NSPrintCopies, NSPrintAllPages, NSPrintFirstPage, NSPrintLastPage,
 * NSPrintJobDisposition (NSPrintSpoolJob, or NSPrintSaveJob with
 * NSPrintJobSavingURL). The print operation then prints or saves through
 * PrintCore.
 */

#import <AppKit/NSPrintPanel.h>
#import <AppKit/NSPrintOperation.h>
#import <AppKit/NSPrintInfo.h>
#import <AppKit/NSPrinter.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSButton.h>
#import <AppKit/NSButtonCell.h>
#import <AppKit/NSTextField.h>
#import <AppKit/NSPopUpButton.h>
#import <AppKit/NSSavePanel.h>
#import <AppKit/NSWindow.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSBundle.h>
#import <Foundation/NSURL.h>

static NSString *printText(NSString *english) {
   return [[NSBundle bundleForClass:[NSPrintPanel class]] localizedStringForKey:english value:english table:nil];
}

static NSTextField *printLabel(NSString *title, NSRect frame, NSView *view) {
   NSTextField *field=[[[NSTextField alloc] initWithFrame:frame] autorelease];
   [field setStringValue:title];
   [field setEditable:NO];
   [field setSelectable:NO];
   [field setBordered:NO];
   [field setDrawsBackground:NO];
   [field setAlignment:NSRightTextAlignment];
   [view addSubview:field];
   return field;
}

/* The panel's controls, read when it ends. */
@interface NSPrintPanelController : NSObject {
@public
   NSPanel       *_panel;
   NSPopUpButton *_printers;
   NSTextField   *_copies, *_from, *_to;
   NSButton      *_all, *_range;
   NSInteger      _result;
   BOOL           _savePDF;
}
@end

@implementation NSPrintPanelController
-(void)print:(id)sender { _result=NSOKButton; _savePDF=NO; [NSApp stopModal]; }
-(void)cancel:(id)sender { _result=NSCancelButton; [NSApp stopModal]; }
-(void)pdf:(id)sender { _result=NSOKButton; _savePDF=YES; [NSApp stopModal]; }
-(void)chooseRange:(NSButton *)sender {
   [_all setState:sender==_all ? NSOnState : NSOffState];
   [_range setState:sender==_range ? NSOnState : NSOffState];
}
@end

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
   [self updateFromPrintInfo];

   NSPrintPanelController *c=[[[NSPrintPanelController alloc] init] autorelease];
   NSPanel *panel=[[[NSPanel alloc] initWithContentRect:NSMakeRect(0,0,420,220)
      styleMask:NSTitledWindowMask backing:NSBackingStoreBuffered defer:NO] autorelease];
   NSView *content=[panel contentView];
   NSArray *names=[NSPrinter printerNames];
   NSPrinter *current=[_attributes objectForKey:NSPrintPrinter];

   c->_panel=panel;
   [panel setTitle:printText(@"Print")];
   printLabel(printText(@"Printer:"),NSMakeRect(10,176,110,20),content);
   c->_printers=[[[NSPopUpButton alloc] initWithFrame:NSMakeRect(126,172,274,26) pullsDown:NO] autorelease];
   if([names count]==0)
    [c->_printers addItemWithTitle:printText(@"No Printer Selected")];
   else
    [c->_printers addItemsWithTitles:names];
   if(current!=nil && [names containsObject:[current name]])
    [c->_printers selectItemWithTitle:[current name]];
   [content addSubview:c->_printers];

   printLabel(printText(@"Copies:"),NSMakeRect(10,140,110,20),content);
   c->_copies=[[[NSTextField alloc] initWithFrame:NSMakeRect(126,138,50,22)] autorelease];
   NSNumber *copies=[_attributes objectForKey:NSPrintCopies];
   [c->_copies setIntegerValue:copies!=nil ? [copies integerValue] : 1];
   [content addSubview:c->_copies];

   printLabel(printText(@"Pages:"),NSMakeRect(10,104,110,20),content);
   c->_all=[[[NSButton alloc] initWithFrame:NSMakeRect(124,102,80,22)] autorelease];
   [c->_all setButtonType:NSRadioButton];
   [c->_all setTitle:printText(@"All")];
   [c->_all setTarget:c];
   [c->_all setAction:@selector(chooseRange:)];
   [content addSubview:c->_all];
   c->_range=[[[NSButton alloc] initWithFrame:NSMakeRect(124,74,70,22)] autorelease];
   [c->_range setButtonType:NSRadioButton];
   [c->_range setTitle:printText(@"From:")];
   [c->_range setTarget:c];
   [c->_range setAction:@selector(chooseRange:)];
   [content addSubview:c->_range];
   c->_from=[[[NSTextField alloc] initWithFrame:NSMakeRect(196,74,44,22)] autorelease];
   [content addSubview:c->_from];
   printLabel(printText(@"to:"),NSMakeRect(242,76,26,20),content);
   c->_to=[[[NSTextField alloc] initWithFrame:NSMakeRect(272,74,44,22)] autorelease];
   [content addSubview:c->_to];
   BOOL all=[_attributes objectForKey:NSPrintAllPages]==nil || [[_attributes objectForKey:NSPrintAllPages] boolValue];
   [c->_all setState:all ? NSOnState : NSOffState];
   [c->_range setState:all ? NSOffState : NSOnState];
   [c->_from setIntegerValue:MAX([[_attributes objectForKey:NSPrintFirstPage] integerValue],1)];
   [c->_to setIntegerValue:MAX([[_attributes objectForKey:NSPrintLastPage] integerValue],1)];

   NSButton *pdf=[[[NSButton alloc] initWithFrame:NSMakeRect(16,14,70,28)] autorelease];
   [pdf setTitle:printText(@"PDF")];
   [pdf setBezelStyle:NSRoundedBezelStyle];
   [pdf setTarget:c];
   [pdf setAction:@selector(pdf:)];
   [content addSubview:pdf];
   NSButton *cancel=[[[NSButton alloc] initWithFrame:NSMakeRect(214,14,90,28)] autorelease];
   [cancel setTitle:printText(@"Cancel")];
   [cancel setBezelStyle:NSRoundedBezelStyle];
   [cancel setKeyEquivalent:@"\033"];
   [cancel setTarget:c];
   [cancel setAction:@selector(cancel:)];
   [content addSubview:cancel];
   NSButton *print=[[[NSButton alloc] initWithFrame:NSMakeRect(310,14,90,28)] autorelease];
   [print setTitle:printText(@"Print")];
   [print setBezelStyle:NSRoundedBezelStyle];
   [print setKeyEquivalent:@"\r"];
   [print setTarget:c];
   [print setAction:@selector(print:)];
   [print setEnabled:[names count]>0];
   [content addSubview:print];

   [panel center];
   [panel makeKeyAndOrderFront:nil];
   [NSApp runModalForWindow:panel];
   [panel orderOut:nil];
   if(c->_result!=NSOKButton)
    return NSCancelButton;

   [_attributes setObject:[NSNumber numberWithInteger:MAX([c->_copies integerValue],1)] forKey:NSPrintCopies];
   BOOL allPages=[c->_all state]==NSOnState;
   [_attributes setObject:[NSNumber numberWithBool:allPages] forKey:NSPrintAllPages];
   if(!allPages){
    NSInteger from=MAX([c->_from integerValue],1), to=MAX([c->_to integerValue],from);
    [_attributes setObject:[NSNumber numberWithInteger:from] forKey:NSPrintFirstPage];
    [_attributes setObject:[NSNumber numberWithInteger:to] forKey:NSPrintLastPage];
   }
   if(c->_savePDF){
    /* PDF: where a save panel says, named after the job. */
    NSSavePanel *save=[NSSavePanel savePanel];
    NSString *title=[[[NSPrintOperation currentOperation] printInfo] dictionary][@"_title"] ?: @"Untitled";
    [save setAllowedFileTypes:[NSArray arrayWithObject:@"pdf"]];
    [save setNameFieldStringValue:[[title stringByDeletingPathExtension] stringByAppendingPathExtension:@"pdf"]];
    if([save runModal]!=NSFileHandlingPanelOKButton || [save URL]==nil)
     return NSCancelButton;
    [_attributes setObject:NSPrintSaveJob forKey:NSPrintJobDisposition];
    [_attributes setObject:[save URL] forKey:NSPrintJobSavingURL];
   } else {
    [_attributes setObject:NSPrintSpoolJob forKey:NSPrintJobDisposition];
    NSPrinter *chosen=[NSPrinter printerWithName:[c->_printers titleOfSelectedItem]];
    if(chosen!=nil)
     [_attributes setObject:chosen forKey:NSPrintPrinter];
   }
   [self finalWritePrintInfo];
   return NSOKButton;
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
