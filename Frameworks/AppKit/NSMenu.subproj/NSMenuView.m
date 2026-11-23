/*
 * Copyright (C) 2024 Zoe Knox <zoe@ravynsoft.com>
 * Contains code Copyright (c) 2006-2007 Christopher J. W. Lloyd
 * Modifications Copyright (c) 2026 The Minkar contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#import "NSMenuBarView.h"
#import "NSMenuWindow.h"

#define MIN_TITLE_KEY_GAP 8
#define WINDOW_BORDER_THICKNESS 1
#define IMAGE_TITLE_GAP 8

static NSRect boundsToTitleAreaRect(NSRect rect){
   return NSInsetRect(rect, WINDOW_BORDER_THICKNESS, WINDOW_BORDER_THICKNESS);
}

#import <AppKit/NSWindow-Private.h>
#include <SkyLight/SkyLight.h>

extern int _CGUsesSkyLight(void);

enum {
	kNSMenuKeyboardNavigationNone,
	kNSMenuKeyboardNavigationUp,
	kNSMenuKeyboardNavigationDown,
	kNSMenuKeyboardNavigationLeft,
	kNSMenuKeyboardNavigationRight,
	kNSMenuKeyboardNavigationLetter,
	kNSMenuKeyboardNavigationStart,	// tracking begun from the keyboard
	kNSMenuKeyboardNavigationSearch	// a key the Help search took
};

@interface NSMenuView (NSMenuViewAlternatesAndSearch)
-(void)_modifiersChanged;
-(void)_showHelpSearch;
-(BOOL)_helpSearchKey:(NSEvent *)event;
@end

@implementation NSMenuView

/* The menu bar answers YES: its items run left to right. */
-(BOOL)_isMenuBar {
    return NO;
}

-(NSScreen *)_screenForPoint:(NSPoint)point {
   NSArray *screens=[NSScreen screens];
   int      i,count=[screens count];

   for(i=0;i<count;i++){
    NSScreen *check=[screens objectAtIndex:i];

    if(NSPointInRect(point,[check frame]))
     return check;
   }

   return [screens objectAtIndex:0];// should not happen
}


/* ---- Searching from the Help menu (MB2) ----
   The Help menu opens with a Search line at its top; what is typed while
   it is open looks through the app's other menus, the items whose titles
   hold it listed below as items of the Help menu that do what the
   originals do. */
enum { kHelpSearchTag = 0x48534552, kHelpResultTag = 0x48524553 };
static NSMutableString *helpSearchText;

static NSMenu *appHelpMenu(void) {
    return [NSApp helpMenu];
}

static void collectHelpResults(NSMenu *menu, NSString *path, NSString *text, NSMutableArray *found) {
    for(NSMenuItem *item in [menu itemArray]) {
        if([found count] >= 12)
            return;
        if([item isSeparatorItem] || [item isHidden] || [[item title] length] == 0)
            continue;
        NSString *here = [path length] ? [path stringByAppendingFormat:@" \u25B8 %@", [item title]] : [item title];
        if([item hasSubmenu]) {
            if([item submenu] != appHelpMenu())
                collectHelpResults([item submenu], [path length] ? here : [item title], text, found);
            continue;
        }
        if([[item title] rangeOfString:text options:NSCaseInsensitiveSearch].location != NSNotFound)
            [found addObject:[NSArray arrayWithObjects:here, item, nil]];
    }
}

static void resetHelpMenu(NSMenu *menu) {
    for(NSInteger i = [menu numberOfItems] - 1; i >= 0; i--) {
        NSInteger tag = [[menu itemAtIndex:i] tag];
        if(tag == kHelpSearchTag || tag == kHelpResultTag)
            [menu removeItemAtIndex:i];
    }
}

-(void)_showHelpSearch {
    NSMenu *menu = [self menu];
    NSString *prompt = [[NSBundle bundleForClass:[NSMenuView class]] localizedStringForKey:@"Search"
        value:@"Search" table:nil];
    NSString *title = [helpSearchText length] ? [NSString stringWithFormat:@"%@: %@", prompt, helpSearchText] : prompt;

    resetHelpMenu(menu);
    NSMenuItem *search = [[[NSMenuItem alloc] initWithTitle:title action:NULL keyEquivalent:@""] autorelease];
    [search setTag:kHelpSearchTag];
    [search setEnabled:NO];
    [menu insertItem:search atIndex:0];
    NSMenuItem *separator = [NSMenuItem separatorItem];
    [separator setTag:kHelpSearchTag];
    [menu insertItem:separator atIndex:1];
    if([helpSearchText length] == 0)
        return;
    NSMutableArray *found = [NSMutableArray array];
    collectHelpResults([NSApp mainMenu], @"", helpSearchText, found);
    NSInteger at = 2;
    for(NSArray *pair in found) {
        NSMenuItem *original = [pair objectAtIndex:1];
        NSMenuItem *result = [[[NSMenuItem alloc] initWithTitle:[pair objectAtIndex:0] action:[original action]
            keyEquivalent:@""] autorelease];
        [result setTarget:[original target]];
        [result setRepresentedObject:[original representedObject]];
        [result setTag:kHelpResultTag];
        [menu insertItem:result atIndex:at++];
    }
}

/* A key typed in the Help menu: the search grows or shrinks, and the
   first result is selected. Arrows, Return and Escape go on as usual. */
-(BOOL)_helpSearchKey:(NSEvent *)event {
    NSString *chars = [event characters];
    unichar c = [chars length] ? [chars characterAtIndex:0] : 0;

    if([self menu] == nil || [self menu] != appHelpMenu() || [self _isMenuBar])
        return NO;
    if(c == NSDeleteCharacter || c == NSBackspaceCharacter) {
        if([helpSearchText length] > 0)
            [helpSearchText deleteCharactersInRange:NSMakeRange([helpSearchText length] - 1, 1)];
    } else if(c >= ' ' && c < NSUpArrowFunctionKey && !([event modifierFlags] & (NSCommandKeyMask | NSControlKeyMask)))
        [helpSearchText appendString:chars];
    else
        return NO;
    [self _showHelpSearch];
    [self _modifiersChanged];
    [self setSelectedItemIndex:[[self visibleItemArray] count] > 2 ? 2 : NSNotFound];
    return YES;
}

-initWithMenu:(NSMenu *)menu {
    NSRect frame = NSZeroRect;
    self = [self initWithFrame:frame];

    _menu = menu;
    _selectedItemIndex = NSNotFound;
    /* The Help menu opens with its Search line and no results. */
    if(menu != nil && menu == appHelpMenu()) {
        if(helpSearchText == nil)
            helpSearchText = [[NSMutableString alloc] init];
        [helpSearchText setString:@""];
        [self _showHelpSearch];
    }

    frame.size = [self sizeForMenuItems:[self visibleItemArray]];
    [self setFrame:frame];
    _contentHeight = frame.size.height;

    return self;
}

-(void)dealloc {
    free(_itemTops);
    [super dealloc];
}

-(Margins)menuItemTextMargins {
    Margins result = {16};
    result.top = 4;
    result.bottom = 4;
    return result;
}

-(BOOL)isFlipped {
    return YES;
}

-(BOOL)isOpaque {
    return YES;
}

-(NSArray *)itemArray {
   return [[self menu] itemArray];
}

