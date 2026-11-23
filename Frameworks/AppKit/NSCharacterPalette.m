/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Emoji & Symbols (Atlas.md, W3): the character panel the Edit menu's
 * item opens (-[NSApplication orderFrontCharacterPalette:], Control-
 * Command-Space). Categories at the left (emoji by kind, then symbols by
 * kind), their characters in a grid, a search field that finds
 * characters by their Unicode names, and the chosen character's name
 * below. A click inserts the character into the main window's first
 * responder, through insertText:replacementRange: (NSTextInputClient) or
 * insertText:. The panel does not take the key window from the text
 * (only its search field does, when clicked). Recently used characters
 * come first (NSCharacterPaletteRecentCharacters, in the app's defaults).
 */

#import <AppKit/NSApplication.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSView.h>
#import <AppKit/NSScrollView.h>
#import <AppKit/NSClipView.h>
#import <AppKit/NSTableView.h>
#import <AppKit/NSTableColumn.h>
#import <AppKit/NSTextField.h>
#import <AppKit/NSSearchField.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSStringDrawing.h>
#import <AppKit/NSAttributedString.h>
#import <AppKit/NSParagraphStyle.h>
#import <Foundation/Foundation.h>

@interface NSObject (NSCharacterPaletteInsertion)
-(void)insertText:(id)string replacementRange:(NSRange)range;
-(void)insertText:(id)string;
@end

#define U_DISABLE_RENAMING 1
#include <unicode/uchar.h>

typedef struct {
   UChar32 first, last;
} NSCharacterRange;

typedef struct {
   NSString              *title;
   BOOL                   emoji;     /* emoji presentation only, else not */
   const NSCharacterRange *ranges;
   NSUInteger             count;
} NSCharacterCategory;

#define RANGES(name, ...) static const NSCharacterRange name[] = { __VA_ARGS__ }
RANGES(smileys, {0x1F600,0x1F64F}, {0x1F910,0x1F92F}, {0x1F970,0x1F97A}, {0x1F466,0x1F487}, {0x1F9D0,0x1F9DF});
RANGES(animals, {0x1F400,0x1F43F}, {0x1F330,0x1F344}, {0x1F980,0x1F9AE}, {0x1F490,0x1F49F});
RANGES(food, {0x1F345,0x1F37F}, {0x1F950,0x1F96F}, {0x1F9C0,0x1F9CB});
RANGES(activity, {0x1F380,0x1F3D3}, {0x26BD,0x26BE}, {0x1F93A,0x1F94F});
RANGES(travel, {0x1F680,0x1F6FF}, {0x1F3D4,0x1F3F0}, {0x26E9,0x26FF});
RANGES(objects, {0x1F4A0,0x1F4FF}, {0x1F500,0x1F53D}, {0x1F56F,0x1F5FF}, {0x231A,0x23FF});
RANGES(emojiSymbols, {0x2600,0x27BF}, {0x1F7E0,0x1F7EB}, {0x2B00,0x2BFF});
RANGES(arrows, {0x2190,0x21FF}, {0x27F0,0x27FF}, {0x2900,0x297F});
RANGES(bullets, {0x2022,0x2023}, {0x2043,0x2043}, {0x25A0,0x25FF}, {0x2605,0x2606}, {0x2729,0x274B});
RANGES(currency, {0x0024,0x0024}, {0x00A2,0x00A5}, {0x20A0,0x20C0});
RANGES(letterlike, {0x2100,0x214F});
RANGES(math, {0x00B1,0x00B1}, {0x00D7,0x00D7}, {0x00F7,0x00F7}, {0x2200,0x22FF});
RANGES(punctuation, {0x00A1,0x00A1}, {0x00AB,0x00AB}, {0x00BB,0x00BB}, {0x00BF,0x00BF}, {0x2010,0x2027}, {0x2030,0x205E});
RANGES(technical, {0x2300,0x23FF});
RANGES(latin, {0x00C0,0x00FF}, {0x0100,0x017F});
RANGES(greek, {0x0391,0x03C9});

#define CATEGORY(t, e, r) { t, e, r, sizeof(r) / sizeof(r[0]) }
static const NSCharacterCategory categories[] = {
   CATEGORY(@"Smileys & People", YES, smileys),
   CATEGORY(@"Animals & Nature", YES, animals),
   CATEGORY(@"Food & Drink", YES, food),
   CATEGORY(@"Activity", YES, activity),
   CATEGORY(@"Travel & Places", YES, travel),
   CATEGORY(@"Objects", YES, objects),
   CATEGORY(@"Symbols", YES, emojiSymbols),
   CATEGORY(@"Arrows", NO, arrows),
   CATEGORY(@"Bullets/Stars", NO, bullets),
   CATEGORY(@"Currency Symbols", NO, currency),
   CATEGORY(@"Letterlike Symbols", NO, letterlike),
   CATEGORY(@"Math Symbols", NO, math),
   CATEGORY(@"Punctuation", NO, punctuation),
   CATEGORY(@"Technical Symbols", NO, technical),
   CATEGORY(@"Latin", NO, latin),
   CATEGORY(@"Greek", NO, greek),
};
enum { kCategoryCount = sizeof(categories) / sizeof(categories[0]) };

static NSString * const kRecentKey = @"NSCharacterPaletteRecentCharacters";

static NSString *stringForCharacter(UChar32 c) {
   UniChar units[2];
   NSUInteger length = 1;

   if(c > 0xFFFF) {
      units[0] = (UniChar)(0xD800 + ((c - 0x10000) >> 10));
      units[1] = (UniChar)(0xDC00 + ((c - 0x10000) & 0x3FF));
      length = 2;
   }
   else
      units[0] = (UniChar)c;
   return [NSString stringWithCharacters:units length:length];
}

static UChar32 characterOfString(NSString *s) {
   UniChar first = [s length] > 0 ? [s characterAtIndex:0] : 0;

   if(first >= 0xD800 && first < 0xDC00 && [s length] > 1)
      return 0x10000 + ((first - 0xD800) << 10) + ([s characterAtIndex:1] - 0xDC00);
   return first;
}

/* The character's Unicode name, as ICU has it ("GRINNING FACE"). */
static NSString *nameOfCharacter(UChar32 c) {
   char       name[128];
   UErrorCode error = U_ZERO_ERROR;
   int32_t    length = u_charName(c, U_UNICODE_CHAR_NAME, name, sizeof(name), &error);

   return (U_SUCCESS(error) && length > 0) ? [NSString stringWithUTF8String:name] : @"";
}

/* A category's characters: those assigned, emoji shown as emoji (or the
   others, for symbols). */
static NSArray *charactersOfCategory(const NSCharacterCategory *category) {
   NSMutableArray *result = [NSMutableArray array];

   for(NSUInteger r = 0; r < category->count; r++)
      for(UChar32 c = category->ranges[r].first; c <= category->ranges[r].last; c++) {
         if(!u_isdefined(c) || u_charType(c) == U_UNASSIGNED)
            continue;
         if(category->emoji != (BOOL)u_hasBinaryProperty(c, UCHAR_EMOJI_PRESENTATION))
            continue;
         [result addObject:stringForCharacter(c)];
      }
   return result;
}

/* The grid: the characters, a click inserts one. */
@interface NSCharacterPaletteGrid : NSView {
   NSArray  *_characters;
   NSInteger _hover;
   id        _owner;
}
-(void)setCharacters:(NSArray *)characters;
-(void)setOwner:(id)owner;
@end

@interface NSCharacterPalette : NSObject <NSTableViewDataSource, NSTableViewDelegate> {
   NSPanel               *_panel;
   NSTableView           *_sidebar;
   NSCharacterPaletteGrid *_grid;
   NSScrollView          *_gridScroll;
   NSSearchField         *_search;
   NSTextField           *_info;
   NSMutableArray        *_titles;
}
+(NSCharacterPalette *)sharedPalette;
-(void)show;
-(void)insertCharacter:(NSString *)character;
-(void)showName:(NSString *)character;
@end

enum { kCell = 32 };

@implementation NSCharacterPaletteGrid

-(BOOL)isFlipped {
   return YES;
}

-(BOOL)acceptsFirstMouse:(NSEvent *)event {
   return YES;
}

-(void)setOwner:(id)owner {
   _owner = owner;
}

-(NSUInteger)_columns {
   return MAX((NSUInteger)1, (NSUInteger)(NSWidth([[self superview] bounds]) / kCell));
}

-(void)setCharacters:(NSArray *)characters {
   characters = [characters copy];
   [_characters release];
   _characters = characters;
   _hover = -1;
   [self _resize];
   [self scrollPoint:NSZeroPoint];
   [self setNeedsDisplay:YES];
}

-(void)_resize {
   NSUInteger columns = [self _columns], rows = ([_characters count] + columns - 1) / columns;
   NSSize     visible = [[self superview] bounds].size;

   [self setFrameSize:NSMakeSize(visible.width, MAX(visible.height, rows * kCell))];
}

-(void)resizeWithOldSuperviewSize:(NSSize)size {
   [self _resize];
}

-(void)dealloc {
   [_characters release];
   [super dealloc];
}

-(NSInteger)_indexAtPoint:(NSPoint)point {
   NSUInteger columns = [self _columns];
   NSInteger  column = (NSInteger)(point.x / kCell), row = (NSInteger)(point.y / kCell);
   NSInteger  index = row * (NSInteger)columns + column;

   if(point.x < 0 || column >= (NSInteger)columns || index < 0 || index >= (NSInteger)[_characters count])
      return -1;
   return index;
}

-(void)mouseDown:(NSEvent *)event {
   NSInteger index = [self _indexAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]];

   if(index < 0)
      return;
   _hover = index;
   [self setNeedsDisplay:YES];
   [_owner showName:[_characters objectAtIndex:(NSUInteger)index]];
   [_owner insertCharacter:[_characters objectAtIndex:(NSUInteger)index]];
}

