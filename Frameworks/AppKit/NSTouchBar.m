/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSTouchBar.h>
#import <AppKit/NSView.h>
#import <AppKit/NSViewController.h>
#import <objc/runtime.h>

const NSTouchBarItemPriority NSTouchBarItemPriorityHigh=1000;
const NSTouchBarItemPriority NSTouchBarItemPriorityNormal=0;
const NSTouchBarItemPriority NSTouchBarItemPriorityLow=-1000;

NSTouchBarItemIdentifier const NSTouchBarItemIdentifierFixedSpaceSmall=@"NSTouchBarItemIdentifierFixedSpaceSmall";
NSTouchBarItemIdentifier const NSTouchBarItemIdentifierFixedSpaceLarge=@"NSTouchBarItemIdentifierFixedSpaceLarge";
NSTouchBarItemIdentifier const NSTouchBarItemIdentifierFlexibleSpace=@"NSTouchBarItemIdentifierFlexibleSpace";
NSTouchBarItemIdentifier const NSTouchBarItemIdentifierOtherItemsProxy=@"NSTouchBarItemIdentifierOtherItemsProxy";
NSTouchBarItemIdentifier const NSTouchBarItemIdentifierCharacterPicker=@"NSTouchBarItemIdentifierCharacterPicker";
NSTouchBarItemIdentifier const NSTouchBarItemIdentifierTextColorPicker=@"NSTouchBarItemIdentifierTextColorPicker";
NSTouchBarItemIdentifier const NSTouchBarItemIdentifierTextStyle=@"NSTouchBarItemIdentifierTextStyle";
NSTouchBarItemIdentifier const NSTouchBarItemIdentifierTextAlignment=@"NSTouchBarItemIdentifierTextAlignment";
NSTouchBarItemIdentifier const NSTouchBarItemIdentifierTextList=@"NSTouchBarItemIdentifierTextList";
NSTouchBarItemIdentifier const NSTouchBarItemIdentifierTextFormat=@"NSTouchBarItemIdentifierTextFormat";
NSTouchBarItemIdentifier const NSTouchBarItemIdentifierCandidateList=@"NSTouchBarItemIdentifierCandidateList";

@implementation NSTouchBarItem

-init {
   return [self initWithIdentifier:@""];
}

-initWithIdentifier:(NSTouchBarItemIdentifier)identifier {
   _identifier=[identifier copy];
   return self;
}

-initWithCoder:(NSCoder *)coder {
   _identifier=[[coder decodeObjectForKey:@"NSTouchBarItemIdentifier"] copy];
   return self;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   [coder encodeObject:_identifier forKey:@"NSTouchBarItemIdentifier"];
}

-(void)dealloc {
   [_identifier release];
   [super dealloc];
}

-(NSTouchBarItemIdentifier)identifier { return _identifier; }
-(NSTouchBarItemPriority)visibilityPriority { return _visibilityPriority; }
-(void)setVisibilityPriority:(NSTouchBarItemPriority)value { _visibilityPriority=value; }
-(NSView *)view { return nil; }
-(NSViewController *)viewController { return nil; }
-(NSString *)customizationLabel { return @""; }
-(BOOL)isVisible { return NO; }

@end

@implementation NSCustomTouchBarItem

-(void)dealloc {
   [_view release];
   [_viewController release];
   [_customizationLabel release];
   [super dealloc];
}

-(NSView *)view { return (_view!=nil)?_view:[_viewController view]; }
-(void)setView:(NSView *)view { view=[view retain]; [_view release]; _view=view; }
-(NSViewController *)viewController { return _viewController; }
-(void)setViewController:(NSViewController *)controller { controller=[controller retain]; [_viewController release]; _viewController=controller; }
-(NSString *)customizationLabel { return (_customizationLabel!=nil)?_customizationLabel:@""; }
-(void)setCustomizationLabel:(NSString *)label { label=[label copy]; [_customizationLabel release]; _customizationLabel=label; }

@end

@implementation NSGroupTouchBarItem

+(instancetype)groupItemWithIdentifier:(NSTouchBarItemIdentifier)identifier items:(NSArray *)items {
   NSGroupTouchBarItem *result=[[[self alloc] initWithIdentifier:identifier] autorelease];
   NSTouchBar          *bar=[[[NSTouchBar alloc] init] autorelease];
   NSMutableArray      *identifiers=[NSMutableArray array];

   for(NSTouchBarItem *item in items)
    [identifiers addObject:[item identifier]];
   [bar setTemplateItems:[NSSet setWithArray:items]];
   [bar setDefaultItemIdentifiers:identifiers];
   [result setGroupTouchBar:bar];
   return result;
}

-(void)dealloc {
   [_groupTouchBar release];
   [_customizationLabel release];
   [super dealloc];
}