-(NSArray *)visibleItemArray {
    NSArray *items = [[self menu] itemArray];
   
    // Construct a new array of just the visible items
    if(_visibleArray == nil)
        _visibleArray = [NSMutableArray new];
    [_visibleArray removeAllObjects];
   
    /* An item and the alternates that follow it (same key, other
       modifiers) show as one: the one whose modifiers are held now, else
       the first, as on the reference platform. */
    NSUInteger held = [NSEvent modifierFlags] &
        (NSCommandKeyMask | NSAlternateKeyMask | NSControlKeyMask | NSShiftKeyMask);
    NSUInteger count = [items count];
    for(NSUInteger i = 0; i < count; ++i) {
        NSMenuItem *item = [items objectAtIndex:i];
        if([item isHidden])
            continue;
        NSUInteger end = i + 1;
        while(end < count && [[items objectAtIndex:end] isAlternate] &&
              [[[items objectAtIndex:end] keyEquivalent] isEqualToString:[item keyEquivalent]])
            end++;
        NSMenuItem *shown = item;
        NSUInteger primary = [item keyEquivalentModifierMask] &
            (NSCommandKeyMask | NSAlternateKeyMask | NSControlKeyMask | NSShiftKeyMask);
        for(NSUInteger j = i; j < end; j++) {
            NSMenuItem *candidate = [items objectAtIndex:j];
            NSUInteger mask = [candidate keyEquivalentModifierMask] &
                (NSCommandKeyMask | NSAlternateKeyMask | NSControlKeyMask | NSShiftKeyMask);
            /* The modifiers that set it apart from the primary are held
               (Option for Option-Command after Command), or all of its. */
            if(j > i && (held == (mask & ~primary) || held == mask) && held != 0 && ![candidate isHidden]) {
                shown = candidate;
                break;
            }
        }
        [_visibleArray addObject:shown];
        i = end - 1;
    }

    return _visibleArray;
}

/* The modifiers changed while open: the alternates that show now, the
   menu sized to them. */
-(void)_modifiersChanged {
    NSArray *items = [self visibleItemArray];
    NSSize size = [self sizeForMenuItems:items];
    NSWindow *window = [self window];

    if(![self _isMenuBar] && window != nil && !NSEqualSizes(size, [self frame].size)) {
        NSRect frame = [window frame];
        CGFloat top = NSMaxY(frame);
        frame.size = [window frameRectForContentRect:NSMakeRect(0, 0, size.width, size.height)].size;
        frame.origin.y = top - frame.size.height;
        [window setFrame:frame display:NO];
        [self setFrameSize:size];
    }
    [self setNeedsDisplay:YES];
}

-(CGFloat)heightOfMenuItem:(NSMenuItem *)item {
    NSSize titleSize = [self contentSizeForItem:item];
    return titleSize.height;
}

#define ITEM_MAX(size) height = MAX(height, size.height); width += size.width;
-(NSSize)contentSizeForItem:(NSMenuItem *)item {
    /* An item with a view (a volume slider): the view's size. */
    if([item view] != nil)
        return [[item view] frame].size;
    if([item isSeparatorItem])	{
        return [[self graphicsStyle] menuItemSeparatorSize];
    } else {
        float width = [[self graphicsStyle] menuItemGutterGap];
        float height = 0.0f;

        NSSize size = [[self graphicsStyle] menuItemGutterSize];
        ITEM_MAX(size);

        size = [self sizeForMenuItemImage:item];
        if(size.width > 0.0f) {
            ITEM_MAX(size);
            width += IMAGE_TITLE_GAP;
        }

        size = [[self graphicsStyle] menuItemTextSize:[item title]];
        ITEM_MAX(size);

        if([[item keyEquivalent] length] == 0) {
            size = [[self graphicsStyle] menuItemTextSize:[item _keyEquivalentDescription]];
            ITEM_MAX(size);
        }

        if([item hasSubmenu] && [[item submenu] _hasItemsToShow]) {
            size = [[self graphicsStyle] menuItemBranchArrowSize];
            ITEM_MAX(size);
        }
		
        return NSMakeSize(width, MAX(height, kNSMenuItemHeight));
    }
}

-(NSSize)sizeForMenuItemImage:(NSMenuItem *)item {
    if([item image] != nil)
        return [[item image] size];
    return NSZeroSize;
}

-(NSSize)sizeForMenuItems:(NSArray *)items {
    NSSize result = NSZeroSize;
    float maxTitleWidth = 0;
    BOOL anItemHasAnImage = NO;
    float maxKeyWidth = 0; 
    float totalHeight = 0;
    NSSize gutterSize = [[self graphicsStyle] menuItemGutterSize];
    NSSize rightArrowSize = [[self graphicsStyle] menuItemBranchArrowSize];
    NSSize separatorSize = [[self graphicsStyle] menuItemSeparatorSize];
	
    float maxViewWidth = 0;

    for(int i = 0; i < [items count]; ++i) {
        NSMenuItem *item = [items objectAtIndex:i];
        if([item view] != nil) {
            NSSize size = [[item view] frame].size;
            totalHeight += size.height;
            maxViewWidth = MAX(maxViewWidth, size.width);
        } else if([item isSeparatorItem]) {
            totalHeight += separatorSize.height;
        } else {
            NSSize size = NSZeroSize;
            float height = 0;
            float titleAndIconWidth = 0;
			
            size = [self sizeForMenuItemImage:item];
            height = MAX(height, size.height);
            if ((titleAndIconWidth = size.width) > 0.0f)
                anItemHasAnImage = YES;

            size = [[self graphicsStyle] menuItemTextSize:[item title]];
            titleAndIconWidth += size.width;
            maxTitleWidth = MAX(maxTitleWidth, titleAndIconWidth);
            height = MAX(height, size.height);
            
            if([[item keyEquivalent] length] != 0)
            {
                size = [[self graphicsStyle] menuItemTextSize:[item _keyEquivalentDescription]];
                maxKeyWidth = MAX(maxKeyWidth, size.width);
                height = MAX(height, size.height);
            }
            height = MAX(height, gutterSize.height);
            height = MAX(height, rightArrowSize.height);
			
            totalHeight += height;
        }
    }
	
    result.height = totalHeight;
    result.width = gutterSize.width;
    result.width += [[self graphicsStyle] menuItemGutterGap];

    if(anItemHasAnImage)
        result.width += IMAGE_TITLE_GAP;

    result.width += maxTitleWidth;
    
    if(maxKeyWidth > 0.0f) {
        result.width += MIN_TITLE_KEY_GAP;
        result.width += maxKeyWidth;
    }

    result.width += rightArrowSize.width;
	
    result.width = MAX(result.width, maxViewWidth);

    // border
    result.width += WINDOW_BORDER_THICKNESS*2;
    result.height += WINDOW_BORDER_THICKNESS*2;
    
    return result;
}

/* Each visible item's top in the content, measured once while the menu
   is open (measuring every title on every move made long menus slow). */
-(void)_layoutItems {
    NSArray *items = [self visibleItemArray];
    NSUInteger count = [items count];
    CGFloat y = WINDOW_BORDER_THICKNESS;

    free(_itemTops);
    _itemTops = malloc((count + 1) * sizeof(CGFloat));
    _itemTopsCount = count;
    for(NSUInteger i = 0; i < count; i++) {
        _itemTops[i] = y;
        y += [self heightOfMenuItem:[items objectAtIndex:i]];
    }
    _itemTops[count] = y;
}

-(CGFloat)_topOfItemAtIndex:(NSUInteger)index {
    if(_itemTops == NULL)
        [self _layoutItems];
    if(index > _itemTopsCount)
        index = _itemTopsCount;
    return _itemTops[index];
}

