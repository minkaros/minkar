/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <AppKit/NSGraphicsStyle.h>
#import <AppKit/NSInterfacePartAttributedString.h>
#import <AppKit/NSInterfacePartDisabledAttributedString.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSGradient.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSGraphicsContextFunctions.h>
#import "NSInterfaceGraphics.h"

@implementation NSGraphicsStyle

static NSDictionary *sNormalMenuTextAttributes = nil;
static NSDictionary *sSelectedMenuTextAttributes = nil;
static NSDictionary *sDimmedMenuTextAttributes = nil;
static NSDictionary *sDimmedMenuTextShadowAttributes = nil;

+ (void)initialize
{
	if (sNormalMenuTextAttributes == nil)
	{
		NSFont *menuFont = [NSFont menuFontOfSize:15.0];
		sNormalMenuTextAttributes = [[NSDictionary dictionaryWithObjectsAndKeys:
									  menuFont,NSFontAttributeName,
									  [NSColor menuItemTextColor],NSForegroundColorAttributeName,
									  nil] retain];
		sSelectedMenuTextAttributes = [[NSDictionary dictionaryWithObjectsAndKeys:
									   menuFont,NSFontAttributeName,
									   [NSColor selectedMenuItemTextColor],NSForegroundColorAttributeName,
										nil] retain];
		
		sDimmedMenuTextAttributes = [[NSDictionary dictionaryWithObjectsAndKeys:
									  menuFont,NSFontAttributeName,
									  [NSColor grayColor],NSForegroundColorAttributeName,
									  nil] retain];

		sDimmedMenuTextShadowAttributes = [[NSDictionary dictionaryWithObjectsAndKeys:
											menuFont,NSFontAttributeName,
											[NSColor whiteColor],NSForegroundColorAttributeName,
											nil] retain];
	}
}

-initWithView:(NSView *)view {
   _view=[view retain];
   return self;
}

-(void)dealloc {
   [_view release];
   [super dealloc];
}

@end

@implementation NSGraphicsStyle (NSMenu)


#define TITLE_TOP_MARGIN 2
#define TITLE_BOTTOM_MARGIN 2
#define BRANCH_ARROW_LEFT_MARGIN 2
#define BRANCH_ARROW_RIGHT_MARGIN 2

-(NSInterfacePartAttributedString *)branchArrow
{
	static NSInterfacePartAttributedString *sBranchArrow = nil;

	if (sBranchArrow == nil)
		sBranchArrow = [[NSInterfacePartAttributedString alloc] initWithFACharacter:0xF054];
	return sBranchArrow;
}

- (NSInterfacePartAttributedString *)checkMark
{
	static NSInterfacePartAttributedString *sCheckMark = nil;
	
	if (sCheckMark == nil)
		sCheckMark = [[NSInterfacePartAttributedString alloc] initWithFACharacter:0xF00C];
	return sCheckMark;
}

-(NSSize)menuItemSeparatorSize {
	return NSMakeSize(0,7 /*9*/);
}

-(Margins)menuItemBranchArrowMargins {
	Margins result = [self menuItemTextMargins];
	
	result.left = BRANCH_ARROW_LEFT_MARGIN;
	result.right = BRANCH_ARROW_RIGHT_MARGIN;
	
	return result;
}

/* With its margins (2 each side, and the item's text margins above and
   below): a 6 by 9 arrow. */
-(NSSize)menuItemBranchArrowSize {
   return NSMakeSize(10,17);
}

-(NSSize)menuItemCheckMarkSize {
	NSSize result = [[self checkMark] size];
	
	return result;
}

-(Margins)menuItemGutterMargins {
	Margins result;
	
	result.left = 0;
	result.right = 0;
	result.top = TITLE_TOP_MARGIN;
	result.bottom = TITLE_BOTTOM_MARGIN;

	return result;
}

-(NSSize)menuItemGutterSize {
	NSSize result = NSZeroSize;
	Margins margins = [self menuItemGutterMargins];
	
	result = [self menuItemCheckMarkSize];
	
	result.height += (margins.top + margins.bottom);
	result.width += (margins.left + margins.right);
	
	return result;
}

/* Delegate this back to view class because horizontal menu bar is different than
 * vertical drop downs
 */
-(Margins)menuItemTextMargins {
    return [_view menuItemTextMargins];
}

-(NSSize)menuItemTextSize:(NSString *)title {
	NSSize result = NSZeroSize;
	Margins margins = [self menuItemTextMargins];
	
	result = [title sizeWithAttributes:sNormalMenuTextAttributes];
	
	result.height += (margins.top + margins.bottom);
	result.width += (margins.left + margins.right);
	
	return result;
}