-(void)drawRect:(NSRect)rect {
   NSUInteger columns = [self _columns];
   NSMutableParagraphStyle *style = [[[NSMutableParagraphStyle alloc] init] autorelease];
   NSDictionary *attributes;

   [style setAlignment:NSCenterTextAlignment];
   attributes = @{ NSFontAttributeName:[NSFont systemFontOfSize:20],
                   NSForegroundColorAttributeName:[NSColor textColor],
                   NSParagraphStyleAttributeName:style };
   [[NSColor textBackgroundColor] setFill];
   NSRectFill(rect);
   for(NSUInteger i = 0; i < [_characters count]; i++) {
      NSRect cell = NSMakeRect((i % columns) * kCell, (i / columns) * kCell, kCell, kCell);

      if(!NSIntersectsRect(cell, rect))
         continue;
      if((NSInteger)i == _hover) {
         [[NSColor selectedTextBackgroundColor] setFill];
         NSRectFill(NSInsetRect(cell, 1, 1));
      }
      NSString *character = [_characters objectAtIndex:i];
      NSSize    size = [character sizeWithAttributes:attributes];
      [character drawInRect:NSMakeRect(NSMinX(cell), floor(NSMidY(cell) - size.height / 2), kCell, size.height)
             withAttributes:attributes];
   }
}