/* The item whose span holds y (in the content), or NSNotFound. */
-(NSUInteger)_itemIndexAtContentY:(CGFloat)y {
    NSUInteger low = 0, high;

    if(_itemTops == NULL)
        [self _layoutItems];
    high = _itemTopsCount;
    if(high == 0 || y < _itemTops[0] || y >= _itemTops[high])
        return NSNotFound;
    while(high - low > 1) {
        NSUInteger middle = (low + high) / 2;
        if(_itemTops[middle] <= y)
            low = middle;
        else
            high = middle;
    }
    return low;
}

/* ---- Scrolling --------------------------------------------------------- */

#define SCROLL_ARROW_HEIGHT 18

-(BOOL)_isScrollable {
    return _contentHeight > [self bounds].size.height + 0.5;
}

-(CGFloat)_maximumScrollOffset {
    CGFloat max = _contentHeight - [self bounds].size.height;
    return max > 0 ? max : 0;
}

-(CGFloat)_scrollOffset {
    return _scrollOffset;
}

-(void)_setScrollOffset:(CGFloat)offset {
    if(offset > [self _maximumScrollOffset])
        offset = [self _maximumScrollOffset];
    if(offset < 0)
        offset = 0;
    if(offset != _scrollOffset) {
        _scrollOffset = floor(offset);
        [self setNeedsDisplay:YES];
    }
}

-(BOOL)_showsTopArrow {
    return [self _isScrollable] && _scrollOffset > 0;
}

-(BOOL)_showsBottomArrow {
    return [self _isScrollable] && _scrollOffset < [self _maximumScrollOffset];
}

/* The window and the view cut to the height; the content scrolls. */
-(void)_fitToHeight:(CGFloat)height {
    NSWindow *window = [self window];
    NSRect frame = [window frame];

    height = floor(height);
    if(_contentHeight <= height || height < 3 * SCROLL_ARROW_HEIGHT)
        return;
    frame.origin.y += frame.size.height - height;
    frame.size.height = height;
    [window setFrame:frame display:NO];
    [self setFrame:NSMakeRect(0, 0, frame.size.width, height)];
    [self _setScrollOffset:_scrollOffset];
}

/* One item's height a step, faster the further over the arrow. */
-(BOOL)_scrollAtPoint:(NSPoint)point {
    CGFloat step = kNSMenuItemHeight;

    if([self _showsTopArrow] && point.y < SCROLL_ARROW_HEIGHT) {
        [self _setScrollOffset:_scrollOffset - (point.y < SCROLL_ARROW_HEIGHT / 2 ? 2 * step : step)];
        return YES;
    }
    if([self _showsBottomArrow] && point.y >= [self bounds].size.height - SCROLL_ARROW_HEIGHT) {
        BOOL deep = point.y >= [self bounds].size.height - SCROLL_ARROW_HEIGHT / 2;
        [self _setScrollOffset:_scrollOffset + (deep ? 2 * step : step)];
        return YES;
    }
    return NO;
}

-(void)_scrollByWheel:(CGFloat)deltaY {
    if([self _isScrollable])
        [self _setScrollOffset:_scrollOffset - deltaY * kNSMenuItemHeight];
}

/* An item's rectangle in the view (scrolled). */
-(NSRect)_rectOfItemAtIndex:(NSUInteger)index {
    CGFloat top = [self _topOfItemAtIndex:index];
    CGFloat bottom = [self _topOfItemAtIndex:index + 1];

    return NSMakeRect(0, top - _scrollOffset, [self bounds].size.width, bottom - top);
}

/* The item brought between the arrows. */
-(void)_scrollItemToVisible:(NSUInteger)index {
    CGFloat top, bottom, height = [self bounds].size.height;

    if(![self _isScrollable] || index >= _itemTopsCount)
        return;
    top = [self _topOfItemAtIndex:index];
    bottom = [self _topOfItemAtIndex:index + 1];
    if(top - _scrollOffset < SCROLL_ARROW_HEIGHT && top > [self _topOfItemAtIndex:0])
        [self _setScrollOffset:top - SCROLL_ARROW_HEIGHT];
    else if(top <= [self _topOfItemAtIndex:0])
        [self _setScrollOffset:0];
    if(bottom - _scrollOffset > height - SCROLL_ARROW_HEIGHT && index + 1 < _itemTopsCount)
        [self _setScrollOffset:bottom - height + SCROLL_ARROW_HEIGHT];
    else if(index + 1 >= _itemTopsCount)
        [self _setScrollOffset:bottom - height + WINDOW_BORDER_THICKNESS];
}

/* A submenu opens to the right of this menu, its first item level with
   the selected one; to the left when it does not fit on the right, and
   moved up when it does not fit below. */
-(void)positionBranchForSelectedItem:(NSWindow *)branch screen:(NSScreen *)screen {
    NSRect branchFrame = [branch frame];
    NSRect screenVisible = [screen visibleFrame];
    CGFloat y = [self _topOfItemAtIndex:_selectedItemIndex] - _scrollOffset;

    /* The view is flipped: the item's top is y, the branch's border
       above it. A branch taller than the screen scrolls. */
    if(branchFrame.size.height > screenVisible.size.height &&
       [branch respondsToSelector:@selector(menuView)]) {
        [[(NSMenuWindow *)branch menuView] _fitToHeight:screenVisible.size.height];
        branchFrame = [branch frame];
    }
    NSPoint topLeft = NSMakePoint(NSMaxX([self bounds]), y - WINDOW_BORDER_THICKNESS);
    topLeft = [[self window] convertBaseToScreen:[self convertPoint:topLeft toView:nil]];

    if(topLeft.x + branchFrame.size.width > NSMaxX(screenVisible))
        topLeft.x = NSMinX([[self window] frame]) - branchFrame.size.width;
    if(topLeft.x < NSMinX(screenVisible))
        topLeft.x = NSMinX(screenVisible);
    if(topLeft.y - branchFrame.size.height < NSMinY(screenVisible))
        topLeft.y = NSMinY(screenVisible) + branchFrame.size.height;
    if(topLeft.y > NSMaxY(screenVisible))
        topLeft.y = NSMaxY(screenVisible);

    [branch setFrameTopLeftPoint:topLeft];
    [self setNeedsDisplay:YES];
}

-(NSMenuView *)viewAtSelectedIndexPositionOnScreen:(NSScreen *)screen {
    NSArray *items = [self visibleItemArray];

#if 0
    if(_selectedItemIndex==[items count]) {
        NSMenuWindow *branch=[[NSMenuWindow alloc] initWithMenu:[self menu] overflowAtIndex:[self overflowIndex]];

        [self positionBranchForSelectedItem:branch screen:screen];

        [branch orderFront:nil];
        return [branch menuView];
    }
#endif

    if(_selectedItemIndex < [items count]) {
        NSMenuItem *item = [items objectAtIndex:_selectedItemIndex];

        if([item hasSubmenu]) {
            if([[item submenu] _fillToOpen]) {
                NSMenuWindow *branch = [[NSMenuWindow alloc] initWithMenu:[item submenu]];

                [self positionBranchForSelectedItem:branch screen:screen];

                [branch orderFront:nil];
                return [branch menuView];
            }
        }
    }
    return nil;
}

-(NSRect)titleRectForItem:(NSMenuItem *)item previousBorderRect:(NSRect)previousBorderRect {
    NSRect result = NSZeroRect;
    NSSize titleSize = NSZeroSize;

    titleSize = [[self graphicsStyle] menuItemTextSize:[item title]];
    NSImage *img = [item image];
    if(img) {
        titleSize.width += [img size].width;
        titleSize.height = MAX(titleSize.height, kNSMenuItemHeight);
    }

    result.origin = NSMakePoint(6, previousBorderRect.origin.y);
    result.size = titleSize;
    return result;
}