-(NSSize)menuItemAttributedTextSize:(NSAttributedString *)title {
	NSSize result = NSZeroSize;
	Margins margins = [self menuItemTextMargins];
	
	result = [title size];
	
	result.height += (margins.top + margins.bottom);
	result.width += (margins.left + margins.right);
	
	return result;
}

// this is the in-window bar which we dropped
-(CGFloat)menuBarHeight
{
    return 0.0;
}

-(CGFloat)menuItemGutterGap
{
	return 0;
}

-(void)drawMenuSeparatorInRect:(NSRect)rect 
{
	NSPoint point = NSMakePoint(rect.origin.x + 6, rect.origin.y + 3);
	float   width = rect.size.width - 6;
	
	[[NSColor grayColor] setFill];
	NSRectFill(NSMakeRect(point.x,point.y,width,1));
	//[[NSColor whiteColor] setFill];
	//NSRectFill(NSMakeRect(point.x,point.y+1,width,1));
}

-(void)drawMenuGutterInRect:(NSRect)rect
{
	// Nothing to do.
}

-(void)drawMenuItemText:(NSString *)string inRect:(NSRect)rect enabled:(BOOL)enabled selected:(BOOL)selected
{
    // Ensure we have enough width - fractional widths give float comparison trouble
    rect.size.width = ceilf(rect.size.width);
    
	Margins margins=[self menuItemTextMargins];
	
	rect.origin.x += margins.left;
	rect.origin.y += margins.top;
	rect.size.width -= (margins.left + margins.right);
	rect.size.height -= (margins.top + margins.bottom);
	
	if (enabled)
	{
		if (selected)
		{
			[string drawInRect:rect withAttributes:sSelectedMenuTextAttributes];
		}
		else
		{
			[string drawInRect:rect withAttributes:sNormalMenuTextAttributes];
		}
	}
	else
	{
#if 0
		if (!selected)
		{
			NSRect offsetRect = rect;
			offsetRect.origin.x += 1;
			offsetRect.origin.y += 1;
			[string drawInRect:offsetRect withAttributes:sDimmedMenuTextShadowAttributes];
		}
#endif
		[string drawInRect:rect withAttributes:sDimmedMenuTextAttributes];
	}
}

-(void)drawAttributedMenuItemText:(NSAttributedString *)string inRect:(NSRect)rect enabled:(BOOL)enabled selected:(BOOL)selected
{
    // Ensure we have enough width - fractional widths give float comparison trouble
    rect.size.width = ceilf(rect.size.width);

	NSMutableAttributedString* mutableString = [string mutableCopy];
	
	Margins margins=[self menuItemTextMargins];
	
	rect.origin.x += margins.left;
	rect.origin.y += margins.top;
	rect.size.width -= (margins.left + margins.right);
	rect.size.height -= (margins.top + margins.bottom);
	
	NSRange range = NSMakeRange(0, [string length]);
	
	if (enabled)
	{
		if (!selected) {
			[mutableString addAttributes: [NSDictionary dictionaryWithObject: [NSColor menuItemTextColor] forKey: NSForegroundColorAttributeName] range: range];
		}
		[mutableString drawInRect:rect];
	}
	else
	{
#if 0
		if (!selected)
		{
			[mutableString addAttributes: [NSDictionary dictionaryWithObject: [NSColor grayColor] forKey: NSForegroundColorAttributeName] range: range];
			NSRect offsetRect = rect;
			offsetRect.origin.x += 1;
			offsetRect.origin.y += 1;
			[mutableString drawInRect:offsetRect];
		} 
#endif
                    [mutableString addAttributes: [NSDictionary dictionaryWithObject: [NSColor grayColor] forKey: NSForegroundColorAttributeName] range: range];
                    [mutableString drawInRect:rect];
	}
}

-(void)drawMenuCheckmarkInRect:(NSRect)rect enabled:(BOOL)enabled selected:(BOOL)selected {
	NSColor                         *color;
	NSInterfacePartAttributedString *checkMark;
	Margins                         margins=[self menuItemTextMargins];
	
	if (enabled)
		color = selected ? [NSColor selectedControlTextColor] : [NSColor menuItemTextColor];
	else
		color = [NSColor disabledControlTextColor];
	
	checkMark = [[NSInterfacePartAttributedString alloc] initWithCharacter:0xF00C
																  fontName:@"Font Awesome 5 Free-Solid"
																 pointSize:10
																	 color:color];
	rect.origin.x += margins.left;
	rect.origin.y += margins.top;

	[checkMark drawAtPoint:rect.origin];
}


