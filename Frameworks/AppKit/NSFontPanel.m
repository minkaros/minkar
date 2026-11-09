/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSFontPanel.h>
#import <AppKit/NSFontManager.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSTextField.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSButton.h>
#import <AppKit/NSPopUpButton.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSMenuItem.h>
#import <AppKit/NSColorWell.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSSlider.h>
#import <AppKit/NSTableView.h>
#import <AppKit/NSTableColumn.h>
#import <AppKit/NSScrollView.h>
#import <AppKit/NSTextFieldCell.h>
#import <AppKit/NSAttributedString.h>
#import <AppKit/NSShadow.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSRaise.h>
#import <Foundation/NSUserDefaults.h>

/* SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 The Minkar contributors
   The panel's layout and behavior below are Minkar's (rewritten in code). */

@interface NSFontManager (NSFontPanelAttributes)
-(void)_fontPanelChangedAttributes:(NSFontPanel *)panel;
@end

#define WIDTH 446.0
#define TOOLBAR 34.0
#define HEADER 20.0
#define LISTS 196.0
#define COLUMN_COLLECTION 122.0
#define COLUMN_FAMILY 122.0
#define COLUMN_TYPEFACE 122.0

static NSString *const kFavoritesKey=@"NSFontPanelFavorites";

@implementation NSFontPanel

static NSTextField *headerLabel(NSString *title,NSRect frame){
   NSTextField *label=[[[NSTextField alloc] initWithFrame:frame] autorelease];

   [label setStringValue:title];
   [label setEditable:NO];
   [label setSelectable:NO];
   [label setBordered:NO];
   [label setDrawsBackground:NO];
   [label setFont:[NSFont systemFontOfSize:11]];
   return label;
}

-(NSTableView *)_listInFrame:(NSRect)frame identifier:(NSString *)identifier {
   NSScrollView  *scroll=[[[NSScrollView alloc] initWithFrame:frame] autorelease];
   NSTableView   *table=[[[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]] autorelease];
   NSTableColumn *column=[[[NSTableColumn alloc] initWithIdentifier:identifier] autorelease];

   [column setWidth:NSWidth(frame)-18];
   [[column dataCell] setFont:[NSFont systemFontOfSize:12]];
   [table addTableColumn:column];
   [table setHeaderView:nil];
   [table setRowHeight:18];
   [table setDataSource:(id)self];
   [table setDelegate:(id)self];
   [table setTarget:self];
   [table setAction:@selector(_clickedList:)];
   [scroll setDocumentView:table];
   [scroll setHasVerticalScroller:YES];
   [scroll setBorderType:NSNoBorder];
   [[self contentView] addSubview:scroll];
   return table;
}

-(NSPopUpButton *)_popUpAt:(NSRect)frame titles:(NSArray *)titles action:(SEL)action {
   NSPopUpButton *popUp=[[[NSPopUpButton alloc] initWithFrame:frame pullsDown:NO] autorelease];

   [popUp addItemsWithTitles:titles];
   [popUp setTarget:self];
   [popUp setAction:action];
   [popUp setFont:[NSFont systemFontOfSize:11]];
   [[self contentView] addSubview:popUp];
   return popUp;
}

/* The panel, made in code (it was a nib): the effects row on top, the
   four columns' titles, then the lists. */