-(NSTouchBar *)groupTouchBar { return _groupTouchBar; }
-(void)setGroupTouchBar:(NSTouchBar *)bar { bar=[bar retain]; [_groupTouchBar release]; _groupTouchBar=bar; }
-(NSString *)customizationLabel { return (_customizationLabel!=nil)?_customizationLabel:@""; }
-(void)setCustomizationLabel:(NSString *)label { label=[label copy]; [_customizationLabel release]; _customizationLabel=label; }

@end

static BOOL automaticCustomizeMenuItem;

@implementation NSTouchBar

-init {
   _items=[[NSMutableDictionary alloc] init];
   return self;
}

-initWithCoder:(NSCoder *)coder {
   return [self init];
}

-(void)encodeWithCoder:(NSCoder *)coder {
}

-(void)dealloc {
   [_customizationIdentifier release];
   [_customizationAllowedItemIdentifiers release];
   [_customizationRequiredItemIdentifiers release];
   [_defaultItemIdentifiers release];
   [_principalItemIdentifier release];
   [_escapeKeyReplacementItemIdentifier release];
   [_templateItems release];
   [_items release];
   [super dealloc];
}

#define COPY_PROPERTY(getter,setter,ivar,type) \
-(type)getter { return ivar; } \
-(void)setter:(type)value { value=[value copy]; [ivar release]; ivar=value; }

COPY_PROPERTY(customizationIdentifier,setCustomizationIdentifier,_customizationIdentifier,NSString *)
COPY_PROPERTY(customizationAllowedItemIdentifiers,setCustomizationAllowedItemIdentifiers,_customizationAllowedItemIdentifiers,NSArray *)
COPY_PROPERTY(customizationRequiredItemIdentifiers,setCustomizationRequiredItemIdentifiers,_customizationRequiredItemIdentifiers,NSArray *)
COPY_PROPERTY(defaultItemIdentifiers,setDefaultItemIdentifiers,_defaultItemIdentifiers,NSArray *)
COPY_PROPERTY(principalItemIdentifier,setPrincipalItemIdentifier,_principalItemIdentifier,NSString *)
COPY_PROPERTY(escapeKeyReplacementItemIdentifier,setEscapeKeyReplacementItemIdentifier,_escapeKeyReplacementItemIdentifier,NSString *)
COPY_PROPERTY(templateItems,setTemplateItems,_templateItems,NSSet *)

-(id)delegate { return _delegate; }
-(void)setDelegate:(id)delegate { _delegate=delegate; }

-(NSArray *)itemIdentifiers {
   return (_defaultItemIdentifiers!=nil)?_defaultItemIdentifiers:[NSArray array];
}

-(NSTouchBarItem *)itemForIdentifier:(NSTouchBarItemIdentifier)identifier {
   NSTouchBarItem *item=[_items objectForKey:identifier];

   if(item!=nil || identifier==nil)
    return item;
   for(NSTouchBarItem *check in _templateItems)
    if([[check identifier] isEqualToString:identifier])
     item=check;
   if(item==nil && [_delegate respondsToSelector:@selector(touchBar:makeItemForIdentifier:)])
    item=[_delegate touchBar:self makeItemForIdentifier:identifier];
   if(item!=nil)
    [_items setObject:item forKey:identifier];
   return item;
}

-(BOOL)isVisible {
   return NO;
}

+(BOOL)isAutomaticCustomizeTouchBarMenuItemEnabled {
   return automaticCustomizeMenuItem;
}

+(void)setAutomaticCustomizeTouchBarMenuItemEnabled:(BOOL)flag {
   automaticCustomizeMenuItem=flag;
}

@end

static char touchBarKey;

@implementation NSResponder (NSTouchBarProvider)

/* Made once, by makeTouchBar, unless set. */
-(NSTouchBar *)touchBar {
   NSTouchBar *bar=objc_getAssociatedObject(self,&touchBarKey);

   if(bar==nil){
    bar=[self makeTouchBar];
    if(bar!=nil)
     objc_setAssociatedObject(self,&touchBarKey,bar,OBJC_ASSOCIATION_RETAIN);
   }
   return bar;
}

-(void)setTouchBar:(NSTouchBar *)touchBar {
   objc_setAssociatedObject(self,&touchBarKey,touchBar,OBJC_ASSOCIATION_RETAIN);
}

-(NSTouchBar *)makeTouchBar {
   return nil;
}

@end

@implementation NSApplication (NSTouchBarCustomization)

-(BOOL)isAutomaticCustomizeTouchBarMenuItemEnabled {
   return [NSTouchBar isAutomaticCustomizeTouchBarMenuItemEnabled];
}

-(void)setAutomaticCustomizeTouchBarMenuItemEnabled:(BOOL)flag {
   [NSTouchBar setAutomaticCustomizeTouchBarMenuItemEnabled:flag];
}

/* There is no Touch Bar to customize. */
-(void)toggleTouchBarCustomizationPalette:(id)sender {
}

@end