-(void)drawMenuBranchArrowInRect:(NSRect)rect enabled:(BOOL)enabled selected:(BOOL)selected {
	CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];
	NSColor      *color;
	Margins      margins=[self menuItemBranchArrowMargins];
	NSRect       themeRect = rect;
	
	if (enabled)
		color = selected ? [NSColor selectedControlTextColor] : [NSColor menuItemTextColor];
	else
		color = [NSColor disabledControlTextColor];
	
	[color set];
	
	themeRect.origin.x += margins.left;
	themeRect.origin.y += margins.top;
	themeRect.size.width -= (margins.left + margins.right);
	themeRect.size.height -= (margins.top + margins.bottom);
	
	CGContextBeginPath(context);
	CGContextMoveToPoint(context,NSMinX(themeRect),NSMaxY(themeRect));
	CGContextAddLineToPoint(context,NSMaxX(themeRect),NSMidY(themeRect));
	CGContextAddLineToPoint(context,NSMinX(themeRect),NSMinY(themeRect));
	CGContextClosePath(context);
	CGContextFillPath(context);
}

-(void)drawMenuSelectionInRect:(NSRect)rect enabled:(BOOL)enabled
{
	if (enabled)
	{
		/* Over the menu's background: the color may be translucent, and
		   NSRectFill copies it, which would leave the window see-through. */
		[[NSColor selectedMenuItemColor] setFill];
		NSRectFillUsingOperation(rect,NSCompositeSourceOver);
	}
}

-(void)drawMenuWindowBackgroundInRect:(NSRect)rect {
    [[NSColor menuBackgroundColor] setFill];
	NSRectFill(rect);
	[[NSColor windowFrameColor] set];
	NSFrameRect(rect);
}

-(void)drawMenuBarItemBorderInRect:(NSRect)rect hover:(BOOL)hovering selected:(BOOL)selected
{
	if (selected || hovering)
	{
		[[NSColor selectedMenuItemColor] setFill];
		NSRectFill(rect);
	}
}

/* Atlas's menu bar is dark: a charcoal gradient with a darker line at
   its bottom edge, and light text. The menus it opens stay light. */
NSColor *_NSMenuBarTextColor(void) {
	return [NSColor colorWithCalibratedWhite:0.93 alpha:1];
}

-(void)drawMenuBarBackgroundInRect:(NSRect)rect {
	NSGradient *gradient=[[NSGradient alloc] initWithStartingColor:[NSColor colorWithCalibratedWhite:0.17 alpha:1]
		endingColor:[NSColor colorWithCalibratedWhite:0.25 alpha:1]];

	[gradient drawInRect:rect angle:90];
	[gradient release];
	[[NSColor colorWithCalibratedWhite:0.08 alpha:1] setFill];
	NSRectFill(NSMakeRect(NSMinX(rect),NSMinY(rect),NSWidth(rect),1));
}

-(void)drawMenuBarItemText:(NSString *)string inRect:(NSRect)rect enabled:(BOOL)enabled selected:(BOOL)selected {
	[self drawAttributedMenuBarItemText:[[[NSAttributedString alloc] initWithString:string
		attributes:sNormalMenuTextAttributes] autorelease] inRect:rect enabled:enabled selected:selected];
}

-(void)drawAttributedMenuBarItemText:(NSAttributedString *)string inRect:(NSRect)rect enabled:(BOOL)enabled selected:(BOOL)selected {
	NSMutableAttributedString *text=[[string mutableCopy] autorelease];
	NSColor *color=!enabled ? [NSColor colorWithCalibratedWhite:0.55 alpha:1]
		: selected ? [NSColor selectedMenuItemTextColor] : _NSMenuBarTextColor();
	Margins margins=[self menuItemTextMargins];

	rect.size.width=ceilf(rect.size.width);
	rect.origin.x+=margins.left;
	rect.origin.y+=margins.top;
	rect.size.width-=(margins.left+margins.right);
	rect.size.height-=(margins.top+margins.bottom);
	[text addAttribute:NSForegroundColorAttributeName value:color range:NSMakeRange(0,[text length])];
	[text drawInRect:rect];
}

@end

/* ---- Atlas's controls ----------------------------------------------------
   Drawn here, without artwork, as Atlas's menu bar and window controls are:
   rounded shapes in its palette (the system colors, System.clr), light
   faces with a thin border, one accent color for what is chosen. */

static BOOL atlasFlipped(void) {
   return [[NSGraphicsContext currentContext] isFlipped];
}

static NSColor *atlasGray(CGFloat white) {
   return [NSColor colorWithCalibratedWhite:white alpha:1];
}

static NSColor *atlasAccent(void) {
   NSColor *accent=[NSColor controlAccentColor];

   return (accent!=nil)?accent:[NSColor colorWithCalibratedRed:0.13 green:0.47 blue:0.95 alpha:1];
}

static NSColor *atlasDarker(NSColor *color,CGFloat fraction) {
   NSColor *rgb=[color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];

   if(rgb==nil)
    return color;
   return [NSColor colorWithCalibratedRed:[rgb redComponent]*(1-fraction) green:[rgb greenComponent]*(1-fraction)
      blue:[rgb blueComponent]*(1-fraction) alpha:[rgb alphaComponent]];
}

