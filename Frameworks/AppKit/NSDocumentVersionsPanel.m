/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * File > Revert To > Browse All Versions… (-[NSDocument
 * browseDocumentVersions:]): a window listing the document's versions,
 * newest first, from which one is restored. What the document holds now
 * is kept as a version before, so a restore can be undone the same way.
 */

#import <AppKit/NSApplication.h>
#import <AppKit/NSButton.h>
#import <AppKit/NSDocument.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSScrollView.h>
#import <AppKit/NSTableColumn.h>
#import <AppKit/NSTableView.h>
#import <AppKit/NSTextField.h>
#import <Foundation/Foundation.h>

@interface NSDocument (NSDocumentVersions)
-(NSString *)_openedVersionIdentifier;
@end

@interface NSDocumentVersionsPanel : NSObject {
   NSDocument *_document;
   NSArray    *_versions;
   NSPanel    *_panel;
   NSTableView *_table;
}
+(void)runForDocument:(NSDocument *)document;
@end

static NSString *localized(NSString *string) {
   return [[NSBundle bundleForClass:[NSDocumentVersionsPanel class]] localizedStringForKey:string value:string table:nil];
}

@implementation NSDocumentVersionsPanel

static NSMutableArray *openPanels;

+(void)runForDocument:(NSDocument *)document {
   NSDocumentVersionsPanel *panel=[[[self alloc] initWithDocument:document] autorelease];

   if(openPanels==nil)
    openPanels=[NSMutableArray new];
   [openPanels addObject:panel];
   [panel show];
}

-(id)initWithDocument:(NSDocument *)document {
   NSArray *others=[NSFileVersion otherVersionsOfItemAtURL:[document fileURL]];

   if((self=[super init])!=nil){
    _document=[document retain];
    _versions=[[others sortedArrayUsingComparator:^NSComparisonResult(NSFileVersion *a,NSFileVersion *b){
       return [[b modificationDate] compare:[a modificationDate]];
    }] retain];
   }
   return self;
}

-(void)dealloc {
   [_document release];
   [_versions release];
   [_panel release];
   [super dealloc];
}

-(NSButton *)button:(NSString *)title action:(SEL)action frame:(NSRect)frame {
   NSButton *button=[[[NSButton alloc] initWithFrame:frame] autorelease];

   [button setTitle:localized(title)];
   [button setBezelStyle:NSRoundedBezelStyle];
   [button setTarget:self];
   [button setAction:action];
   return button;
}

-(void)show {
   NSRect        frame=NSMakeRect(0,0,420,300);
   NSScrollView *scroll=[[[NSScrollView alloc] initWithFrame:NSMakeRect(20,56,380,200)] autorelease];
   NSTableColumn *column=[[[NSTableColumn alloc] initWithIdentifier:@"version"] autorelease];
   NSTextField  *title=[[[NSTextField alloc] initWithFrame:NSMakeRect(20,264,380,20)] autorelease];
   NSButton     *restore=[self button:@"Restore" action:@selector(restore:) frame:NSMakeRect(300,14,100,28)];
   NSButton     *done=[self button:@"Done" action:@selector(done:) frame:NSMakeRect(190,14,100,28)];

   _panel=[[NSPanel alloc] initWithContentRect:frame styleMask:NSTitledWindowMask|NSClosableWindowMask
      backing:NSBackingStoreBuffered defer:NO];
   [_panel setTitle:[NSString stringWithFormat:localized(@"Versions of %@"),[_document displayName]]];
   [_panel setReleasedWhenClosed:NO];
   [_panel setDelegate:(id)self];

   [title setStringValue:([_versions count]>0)?localized(@"Choose a version to restore:"):
      localized(@"There are no earlier versions of this document.")];
   [title setBezeled:NO];
   [title setDrawsBackground:NO];
   [title setEditable:NO];
   [[_panel contentView] addSubview:title];

   [column setWidth:360];
   [[column headerCell] setStringValue:localized(@"Saved")];
   _table=[[[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]] autorelease];
   [_table addTableColumn:column];
   [_table setDataSource:(id)self];
   [_table setDelegate:(id)self];
   [_table setDoubleAction:@selector(restore:)];
   [_table setTarget:self];
   [scroll setDocumentView:_table];
   [scroll setHasVerticalScroller:YES];
   [scroll setBorderType:NSBezelBorder];
   [[_panel contentView] addSubview:scroll];

   [restore setKeyEquivalent:@"\r"];
   [restore setEnabled:[_versions count]>0];
   [[_panel contentView] addSubview:restore];
   [[_panel contentView] addSubview:done];
   [_table reloadData];
   if([_versions count]>0)
    [_table selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];

   [_panel center];
   [_panel makeKeyAndOrderFront:nil];
}

-(NSInteger)numberOfRowsInTableView:(NSTableView *)table {
   return [_versions count];
}

-(id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row {
   NSFileVersion   *version=[_versions objectAtIndex:row];
   NSDateFormatter *formatter=[[[NSDateFormatter alloc] init] autorelease];
   NSString        *when;

   [formatter setDateStyle:NSDateFormatterMediumStyle];
   [formatter setTimeStyle:NSDateFormatterShortStyle];
   when=[formatter stringFromDate:[version modificationDate]];
   if([[version persistentIdentifier] isEqual:[_document _openedVersionIdentifier]])
    return [NSString stringWithFormat:localized(@"%@ (last opened)"),when];
   return when;
}

-(void)close {
   [_panel orderOut:nil];
   [[self retain] autorelease];
   [openPanels removeObjectIdenticalTo:self];
}

-(void)done:(id)sender {
   [self close];
}

-(void)windowWillClose:(NSNotification *)note {
   [[self retain] autorelease];
   [openPanels removeObjectIdenticalTo:self];
}

-(void)restore:(id)sender {
   NSInteger      row=[_table selectedRow];
   NSURL         *url=[_document fileURL];
   NSFileVersion *version;
   NSError       *error=nil;

   if(row<0 || row>=(NSInteger)[_versions count] || url==nil)
    return;
   version=[_versions objectAtIndex:row];

   /* What there is now becomes a version, then the chosen one comes back. */
   if([[_document class] preservesVersions])
    [_document saveToURL:url ofType:[_document fileType] forSaveOperation:NSSaveOperation error:NULL];
   if([version replaceItemAtURL:url options:0 error:&error]==nil ||
      ![_document revertToContentsOfURL:url ofType:[_document fileType] error:&error]){
    if(error!=nil)
     [_document presentError:error];
    return;
   }
   [self close];
}

@end
