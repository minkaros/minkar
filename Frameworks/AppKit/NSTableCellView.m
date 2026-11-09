/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSTableCellView.h>
#import <AppKit/NSTextField.h>
#import <AppKit/NSImageView.h>
#import <AppKit/NSColor.h>

@implementation NSTableCellView

-initWithFrame:(NSRect)frame {
   self=[super initWithFrame:frame];
   if(self!=nil)
    _rowSizeStyle=NSTableViewRowSizeStyleDefault;
   return self;
}

-initWithCoder:(NSCoder *)coder {
   self=[super initWithCoder:coder];
   if(self!=nil && [coder allowsKeyedCoding]){
    _textField=[coder decodeObjectForKey:@"NSTextField"];
    _imageView=[coder decodeObjectForKey:@"NSImageView"];
    _rowSizeStyle=NSTableViewRowSizeStyleDefault;
   }
   return self;
}

-(void)dealloc {
   [_objectValue release];
   [_draggingImageComponents release];
   [super dealloc];
}

-(BOOL)isFlipped {
   return YES;
}

-(id)objectValue {
   return _objectValue;
}

/* A text field shows a string value, an image view an image, unless they
   are bound. */
-(void)setObjectValue:(id)value {
   value=[value retain];
   [_objectValue release];
   _objectValue=value;
}

-(NSTextField *)textField {
   return _textField;
}

-(void)setTextField:(NSTextField *)field {
   _textField=field;
}

-(NSImageView *)imageView {
   return _imageView;
}

-(void)setImageView:(NSImageView *)view {
   _imageView=view;
}

-(NSBackgroundStyle)backgroundStyle {
   return _backgroundStyle;
}

/* The style the row's selection gives: the text field's cell draws on
   it (white text on the accent color). */
-(void)setBackgroundStyle:(NSBackgroundStyle)style {
   _backgroundStyle=style;
   for(NSView *view in [self subviews])
    if([view respondsToSelector:@selector(cell)] && [[(id)view cell] respondsToSelector:@selector(setBackgroundStyle:)]){
     [[(id)view cell] setBackgroundStyle:style];
     [view setNeedsDisplay:YES];
    }
}

-(NSTableViewRowSizeStyle)rowSizeStyle {
   return _rowSizeStyle;
}

-(void)setRowSizeStyle:(NSTableViewRowSizeStyle)style {
   _rowSizeStyle=style;
}

-(NSArray *)draggingImageComponents {
   return _draggingImageComponents;
}

@end