static NSColor *atlasLighter(NSColor *color,CGFloat fraction) {
   NSColor *rgb=[color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];

   if(rgb==nil)
    return color;
   return [NSColor colorWithCalibratedRed:[rgb redComponent]+(1-[rgb redComponent])*fraction
      green:[rgb greenComponent]+(1-[rgb greenComponent])*fraction
      blue:[rgb blueComponent]+(1-[rgb blueComponent])*fraction alpha:[rgb alphaComponent]];
}

/* A rounded shape, its fill a gradient from its top to its bottom (in the
   view's sense of up), and a one-point border on the pixel grid. */
static void atlasRounded(NSRect rect,CGFloat radius,NSColor *top,NSColor *bottom,NSColor *border) {
   NSRect       inner=NSInsetRect(rect,0.5,0.5);
   NSBezierPath *path;

   if(NSWidth(inner)<=0 || NSHeight(inner)<=0)
    return;
   radius=MIN(radius,MIN(NSWidth(inner),NSHeight(inner))/2);

   /* Only what the clip shows: nothing when it misses the shape, and the
      plain fill when it lies inside, away from the border and corners (a
      small redraw inside a big pane or box need not stroke all of it). */
   CGContextRef context=[[NSGraphicsContext currentContext] graphicsPort];
   NSRect       clip=CGContextGetClipBoundingBox(context);

   if(!NSIntersectsRect(NSInsetRect(rect,-1,-1),clip))
    return;
   if(NSContainsRect(NSInsetRect(inner,1.5,radius+1),clip) || NSContainsRect(NSInsetRect(inner,radius+1,1.5),clip)){
    if(top!=nil){
     NSGradient *gradient=[[NSGradient alloc] initWithStartingColor:bottom endingColor:top];

     [gradient drawInRect:inner angle:atlasFlipped()?-90:90];
     [gradient release];
    }
    return;
   }

   path=[NSBezierPath bezierPathWithRoundedRect:inner xRadius:radius yRadius:radius];
   if(top!=nil){
    NSGradient *gradient=[[NSGradient alloc] initWithStartingColor:bottom endingColor:top];

    [gradient drawInBezierPath:path angle:atlasFlipped()?-90:90];
    [gradient release];
   }
   if(border!=nil){
    [border setStroke];
    [path setLineWidth:1];
    [path stroke];
   }
}

/* A chevron (an open arrow), centered in rect, pointing down, up, left or
   right: 0, 1, 2, 3. */
static void atlasChevron(NSRect rect,int direction,CGFloat size,NSColor *color) {
   NSPoint      c=NSMakePoint(NSMidX(rect),NSMidY(rect));
   CGFloat      h=size/2,q=size/4;
   BOOL         down=(direction==0)!=atlasFlipped();
   NSBezierPath *path=[NSBezierPath bezierPath];

   if(direction==0 || direction==1){
    CGFloat dy=(down?1:-1)*q;
    [path moveToPoint:NSMakePoint(c.x-h,c.y+dy)];
    [path lineToPoint:NSMakePoint(c.x,c.y-dy)];
    [path lineToPoint:NSMakePoint(c.x+h,c.y+dy)];
   }
   else {
    CGFloat dx=(direction==2?1:-1)*q;
    [path moveToPoint:NSMakePoint(c.x+dx,c.y-h)];
    [path lineToPoint:NSMakePoint(c.x-dx,c.y)];
    [path lineToPoint:NSMakePoint(c.x+dx,c.y+h)];
   }
   [color setStroke];
   [path setLineWidth:1.5];
   [path setLineCapStyle:NSRoundLineCapStyle];
   [path setLineJoinStyle:NSRoundLineJoinStyle];
   [path stroke];
}

/* The face of a push button: light, or the accent when it is the
   default; darker while pressed. */
static void atlasButtonFace(NSRect rect,BOOL defaulted,BOOL pressed) {
   if(defaulted){
    NSColor *accent=atlasAccent();

    if(pressed)
     atlasRounded(rect,5,atlasDarker(accent,0.15),atlasDarker(accent,0.22),atlasDarker(accent,0.35));
    else
     atlasRounded(rect,5,atlasLighter(accent,0.12),accent,atlasDarker(accent,0.20));
   }
   else if(pressed)
    atlasRounded(rect,5,atlasGray(0.84),atlasGray(0.80),atlasGray(0.58));
   else
    atlasRounded(rect,5,atlasGray(1.0),atlasGray(0.96),atlasGray(0.70));
}