-(NSRect)borderRectFromTitleRect:(NSRect)titleRect {
    NSRect result = NSInsetRect(titleRect, -6, 0);
    result.size.width = [self bounds].size.width;
    return result;
}
 
-(NSImage *)overflowImage {
   return [NSImage imageNamed:@"NSMenuViewDoubleRightArrow"];
}

-(NSRect)overflowRect {
   NSRect bounds = [self bounds];
   NSImage *image = [self overflowImage];
   NSSize size = [image size];
   NSRect rect = NSInsetRect(NSMakeRect(0, 0, size.width, bounds.size.height), -3, 0);

   rect.origin.x = NSMaxX(bounds) - rect.size.width;
   return rect;
}


/* The item under a point of the view: none over a scroll arrow. */
-(NSUInteger)itemIndexAtPoint:(NSPoint)point {
    if(point.x < 0 || point.x >= [self bounds].size.width)
        return NSNotFound;
    if([self _showsTopArrow] && point.y < SCROLL_ARROW_HEIGHT)
        return NSNotFound;
    if([self _showsBottomArrow] && point.y >= [self bounds].size.height - SCROLL_ARROW_HEIGHT)
        return NSNotFound;
    return [self _itemIndexAtContentY:point.y + _scrollOffset];
}

-(NSUInteger)selectedItemIndex {
   return _selectedItemIndex;
}

-(void)setSelectedItemIndex:(NSUInteger)itemIndex {
    if (_selectedItemIndex != itemIndex) {
        NSArray *items = [self visibleItemArray];
        id delegate = [[self menu] delegate];

        if ([delegate respondsToSelector:@selector(menu:willHighlightItem:)])
            [delegate menu:[self menu] willHighlightItem:
                (itemIndex < [items count]) ? [items objectAtIndex:itemIndex] : nil];
        NSUInteger previous = _selectedItemIndex;

        _selectedItemIndex=itemIndex;
        if([self _isMenuBar]) {
            [self setNeedsDisplay:YES];
            return;
        }
        /* Only the two items change. */
        if(previous < [items count])
            [self setNeedsDisplayInRect:[self _rectOfItemAtIndex:previous]];
        if(itemIndex < [items count]) {
            [self _scrollItemToVisible:itemIndex];
            [self setNeedsDisplayInRect:[self _rectOfItemAtIndex:itemIndex]];
        }
    }
}

-(NSMenuItem *)itemAtSelectedIndex {
   NSArray *items=[self visibleItemArray];

   if(_selectedItemIndex<[items count])
    return [items objectAtIndex:_selectedItemIndex];

   return nil;
}

// This exists because most of AppKit (incl NSView) is not built with ARC, and
// we need something that uses ARC to hold a ref to the window, or it will get
// deallocated prematurely!
-(void)setWindow:(NSWindow *)window {
    _menuWindow = window;
}

#define CENTER_PART_RECT_VERTICALLY(partSize)                                \
{                                                                            \
	NSSize __partSize = (partSize);                                      \
	partRect.origin.y = origin.y + (itemHeight - __partSize.height) / 2; \
	partRect.size.height = __partSize.height;                            \
	partRect.size.width = __partSize.width;                              \
}

#define NSMENUDEBUG 0
-(void)drawRect:(NSRect)rect {
    NSRect bounds = [self bounds];
    NSRect itemArea = boundsToTitleAreaRect(bounds);
    NSArray *items = [self visibleItemArray];
    unsigned i, count = [items count];
    NSPoint origin = itemArea.origin;
    float separatorHeight = [[self graphicsStyle] menuItemSeparatorSize].height; 

    [[self graphicsStyle] drawMenuWindowBackgroundInRect:rect]; // FIXME: MenuBarBackground?	

    /* Only the items in the rectangle; the content moved up by the
       scroll. */
    if(_itemTops == NULL || _itemTopsCount != count)
        [self _layoutItems];
    CGContextRef scrollContext = [[NSGraphicsContext currentContext] graphicsPort];
    CGFloat contentTop = NSMinY(rect) + _scrollOffset, contentBottom = NSMaxY(rect) + _scrollOffset;
    NSUInteger first = [self _itemIndexAtContentY:contentTop];

    if(first == NSNotFound)
        first = (count > 0 && contentTop < _itemTops[0]) ? 0 : count;
    if(first < count)
        origin.y = _itemTops[first];
    CGContextSaveGState(scrollContext);
    CGContextTranslateCTM(scrollContext, 0, -_scrollOffset);
    for(i = first; i < count && _itemTops[i] < contentBottom; i++) {
        NSMenuItem *item = [items objectAtIndex:i];
		
        if([item view] != nil) {
            /* An item's view: in its place, drawing itself; no selection. */
            NSView *view = [item view];
            NSRect frame = NSMakeRect(origin.x, origin.y - _scrollOffset, NSWidth(itemArea), NSHeight([view frame]));
            if([view superview] != self)
                [self addSubview:view];
            if(!NSEqualRects([view frame], frame))
                [view setFrame:frame];
            origin.y += NSHeight(frame);
        } else if([item isSeparatorItem]) {
            NSRect separatorRect = NSMakeRect(origin.x, origin.y, NSWidth(itemArea), separatorHeight);
            [[self graphicsStyle] drawMenuSeparatorInRect:separatorRect];
#if NSMENUDEBUG
            [[NSColor yellowColor] setStroke];
            NSFrameRect(separatorRect);
#endif
            origin.y += NSHeight(separatorRect);
        } else {
            NSImage *image = [item image];
            BOOL selected = (i == _selectedItemIndex) ? YES : NO;
            float itemHeight = [self heightOfMenuItem:item];
            NSRect partRect;
            NSSize partSize;
            BOOL showsEnabled = ([item isEnabled] || [item hasSubmenu]);
			
            partRect = NSMakeRect(origin.x, origin.y, itemArea.size.width, itemHeight);
#if NSMENUDEBUG
            [[NSColor redColor] setStroke];
            NSFrameRect(partRect);
#endif
            if(selected)
                [[self graphicsStyle] drawMenuSelectionInRect:partRect enabled:showsEnabled];

            // Draw the gutter and checkmark (if any)
            CENTER_PART_RECT_VERTICALLY([[self graphicsStyle] menuItemGutterSize]);
            if ([item state]) {
                [[self graphicsStyle] drawMenuGutterInRect:partRect];			
                [[self graphicsStyle] drawMenuCheckmarkInRect:partRect enabled:showsEnabled selected:selected];
            }
			
            partRect.origin.x += [[self graphicsStyle] menuItemGutterGap];;
			
            // Draw the image
            if(image) {
                NSRect imageRect;
				
                partRect.origin.x += partRect.size.width;
                CENTER_PART_RECT_VERTICALLY([image size]);
				
                CGContextRef ctx = [[NSGraphicsContext currentContext] graphicsPort];
                CGContextSaveGState(ctx);
                CGContextTranslateCTM(ctx, partRect.origin.x, partRect.origin.y);
                if([self isFlipped]) {
                    CGContextTranslateCTM(ctx, 0, partRect.size.height);
                    CGContextScaleCTM(ctx, 1, -1);
                }

                NSRect drawingRect = partRect;
                drawingRect.origin = NSZeroPoint;
                [[self graphicsStyle] drawButtonImage:image inRect:drawingRect enabled:showsEnabled mixed:NO];
                CGContextRestoreGState(ctx);

                partRect.origin.x += IMAGE_TITLE_GAP;
            }
			
            // Draw the title
            partRect.origin.x += partRect.size.width;

            NSAttributedString *atitle = [item attributedTitle];
            if(atitle != nil && [atitle length] > 0) {
                CENTER_PART_RECT_VERTICALLY([atitle size]);
                [[self graphicsStyle] drawAttributedMenuItemText:atitle
                                                          inRect:partRect
                                                         enabled:showsEnabled
                                                        selected:selected];
            } else {
                NSString *title = [item title];
                CENTER_PART_RECT_VERTICALLY([[self graphicsStyle] menuItemTextSize:title]);
                [[self graphicsStyle] drawMenuItemText:title
                                                inRect:partRect
                                               enabled:showsEnabled
                                              selected:selected];
            }
			
            // Draw the key equivalent
            if([[item keyEquivalent] length] != 0) {
                NSString *keyString = [item _keyEquivalentDescription];
                NSSize branchArrowSize = [[self graphicsStyle] menuItemBranchArrowSize];
                NSSize keyEquivalentSize = [[self graphicsStyle] menuItemTextSize:keyString];
				
                partRect.origin.x = origin.x + NSWidth(itemArea) - branchArrowSize.width - keyEquivalentSize.width;
                CENTER_PART_RECT_VERTICALLY(keyEquivalentSize);
                [[self graphicsStyle] drawMenuItemText:keyString
                                                inRect:partRect
                                               enabled:showsEnabled
                                              selected:selected];
            }
			
            // Draw the submenu arrow
            if([item hasSubmenu] && [[item submenu] _hasItemsToShow]) {
                NSSize branchArrowSize = [[self graphicsStyle] menuItemBranchArrowSize];
                partRect.origin.x = origin.x + NSWidth(itemArea) - branchArrowSize.width;
                partRect.size.width = branchArrowSize.width;
                CENTER_PART_RECT_VERTICALLY(branchArrowSize);
                [[self graphicsStyle] drawMenuBranchArrowInRect:partRect enabled:showsEnabled selected:selected];
            }

#if NSMENUDEBUG
            [[NSColor yellowColor] setStroke];
            NSFrameRect(partRect);
#endif

            origin.y += itemHeight;
        }
    }
    CGContextRestoreGState(scrollContext);
    [self _drawScrollArrows];
}

