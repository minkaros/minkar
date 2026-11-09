/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <AppKit/NSTextField.h>
#import <AppKit/NSTextFieldCell.h>
#import <AppKit/NSLayoutConstraint.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSTextView.h>
#import <AppKit/NSTextStorage.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSCursor.h>
#import <AppKit/NSPasteboard.h>
#import <AppKit/NSDragging.h>
#import "NSKeyValueBinding/NSTextFieldBinder.h"
#import "NSKeyValueBinding/NSObject+BindingSupport.h"


@interface NSTextFieldCell (Private)

- (CGFloat) _fontSize;
- (void) _setFontSize:(CGFloat)fontSize;
- (NSString*) _fontFamilyName;
- (void) _setFontFamilyName:(NSString*)familyName;

@end



@implementation NSTextField

/* A label (neither editable nor selectable) is part of what it sits on:
   in a title bar, the window moves by it. */
-(BOOL)mouseDownCanMoveWindow {
   return ![self isEditable] && ![self isSelectable];
}

+(Class)cellClass {
   return [NSTextFieldCell class];
}

+(Class)_binderClassForBinding:(id)binding
{
  if ([binding isEqual:@"value"])
    return [_NSTextFieldBinder class];
  else
    return [super _binderClassForBinding:binding];
}

-initWithCoder:(NSCoder *)coder {
   [super initWithCoder:coder];
   [self registerForDraggedTypes:[NSArray arrayWithObject:NSStringPboardType]];
   return self;
}

-initWithFrame:(NSRect)frame {
   [super initWithFrame:frame];
// Default for a NSTextFieldCell is NOT the same as NSTextField
   [_cell setEditable:YES];
   [_cell setSelectable:YES];
   [_cell setBezeled:YES];
   [self registerForDraggedTypes:[NSArray arrayWithObject:NSStringPboardType]];
   return self;
}

-(BOOL)isFlipped { return YES; }

-(BOOL)isOpaque {
   return [_cell drawsBackground] || [_cell isBezeled];
}

-(void)resetCursorRects {
   [_cell resetCursorRect:[self bounds] inView:self];
}

-delegate {
   return _delegate;
}

-(void)setDelegate:delegate {
   NSNotificationCenter *center=[NSNotificationCenter defaultCenter];
   struct {
    SEL       selector;
    NSString *name;
   } notes[]={
    { @selector(controlTextDidBeginEditing:), NSControlTextDidBeginEditingNotification },
    { @selector(controlTextDidChange:), NSControlTextDidChangeNotification },
    { @selector(controlTextDidEndEditing:), NSControlTextDidEndEditingNotification },
    { NULL, nil }
   };
   int i;

   if(_delegate!=nil)
    for(i=0;notes[i].selector!=NULL;i++)
     [center removeObserver:_delegate name:notes[i].name object:self];

   _delegate=delegate;

   for(i=0;notes[i].selector!=NULL;i++)
    if([_delegate respondsToSelector:notes[i].selector])
     [center addObserver:_delegate selector:notes[i].selector name:notes[i].name object:self];
}

-(NSColor *)backgroundColor {
   return [_cell backgroundColor];
}

-(NSColor *)textColor {
   return [_cell textColor];
}

-(BOOL)drawsBackground {
   return [_cell drawsBackground];
}

-(void)setBackgroundColor:(NSColor *)color {
   [_cell setBackgroundColor:color];
   [self setNeedsDisplay:YES];
}

-(void)setTextColor:(NSColor *)color {
   [_cell setTextColor:color];
   [self setNeedsDisplay:YES];
}

-(void)setDrawsBackground:(BOOL)flag {
   [_cell setDrawsBackground:flag];
   [self setNeedsDisplay:YES];
}

-(BOOL)acceptsFirstResponder {
    return YES;
}

-(BOOL)canBecomeKeyView {
    if(![self isEditable])
        return NO;
    
    return [super canBecomeKeyView];
}

-(BOOL)needsPanelToBecomeKey {
    return YES;
}

-(BOOL)becomeFirstResponder {
   [self selectText:nil];
   return YES;
}

-(void)_selectTextWithRange:(NSRange)range {
   NSTextFieldCell *cell=[self selectedCell];

   if(![cell isEnabled])
    return;

   if([cell isEditable] || [cell isSelectable]){
    if(_currentEditor==nil){
        NSText* editor =[[self window] fieldEditor:YES forObject:self];
        _currentEditor = [[cell setUpFieldEditorAttributes: editor] retain];
     [_currentEditor setDelegate:self];
     [_currentEditor registerForDraggedTypes:[self _draggedTypes]];
     [_currentEditor becomeFirstResponder];
    }

    [cell selectWithFrame:[self bounds] inView:self editor:_currentEditor delegate:self start:range.location length:range.length];
   }
}