@end

@implementation NSCharacterPalette

+(NSCharacterPalette *)sharedPalette {
   static NSCharacterPalette *shared;

   if(shared == nil)
      shared = [[NSCharacterPalette alloc] init];
   return shared;
}

-(NSString *)_text:(NSString *)english {
   return [[NSBundle bundleForClass:[NSCharacterPalette class]] localizedStringForKey:english value:english table:nil];
}

-(NSArray *)_recent {
   NSArray *recent = [[NSUserDefaults standardUserDefaults] arrayForKey:kRecentKey];

   return [recent isKindOfClass:[NSArray class]] ? recent : [NSArray array];
}

-(void)_build {
   NSRect frame = NSMakeRect(0, 0, 440, 380);

   _panel = [[NSPanel alloc] initWithContentRect:frame
      styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable |
                NSWindowStyleMaskUtilityWindow | NSWindowStyleMaskNonactivatingPanel
      backing:NSBackingStoreBuffered defer:NO];
   [_panel setTitle:[self _text:@"Characters"]];
   [_panel setBecomesKeyOnlyIfNeeded:YES];
   [_panel setFloatingPanel:YES];
   [_panel setHidesOnDeactivate:YES];
   [_panel setReleasedWhenClosed:NO];
   [_panel setMinSize:NSMakeSize(320, 240)];

   NSView *content = [_panel contentView];

   _search = [[NSSearchField alloc] initWithFrame:NSMakeRect(8, NSHeight(frame) - 32, NSWidth(frame) - 16, 24)];
   [_search setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
   [_search setPlaceholderString:[self _text:@"Search"]];
   [_search setTarget:self];
   [_search setAction:@selector(search:)];
   [content addSubview:_search];

   _titles = [[NSMutableArray alloc] init];
   [_titles addObject:[self _text:@"Frequently Used"]];
   for(NSUInteger i = 0; i < kCategoryCount; i++)
      [_titles addObject:[self _text:categories[i].title]];

   NSScrollView *sideScroll = [[[NSScrollView alloc] initWithFrame:NSMakeRect(0, 28, 150, NSHeight(frame) - 68)] autorelease];
   [sideScroll setAutoresizingMask:NSViewHeightSizable];
   [sideScroll setHasVerticalScroller:YES];
   [sideScroll setBorderType:NSNoBorder];
   _sidebar = [[NSTableView alloc] initWithFrame:[[sideScroll contentView] bounds]];
   NSTableColumn *column = [[[NSTableColumn alloc] initWithIdentifier:@"title"] autorelease];
   [column setWidth:146];
   [column setEditable:NO];
   [_sidebar addTableColumn:column];
   [_sidebar setHeaderView:nil];
   [_sidebar setRowHeight:20];
   [_sidebar setDataSource:self];
   [_sidebar setDelegate:self];
   [_sidebar setRefusesFirstResponder:YES];
   [sideScroll setDocumentView:_sidebar];
   [content addSubview:sideScroll];

   _gridScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(151, 28, NSWidth(frame) - 151, NSHeight(frame) - 68)];
   [_gridScroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
   [_gridScroll setHasVerticalScroller:YES];
   [_gridScroll setBorderType:NSNoBorder];
   _grid = [[NSCharacterPaletteGrid alloc] initWithFrame:[[_gridScroll contentView] bounds]];
   [_grid setOwner:self];
   [_gridScroll setDocumentView:_grid];
   [content addSubview:_gridScroll];

   _info = [[NSTextField alloc] initWithFrame:NSMakeRect(8, 4, NSWidth(frame) - 16, 20)];
   [_info setAutoresizingMask:NSViewWidthSizable | NSViewMaxYMargin];
   [_info setEditable:NO];
   [_info setSelectable:YES];
   [_info setBordered:NO];
   [_info setDrawsBackground:NO];
   [_info setFont:[NSFont systemFontOfSize:11]];
   [_info setTextColor:[NSColor secondaryLabelColor]];
   [_info setStringValue:@""];
   [content addSubview:_info];

   [_panel center];
   NSInteger first = [[self _recent] count] > 0 ? 0 : 1;
   [_sidebar selectRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)first] byExtendingSelection:NO];
   [self _showCategory:first];
}