-(instancetype)_initSharedPanel {
   CGFloat height=TOOLBAR+HEADER+LISTS;
   CGFloat top,x;

   [self initWithContentRect:NSMakeRect(0,0,WIDTH,height)
      styleMask:NSTitledWindowMask|NSClosableWindowMask|NSResizableWindowMask|NSUtilityWindowMask
      backing:NSBackingStoreBuffered defer:YES];
   [self setTitle:NSLocalizedStringFromTableInBundle(@"Fonts",nil,[NSBundle bundleForClass:[NSFontPanel class]],@"The font panel's title")];
   [self setReleasedWhenClosed:NO];
   [self setHidesOnDeactivate:YES];
   [self setFloatingPanel:YES];
   [self setBecomesKeyOnlyIfNeeded:YES];
   [self setMinSize:NSMakeSize(WIDTH,height)];
   _enabled=YES;
   _pointSize=12;
   _favorites=[[NSMutableArray alloc] initWithArray:[[NSUserDefaults standardUserDefaults] arrayForKey:kFavoritesKey]?:[NSArray array]];
   _recent=[NSMutableArray new];
   _sizes=[[NSArray alloc] initWithObjects:@9,@10,@11,@12,@13,@14,@18,@24,@36,@48,@64,@72,@96,@144,@288,nil];
   _collections=[[NSArray alloc] initWithObjects:@"All Fonts",@"Favorites",@"Recently Used",@"Fixed Width",nil];

   /* The effects. */
   top=height-TOOLBAR+6;
   _actionPopUp=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(8,top,44,22) pullsDown:YES];
   [_actionPopUp addItemsWithTitles:@[@"",@"Add to Favorites",@"Remove from Favorites",@"Show All Fonts"]];
   [[_actionPopUp itemAtIndex:0] setImage:[NSImage imageNamed:NSImageNameActionTemplate]];
   [_actionPopUp setTarget:self];
   [_actionPopUp setAction:@selector(_action:)];
   [_actionPopUp setFont:[NSFont systemFontOfSize:11]];
   [[self contentView] addSubview:_actionPopUp];
   _underlinePopUp=[[self _popUpAt:NSMakeRect(60,top,60,22) titles:@[@"U̲",@"None",@"Single",@"Double"] action:@selector(_underline:)] retain];
   _strikethroughPopUp=[[self _popUpAt:NSMakeRect(124,top,60,22) titles:@[@"S̶",@"None",@"Single",@"Double"] action:@selector(_strikethrough:)] retain];
   [_underlinePopUp setPullsDown:YES];
   [_strikethroughPopUp setPullsDown:YES];
   _textColorWell=[[NSColorWell alloc] initWithFrame:NSMakeRect(192,top,40,22)];
   [_textColorWell setColor:[NSColor blackColor]];
   [_textColorWell setTarget:self];
   [_textColorWell setAction:@selector(_textColor:)];
   [[self contentView] addSubview:_textColorWell];
   _documentColorWell=[[NSColorWell alloc] initWithFrame:NSMakeRect(238,top,40,22)];
   [_documentColorWell setColor:[NSColor whiteColor]];
   [_documentColorWell setTarget:self];
   [_documentColorWell setAction:@selector(_documentColor:)];
   [[self contentView] addSubview:_documentColorWell];
   _shadowButton=[[NSButton alloc] initWithFrame:NSMakeRect(290,top,36,22)];
   [_shadowButton setButtonType:NSPushOnPushOffButton];
   [_shadowButton setBezelStyle:NSTexturedRoundedBezelStyle];
   [_shadowButton setTitle:@"T"];
   [_shadowButton setTarget:self];
   [_shadowButton setAction:@selector(_shadow:)];
   [[self contentView] addSubview:_shadowButton];
   _shadowOpacitySlider=[[NSSlider alloc] initWithFrame:NSMakeRect(332,top+2,50,18)];
   [_shadowOpacitySlider setMinValue:0];
   [_shadowOpacitySlider setMaxValue:1];
   [_shadowOpacitySlider setDoubleValue:0.5];
   [_shadowOpacitySlider setTarget:self];
   [_shadowOpacitySlider setAction:@selector(_shadow:)];
   [[self contentView] addSubview:_shadowOpacitySlider];
   _shadowBlurSlider=[[NSSlider alloc] initWithFrame:NSMakeRect(388,top+2,50,18)];
   [_shadowBlurSlider setMinValue:0];
   [_shadowBlurSlider setMaxValue:10];
   [_shadowBlurSlider setDoubleValue:2];
   [_shadowBlurSlider setTarget:self];
   [_shadowBlurSlider setAction:@selector(_shadow:)];
   [[self contentView] addSubview:_shadowBlurSlider];

   /* The columns' titles. */
   top=LISTS;
   x=0;
   [[self contentView] addSubview:headerLabel(@"Collection",NSMakeRect(x+6,top+2,COLUMN_COLLECTION-12,16))];
   x+=COLUMN_COLLECTION;
   [[self contentView] addSubview:headerLabel(@"Family",NSMakeRect(x+6,top+2,COLUMN_FAMILY-12,16))];
   x+=COLUMN_FAMILY;
   [[self contentView] addSubview:headerLabel(@"Typeface",NSMakeRect(x+6,top+2,COLUMN_TYPEFACE-12,16))];
   x+=COLUMN_TYPEFACE;
   [[self contentView] addSubview:headerLabel(@"Size",NSMakeRect(x+6,top+2,WIDTH-x-12,16))];

   /* The lists; the size: a field, its list and a slider. */
   x=0;
   _collectionTable=[[self _listInFrame:NSMakeRect(x,0,COLUMN_COLLECTION,LISTS) identifier:@"collection"] retain];
   x+=COLUMN_COLLECTION;
   _familyTable=[[self _listInFrame:NSMakeRect(x,0,COLUMN_FAMILY,LISTS) identifier:@"family"] retain];
   x+=COLUMN_FAMILY;
   _typefaceTable=[[self _listInFrame:NSMakeRect(x,0,COLUMN_TYPEFACE,LISTS) identifier:@"typeface"] retain];
   x+=COLUMN_TYPEFACE;
   _sizeTextField=[[NSTextField alloc] initWithFrame:NSMakeRect(x+2,LISTS-24,WIDTH-x-24,20)];
   [_sizeTextField setAlignment:NSRightTextAlignment];
   [_sizeTextField setTarget:self];
   [_sizeTextField setAction:@selector(_sizeField:)];
   [[self contentView] addSubview:_sizeTextField];
   _sizeTable=[[self _listInFrame:NSMakeRect(x,0,WIDTH-x-20,LISTS-28) identifier:@"size"] retain];
   _sizeSlider=[[NSSlider alloc] initWithFrame:NSMakeRect(WIDTH-18,4,16,LISTS-32)];
   [_sizeSlider setMinValue:7];
   [_sizeSlider setMaxValue:288];
   [_sizeSlider setTarget:self];
   [_sizeSlider setAction:@selector(_sizeSlider:)];
   [[self contentView] addSubview:_sizeSlider];

   [_collectionTable reloadData];
   [_sizeTable reloadData];
   [_collectionTable selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
   [self _reloadFamilies];
   return self;
}

