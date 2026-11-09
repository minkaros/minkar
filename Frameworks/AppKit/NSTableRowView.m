/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSTableRowView.h>
#import <AppKit/NSTableCellView.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSGraphics.h>

@implementation NSTableRowView

-initWithFrame:(NSRect)frame {
   self=[super initWithFrame:frame];
   if(self!=nil){
    _emphasized=YES;
    _draggingDestinationFeedbackStyle=NSTableViewDraggingDestinationFeedbackStyleRegular;
   }
   return self;
}

-(void)dealloc {
   [_backgroundColor release];
   [super dealloc];
}

-(BOOL)isFlipped {
   return YES;
}

-(BOOL)isOpaque {
   return NO;
}

-(NSTableViewSelectionHighlightStyle)selectionHighlightStyle {
   return _selectionHighlightStyle;
}

-(void)setSelectionHighlightStyle:(NSTableViewSelectionHighlightStyle)style {
   _selectionHighlightStyle=style;
   [self setNeedsDisplay:YES];
}

-(NSBackgroundStyle)interiorBackgroundStyle {
   if(_selected && _emphasized && _selectionHighlightStyle!=NSTableViewSelectionHighlightStyleNone)
    return NSBackgroundStyleDark;
   return NSBackgroundStyleLight;
}

/* The cell views take the style the selection gives. */
-(void)_updateInteriorBackgroundStyle {
   NSBackgroundStyle style=[self interiorBackgroundStyle];

   for(NSView *view in [self subviews])
    if([view respondsToSelector:@selector(setBackgroundStyle:)])
     [(id)view setBackgroundStyle:style];
   [self setNeedsDisplay:YES];
}

-(BOOL)isEmphasized {
   return _emphasized;
}

-(void)setEmphasized:(BOOL)flag {
   if(_emphasized==flag)
    return;
   _emphasized=flag;
   [self _updateInteriorBackgroundStyle];
}

-(BOOL)isSelected {
   return _selected;
}

-(void)setSelected:(BOOL)flag {
   if(_selected==flag)
    return;
   _selected=flag;
   [self _updateInteriorBackgroundStyle];
}

-(BOOL)isGroupRowStyle { return _groupRowStyle; }
-(void)setGroupRowStyle:(BOOL)flag { _groupRowStyle=flag; [self setNeedsDisplay:YES]; }
-(BOOL)isPreviousRowSelected { return _previousRowSelected; }
-(void)setPreviousRowSelected:(BOOL)flag { _previousRowSelected=flag; }
-(BOOL)isNextRowSelected { return _nextRowSelected; }
-(void)setNextRowSelected:(BOOL)flag { _nextRowSelected=flag; }
-(BOOL)isFloating { return _floating; }
-(void)setFloating:(BOOL)flag { _floating=flag; }
-(BOOL)isTargetForDropOperation { return _targetForDropOperation; }
-(void)setTargetForDropOperation:(BOOL)flag { _targetForDropOperation=flag; [self setNeedsDisplay:YES]; }
-(NSTableViewDraggingDestinationFeedbackStyle)draggingDestinationFeedbackStyle { return _draggingDestinationFeedbackStyle; }
-(void)setDraggingDestinationFeedbackStyle:(NSTableViewDraggingDestinationFeedbackStyle)style { _draggingDestinationFeedbackStyle=style; }
-(CGFloat)indentationForDropOperation { return _indentationForDropOperation; }
-(void)setIndentationForDropOperation:(CGFloat)value { _indentationForDropOperation=value; }

-(NSColor *)backgroundColor {
   return _backgroundColor;
}

-(void)setBackgroundColor:(NSColor *)color {
   color=[color copy];
   [_backgroundColor release];
   _backgroundColor=color;
   [self setNeedsDisplay:YES];
}

-(NSInteger)numberOfColumns {
   return [[self subviews] count];
}

/* The cell views are the row's subviews in column order. */
-(id)viewAtColumn:(NSInteger)column {
   NSArray *views=[self subviews];

   return (column>=0 && column<(NSInteger)[views count])?[views objectAtIndex:column]:nil;
}

-(void)drawBackgroundInRect:(NSRect)dirtyRect {
   if(_backgroundColor!=nil){
    [_backgroundColor setFill];
    NSRectFillUsingOperation(NSIntersectionRect(dirtyRect,[self bounds]),NSCompositeSourceOver);
   }
}

/* The accent selection, grey when the table is not first responder; a
   source list's is inset with rounded ends. */
-(void)drawSelectionInRect:(NSRect)dirtyRect {
   NSColor *color=_emphasized?[NSColor selectedContentBackgroundColor]:[NSColor unemphasizedSelectedContentBackgroundColor];
   NSRect   rect=[self bounds];

   if(_selectionHighlightStyle==NSTableViewSelectionHighlightStyleNone)
    return;
   [color setFill];
   if(_selectionHighlightStyle==NSTableViewSelectionHighlightStyleSourceList)
    [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(rect,10,0) xRadius:5 yRadius:5] fill];
   else
    NSRectFill(rect);
}

-(void)drawSeparatorInRect:(NSRect)dirtyRect {
}

-(void)drawDraggingDestinationFeedbackInRect:(NSRect)dirtyRect {
   [[NSColor selectedContentBackgroundColor] setStroke];
   [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect([self bounds],1.5,1.5) xRadius:4 yRadius:4] stroke];
}

-(void)drawRect:(NSRect)dirtyRect {
   [self drawBackgroundInRect:dirtyRect];
   if(_selected)
    [self drawSelectionInRect:dirtyRect];
   [self drawSeparatorInRect:dirtyRect];
   if(_targetForDropOperation)
    [self drawDraggingDestinationFeedbackInRect:dirtyRect];
}

-(void)didAddSubview:(NSView *)subview {
   [super didAddSubview:subview];
   if([subview respondsToSelector:@selector(setBackgroundStyle:)])
    [(id)subview setBackgroundStyle:[self interiorBackgroundStyle]];
}

@end
