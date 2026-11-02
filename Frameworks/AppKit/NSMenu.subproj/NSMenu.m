/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSMenu.h>
#import <AppKit/NSMenuItem.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSRaise.h>
#import <AppKit/NSScreen.h>
#import "NSMenuWindow.h"
#import "NSMenuView.h"
#import <CoreGraphics/CGDirectDisplay.h>
#import <Foundation/NSKeyedArchiver.h>

NSString *const NSMenuWillSendActionNotification = @"NSMenuWillSendActionNotification";
NSString *const NSMenuDidSendActionNotification = @"NSMenuDidSendActionNotification";
NSString *const NSMenuDidAddItemNotification = @"NSMenuDidAddItemNotification";
NSString *const NSMenuDidRemoveItemNotification = @"NSMenuDidRemoveItemNotification";
NSString *const NSMenuDidChangeItemNotification = @"NSMenuDidChangeItemNotification";
NSString *const NSMenuDidBeginTrackingNotification = @"NSMenuDidBeginTrackingNotification";
NSString *const NSMenuDidEndTrackingNotification = @"NSMenuDidEndTrackingNotification";

static void postItemNotification(NSMenu *menu, NSString *name, NSUInteger index) {
   NSDictionary *info=[NSDictionary dictionaryWithObject:[NSNumber numberWithInteger:index]
                                                  forKey:@"NSMenuItemIndex"];

   [[NSNotificationCenter defaultCenter] postNotificationName:name object:menu userInfo:info];
}

@implementation NSMenu

/* Opens the menu with its top left (or the given item's) at a point on
   the screen, kept on the screen: above the point when it does not fit
   below, to its left when it does not fit on the right; at least
   minimumWidth wide. Then tracks it and, with perform, sends the chosen
   item's action; the item chosen, or nil. */
-(NSMenuItem *)_popUpAtScreenPoint:(NSPoint)screenPoint positioning:(NSMenuItem *)positioned
    event:(NSEvent *)event startedOpen:(BOOL)startedOpen minimumWidth:(CGFloat)minimumWidth
    perform:(BOOL)perform {
   if([_itemArray count]==0)
    return nil;

   NSMenuWindow *menuWindow=[[NSMenuWindow alloc] initWithMenu:self];
   NSMenuView   *menuView=[menuWindow menuView];
   NSRect        frame=[menuWindow frame];
   NSScreen     *screen=[NSScreen mainScreen];

   if(frame.size.width<minimumWidth){
    frame.size.width=minimumWidth;
    [menuWindow setFrame:frame display:NO];
    [menuView setFrameSize:NSMakeSize(minimumWidth,[menuView frame].size.height)];
   }
   for(NSScreen *check in [NSScreen screens])
    if(NSPointInRect(screenPoint,[check frame]))
     screen=check;

   NSRect   visible=[screen visibleFrame];
   NSPoint  topLeft=screenPoint;
   NSUInteger index=positioned?[[menuView visibleItemArray] indexOfObjectIdenticalTo:positioned]:NSNotFound;

   if(index!=NSNotFound)
    topLeft.y+=[menuView _topOfItemAtIndex:index];
   else if(!startedOpen){
    /* Just beside the pointer, not under it: no item is chosen until the
       pointer moves onto one. */
    topLeft.x+=1;
    topLeft.y-=1;
   }
   if(topLeft.y-frame.size.height<NSMinY(visible))
    topLeft.y=(index==NSNotFound)?screenPoint.y+frame.size.height:NSMinY(visible)+frame.size.height;
   if(topLeft.y>NSMaxY(visible))
    topLeft.y=NSMaxY(visible);
   if(topLeft.x+frame.size.width>NSMaxX(visible))
    topLeft.x=MAX(NSMinX(visible),screenPoint.x-frame.size.width);
   [menuWindow setFrameTopLeftPoint:topLeft];
   [menuWindow orderFront:nil];

   NSMenuItem *item=perform?[menuView _trackAndPerform:event selecting:index startedOpen:startedOpen]
                           :[menuView _trackMenus:event selecting:index startedOpen:startedOpen];

   [menuWindow close];
   return item;
}

+(void)popUpContextMenu:(NSMenu *)menu withEvent:(NSEvent *)event forView:(NSView *)view {
   NSWindow *window=[event window]?[event window]:[view window];

   [menu _popUpAtScreenPoint:[window convertBaseToScreen:[event locationInWindow]] positioning:nil
       event:event startedOpen:NO minimumWidth:0 perform:YES];
}

+(void)popUpContextMenu:(NSMenu *)menu withEvent:(NSEvent *)event forView:(NSView *)view withFont:(NSFont *)font {
   [self popUpContextMenu:menu withEvent:event forView:view];
}