-(void)dealloc {
   [_collectionTable release];
   [_familyTable release];
   [_typefaceTable release];
   [_sizeTable release];
   [_sizeTextField release];
   [_sizeSlider release];
   [_actionPopUp release];
   [_underlinePopUp release];
   [_strikethroughPopUp release];
   [_textColorWell release];
   [_documentColorWell release];
   [_shadowButton release];
   [_shadowOpacitySlider release];
   [_shadowBlurSlider release];
   [_accessoryView release];
   [_collections release];
   [_families release];
   [_typefaces release];
   [_sizes release];
   [_favorites release];
   [_recent release];
   [_pendingAttributes release];
   [super dealloc];
}

+(BOOL)sharedFontPanelExists {
   return ([[NSFontManager sharedFontManager] fontPanel:NO]!=nil)?YES:NO;
}

+(NSFontPanel *)sharedFontPanel {
   return [[NSFontManager sharedFontManager] fontPanel:YES];
}

-(BOOL)isEnabled {
   return _enabled;
}

-(void)setEnabled:(BOOL)value {
   _enabled=value;
}

-(NSView *)accessoryView {
   return _accessoryView;
}

-(void)setAccessoryView:(NSView *)view {
   view=[view retain];
   [_accessoryView release];
   _accessoryView=view;
}

