/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <AppKit/NSSearchField.h>
#import <AppKit/NSSearchFieldCell.h>
#import <objc/runtime.h>

@implementation NSSearchField

/* Its own cell when made in code (a nib names it): not its superclass's. */
+(Class)cellClass {
   return [NSSearchFieldCell class];
}


-(NSSearchFieldCell *)_searchCell {
   return [self cell];
}

-(NSArray *)recentSearches {
   return [[self _searchCell] recentSearches];
}

-(NSString *)recentsAutosaveName {
   return [[self _searchCell] recentsAutosaveName];
}

-(void)setRecentSearches:(NSArray *)searches {
   [[self _searchCell] setRecentSearches:searches];
}

-(void)setRecentsAutosaveName:(NSString *)name {
   [[self _searchCell] setRecentsAutosaveName:name];
}

/* The cell's settings, on the field too, as on the reference (API 10.10). */
-(NSMenu *)searchMenuTemplate {
   return [[self _searchCell] searchMenuTemplate];
}

-(void)setSearchMenuTemplate:(NSMenu *)menu {
   [[self _searchCell] setSearchMenuTemplate:menu];
}

-(BOOL)sendsWholeSearchString {
   return [[self _searchCell] sendsWholeSearchString];
}

-(void)setSendsWholeSearchString:(BOOL)flag {
   [[self _searchCell] setSendsWholeSearchString:flag];
}

-(NSInteger)maximumRecents {
   return [[self _searchCell] maximumRecents];
}

-(void)setMaximumRecents:(NSInteger)value {
   [[self _searchCell] setMaximumRecents:value];
}

-(BOOL)sendsSearchStringImmediately {
   return [[self _searchCell] sendsSearchStringImmediately];
}

-(void)setSendsSearchStringImmediately:(BOOL)flag {
   [[self _searchCell] setSendsSearchStringImmediately:flag];
}

/* Whether the placeholder sits in the middle when the field is empty and
   not being edited (API 10.11); YES by default, as on the reference. */
static char centersPlaceholderKey;

-(BOOL)centersPlaceholder {
   NSNumber *value=objc_getAssociatedObject(self,&centersPlaceholderKey);

   return value!=nil?[value boolValue]:YES;
}

-(void)setCentersPlaceholder:(BOOL)flag {
   objc_setAssociatedObject(self,&centersPlaceholderKey,[NSNumber numberWithBool:flag],OBJC_ASSOCIATION_RETAIN_NONATOMIC);
   [self setNeedsDisplay:YES];
}

@end