-(void)selectText:sender {
   [self _selectTextWithRange:NSMakeRange(0,[[self stringValue] length])];        
}


-(void)setPreviousText:text {
// FIX, not sure how to implement this
//   [self setPreviousKeyView:text];
}

-(void)setNextText:text {
   [self setNextKeyView:text];
}

-previousText {
   return [self previousKeyView];
}

-nextText {
   return [self nextKeyView];
}

-(void)validateEditing
{
    if (_currentEditor) {
        NSString *string = [_currentEditor string];
        BOOL acceptsString = YES;
        NSFormatter *formatter = [self formatter];
        if (formatter) {
            acceptsString = NO;

            id objectValue = nil;
            NSString *error = nil;
            if ([formatter getObjectValue: &objectValue
                                forString: string
                         errorDescription: &error] == YES) {
                [[self selectedCell] setObjectValue:objectValue];
            } else {
                // Ask the delegate what to do
                SEL sel = @selector(control:didFailToFormatString:errorDescription:);
                if ([_delegate respondsToSelector: sel]) {
                    acceptsString = [_delegate control: self
                                   didFailToFormatString: string
                                        errorDescription: error];
                }
            }
        }
        if (acceptsString) {
            if ([_currentEditor isRichText]) {
                if ([_currentEditor isKindOfClass:[NSTextView class]]) {
                    NSTextView *textview = (NSTextView *)_currentEditor;
                    NSAttributedString *text = [textview textStorage];
                    NSAttributedString *string = [[[NSAttributedString alloc] initWithAttributedString:text] autorelease];
                    [[self selectedCell] setAttributedStringValue:string];
                } else {
                    [[self selectedCell] setStringValue:string];
                }
            } else {
                [[self selectedCell] setStringValue:string];
            }
        }
    }
}

-(void)textDidEndEditing:(NSNotification *)note {
    
   int movement=[[[note userInfo] objectForKey:@"NSTextMovement"] intValue];

   [super textDidEndEditing:note];

   if(movement==NSReturnTextMovement || [[self selectedCell] sendsActionOnEndEditing])
    [self sendAction:[self action] to:[self target]];

   if(movement==NSTabTextMovement)
    [[self window] selectKeyViewFollowingView:self];
    
   if (movement == NSBacktabTextMovement)
    [[self window] selectKeyViewPrecedingView:self];
}

-(void)mouseDown:(NSEvent *)event {
   NSTextFieldCell *cell=[self selectedCell];

   if(![cell isEnabled])
    return;

    if ([[self window] firstResponder] != self) {
        // This will create our editor if needed
        [[self window] makeFirstResponder:self];
    }
    
    NSPoint point=[self convertPoint:[event locationInWindow] fromView:nil];
    NSRect  editingFrame=[cell titleRectForBounds:[self bounds]];
    
    // In case we get the mouse down because we hadn't an editor yet, propagate the mouse event to the editor
    if(_currentEditor && NSMouseInRect(point,editingFrame,[self isFlipped])) {
        [_currentEditor mouseDown:event];
    } else {
        [super mouseDown:event];
    }
}

-(BOOL)textShouldBeginEditing:(NSText *)text {
   if ([_delegate respondsToSelector:@selector(control:textShouldBeginEditing:)])
     if ([_delegate control:self textShouldBeginEditing:text] == NO) {
       NSBeep();
       return NO;
     }
   return YES;
}