/* Check boxes and radio buttons: AppKit's NSSwitch, NSHighlightedSwitch,
   NSRadioButton and NSHighlightedRadioButton images, made here and named
   before anything loads the old artwork of the same names. */
static NSImage *atlasToggleImage(NSString *name,BOOL radio,BOOL on) {
   NSImage *image=[[NSImage alloc] initWithSize:NSMakeSize(14,14)];
   NSRect   rect=NSMakeRect(0,0,14,14);

   [image lockFocus];
   if(on){
    NSColor *accent=atlasAccent();

    if(radio){
     NSBezierPath *dot;
     [accent setFill];
     [[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(rect,0.5,0.5)] fill];
     [atlasDarker(accent,0.2) setStroke];
     [[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(rect,0.5,0.5)] stroke];
     dot=[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(rect,4.5,4.5)];
     [[NSColor whiteColor] setFill];
     [dot fill];
    }
    else {
     NSBezierPath *check=[NSBezierPath bezierPath];

     atlasRounded(rect,3.5,atlasLighter(accent,0.12),accent,atlasDarker(accent,0.20));
     [check moveToPoint:NSMakePoint(3.5,7.5)];
     [check lineToPoint:NSMakePoint(6,4.5)];
     [check lineToPoint:NSMakePoint(10.5,10)];
     [[NSColor whiteColor] setStroke];
     [check setLineWidth:2];
     [check setLineCapStyle:NSRoundLineCapStyle];
     [check setLineJoinStyle:NSRoundLineJoinStyle];
     [check stroke];
    }
   }
   else if(radio){
    NSBezierPath *ring=[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(rect,0.5,0.5)];

    [atlasGray(1.0) setFill];
    [ring fill];
    [atlasGray(0.62) setStroke];
    [ring setLineWidth:1];
    [ring stroke];
   }
   else
    atlasRounded(rect,3.5,atlasGray(1.0),atlasGray(0.97),atlasGray(0.62));
   [image unlockFocus];
   [image setName:name];
   return image;
}

void _NSGraphicsStyleRegisterButtonImages(void) {
   static BOOL done=NO;

   if(done)
    return;
   done=YES;
   atlasToggleImage(@"NSSwitch",NO,NO);
   atlasToggleImage(@"NSHighlightedSwitch",NO,YES);
   atlasToggleImage(@"NSRadioButton",YES,NO);
   atlasToggleImage(@"NSHighlightedRadioButton",YES,YES);
}

@implementation NSGraphicsStyle (NSButton)

-(void)drawUnborderedButtonInRect:(NSRect)rect defaulted:(BOOL)defaulted {
}

-(void)drawPushButtonNormalInRect:(NSRect)rect defaulted:(BOOL)defaulted {
   atlasButtonFace(rect,defaulted,NO);
}

/* Pressed, a push button turns the accent color (its title white), as on
   the reference platform. */
-(void)drawPushButtonPressedInRect:(NSRect)rect {
   atlasButtonFace(rect,YES,NO);
}

-(void)drawPushButtonHighlightedInRect:(NSRect)rect {
   atlasButtonFace(rect,NO,YES);
}

-(NSSize)sizeOfButtonImage:(NSImage *)image enabled:(BOOL)enabled mixed:(BOOL)mixed {
   return [image size];
}

-(void)drawButtonImage:(NSImage *)image inRect:(NSRect)rect enabled:(BOOL)enabled mixed:(BOOL)mixed {
   float fraction=enabled?1.0:0.5;

   /* A mixed check box: the accent box with a dash, no check mark. */
   if(mixed && ([[image name] isEqualToString:@"NSSwitch"] || [[image name] isEqualToString:@"NSHighlightedSwitch"])){
    NSColor *accent=atlasAccent();
    NSRect   box=NSMakeRect(NSMidX(rect)-7,NSMidY(rect)-7,14,14);

    if(!enabled)
     accent=atlasGray(0.75);
    atlasRounded(box,3.5,atlasLighter(accent,0.12),accent,atlasDarker(accent,0.20));
    [[NSColor whiteColor] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(NSMinX(box)+3.5,NSMidY(box)-1,7,2) xRadius:1 yRadius:1] fill];
    return;
   }
   [image drawInRect:rect fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:fraction];
   if(mixed){
    NSRect bar=NSMakeRect(NSMinX(rect)+3,NSMidY(rect)-1,NSWidth(rect)-6,2);

    [[NSColor whiteColor] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:bar xRadius:1 yRadius:1] fill];
   }
}

@end

@implementation NSGraphicsStyle (NSBrowser)

-(void)drawBrowserTitleBackgroundInRect:(NSRect)rect {
   [self drawTableViewHeaderInRect:rect highlighted:NO];
}

-(void)drawBrowserHorizontalScrollerWellInRect:(NSRect)rect clipRect:(NSRect)clipRect {
   [atlasGray(0.96) setFill];
   NSRectFill(rect);
}