-(BOOL)worksWhenModal {
   return YES;
}

-(void)reloadDefaultFontFamilies {
   [self _reloadFamilies];
}

/* ---- The lists ---- */

-(NSArray *)_familiesInCollection:(NSInteger)collection {
   NSFontManager *manager=[NSFontManager sharedFontManager];
   NSArray       *all=[manager availableFontFamilies];

   switch(collection){
    case 1:
     return [_favorites sortedArrayUsingSelector:@selector(compare:)];
    case 2:
     return [[_recent copy] autorelease];
    case 3:{
     NSMutableArray *fixed=[NSMutableArray array];
     for(NSString *family in all){
      NSArray *members=[manager availableMembersOfFontFamily:family];
      NSFont  *font=[members count]?[NSFont fontWithName:[[members objectAtIndex:0] objectAtIndex:0] size:12]:nil;
      if([font isFixedPitch])
       [fixed addObject:family];
     }
     return fixed;
    }
    default:
     return [all sortedArrayUsingSelector:@selector(compare:)];
   }
}

-(void)_reloadFamilies {
   NSInteger collection=MAX(0,[_collectionTable selectedRow]);

   [_families release];
   _families=[[self _familiesInCollection:collection] retain];
   [_familyTable reloadData];
   [self _reloadTypefaces];
}

-(NSString *)_selectedFamily {
   NSInteger row=[_familyTable selectedRow];

   if(row<0 || row>=(NSInteger)[_families count])
    return [_families count]?[_families objectAtIndex:0]:nil;
   return [_families objectAtIndex:row];
}

static NSInteger typefaceWeight(NSString *name){
   static NSString *const order[]={@"Thin",@"ExtraLight",@"UltraLight",@"Light",@"Book",@"Regular",@"Medium",
      @"SemiBold",@"DemiBold",@"ExtraBold",@"UltraBold",@"Bold",@"Heavy",@"Black"};
   static const NSInteger weights[]={1,2,2,3,4,4,5,6,6,8,8,7,9,9};
   NSUInteger i;

   /* Longest names first in the table above where one holds another
      (ExtraBold before Bold). */
   for(i=0;i<sizeof(weights)/sizeof(weights[0]);i++)
    if([name rangeOfString:order[i]].location!=NSNotFound)
     return weights[i];
   return 4;
}

-(void)_reloadTypefaces {
   NSString *family=[self _selectedFamily];

   [_typefaces release];
   /* Light to heavy, each roman before its italic, as the reference lists
      them (by the typeface's name: the weights fontconfig gives can be
      alike). */
   _typefaces=[[(family?[[NSFontManager sharedFontManager] availableMembersOfFontFamily:family]:[NSArray array])
      sortedArrayUsingComparator:^NSComparisonResult(NSArray *a,NSArray *b){
       NSInteger wa=typefaceWeight([a objectAtIndex:1]),wb=typefaceWeight([b objectAtIndex:1]);
       BOOL      ia=[[a objectAtIndex:1] rangeOfString:@"Italic"].location!=NSNotFound || [[a objectAtIndex:1] rangeOfString:@"Oblique"].location!=NSNotFound;
       BOOL      ib=[[b objectAtIndex:1] rangeOfString:@"Italic"].location!=NSNotFound || [[b objectAtIndex:1] rangeOfString:@"Oblique"].location!=NSNotFound;

       if(wa!=wb)
        return wa<wb?NSOrderedAscending:NSOrderedDescending;
       if(ia!=ib)
        return ia?NSOrderedDescending:NSOrderedAscending;
       return [[a objectAtIndex:1] compare:[b objectAtIndex:1]];
      }] retain];
   [_typefaceTable reloadData];
}

/* Each list by its column's identifier: a table asks for its rows while
   it is made, before the panel holds it. */
static NSString *listName(NSTableView *table){
   NSArray *columns=[table tableColumns];

   return [columns count]?[[columns objectAtIndex:0] identifier]:nil;
}