-(BOOL)textShouldEndEditing:(NSText *)text {
   if ([_delegate respondsToSelector:@selector(control:textShouldEndEditing:)])
     if ([_delegate control:self textShouldEndEditing:text] == NO) {
       NSBeep();
       return NO;
     }
    
    NSFormatter *formatter = [[self selectedCell] formatter];
    BOOL acceptsString = YES;
    if (formatter) {
        // Ask the formatter if the string is valid
        // If it's not, then we'll present an error and refuse the end of the editing
        
        NSString *string = [text string];
        id objectValue;
        NSError *error = nil;
        if ([formatter isKindOfClass:[NSNumberFormatter class]]) {
            NSNumberFormatter *numberFormatter = (NSNumberFormatter *)formatter;
            acceptsString = [numberFormatter getObjectValue:&objectValue forString:string range:NULL error:&error];
        } else {
            NSString *errorString;
            if ([formatter getObjectValue: &objectValue
                                forString: string
                         errorDescription: &errorString] == NO) {
                
                acceptsString = NO;
                NSDictionary *info = nil;
                if (errorString == nil) {
                    // Just in case we get no error from the formatter
                    errorString = NSLocalizedStringFromTableInBundle(@"Invalid number", nil, [NSBundle bundleForClass: [NSTextField class]], @"");
                }
                info = [NSDictionary dictionaryWithObject:errorString forKey:NSLocalizedDescriptionKey];
                error = [NSError errorWithDomain:NSCocoaErrorDomain code:2048 userInfo:info];
            }
       }
        if (error) {
            [self presentError:error];
        }
    }
   return acceptsString;
}

-(void)setEditable:(BOOL)flag {
  [super setEditable:flag];
  [_currentEditor setEditable:[self isEditable]];
}

-(void)setSelectable:(BOOL)flag {
  [super setSelectable:flag];
  [_currentEditor setSelectable:[self isSelectable]];
}

-(NSString *)placeholderString {
   return [[self cell] respondsToSelector:@selector(placeholderString)]?[[self cell] placeholderString]:nil;
}

-(void)setPlaceholderString:(NSString *)value {
   if([[self cell] respondsToSelector:@selector(setPlaceholderString:)])
    [[self cell] setPlaceholderString:value];
   [self setNeedsDisplay:YES];
}

-(NSAttributedString *)placeholderAttributedString {
   return [[self cell] respondsToSelector:@selector(placeholderAttributedString)]?[[self cell] placeholderAttributedString]:nil;
}

-(void)setPlaceholderAttributedString:(NSAttributedString *)value {
   if([[self cell] respondsToSelector:@selector(setPlaceholderAttributedString:)])
    [[self cell] setPlaceholderAttributedString:value];
   [self setNeedsDisplay:YES];
}

-(void)setStringValue:(NSString *)value {
   if (!_currentEditor)
      [super setStringValue:value];
   else
   {
      NSRange selectedRange=[_currentEditor selectedRange];
      BOOL isEntireString=NSEqualRanges(selectedRange, NSMakeRange(0, [[_currentEditor string] length]));
    
      [super setStringValue:value];
      [_currentEditor setString:[self stringValue]];
      NSRange entireString=NSMakeRange(0, [[_currentEditor string] length]);
    
      if (isEntireString) {
         // NSTextField will re-select entire string on a setString: if the previous value is completely selected
         [_currentEditor setSelectedRange:entireString];
      }
      else {
        // otherwise it will re-select what it can
        selectedRange=NSIntersectionRange(selectedRange, entireString);
        
        if (selectedRange.length==0) // 0 on intersection is undefined location, so we have to set it
            selectedRange.location=0;
        [_currentEditor setSelectedRange:entireString];
      }
   }
}

-(void)setFont:(NSFont *)font {
	// Don't do extra work if we don't need to...
	if (font != [self font]) {
		[super setFont:font];
		[_currentEditor setFont:[self font]];
	}
}

-(void)setAlignment:(NSTextAlignment)alignment {
  [super setAlignment:alignment];
  [_currentEditor setAlignment:[self alignment]];
}

-(void)setRefusesFirstResponder:(BOOL)flag {
   [_cell setRefusesFirstResponder:flag];
}

-(void)doCommandBySelector:(SEL)selector {
   BOOL didSelector=NO;
   
   if([_delegate respondsToSelector:@selector(control:textView:doCommandBySelector:)])
    didSelector=[_delegate control:self textView:(NSTextView *)_currentEditor doCommandBySelector:selector];
   
   if(!didSelector)
    [super doCommandBySelector:selector];
}

+(instancetype)labelWithString:(NSString *)stringValue {
   NSTextField *result=[[[self alloc] initWithFrame:NSZeroRect] autorelease];

   [result setStringValue:(stringValue!=nil)?stringValue:@""];
   [result setEditable:NO];
   [result setSelectable:NO];
   [result setBezeled:NO];
   [result setBordered:NO];
   [result setDrawsBackground:NO];
   [result setFont:[NSFont systemFontOfSize:0]];
   [result setTextColor:[NSColor labelColor]];
   [result setLineBreakMode:NSLineBreakByClipping];
   [result setUsesSingleLineMode:YES];
   [result sizeToFit];
   return result;
}