@end

@implementation NSGraphicsStyle (NSColorWell)

-(NSRect)drawColorWellBorderInRect:(NSRect)rect enabled:(BOOL)enabled bordered:(BOOL)bordered active:(BOOL)active {
   if(bordered){
    atlasButtonFace(rect,NO,active);
    rect=NSInsetRect(rect,5,5);
   }
   atlasRounded(rect,3,nil,nil,enabled?atlasGray(0.55):atlasGray(0.75));
   return NSInsetRect(rect,1.5,1.5);
}

@end

@implementation NSGraphicsStyle (NSPopUpButton)

-(void)drawPopUpButtonWindowBackgroundInRect:(NSRect)rect {
   [[NSColor menuBackgroundColor] setFill];
   NSRectFill(rect);
   [atlasGray(0.70) setStroke];
   NSFrameRect(rect);
}

@end

@implementation NSGraphicsStyle (NSOutlineView)

-(void)drawOutlineViewGridInRect:(NSRect)rect {
   NSInterfaceDrawOutlineGrid(rect,NSCurrentGraphicsPort());
}

@end

@implementation NSGraphicsStyle (NSProgressIndicator)

/* A rounded track, and the accent's bar in it. */
-(NSRect)drawProgressIndicatorBackground:(NSRect)rect clipRect:(NSRect)clipRect bezeled:(BOOL)bezeled {
   NSRect track=rect;

   track.size.height=MIN(NSHeight(rect),6);
   track.origin.y=floor(NSMidY(rect)-NSHeight(track)/2);
   atlasRounded(track,3,atlasGray(0.86),atlasGray(0.88),atlasGray(0.78));
   return track;
}

-(void)drawProgressIndicatorChunk:(NSRect)rect {
   NSColor *accent=atlasAccent();

   if(NSWidth(rect)<1)
    return;
   atlasRounded(rect,NSHeight(rect)/2,atlasLighter(accent,0.12),accent,nil);
}

-(void)drawProgressIndicatorIndeterminate:(NSRect)rect clipRect:(NSRect)clipRect bezeled:(BOOL)bezeled animation:(double)animation {
   NSRect track=[self drawProgressIndicatorBackground:rect clipRect:clipRect bezeled:bezeled];
   NSRect bar=track;

   /* A third of the track, sliding across it and back in. */
   bar.size.width=floor(NSWidth(track)/3);
   bar.origin.x=NSMinX(track)+floor((NSWidth(track)+NSWidth(bar))*animation)-NSWidth(bar);
   bar=NSIntersectionRect(bar,track);
   [self drawProgressIndicatorChunk:bar];
}

-(void)drawProgressIndicatorDeterminate:(NSRect)rect clipRect:(NSRect)clipRect bezeled:(BOOL)bezeled value:(double)value {
   NSRect track=[self drawProgressIndicatorBackground:rect clipRect:clipRect bezeled:bezeled];
   NSRect bar=track;

   bar.size.width=floor(NSWidth(track)*MAX(0,MIN(1,value)));
   [self drawProgressIndicatorChunk:bar];
}

@end

@implementation NSGraphicsStyle (NSScroller)

/* Thin and quiet: a light track, a rounded grey knob, small chevrons. */
-(void)drawScrollerButtonInRect:(NSRect)rect enabled:(BOOL)enabled pressed:(BOOL)pressed vertical:(BOOL)vertical upOrLeft:(BOOL)upOrLeft {
   int direction=vertical?(upOrLeft?1:0):(upOrLeft?2:3);

   if(NSIsEmptyRect(rect))
    return;
   [atlasGray(pressed?0.88:0.96) setFill];
   NSRectFill(rect);
   if(NSWidth(rect)>8 && NSHeight(rect)>8)
    atlasChevron(rect,direction,6,enabled?atlasGray(0.40):atlasGray(0.70));
}

-(void)drawScrollerKnobInRect:(NSRect)rect vertical:(BOOL)vertical highlight:(BOOL)highlight {
   NSRect knob=vertical?NSInsetRect(rect,3,2):NSInsetRect(rect,2,3);

   atlasRounded(knob,MIN(NSWidth(knob),NSHeight(knob))/2,
      highlight?atlasGray(0.45):atlasGray(0.62),highlight?atlasGray(0.45):atlasGray(0.62),nil);
}

-(void)drawScrollerTrackInRect:(NSRect)rect vertical:(BOOL)vertical upOrLeft:(BOOL)upOrLeft {
   [atlasGray(0.96) setFill];
   NSRectFill(rect);
}

-(void)drawScrollerTrackInRect:(NSRect)rect vertical:(BOOL)vertical {
   [self drawScrollerTrackInRect:rect vertical:vertical upOrLeft:NO];
}