/* The scroll arrows, over the items at the top and the bottom, while
   there is more to see that way. */
/* The view of an item with one under a point on the screen (what in it a
   press goes to), else nil. */
-(NSView *)_itemViewHitAtScreenPoint:(NSPoint)screenPoint {
    NSPoint point = [self convertPoint:[[self window] convertScreenToBase:screenPoint] fromView:nil];

    for(NSMenuItem *item in [self visibleItemArray]) {
        NSView *view = [item view];
        if(view != nil && [view superview] == self && NSMouseInRect(point, [view frame], [self isFlipped]))
            return [view hitTest:point];
    }
    return nil;
}

-(void)_drawScrollArrows {
    NSRect bounds = [self bounds];
    CGFloat center = floor(NSMidX(bounds));

    [[NSColor controlTextColor] setFill];
    if([self _showsTopArrow]) {
        NSRect zone = NSMakeRect(WINDOW_BORDER_THICKNESS, WINDOW_BORDER_THICKNESS,
            bounds.size.width - 2 * WINDOW_BORDER_THICKNESS, SCROLL_ARROW_HEIGHT - WINDOW_BORDER_THICKNESS);
        NSBezierPath *arrow = [NSBezierPath bezierPath];

        [[self graphicsStyle] drawMenuWindowBackgroundInRect:zone];
        [[NSColor controlTextColor] setFill];
        [arrow moveToPoint:NSMakePoint(center - 5, NSMinY(zone) + 12)];
        [arrow lineToPoint:NSMakePoint(center + 5, NSMinY(zone) + 12)];
        [arrow lineToPoint:NSMakePoint(center, NSMinY(zone) + 6)];
        [arrow closePath];
        [arrow fill];
    }
    if([self _showsBottomArrow]) {
        NSRect zone = NSMakeRect(WINDOW_BORDER_THICKNESS, NSMaxY(bounds) - SCROLL_ARROW_HEIGHT,
            bounds.size.width - 2 * WINDOW_BORDER_THICKNESS, SCROLL_ARROW_HEIGHT - WINDOW_BORDER_THICKNESS);
        NSBezierPath *arrow = [NSBezierPath bezierPath];

        [[self graphicsStyle] drawMenuWindowBackgroundInRect:zone];
        [[NSColor controlTextColor] setFill];
        [arrow moveToPoint:NSMakePoint(center - 5, NSMinY(zone) + 6)];
        [arrow lineToPoint:NSMakePoint(center + 5, NSMinY(zone) + 6)];
        [arrow lineToPoint:NSMakePoint(center, NSMinY(zone) + 12)];
        [arrow closePath];
        [arrow fill];
    }
}

/*
 * Tracking: from a menu bar (NSMenuBarView), a context menu or a pop-up
 * menu, the same loop over the stack of open menu views.
 */
const float kMenuInitialClickThreshold = .3f;
const float kMouseMovementThreshold = .001f;
#if 0
#define MENUDEBUG(...) NSLog(__VA_ARGS__)
#else
#define MENUDEBUG(...)
#endif

-(NSMenuItem *)trackForEvent:(NSEvent *)event {
	return [self _trackForEvent:event selecting:NSNotFound startedOpen:NO];
}

/* startedOpen: the menu was opened by the keyboard or by a program, not by
   a press now held: it stays open until an item is chosen or it is left,
   and the pointer is not looked at until it moves. selecting: the item
   selected at first, or NSNotFound. */
/* Minkar: menus tracking now (cancelTracking ends them), and the event
   subtype that does it. */
NSInteger _NSMenuTrackingDepth=0;
enum { kNSMenuCancelTrackingSubtype=0x4d43 };