/* The item (if any) is placed at the location, in the view's coordinates
   (the screen's with no view), and selected; the menu stays open until
   an item is chosen or it is left. YES when an item was chosen. */
-(BOOL)popUpMenuPositioningItem:(NSMenuItem *)item atLocation:(NSPoint)location inView:(NSView *)view {
   NSPoint screenPoint=location;

   if(view!=nil)
    screenPoint=[[view window] convertBaseToScreen:[view convertPoint:location toView:nil]];
   return [self _popUpAtScreenPoint:screenPoint positioning:item event:[NSApp currentEvent]
       startedOpen:YES minimumWidth:0 perform:YES]!=nil;
}

-(void)encodeWithCoder:(NSCoder *)coder {
   [coder encodeObject:_title forKey:@"NSTitle"];
   [coder encodeObject:_name forKey:@"NSName"];
   [coder encodeObject:_itemArray forKey:@"NSMenuItems"];
   [coder encodeBool:!_autoenablesItems forKey:@"NSNoAutoenable"];
}

-initWithCoder:(NSCoder *)coder {
   if([coder allowsKeyedCoding]){
    NSKeyedUnarchiver *keyed=(NSKeyedUnarchiver *)coder;
    
    _supermenu=[keyed decodeObjectForKey:@"NSMenu"];
    _title=[[keyed decodeObjectForKey:@"NSTitle"] copy];
    _name=[[keyed decodeObjectForKey:@"NSName"] copy];
    
    _itemArray=[[NSMutableArray alloc] initWithArray:[keyed decodeObjectForKey:@"NSMenuItems"]];
    _autoenablesItems=![keyed decodeBoolForKey:@"NSNoAutoenable"];    
   }
   else {
    [NSException raise:NSInvalidArgumentException format:@"%@ can not initWithCoder:%@",[self class], [coder class]];
   }
   return self;
}

-initWithTitle:(NSString *)title {
   _title=[title copy];
   _itemArray=[NSMutableArray new];
   _autoenablesItems=YES;
   return self;
}

-init {
   return [self initWithTitle:@""];
}

-initApplicationMenu:(NSString*)appName {
   [self init];
   NSMenuItem *quitItem = [[NSMenuItem alloc] initWithTitle:[@"Quit " stringByAppendingString:appName] action:@selector(terminate:) keyEquivalent:@"q"];
   [self addItem: quitItem];
   [self setValue:@"NSAppleMenu" forKey:@"name"];
   return self;
}

-(void)dealloc {
   [_title release];
   [_name release];
   [_itemArray makeObjectsPerformSelector:@selector(_setMenu:) withObject:nil];
   [_itemArray release];
   [super dealloc];
}

-copyWithZone:(NSZone *)zone {
	NSMenu *copy=NSCopyObject(self, 0, zone);
	
	copy->_title=[_title copyWithZone:zone];
	copy->_name=[_name copyWithZone:zone];
	copy->_itemArray = [[NSMutableArray alloc] init];
	for (NSMenuItem *item in _itemArray) {
		[copy addItem: [[item copyWithZone:zone] autorelease]];
    }
	
	return copy;
}

-(NSMenu *)supermenu {
   return _supermenu;
}

extern CGFloat _NSMenuBarHeightForDisplay(CGDirectDisplayID display);

-(CGFloat)menuBarHeight {
   if(self!=[NSApp mainMenu])
      return 0;
   return _NSMenuBarHeightForDisplay(CGMainDisplayID());
}

-(NSString *)title {
   return _title;
}

-(int)numberOfItems {
   return [_itemArray count];
}

-(NSArray *)itemArray {
   return _itemArray;
}

-(void)_setItemArray:itemArray {
	if(_itemArray!=itemArray)
	{
		[_itemArray release];
		_itemArray=[itemArray retain];
	}
}

-(BOOL)autoenablesItems {
   return _autoenablesItems;
}

-(NSMenuItem *)itemAtIndex:(int)index {
   return [_itemArray objectAtIndex:index];
}

-(NSMenuItem *)itemWithTag:(int)tag {
    int i,count=[_itemArray count];

    for(i=0;i<count;i++){
        NSMenuItem *item=[_itemArray objectAtIndex:i];

        if ([item tag] == tag)
            return item;
    }

    return nil;
}

-(NSMenuItem *)itemWithTitle:(NSString *)title {
   int i,count=[_itemArray count];

   for(i=0;i<count;i++){
    NSMenuItem *item=[_itemArray objectAtIndex:i];

    if([[item title] isEqualToString:title])
     return item;
   }

   return nil;
}

-(int)indexOfItem:(NSMenuItem *)item {
    return [_itemArray indexOfObjectIdenticalTo:item];
}