+(instancetype)wrappingLabelWithString:(NSString *)stringValue {
   NSTextField *result=[self labelWithString:stringValue];

   [result setSelectable:YES];
   [result setUsesSingleLineMode:NO];
   [result setLineBreakMode:NSLineBreakByWordWrapping];
   [[result cell] setWraps:YES];
   [result sizeToFit];
   return result;
}

+(instancetype)labelWithAttributedString:(NSAttributedString *)attributedStringValue {
   NSTextField *result=[self labelWithString:@""];

   [result setAttributedStringValue:attributedStringValue];
   [result sizeToFit];
   return result;
}

+(instancetype)textFieldWithString:(NSString *)stringValue {
   NSTextField *result=[[[self alloc] initWithFrame:NSZeroRect] autorelease];

   [result setStringValue:(stringValue!=nil)?stringValue:@""];
   [result setFont:[NSFont systemFontOfSize:0]];
   [result setLineBreakMode:NSLineBreakByClipping];
   [result setUsesSingleLineMode:YES];
   [result setScrollable:YES];
   [result sizeToFit];
   return result;
}

-(CGFloat)preferredMaxLayoutWidth {
   return _preferredMaxLayoutWidth;
}

-(void)setPreferredMaxLayoutWidth:(CGFloat)width {
   _preferredMaxLayoutWidth=width;
   [self invalidateIntrinsicContentSize];
}

-(NSInteger)maximumNumberOfLines {
   return _maximumNumberOfLines;
}

-(void)setMaximumNumberOfLines:(NSInteger)count {
   _maximumNumberOfLines=count;
   [self invalidateIntrinsicContentSize];
   [self setNeedsDisplay:YES];
}

-(BOOL)allowsDefaultTighteningForTruncation {
   return _allowsDefaultTighteningForTruncation;
}

-(void)setAllowsDefaultTighteningForTruncation:(BOOL)flag {
   _allowsDefaultTighteningForTruncation=flag;
}

-(BOOL)allowsEditingTextAttributes {
   return [_cell allowsEditingTextAttributes];
}

-(void)setAllowsEditingTextAttributes:(BOOL)flag {
   [_cell setAllowsEditingTextAttributes:flag];
}

-(BOOL)importsGraphics {
   return [_cell importsGraphics];
}

-(void)setImportsGraphics:(BOOL)flag {
   [_cell setImportsGraphics:flag];
}

/* A wrapping field is as tall as its text broken at the preferred width,
   up to its maximum number of lines. */
-(NSSize)intrinsicContentSize {
   NSSize size;

   if(_cell==nil)
    return NSMakeSize(NSViewNoIntrinsicMetric,NSViewNoIntrinsicMetric);
   if(![_cell wraps] || [self usesSingleLineMode])
    return [_cell cellSize];

   CGFloat width=(_preferredMaxLayoutWidth>0)?_preferredMaxLayoutWidth:NSWidth([self bounds]);

   if(width<=0)
    return [_cell cellSize];
   size=[_cell cellSizeForBounds:NSMakeRect(0,0,width,CGFLOAT_MAX)];
   if(_maximumNumberOfLines>0){
    /* A line's height and the cell's padding, from the same cell on one
       line. */
    NSCell  *oneLine=[[_cell copy] autorelease];
    CGFloat  lineHeight,padding;

    [oneLine setStringValue:@"X"];
    lineHeight=[[self font] ascender]-[[self font] descender]+[[self font] leading];
    padding=[oneLine cellSize].height-lineHeight;
    if(lineHeight>0 && size.height>lineHeight*_maximumNumberOfLines+padding)
     size.height=ceil(lineHeight*_maximumNumberOfLines+padding);
   }
   return NSMakeSize((_preferredMaxLayoutWidth>0)?NSViewNoIntrinsicMetric:size.width,size.height);
}

-(void)setTitleWithMnemonic:(NSString *)value {
   [self setStringValue:value];
}

@end

@implementation NSTextField (Bindings)

- (CGFloat) _fontSize {
    return [(NSTextFieldCell*)_cell _fontSize];
}
- (void) _setFontSize:(CGFloat)fontSize {
    [(NSTextFieldCell*)_cell _setFontSize:fontSize];
}
- (NSString*) _fontFamilyName {
    return [(NSTextFieldCell*)_cell _fontFamilyName];
}
- (void) _setFontFamilyName:(NSString*)familyName {
    [(NSTextFieldCell*)_cell _setFontFamilyName:familyName];
}

@end