@end

@implementation NSGraphicsStyle (NSSlider)

-(NSSize)sliderKnobSizeForControlSize:(NSControlSize)controlSize {
   switch(controlSize){
    default:
    case NSRegularControlSize:
     return NSMakeSize(16,16);

    case NSSmallControlSize:
     return NSMakeSize(12,12);

    case NSMiniControlSize:
     return NSMakeSize(10,10);
   }
}

/* A round white knob with a light border; with tick marks, a narrower
   knob pointing at them. */
-(void)drawSliderKnobInRect:(NSRect)rect vertical:(BOOL)vertical highlighted:(BOOL)highlighted hasTickMarks:(BOOL)hasTickMarks tickMarkPosition:(NSTickMarkPosition)tickMarkPosition {
   CGFloat       side=MIN(NSWidth(rect),NSHeight(rect));
   NSRect        knob=NSMakeRect(floor(NSMidX(rect)-side/2)+0.5,floor(NSMidY(rect)-side/2)+0.5,side-1,side-1);
   NSBezierPath *path=[NSBezierPath bezierPathWithOvalInRect:knob];

   if(hasTickMarks){
    BOOL    flipped=atlasFlipped();
    CGFloat w=vertical?side-1:floor(side*0.62),h=vertical?floor(side*0.62):side-1;
    NSRect  body=NSMakeRect(floor(NSMidX(rect)-w/2)+0.5,floor(NSMidY(rect)-h/2)+0.5,w,h);
    CGFloat r=2.5,tip=floor(MIN(w,h)/2);

    path=[NSBezierPath bezierPath];
    if(!vertical){
     /* The point toward the marks: below unless they are above. */
     BOOL down=(tickMarkPosition!=NSTickMarkAbove)!=flipped;
     CGFloat flat=down?NSMinY(body)+tip:NSMaxY(body)-tip,point=down?NSMinY(body):NSMaxY(body),end=down?NSMaxY(body):NSMinY(body);

     [path moveToPoint:NSMakePoint(NSMidX(body),point)];
     [path lineToPoint:NSMakePoint(NSMaxX(body),flat)];
     [path lineToPoint:NSMakePoint(NSMaxX(body),end+(down?-r:r))];
     [path lineToPoint:NSMakePoint(NSMaxX(body)-r,end)];
     [path lineToPoint:NSMakePoint(NSMinX(body)+r,end)];
     [path lineToPoint:NSMakePoint(NSMinX(body),end+(down?-r:r))];
     [path lineToPoint:NSMakePoint(NSMinX(body),flat)];
    }
    else {
     BOOL right=(tickMarkPosition==NSTickMarkRight);
     CGFloat flat=right?NSMaxX(body)-tip:NSMinX(body)+tip,point=right?NSMaxX(body):NSMinX(body),end=right?NSMinX(body):NSMaxX(body);

     [path moveToPoint:NSMakePoint(point,NSMidY(body))];
     [path lineToPoint:NSMakePoint(flat,NSMaxY(body))];
     [path lineToPoint:NSMakePoint(end,NSMaxY(body))];
     [path lineToPoint:NSMakePoint(end,NSMinY(body))];
     [path lineToPoint:NSMakePoint(flat,NSMinY(body))];
    }
    [path closePath];
   }

   [(highlighted?atlasGray(0.92):atlasGray(1.0)) setFill];
   [path fill];
   [atlasGray(0.62) setStroke];
   [path setLineWidth:1];
   [path stroke];
}

/* A thin rounded groove. */
-(void)drawSliderTrackInRect:(NSRect)rect vertical:(BOOL)vertical hasTickMarks:(BOOL)hasTickMarks {
   NSRect groove=rect;

   if(vertical){
    groove.size.width=4;
    groove.origin.x=floor(NSMidX(rect)-2);
   }
   else {
    groove.size.height=4;
    groove.origin.y=floor(NSMidY(rect)-2);
   }
   atlasRounded(groove,2,atlasGray(0.82),atlasGray(0.84),nil);
}

-(void)drawSliderTickInRect:(NSRect)rect {
   [atlasGray(0.55) setFill];
   NSRectFill(rect);
}

@end

@implementation NSGraphicsStyle (NSStepper)

-(void)drawStepperButtonInRect:(NSRect)rect clipRect:(NSRect)clipRect enabled:(BOOL)enabled highlighted:(BOOL)highlighted upNotDown:(BOOL)upNotDown {
   atlasRounded(NSInsetRect(rect,0.5,0),3,highlighted?atlasGray(0.84):atlasGray(1.0),
      highlighted?atlasGray(0.80):atlasGray(0.96),atlasGray(0.70));
   atlasChevron(rect,upNotDown?1:0,6,enabled?atlasGray(0.30):atlasGray(0.65));
}