-(int)indexOfItemWithTag:(int)tag {
    int i,count=[_itemArray count];

    for (i=0; i<count; ++i)
        if ([[_itemArray objectAtIndex:i] tag] == tag)
            return i;

    return -1;
}

-(int)indexOfItemWithTitle:(NSString *)title {
    int i,count=[_itemArray count];

    for (i=0;i<count;i++)
        if ([[[_itemArray objectAtIndex:i] title] isEqualToString:title])
            return i;

    return -1;
}

-(int)indexOfItemWithRepresentedObject:object {
   int i,count=[_itemArray count];
   
   for(i=0;i<count;i++)
    if([[(NSMenuItem *)[_itemArray objectAtIndex:i] representedObject] isEqual:object])
     return i;

   return -1;
}

// needed this for NSApplication windowsMenu stuff, so i did 'em all..
-(int)indexOfItemWithTarget:(id)target andAction:(SEL)action {
    int i,count=[_itemArray count];

    for (i=0; i<count; ++i) {
        NSMenuItem *item = [_itemArray objectAtIndex:i];

        if ([item target] == target) {
            if (action == NULL)
                return i;
            else if ([item action] == action)
                return i;
        }
    }

    return -1;
}

-(int)indexOfItemWithSubmenu:(NSMenu *)submenu {
    int i, count=[_itemArray count];

    for (i = 0; i < count; ++i) 
        if ([[_itemArray objectAtIndex:i] submenu] == submenu)
            return i;

    return -1;
}

-(void)setSupermenu:(NSMenu *)value {
   _supermenu=value;
}

-(void)setTitle:(NSString *)title {
   title=[title copy];
   [_title release];
   _title=title;
}

-(void)setAutoenablesItems:(BOOL)flag {
   _autoenablesItems=flag;
}

-(void)addItem:(NSMenuItem *)item {
   [item performSelector:@selector(_setMenu:) withObject:self];
   [_itemArray addObject:item];
   postItemNotification(self,NSMenuDidAddItemNotification,[_itemArray count]-1);
}