-(NSMenuItem *)_trackForEvent:(NSEvent *)event selecting:(NSUInteger)selecting startedOpen:(BOOL)startedOpen {
	NSMenuItem *item=nil;
	
	enum {
		STATE_FIRSTMOUSEDOWN,
		STATE_MOUSEDOWN,
		STATE_MOUSEUP,
		STATE_EXIT
	} state=STATE_FIRSTMOUSEDOWN;
	
	// Get the menu management ball rolling
	NSPoint point=[event locationInWindow];
	NSPoint firstPoint=point;
	NSTimeInterval firstTimestamp = [event timestamp];
	
	// Cascading menus we manage will be pushed and popped on the viewStack
	NSMutableArray *viewStack=[NSMutableArray array];
	
	// Make sure we can put things back the way we found them
	BOOL oldAcceptsMouseMovedEvents = [[self window] acceptsMouseMovedEvents];
	
	// But while we're dealing with menus we want to track the mouse...
	[[self window] setAcceptsMouseMovedEvents:YES];
	
	// Each menu is brought up to date as it opens (NSMenuWindow)
	
	// And we, of course, are first on the stack
	[viewStack addObject:self];
	
	int keyboardNavigationAction = kNSMenuKeyboardNavigationNone;

	if (startedOpen) {
		[self setSelectedItemIndex:selecting];
		keyboardNavigationAction = kNSMenuKeyboardNavigationStart;
		state = STATE_MOUSEUP;
	}
	
	BOOL cancelled = NO;
	// A release over no item closes the menus only after a new press, or
	// once the pointer has been over an item: a press held and released
	// where the menu opened leaves it open
	BOOL pressedSinceOpen = NO, overItemOnce = NO;
	/* Over a scroll arrow: the loop wakes by itself to go on scrolling. */
	BOOL autoscrolling = NO;
	NSEvent *lastMouseEvent = [[event retain] autorelease];
	
	MENUDEBUG(@"entering outer loop");
	_NSMenuTrackingDepth++;
	
	// Start tracking the mouse movements and clicks
	do {
		/* As this turn starts: a change of the modifiers alone repeats it. */
		NSPoint            turnPoint=point;
		NSEvent           *turnEvent=event;
		int                count=[viewStack count];
		NSScreen          *screen=[self _screenForPoint:[[event window] convertBaseToScreen:point]];
		
		point=[[event window] convertBaseToScreen:point];
		
		// We've not pushed any views yet so the screen is where our window is
		if(count==1) {
			screen=[self _screenForPoint:point];
		}
                MENUDEBUG(@"INITIAL EVENT POINT IS %@ (count == %d, screen == %@)",
                        NSStringFromPoint(point), count, screen);
		
        if (keyboardNavigationAction == kNSMenuKeyboardNavigationNone) {
            // We're not current dealing with a keyboard event
            // Take a look at the visible menu stack (we're within a big loop so views can come and
            // go and the mouse can wander all over) deepest first
            while(--count>=0){
                
                // get the deepest one
                NSMenuView *checkView=[viewStack objectAtIndex:count];
                
                // And find out where the mouse is relative to it
                NSPoint     checkPoint=[[checkView window] convertScreenToBase:point];
                MENUDEBUG(@"1 CHECKPOINT IS %@", NSStringFromPoint(checkPoint));
                
                checkPoint=[checkView convertPoint:checkPoint fromView:nil];
                MENUDEBUG(@"2 CHECKPOINT IS %@", NSStringFromPoint(checkPoint));
                
                // If it's inside the menu view
                if(NSMouseInRect(checkPoint,[checkView bounds],[checkView isFlipped])){
                    
                    MENUDEBUG(@"found a menu: %@", checkView);
                    if([checkView _scrollAtPoint:checkPoint])
                        autoscrolling = YES;
                    
                    // Which item is the cursor on top of?
                    unsigned itemIndex=[checkView itemIndexAtPoint:checkPoint];

                    MENUDEBUG(@"found an item index: %u", itemIndex);

                    // If it's not the currently selected item
                    if(itemIndex!=[checkView selectedItemIndex]){
                        NSMenuView *branch;
                        
                        // This looks like it's dealing with pushed cascading menu
                        // views that are no longer needed because the user has moved
                        // on - so pop them all off.
                        while (count+1<[viewStack count]) {
                            NSView* view = [viewStack lastObject];
                            MENUDEBUG(@"popping cascading view: %@", view);
                            [[view window] close];
                            [viewStack removeLastObject];
                        }
                        
                        // And now select the new item
                        MENUDEBUG(@"about to setSelectedItemIndex");
                        [checkView setSelectedItemIndex:itemIndex];
                        
                        // If it's got a cascading menu then push that on the stack
                        if((branch=[checkView viewAtSelectedIndexPositionOnScreen:screen])!=nil) {
                            MENUDEBUG(@"adding a new cascading view: %@", branch);
                            [viewStack addObject:branch];
                        }
                    }
                    // And bail out of the while loop - we're in the right place
                    break;
                } else {
                    // We've wandered off the menu so don't show anything selected if it's the deepest
                    // visible view
                    if (checkView == [viewStack lastObject]) {
                        MENUDEBUG(@"clearing selection in view: %@", checkView);
                        // The mouse is outside of the top menu - be sure no item is selected anymore
                        [checkView setSelectedItemIndex:NSNotFound];
                    }
                }
            }
            MENUDEBUG(@"exited --count>=0 loop");
            
            // Looks like we've popped everything so nothing can be selected
            if(count<0){
                MENUDEBUG(@"clearing all selection");
                [[viewStack lastObject] setSelectedItemIndex:NSNotFound];
            }
        }
		
		// Let's take a look at what's come in on the event queue
                MENUDEBUG(@"getting next window event for %@", [self window]);
		// Presses are taken too, so a click that closes the menus goes no
		// further; the right and other buttons drive menus as the left one
		/* While scrolling by an arrow, a move where the pointer is when
		   nothing comes for a moment, so the menu goes on scrolling. */
		event=[[self window] nextEventMatchingMask:NSLeftMouseDownMask|NSLeftMouseUpMask|
			NSRightMouseDownMask|NSRightMouseUpMask|NSOtherMouseDownMask|NSOtherMouseUpMask|
			NSMouseMovedMask|NSLeftMouseDraggedMask|NSRightMouseDraggedMask|NSOtherMouseDraggedMask|
			NSKeyDownMask|NSAppKitDefinedMask|NSScrollWheelMask|NSFlagsChangedMask
			untilDate:autoscrolling?[NSDate dateWithTimeIntervalSinceNow:0.04]:[NSDate distantFuture]
			inMode:NSEventTrackingRunLoopMode dequeue:YES];
		if(event==nil)
			event=[NSEvent mouseEventWithType:NSMouseMoved location:[lastMouseEvent locationInWindow]
			    modifierFlags:[lastMouseEvent modifierFlags] timestamp:[NSDate timeIntervalSinceReferenceDate]
			    windowNumber:[lastMouseEvent windowNumber] context:nil eventNumber:0 clickCount:0 pressure:0];
		autoscrolling = NO;
		if([event type]==NSFlagsChanged){
			/* Option and the other modifiers swap alternates in place;
			   then this turn again, as it was, so nothing moves. */
			for(NSMenuView *open in viewStack)
				[open _modifiersChanged];
			point=turnPoint;
			event=turnEvent;
			continue;
		}
		if([event type]==NSScrollWheel){
			/* The wheel scrolls the menu under the pointer (else the last
			   one opened). */
			NSPoint where=[[event window] convertBaseToScreen:[event locationInWindow]];
			NSMenuView *wheeled=[viewStack lastObject];

			for(NSMenuView *candidate in [viewStack reverseObjectEnumerator])
				if(NSPointInRect(where,[[candidate window] frame])){
					wheeled=candidate;
					break;
				}
			[wheeled _scrollByWheel:[event deltaY]];
		}
		else if([event type]!=NSKeyDown && [event type]!=NSAppKitDefined)
			lastMouseEvent=event;
		BOOL isMouseUp = ([event type]==NSLeftMouseUp || [event type]==NSRightMouseUp || [event type]==NSOtherMouseUp);
		BOOL isMouseDown = ([event type]==NSLeftMouseDown || [event type]==NSRightMouseDown || [event type]==NSOtherMouseDown);
		if (isMouseDown)
			pressedSinceOpen = YES;
		/* A press on an item's view (a slider) is the view's: it tracks
		   the pointer itself, and the menu stays open after. */
		if ([event type] == NSLeftMouseDown) {
			NSPoint where = [[event window] convertBaseToScreen:[event locationInWindow]];
			NSView *hit = nil;

			for (NSMenuView *open in [viewStack reverseObjectEnumerator])
				if ((hit = [open _itemViewHitAtScreenPoint:where]) != nil)
					break;
			if (hit != nil) {
				[hit mouseDown:event];
				state = STATE_MOUSEUP;
				point = turnPoint;
				event = turnEvent;
				continue;
			}
		}
		
		if (keyboardNavigationAction != kNSMenuKeyboardNavigationNone) {
			// We didn't enter the mouse handling loop that predecrements count - so do it here...
			count--;
		}
		// Reset the keyboard navigation state
		keyboardNavigationAction = kNSMenuKeyboardNavigationNone;
		
        // Sometimes we can get key events with no characters (if the user
        // has invoked an accelerator while the menu is open for example)
		/* A key the Help search takes: the keyboard keeps the menus (the
		   pointer does not take the selection back on the next turn). */
		if ([event type] == NSKeyDown && [[event characters] length] > 0 &&
		    [[viewStack lastObject] _helpSearchKey:event]) {
			keyboardNavigationAction = kNSMenuKeyboardNavigationSearch;
		}
		else if ([event type] == NSKeyDown && [[event characters] length] > 0) {

			NSString* chars = [event characters];
			unichar ch = [chars characterAtIndex: 0];
			switch (ch) {
				case NSUpArrowFunctionKey:
					keyboardNavigationAction = kNSMenuKeyboardNavigationUp;
					break;
				case NSDownArrowFunctionKey:
					keyboardNavigationAction = kNSMenuKeyboardNavigationDown;
					break;
				case NSLeftArrowFunctionKey:
					keyboardNavigationAction = kNSMenuKeyboardNavigationLeft;
					break;
				case NSRightArrowFunctionKey:
					keyboardNavigationAction = kNSMenuKeyboardNavigationRight;
					break;
					
				case '\r': // Return = select the current item and exit the loop
				case 3: // Enter
					MENUDEBUG(@"Selecting current item and exit");
					// The keyboard's item, not one the mouse passed over
					item = [[viewStack lastObject] itemAtSelectedIndex];
					state = STATE_EXIT;
					break;
				case 27: // Escape = pop unless we're done then it's cancel
				{
					// Menus opened with the pointer close at once; from the
					// keyboard (Control-F2) one level at a time
					if (!startedOpen) {
						cancelled = YES;
					} else if ([viewStack count] > 1) {
						NSView* view = [viewStack lastObject];
						MENUDEBUG(@"popping cascading view: %@", view);
						[[view window] close];
						[viewStack removeLastObject];
					}	else {
						MENUDEBUG(@"Cancelling");
						cancelled = YES;
					}
				}
					break;
				default:
					keyboardNavigationAction = kNSMenuKeyboardNavigationLetter;
					break;
			}
			
			NSMenuView* activeMenuView = [viewStack lastObject];

			BOOL ignoreEnabledState = NO;
			if ([viewStack count] == 1 && [activeMenuView _isMenuBar]) {
				// For some reason main menu items are disabled - even though they work fine...
				ignoreEnabledState = YES;
				// we're navigating the top menu which has opposite semantics than a regular menu
				switch (keyboardNavigationAction) {
					case kNSMenuKeyboardNavigationDown:
						keyboardNavigationAction = kNSMenuKeyboardNavigationRight;
						break;
					case kNSMenuKeyboardNavigationUp:
						keyboardNavigationAction = kNSMenuKeyboardNavigationLeft;
						break;
					case kNSMenuKeyboardNavigationLeft:
						keyboardNavigationAction = kNSMenuKeyboardNavigationUp;
						break;
					case kNSMenuKeyboardNavigationRight:
						keyboardNavigationAction = kNSMenuKeyboardNavigationDown;
						break;
				}
			}
			
			switch (keyboardNavigationAction) {
				case kNSMenuKeyboardNavigationUp:
				{
					MENUDEBUG(@"Up...");
					
					unsigned oldIndex = [activeMenuView selectedItemIndex];
					NSArray *items = [activeMenuView itemArray];
					// Look for the next enabled item by search up and wrapping around the bottom
					unsigned newIndex = 0;
					if (oldIndex != NSNotFound) {
						newIndex = oldIndex == 0 ? [items count] - 1 : oldIndex - 1;
					}
					MENUDEBUG(@"oldIndex = %u", oldIndex);
					MENUDEBUG(@"newIndex = %u", newIndex);
					BOOL found = NO;
					while (!found && newIndex != oldIndex) {
						// Make sure we stop eventually
						if (oldIndex == NSNotFound) {
							oldIndex = 0;
						}
						// Try and find a new item to select
						NSMenuItem *item = [items objectAtIndex: newIndex];
						if ([item isSeparatorItem] == NO &&
							((ignoreEnabledState || [item isEnabled]) || [item hasSubmenu])) {
							MENUDEBUG(@"selecting item = %@", item);
							[activeMenuView setSelectedItemIndex: newIndex];
							found = YES;
						} else {
							MENUDEBUG(@"skipping item: %@", item);
							if (newIndex == 0) {
								newIndex = [items count] - 1;
							} else {
								newIndex --;
							}
						}
					}
				}
					break;
				case kNSMenuKeyboardNavigationDown:
				{
					MENUDEBUG(@"Down...");
					unsigned oldIndex = [activeMenuView selectedItemIndex];
					NSArray *items = [activeMenuView itemArray];
					// Look for the next enabled item by search down and wrapping around to the top
					unsigned newIndex = 0;
					if (oldIndex != NSNotFound) {
						newIndex = oldIndex == [items count] -1 ? 0 : oldIndex + 1;
					}
					
					MENUDEBUG(@"oldIndex = %u", oldIndex);
					MENUDEBUG(@"newIndex = %u", newIndex);
					BOOL found = NO;
					while (!found && newIndex != oldIndex) {
						// Make sure we stop eventually
						if (oldIndex == NSNotFound) {
							oldIndex = 0;
						}
						// Try and find a new item to select
						NSMenuItem *item = [items objectAtIndex: newIndex];
						if ([item isSeparatorItem] == NO &&
							((ignoreEnabledState || [item isEnabled]) || [item hasSubmenu])) {
							MENUDEBUG(@"selecting item: %u", item);
							[activeMenuView setSelectedItemIndex: newIndex];
							found = YES;
						} else {
							MENUDEBUG(@"skipping item: %@", item);
							if (newIndex == [items count] - 1) {
								newIndex = 0;
							} else {
								newIndex++;
							}
						}
					}
				}
					break;
				case kNSMenuKeyboardNavigationLeft:
					MENUDEBUG(@"Left...");
					if ([viewStack count] > 1) {
						NSView* view = [viewStack lastObject];
						MENUDEBUG(@"popping cascading view: %@", view);
						[[view window] close];
						[viewStack removeLastObject];
					}	
					break;
				case kNSMenuKeyboardNavigationRight:
                {
						MENUDEBUG(@"Right...");
					NSMenuView *branch = nil;
					// If there's a submenu at the current  selected index
					if((branch=[activeMenuView viewAtSelectedIndexPositionOnScreen:screen])!=nil) {
						MENUDEBUG(@"adding a new cascading view: %@", branch);
						[viewStack addObject:branch];
						// Opened from the keyboard: its first usable item is selected
						NSArray *branchItems = [branch itemArray];
						for (NSUInteger i = 0; i < [branchItems count]; i++) {
							NSMenuItem *candidate = [branchItems objectAtIndex: i];
							if (![candidate isSeparatorItem] && ([candidate isEnabled] || [candidate hasSubmenu])) {
								[branch setSelectedItemIndex: i];
								break;
							}
						}
					} else {
						// We'll pop it - they're trying to navigate to the next menu most likely
						if ([viewStack count] > 1) {
							NSView* view = [viewStack lastObject];
							MENUDEBUG(@"popping cascading view: %@", view);
							[[view window] close];
							[viewStack removeLastObject];
						}
					}
                }
					break;
				case kNSMenuKeyboardNavigationLetter:
				{
					MENUDEBUG(@"Letter...");
					NSString *letterString = [[NSString stringWithCharacters: &ch length: 1] uppercaseString];
					unsigned oldIndex = [activeMenuView selectedItemIndex];
					NSArray *items = [activeMenuView itemArray];
					// Look for the next enabled item by search down and wrapping around to the top
					unsigned newIndex = 0;
					if (oldIndex != NSNotFound) {
						newIndex = oldIndex == [items count] -1 ? 0 : oldIndex + 1;
					}
					
					MENUDEBUG(@"oldIndex = %u", oldIndex);
					MENUDEBUG(@"newIndex = %u", newIndex);
					BOOL found = NO;
					while (!found && newIndex != oldIndex) {
						// Make sure we stop eventually
						if (oldIndex == NSNotFound) {
							oldIndex = 0;
						}
						// Try and find a new item to select
						NSMenuItem *item = [items objectAtIndex: newIndex];
						if ((ignoreEnabledState || [item isEnabled] == YES) || [item hasSubmenu] == YES) {
							NSRange range = [[item title] rangeOfString: letterString];
							if (range.location != NSNotFound) {
								MENUDEBUG(@"selecting item: %u", item);
								[activeMenuView setSelectedItemIndex: newIndex];
								found = YES;
							}
						}
						if (!found) {
							MENUDEBUG(@"skipping item: %@", item);
							if (newIndex == [items count] - 1) {
								newIndex = 0;
							} else {
								newIndex++;
							}
						}
					}
				}
					break;
			}
		}
		// We use this special AppKitDefined event to let the menu respond to the app deactivation - it *has*
		// to be passed through the event system, unfortunately
		if ([event type] == NSAppKitDefined) {
			if ([event subtype] == NSApplicationDeactivated || [event subtype] == kNSMenuCancelTrackingSubtype) {
				MENUDEBUG(@"NSApplicationDeactivated or cancelTracking");
				cancelled = YES;
			}
		}
		
		if (cancelled == NO && [event type] != NSAppKitDefined && [event type] != NSKeyDown) {
			
			// looks like we can keep rolling
			
			point=[event locationInWindow];
                        MENUDEBUG(@"1 SECOND POINT is now %@", NSStringFromPoint(point));
                        point = [[event window] convertScreenToBase:point];
                        MENUDEBUG(@"2 SECOND POINT is now %@", NSStringFromPoint(point));
                        point.y += [screen frame].size.height;
                        MENUDEBUG(@"3 SECOND POINT is now %@", NSStringFromPoint(point));

			// Don't test for "== 0." - we tend to receive some delta with some .000000... values while the mouse doesn't move
			BOOL mouseMoved = ([event type] != NSAppKitDefined) &&
								(fabs([event deltaX]) > kMouseMovementThreshold || fabs([event deltaY]) > kMouseMovementThreshold);
			
			NSMenuView * activeView = nil;

			// We may not have a menuview here - so be cautious - and we may have added a cascading menu
			// so lastObject is also not the right thing to look at - we need to look at the menuview found in
			// the preceeding block (if there was one found - the user could have moused somewhere else entirely
			// remember)
			if (count >= 0) {
				activeView = [viewStack objectAtIndex: count];
                                MENUDEBUG(@"activeView == %@ at index %d", activeView, count);
			}
			
                        MENUDEBUG(@"switch(%d)", state);
			switch(state){
				case STATE_FIRSTMOUSEDOWN:
					// Let's take a look at the item under the cursor (if there is one)
					item=[activeView itemAtSelectedIndex];
                                        MENUDEBUG(@"itemAtSelectedIndex == %@ for index %d", item, _selectedItemIndex);
					if (item != nil)
						overItemOnce = YES;
					
					if(isMouseUp) {
						// The menu is really active after a mouse up (which means the menu will stay sticky)...
						// The timestamp is to avoid false clicks - make sure there's a delay so the user can
						if ([event timestamp] - firstTimestamp > kMenuInitialClickThreshold &&
							[viewStack count]==1 && [item isEnabled]) {
							MENUDEBUG(@"Handling selected item - exiting");
							state=STATE_EXIT;
						} else {
							MENUDEBUG(@"mouse up - continuing");
							state=STATE_MOUSEUP;
						}
					} else if(isMouseDown || mouseMoved) {
						// .. Or a mouse down (second click after the sticky menu) or a real move
						state=STATE_MOUSEDOWN;
					}
					break;
					
				default:
					item=[activeView itemAtSelectedIndex];
					if (item != nil)
						overItemOnce = YES;
					if(isMouseUp){
						MENUDEBUG(@"mouseUp on item: %@", item);
						/* Released over a disabled item or one with a submenu
						   (a menu title too), the menu stays open. */
						if((item == nil && (pressedSinceOpen || overItemOnce)) ||
						   ([item isEnabled] && ![item hasSubmenu] && [item view] == nil)) {
							MENUDEBUG(@"mouse up - exiting because of many possible reasons...");
							state=STATE_EXIT;
						} else {
							MENUDEBUG(@"mouse up");
							state=STATE_MOUSEUP;
						}
					}
					break;
			}
                        point = [event locationInWindow];
                        MENUDEBUG(@"reset point!");
		}
	}while(cancelled == NO && state!=STATE_EXIT);
	_NSMenuTrackingDepth--;
	
	MENUDEBUG(@"done with the event loop");
	
	// If we've got a menu still visible
	if(item == nil && [viewStack count]>0) {
		// Get the selected item
		item=[[viewStack lastObject] itemAtSelectedIndex];
		MENUDEBUG(@"got the selected item at the top most menu view: %@", item);
	}
	
	MENUDEBUG(@"removing the visible menu views");
	while([viewStack count]>1){
                NSView *v = (NSView *)[viewStack lastObject];
                NSWindow *w = [v window];
                MENUDEBUG(@"closing window %@ for view %@", w, v);
		[[(NSView *)[viewStack lastObject] window] close];
		[viewStack removeLastObject];
	}
	[viewStack removeLastObject];
	
	_selectedItemIndex=NSNotFound;
	[[self window] setAcceptsMouseMovedEvents:oldAcceptsMouseMovedEvents];
	[self setNeedsDisplay:YES];
	
	return ([item isEnabled] && ![item hasSubmenu])?item:(NSMenuItem *)nil;
}