@end

@implementation NSGraphicsStyle (NSTableView)

-(void)drawTableViewHeaderInRect:(NSRect)rect highlighted:(BOOL)highlighted {
   NSGradient *gradient=[[NSGradient alloc] initWithStartingColor:highlighted?atlasGray(0.88):atlasGray(0.95)
      endingColor:highlighted?atlasGray(0.90):atlasGray(0.99)];
   NSRect      line=rect;

   [gradient drawInRect:rect angle:atlasFlipped()?-90:90];
   [gradient release];
   /* The separator on its right and the line under it. */
   [atlasGray(0.82) setFill];
   NSRectFill(NSMakeRect(NSMaxX(rect)-1,NSMinY(rect)+3,1,NSHeight(rect)-6));
   line.size.height=1;
   if(atlasFlipped())
    line.origin.y=NSMaxY(rect)-1;
   [atlasGray(0.78) setFill];
   NSRectFill(line);
}

-(void)drawTableViewCornerInRect:(NSRect)rect {
   [self drawTableViewHeaderInRect:rect highlighted:NO];
}

@end

@implementation NSGraphicsStyle (NSBox)

-(void)drawBoxWithLineInRect:(NSRect)rect {
   [atlasGray(0.80) setStroke];
   NSFrameRect(rect);
}

/* Atlas's group box: a slightly darker rounded area with a light border. */
-(void)drawBoxWithBezelInRect:(NSRect)rect clipRect:(NSRect)clipRect {
   atlasRounded(rect,6,[NSColor colorWithCalibratedWhite:0 alpha:0.035],
      [NSColor colorWithCalibratedWhite:0 alpha:0.035],atlasGray(0.80));
}

-(void)drawBoxWithGrooveInRect:(NSRect)rect clipRect:(NSRect)clipRect {
   [self drawBoxWithBezelInRect:rect clipRect:clipRect];
}

@end

@implementation NSGraphicsStyle (NSComboBox)

/* The accent-colored button at a combo box's right, a white chevron in it. */
-(void)drawComboBoxButtonInRect:(NSRect)rect enabled:(BOOL)enabled bordered:(BOOL)bordered pressed:(BOOL)pressed {
   NSColor *accent=enabled?atlasAccent():atlasGray(0.78);
   NSRect   box=NSInsetRect(rect,0.5,0.5);

   if(pressed)
    accent=atlasDarker(accent,0.15);
   atlasRounded(box,4,atlasLighter(accent,0.08),accent,atlasDarker(accent,0.15));
   atlasChevron(box,0,7,[NSColor whiteColor]);
}

@end

@implementation NSGraphicsStyle (NSTabView)

/* Tabs on the pane's top edge: the chosen one white and joined to it. */
-(void)drawTabInRect:(NSRect)rect clipRect:(NSRect)clipRect color:(NSColor *)color selected:(BOOL)selected {
   NSRect tab=NSInsetRect(rect,1,0);

   if(selected)
    atlasRounded(tab,5,atlasGray(1.0),atlasGray(1.0),atlasGray(0.72));
   else
    atlasRounded(tab,5,atlasGray(0.92),atlasGray(0.88),atlasGray(0.74));
}

-(void)drawTabPaneInRect:(NSRect)rect {
   atlasRounded(rect,6,atlasGray(0.97),atlasGray(0.97),atlasGray(0.78));
}

-(void)drawTabViewBackgroundInRect:(NSRect)rect {
}

@end

@implementation NSGraphicsStyle (NSTextField)

/* A white field with a thin square border, its top a little darker, as
   the reference platform draws text fields. */
-(void)drawTextFieldBorderInRect:(NSRect)rect bezeledNotLine:(BOOL)bezeledNotLine {
   if(bezeledNotLine){
    NSRect inner=NSInsetRect(rect,0.5,0.5);

    [atlasGray(0.74) setStroke];
    [NSBezierPath strokeRect:inner];
    [atlasGray(0.62) setStroke];
    [NSBezierPath strokeLineFromPoint:NSMakePoint(NSMinX(inner),atlasFlipped()?NSMinY(inner):NSMaxY(inner))
       toPoint:NSMakePoint(NSMaxX(inner),atlasFlipped()?NSMinY(inner):NSMaxY(inner))];
   }
   else {
    [atlasGray(0.70) setStroke];
    NSFrameRect(rect);
   }
}

-(void)drawTextViewInsertionPointInRect:(NSRect)rect color:(NSColor *)color {
   [color setFill];
   NSRectFill(rect);
}

@end

@implementation NSView(NSGraphicsStyle)

-(NSGraphicsStyle *)graphicsStyle {
   return [[[NSGraphicsStyle alloc] initWithView:self] autorelease];
}

@end