-(NSMenuItem *)addItemWithTitle:(NSString *)title action:(SEL)action keyEquivalent:(NSString *)keyEquivalent {
   NSMenuItem *item=[[[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:keyEquivalent] autorelease];

   [self addItem:item];

   return item;
}

-(void)removeAllItems {
   while([_itemArray count]>0)
    [self removeItem:[_itemArray lastObject]];
}

-(void)removeItem:(NSMenuItem *)item {
   NSUInteger index=[_itemArray indexOfObjectIdenticalTo:item];

   if(index==NSNotFound)
    return;
   [item retain];
   [item performSelector:@selector(_setMenu:) withObject:nil];
   [_itemArray removeObjectAtIndex:index];
   postItemNotification(self,NSMenuDidRemoveItemNotification,index);
   [item release];
}

-(void)removeItemAtIndex:(int)index {
   [self removeItem:[_itemArray objectAtIndex:index]];
}

-(void)insertItem:(NSMenuItem *)item atIndex:(int)index {
   [item performSelector:@selector(_setMenu:) withObject:self];
   [_itemArray insertObject:item atIndex:index];
   postItemNotification(self,NSMenuDidAddItemNotification,index);
}

-(NSMenuItem *)insertItemWithTitle:(NSString *)title action:(SEL)action keyEquivalent:(NSString *)keyEquivalent atIndex:(int)index {
   NSMenuItem *item=[[[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:keyEquivalent] autorelease];

   [self insertItem:item atIndex:index];

   return item;
}

-(void)setSubmenu:(NSMenu *)submenu forItem:(NSMenuItem *)item {
   [item setSubmenu:submenu];
}

BOOL itemIsEnabled(NSMenuItem *item) {
    BOOL enabled=NO;
    
    if([item action]!=NULL){
        id target=[item target];
        
        target=[NSApp targetForAction:[item action] to:[item target] from:nil];
        
        if ((target == nil) || ![target respondsToSelector:[item action]]) {
            enabled = NO;
        } else if ([target respondsToSelector:@selector(validateMenuItem:)]) {
            enabled = [target validateMenuItem:item];
        } else if ([target respondsToSelector:@selector(validateUserInterfaceItem:)]) { // New validation scheme
            enabled = (BOOL)[target validateUserInterfaceItem:item];
        } else {
            enabled = YES;
        }
    } 

    return enabled;
}

-(void)update {
    if ([_delegate respondsToSelector:@selector(menuNeedsUpdate:)]) {
        [_delegate menuNeedsUpdate:self];
    }

    int i,count=[_itemArray count];
    
    for(i=0;i<count;i++){
        NSMenuItem *item=[_itemArray objectAtIndex:i];
        
        if(_autoenablesItems){
            BOOL enabled = itemIsEnabled(item) ? YES : NO;
            BOOL currentlyEnabled = [item isEnabled] ? YES : NO;
            
            if(enabled!=currentlyEnabled && ![item _binderForBinding:@"enabled" create:NO]){
                [item setEnabled:enabled]; // posts the change
            }
        }
    }
    /* Not the submenus: each is updated as it opens, or before its key
       equivalents are looked at. */
}

-(void)itemChanged:(NSMenuItem *)item {
   NSUInteger index=[_itemArray indexOfObjectIdenticalTo:item];

   if(index!=NSNotFound)
    postItemNotification(self,NSMenuDidChangeItemNotification,index);
}

/* Whether an item's key equivalent is this key event: the same key
   (charactersIgnoringModifiers, which keeps Shift's effect) and the same
   Control, Option, Shift and Command. An uppercase letter as the key
   equivalent implies Shift. */
static BOOL itemMatchesKeyEvent(NSMenuItem *item, NSString *characters, NSUInteger modifiers) {
    NSString  *key=[item keyEquivalent];
    NSUInteger relevant=NSControlKeyMask|NSAlternateKeyMask|NSShiftKeyMask|NSCommandKeyMask;
    NSUInteger itemModifiers=[item keyEquivalentModifierMask]&relevant;

    if([key length]==0 || [item isHidden])
        return NO;
    if(![key isEqualToString:[key lowercaseString]])
        itemModifiers|=NSShiftKeyMask;
    modifiers&=relevant;
    if([[key lowercaseString] isEqualToString:[characters lowercaseString]])
        return modifiers==itemModifiers;
    /* A key Shift makes (such as ? on most layouts) matches as typed. */
    if([key isEqualToString:characters])
        return (modifiers&~NSShiftKeyMask)==(itemModifiers&~NSShiftKeyMask);
    return NO;
}

-(BOOL)performKeyEquivalent:(NSEvent *)event {
    int       i,count;
    NSString *characters=[event charactersIgnoringModifiers];
    NSUInteger modifiers=[event modifierFlags];

    /* The delegate may answer for the menu without its items being
       brought up to date. */
    if([_delegate respondsToSelector:@selector(menuHasKeyEquivalent:forEvent:target:action:)]){
        id  target=nil;
        SEL action=NULL;

        if(![_delegate menuHasKeyEquivalent:self forEvent:event target:&target action:&action])
            return NO;
        if(action!=NULL)
            return [NSApp sendAction:action to:target from:self];
    }

    if (_autoenablesItems)
        [self update];

    count=[_itemArray count];
    for(i=0;i<count;i++){
        NSMenuItem *item=[_itemArray objectAtIndex:i];

        if(itemMatchesKeyEvent(item,characters,modifiers)){
            /* This *must* accurately reflect menu validation when ignoring or processing
                key equivalents. Relying on update to keep isEnabled in the proper state is
                unfortunately too tenuous.
             */
            BOOL enabled=_autoenablesItems?itemIsEnabled(item):[item isEnabled];

            if(enabled)
                return [self _sendActionOfItem:item];
            return NO;
        }

        if([[item submenu] performKeyEquivalent:event])
            return YES;
    }

    return NO;
}

/* An item's action, between NSMenuWillSendActionNotification and
   NSMenuDidSendActionNotification (MenuItem in their user info). */
-(BOOL)_sendActionOfItem:(NSMenuItem *)item {
    NSNotificationCenter *center=[NSNotificationCenter defaultCenter];
    NSDictionary         *info=[NSDictionary dictionaryWithObject:item forKey:@"MenuItem"];
    BOOL                  result;

    [center postNotificationName:NSMenuWillSendActionNotification object:self userInfo:info];
    result=[NSApp sendAction:[item action] to:[item target] from:item];
    [center postNotificationName:NSMenuDidSendActionNotification object:self userInfo:info];
    return result;
}

-(BOOL)performClickEquivalent:(NSMenuItem *)item {
    if(_autoenablesItems)
        [self update];

    if(itemIsEnabled(item))
        return [NSApp sendAction:[item action] to:[item target] from:item];

    return NO;
}


- (void)setDelegate:(id <NSMenuDelegate>)object
{
	_delegate = object;
}

- (id<NSMenuDelegate>)delegate
{
	return _delegate;
}

-(NSString *)_name {
   return _name;
}

-(NSMenu *)_menuWithName:(NSString *)name {
   if([_name isEqual:name])
    return self;
   else {
    int i,count=[_itemArray count];
    
    for(i=0;i<count;i++){
     NSMenu *check=[[[_itemArray objectAtIndex:i] submenu] _menuWithName:name];
     
     if(check!=nil)
      return check;
    }
   }   
   
   return nil;
}

-(int)DBusItemID {
    return _DBusItemID;
}

-(void)setDBusItemID:(int)itemID {
    _DBusItemID=itemID;
}


@end