-(void)mouseDown:(NSEvent *)event {
   [self _trackAndPerform:event selecting:NSNotFound startedOpen:NO];
}

-(void)_trackFromKeyboard {
   [self _trackAndPerform:[NSApp currentEvent] selecting:0 startedOpen:YES];
}

/* Tracks, then sends the chosen item's action; the item, or nil. */
-(NSMenuItem *)_trackAndPerform:(NSEvent *)event selecting:(NSUInteger)selecting startedOpen:(BOOL)startedOpen {
   NSMenuItem *item=[self _trackMenus:event selecting:selecting startedOpen:startedOpen];

   if(item!=nil)
    [[item menu] _sendActionOfItem:item];
   return item;
}

/* Tracks with the pointer held by this menu, between the tracking
   notifications; the item chosen, or nil. */
-(NSMenuItem *)_trackMenus:(NSEvent *)event selecting:(NSUInteger)selecting startedOpen:(BOOL)startedOpen {
   BOOL        didAccept=[[self window] acceptsMouseMovedEvents];
   NSMenuItem *item;

   [[self window] setAcceptsMouseMovedEvents:YES];
   /* With the new server, the menu holding the pointer while its menus
      are open has clicks outside them too (they close them). */
   NSNotificationCenter *center=[NSNotificationCenter defaultCenter];

   [center postNotificationName:NSMenuDidBeginTrackingNotification object:[self menu]];
   if(_CGUsesSkyLight())
    SLSCapturePointer([[self window] _skyLightWindowID]);
   item=[self _trackForEvent:event selecting:selecting startedOpen:startedOpen];
   if(_CGUsesSkyLight())
    SLSCapturePointer(0);
   [[self window] setAcceptsMouseMovedEvents:didAccept];
   [center postNotificationName:NSMenuDidEndTrackingNotification object:[self menu]];
   return item;
}

@end