-(void)show {
   if(_panel == nil)
      [self _build];
   [_panel orderFront:nil];
}

-(void)_showCategory:(NSInteger)row {
   if(row == 0)
      [_grid setCharacters:[self _recent]];
   else if(row > 0 && row <= kCategoryCount)
      [_grid setCharacters:charactersOfCategory(&categories[row - 1])];
}

-(NSInteger)numberOfRowsInTableView:(NSTableView *)table {
   return (NSInteger)[_titles count];
}

-(id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row {
   return [_titles objectAtIndex:(NSUInteger)row];
}

-(void)tableViewSelectionDidChange:(NSNotification *)note {
   [_search setStringValue:@""];
   [self _showCategory:[_sidebar selectedRow]];
}

/* Characters whose names have every word typed, from all the categories. */
-(void)search:(id)sender {
   NSArray        *words = [[[_search stringValue] uppercaseString] componentsSeparatedByCharactersInSet:
                              [NSCharacterSet whitespaceCharacterSet]];
   NSMutableArray *found = [NSMutableArray array];

   if([[[_search stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] length] == 0) {
      [self _showCategory:[_sidebar selectedRow]];
      return;
   }
   for(NSUInteger i = 0; i < kCategoryCount; i++)
      for(NSString *character in charactersOfCategory(&categories[i])) {
         NSString *name = nameOfCharacter(characterOfString(character));
         BOOL      all = YES;

         for(NSString *word in words)
            if([word length] > 0 && [name rangeOfString:word].location == NSNotFound) {
               all = NO;
               break;
            }
         if(all && ![found containsObject:character])
            [found addObject:character];
      }
   /* The name typed first, then names that begin with it, then the others
      (in their order). */
   NSString *typed = [[[_search stringValue] uppercaseString]
      stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
   NSMutableArray *ranked[3] = { [NSMutableArray array], [NSMutableArray array], [NSMutableArray array] };
   for(NSString *character in found) {
      NSString *name = nameOfCharacter(characterOfString(character));

      [ranked[[name isEqualToString:typed] ? 0 : [name hasPrefix:typed] ? 1 : 2] addObject:character];
   }
   [ranked[0] addObjectsFromArray:ranked[1]];
   [ranked[0] addObjectsFromArray:ranked[2]];
   [_grid setCharacters:ranked[0]];
}

-(void)showName:(NSString *)character {
   UChar32 c = characterOfString(character);

   [_info setStringValue:[NSString stringWithFormat:@"%@  U+%04X  %@", character, (unsigned)c,
      [nameOfCharacter(c) capitalizedString]]];
}

/* Into the text the user is typing in: the main window's first
   responder (the key window's when it is not this panel). */
-(void)insertCharacter:(NSString *)character {
   NSWindow *window = [NSApp keyWindow];
   id        target;

   if(window == nil || window == _panel)
      window = [NSApp mainWindow];
   target = [window firstResponder];
   if([target respondsToSelector:@selector(insertText:replacementRange:)])
      [target insertText:character replacementRange:NSMakeRange(NSNotFound, 0)];
   else if([target respondsToSelector:@selector(insertText:)])
      [target insertText:character];
   else {
      NSBeep();
      return;
   }

   NSMutableArray *recent = [[[self _recent] mutableCopy] autorelease];
   [recent removeObject:character];
   [recent insertObject:character atIndex:0];
   while([recent count] > 30)
      [recent removeLastObject];
   [[NSUserDefaults standardUserDefaults] setObject:recent forKey:kRecentKey];
}

@end

/* -[NSApplication orderFrontCharacterPalette:]. */
void _NSOrderFrontCharacterPalette(void);
void _NSOrderFrontCharacterPalette(void) {
   [[NSCharacterPalette sharedPalette] show];
}