-(NSArray *)_rowsOfList:(NSTableView *)table {
   NSString *name=listName(table);

   if([name isEqualToString:@"collection"]) return _collections;
   if([name isEqualToString:@"family"]) return _families;
   if([name isEqualToString:@"typeface"]) return _typefaces;
   if([name isEqualToString:@"size"]) return _sizes;
   return nil;
}

-(NSInteger)numberOfRowsInTableView:(NSTableView *)table {
   return [[self _rowsOfList:table] count];
}

-(id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row {
   NSArray  *rows=[self _rowsOfList:table];
   NSString *name=listName(table);
   id        value;

   if(row<0 || row>=(NSInteger)[rows count])
    return nil;
   value=[rows objectAtIndex:row];
   if([name isEqualToString:@"collection"])
    return NSLocalizedStringFromTableInBundle(value,nil,[NSBundle bundleForClass:[NSFontPanel class]],@"A font collection");
   if([name isEqualToString:@"typeface"])
    return [value objectAtIndex:1];
   if([name isEqualToString:@"size"])
    return [value stringValue];
   return value;
}

-(void)_clickedList:(NSTableView *)table {
   if(table==_collectionTable)
    [self _reloadFamilies];
   else if(table==_familyTable)
    [self _reloadTypefaces];
   else if(table==_sizeTable && [table selectedRow]>=0)
    _pointSize=[[_sizes objectAtIndex:[table selectedRow]] floatValue];
   [_sizeTextField setFloatValue:_pointSize];
   [_sizeSlider setFloatValue:_pointSize];
   if(table!=_collectionTable)
    [self _apply];
}

-(void)_sizeField:sender {
   if([sender floatValue]>0){
    _pointSize=[sender floatValue];
    [_sizeSlider setFloatValue:_pointSize];
    [self _apply];
   }
}

-(void)_sizeSlider:sender {
   _pointSize=floor([sender floatValue]);
   [_sizeTextField setFloatValue:_pointSize];
   [self _apply];
}

/* The chosen font, sent down the responder chain at once. */
-(void)_apply {
   NSString *family=[self _selectedFamily];

   if(!_enabled || family==nil)
    return;
   [_recent removeObject:family];
   [_recent insertObject:family atIndex:0];
   if([_recent count]>10)
    [_recent removeLastObject];
   [[NSFontManager sharedFontManager] modifyFontViaPanel:self];
}

-(NSString *)selectedFontName {
   NSInteger row=[_typefaceTable selectedRow];

   if([_typefaces count]==0)
    return nil;
   if(row<0 || row>=(NSInteger)[_typefaces count])
    row=0;
   return [[_typefaces objectAtIndex:row] objectAtIndex:0];
}

-(NSFont *)selectedFont {
   NSString *name=[self selectedFontName];

   return name?[NSFont fontWithName:name size:_pointSize]:nil;
}

/* The text's font shown: its collection, family, typeface and size. */
-(void)setPanelFont:(NSFont *)font isMultiple:(BOOL)isMultiple {
   NSFontManager *manager=[NSFontManager sharedFontManager];
   NSString      *family=[font familyName];
   NSUInteger     index;

   if(font==nil)
    return;
   _pointSize=[font pointSize];
   if([_families indexOfObject:family]==NSNotFound){
    [_collectionTable selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
    [self _reloadFamilies];
   }
   index=[_families indexOfObject:family];
   if(index!=NSNotFound){
    [_familyTable selectRowIndexes:[NSIndexSet indexSetWithIndex:index] byExtendingSelection:NO];
    [_familyTable scrollRowToVisible:index];
   }
   [self _reloadTypefaces];
   for(index=0;index<[_typefaces count];index++)
    if([[[_typefaces objectAtIndex:index] objectAtIndex:0] isEqualToString:[font fontName]]){
     [_typefaceTable selectRowIndexes:[NSIndexSet indexSetWithIndex:index] byExtendingSelection:NO];
     break;
    }
   index=[_sizes indexOfObject:[NSNumber numberWithFloat:_pointSize]];
   if(index!=NSNotFound)
    [_sizeTable selectRowIndexes:[NSIndexSet indexSetWithIndex:index] byExtendingSelection:NO];
   else
    [_sizeTable deselectAll:nil];
   [_sizeTextField setFloatValue:_pointSize];
   [_sizeSlider setFloatValue:_pointSize];
   (void)manager;
}

-(NSFont *)panelConvertFont:(NSFont *)font {
   NSFont *selected=[self selectedFont];

   return selected?selected:font;
}

-(void)set:sender {
   [self _apply];
}

-(void)revert:sender {
   NSFontManager *manager=[NSFontManager sharedFontManager];

   [self setPanelFont:[manager selectedFont] isMultiple:[manager isMultiple]];
}

/* ---- The effects: changeAttributes: down the responder chain ---- */

-(void)_sendAttributes:(NSDictionary *)attributes {
   [_pendingAttributes release];
   _pendingAttributes=[attributes retain];
   [[NSFontManager sharedFontManager] _fontPanelChangedAttributes:self];
   [_pendingAttributes release];
   _pendingAttributes=nil;
}

/* What -[NSFontManager convertAttributes:] gives a text view. */
-(NSDictionary *)_convertAttributes:(NSDictionary *)attributes {
   NSMutableDictionary *result=[[attributes mutableCopy] autorelease];

   for(NSString *key in _pendingAttributes){
    id value=[_pendingAttributes objectForKey:key];

    if(value==[NSNull null])
     [result removeObjectForKey:key];
    else
     [result setObject:value forKey:key];
   }
   return result;
}

static id lineStyle(NSInteger index){
   switch(index){
    case 2: return [NSNumber numberWithInteger:NSUnderlineStyleSingle];
    case 3: return [NSNumber numberWithInteger:NSUnderlineStyleDouble];
    default: return [NSNull null];
   }
}

-(void)_underline:sender {
   [self _sendAttributes:[NSDictionary dictionaryWithObject:lineStyle([sender indexOfSelectedItem]) forKey:NSUnderlineStyleAttributeName]];
}

-(void)_strikethrough:sender {
   [self _sendAttributes:[NSDictionary dictionaryWithObject:lineStyle([sender indexOfSelectedItem]) forKey:NSStrikethroughStyleAttributeName]];
}

-(void)_textColor:sender {
   [self _sendAttributes:[NSDictionary dictionaryWithObject:[sender color] forKey:NSForegroundColorAttributeName]];
}

-(void)_documentColor:sender {
   [NSApp sendAction:@selector(changeDocumentBackgroundColor:) to:nil from:sender];
}

-(void)_shadow:sender {
   id value=[NSNull null];

   if([_shadowButton state]){
    NSShadow *shadow=[[[NSShadow alloc] init] autorelease];

    [shadow setShadowOffset:NSMakeSize(2,-2)];
    [shadow setShadowBlurRadius:[_shadowBlurSlider doubleValue]];
    [shadow setShadowColor:[NSColor colorWithCalibratedWhite:0 alpha:[_shadowOpacitySlider doubleValue]]];
    value=shadow;
   }
   [self _sendAttributes:[NSDictionary dictionaryWithObject:value forKey:NSShadowAttributeName]];
}

-(void)_action:sender {
   NSString *family=[self _selectedFamily];

   switch([sender indexOfSelectedItem]){
    case 1:
     if(family!=nil && ![_favorites containsObject:family])
      [_favorites addObject:family];
     break;
    case 2:
     if(family!=nil)
      [_favorites removeObject:family];
     break;
    case 3:
     [_collectionTable selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
     break;
   }
   [[NSUserDefaults standardUserDefaults] setObject:_favorites forKey:kFavoritesKey];
   [self _reloadFamilies];
}

@end
