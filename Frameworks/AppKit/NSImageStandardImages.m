/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The standard images (NSImageName*): the templates drawn here, in code,
 * black on clear for AppKit to tint; the others are Atlas's system icons
 * (/System/Library/Icons). +[NSImage imageNamed:] asks for them when no
 * bundle has an image of that name.
 */

#import <AppKit/NSImage.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSApplication.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSFileManager.h>
#import <math.h>

NSImageName const NSImageNameAddTemplate=@"NSAddTemplate";
NSImageName const NSImageNameRemoveTemplate=@"NSRemoveTemplate";
NSImageName const NSImageNameActionTemplate=@"NSActionTemplate";
NSImageName const NSImageNameSmartBadgeTemplate=@"NSSmartBadgeTemplate";
NSImageName const NSImageNameIconViewTemplate=@"NSIconViewTemplate";
NSImageName const NSImageNameListViewTemplate=@"NSListViewTemplate";
NSImageName const NSImageNameColumnViewTemplate=@"NSColumnViewTemplate";
NSImageName const NSImageNameFlowViewTemplate=@"NSFlowViewTemplate";
NSImageName const NSImageNamePathTemplate=@"NSPathTemplate";
NSImageName const NSImageNameInvalidDataFreestandingTemplate=@"NSInvalidDataFreestandingTemplate";
NSImageName const NSImageNameLockLockedTemplate=@"NSLockLockedTemplate";
NSImageName const NSImageNameLockUnlockedTemplate=@"NSLockUnlockedTemplate";
NSImageName const NSImageNameGoRightTemplate=@"NSGoRightTemplate";
NSImageName const NSImageNameGoLeftTemplate=@"NSGoLeftTemplate";
NSImageName const NSImageNameGoForwardTemplate=@"NSGoForwardTemplate";
NSImageName const NSImageNameGoBackTemplate=@"NSGoBackTemplate";
NSImageName const NSImageNameRightFacingTriangleTemplate=@"NSRightFacingTriangleTemplate";
NSImageName const NSImageNameLeftFacingTriangleTemplate=@"NSLeftFacingTriangleTemplate";
NSImageName const NSImageNameRevealFreestandingTemplate=@"NSRevealFreestandingTemplate";
NSImageName const NSImageNameFollowLinkFreestandingTemplate=@"NSFollowLinkFreestandingTemplate";
NSImageName const NSImageNameEnterFullScreenTemplate=@"NSEnterFullScreenTemplate";
NSImageName const NSImageNameExitFullScreenTemplate=@"NSExitFullScreenTemplate";
NSImageName const NSImageNameStopProgressTemplate=@"NSStopProgressTemplate";
NSImageName const NSImageNameStopProgressFreestandingTemplate=@"NSStopProgressFreestandingTemplate";
NSImageName const NSImageNameRefreshTemplate=@"NSRefreshTemplate";
NSImageName const NSImageNameRefreshFreestandingTemplate=@"NSRefreshFreestandingTemplate";
NSImageName const NSImageNameQuickLookTemplate=@"NSQuickLookTemplate";
NSImageName const NSImageNameSlideshowTemplate=@"NSSlideshowTemplate";
NSImageName const NSImageNameHomeTemplate=@"NSHomeTemplate";
NSImageName const NSImageNameBookmarksTemplate=@"NSBookmarksTemplate";
NSImageName const NSImageNameShareTemplate=@"NSShareTemplate";
NSImageName const NSImageNameMenuOnStateTemplate=@"NSMenuOnStateTemplate";
NSImageName const NSImageNameMenuMixedStateTemplate=@"NSMenuMixedStateTemplate";
NSImageName const NSImageNameApplicationIcon=@"NSApplicationIcon";
NSImageName const NSImageNameCaution=@"NSCaution";
NSImageName const NSImageNameInfo=@"NSInfo";
NSImageName const NSImageNameAdvanced=@"NSAdvanced";
NSImageName const NSImageNamePreferencesGeneral=@"NSPreferencesGeneral";
NSImageName const NSImageNameFolder=@"NSFolder";
NSImageName const NSImageNameFolderBurnable=@"NSFolderBurnable";
NSImageName const NSImageNameFolderSmart=@"NSFolderSmart";
NSImageName const NSImageNameTrashEmpty=@"NSTrashEmpty";
NSImageName const NSImageNameTrashFull=@"NSTrashFull";
NSImageName const NSImageNameComputer=@"NSComputer";
NSImageName const NSImageNameNetwork=@"NSNetwork";
NSImageName const NSImageNameBonjour=@"NSBonjour";
NSImageName const NSImageNameMultipleDocuments=@"NSMultipleDocuments";
NSImageName const NSImageNameUser=@"NSUser";
NSImageName const NSImageNameUserGroup=@"NSUserGroup";
NSImageName const NSImageNameEveryone=@"NSEveryone";
NSImageName const NSImageNameUserGuest=@"NSUserGuest";
NSImageName const NSImageNameColorPanel=@"NSColorPanel";
NSImageName const NSImageNameFontPanel=@"NSFontPanel";
NSImageName const NSImageNameStatusAvailable=@"NSStatusAvailable";
NSImageName const NSImageNameStatusPartiallyAvailable=@"NSStatusPartiallyAvailable";
NSImageName const NSImageNameStatusUnavailable=@"NSStatusUnavailable";
NSImageName const NSImageNameStatusNone=@"NSStatusNone";

#define SIDE 16.0

/* A path's lines, 1.5 wide with round ends. */
static void strokePath(NSBezierPath *path) {
   [path setLineWidth:1.5];
   [path setLineCapStyle:NSRoundLineCapStyle];
   [path setLineJoinStyle:NSRoundLineJoinStyle];
   [path stroke];
}

static void line(CGFloat x0,CGFloat y0,CGFloat x1,CGFloat y1) {
   NSBezierPath *path=[NSBezierPath bezierPath];

   [path moveToPoint:NSMakePoint(x0,y0)];
   [path lineToPoint:NSMakePoint(x1,y1)];
   strokePath(path);
}

static void chevron(CGFloat cx,CGFloat cy,CGFloat size,int dx) {
   NSBezierPath *path=[NSBezierPath bezierPath];

   [path moveToPoint:NSMakePoint(cx-dx*size/2,cy+size)];
   [path lineToPoint:NSMakePoint(cx+dx*size/2,cy)];
   [path lineToPoint:NSMakePoint(cx-dx*size/2,cy-size)];
   strokePath(path);
}

static void triangle(int dx) {
   NSBezierPath *path=[NSBezierPath bezierPath];

   [path moveToPoint:NSMakePoint(8-dx*3.5,3)];
   [path lineToPoint:NSMakePoint(8+dx*4,8)];
   [path lineToPoint:NSMakePoint(8-dx*3.5,13)];
   [path closePath];
   [path fill];
}

static void ring(void) {
   NSBezierPath *path=[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(1.5,1.5,13,13)];

   strokePath(path);
}

static void gear(CGFloat cx,CGFloat cy,CGFloat r) {
   NSBezierPath *path=[NSBezierPath bezierPath];
   int           i;

   for(i=0;i<16;i++){
    CGFloat radius=(i%2)?r*0.72:r,angle=i*M_PI/8;
    NSPoint p=NSMakePoint(cx+radius*cos(angle),cy+radius*sin(angle));

    if(i==0)
     [path moveToPoint:p];
    else
     [path lineToPoint:p];
   }
   [path closePath];
   [path appendBezierPathWithOvalInRect:NSMakeRect(cx-r*0.32,cy-r*0.32,r*0.64,r*0.64)];
   [path setWindingRule:NSEvenOddWindingRule];
   [path fill];
}

static void circularArrow(void) {
   NSBezierPath *path=[NSBezierPath bezierPath];
   NSBezierPath *head=[NSBezierPath bezierPath];

   [path appendBezierPathWithArcWithCenter:NSMakePoint(8,8) radius:5 startAngle:60 endAngle:350];
   strokePath(path);
   [head moveToPoint:NSMakePoint(10.2,13.8)];
   [head lineToPoint:NSMakePoint(14.2,11.6)];
   [head lineToPoint:NSMakePoint(10.4,9.4)];
   [head closePath];
   [head fill];
}

/* A padlock: its body, and its shackle closed over it or open to one side. */
static void lock(BOOL locked) {
   NSBezierPath *shackle=[NSBezierPath bezierPath];

   [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(3,1.5,10,7.5) xRadius:1.5 yRadius:1.5] fill];
   if(locked){
    [shackle moveToPoint:NSMakePoint(5,9)];
    [shackle lineToPoint:NSMakePoint(5,10.5)];
    [shackle appendBezierPathWithArcWithCenter:NSMakePoint(8,10.5) radius:3 startAngle:180 endAngle:0 clockwise:YES];
    [shackle lineToPoint:NSMakePoint(11,9)];
   }
   else {
    [shackle moveToPoint:NSMakePoint(8,9)];
    [shackle lineToPoint:NSMakePoint(8,11.5)];
    [shackle appendBezierPathWithArcWithCenter:NSMakePoint(11,11.5) radius:3 startAngle:180 endAngle:0 clockwise:YES];
    [shackle lineToPoint:NSMakePoint(14,10.5)];
   }
   strokePath(shackle);
}

/* Draws the template of that name (without NSImageName/Template). */
static BOOL drawTemplate(NSString *name) {
   [[NSColor blackColor] set];

   if([name isEqualToString:@"Add"]){
    line(8,3,8,13); line(3,8,13,8);
   }
   else if([name isEqualToString:@"Remove"])
    line(3,8,13,8);
   else if([name isEqualToString:@"Action"])
    gear(8,8,6.5);
   else if([name isEqualToString:@"SmartBadge"]){
    gear(8,8,5.5);
    ring();
   }
   else if([name isEqualToString:@"IconView"]){
    for(int i=0;i<4;i++)
     [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(2+(i%2)*7,2+(i/2)*7,5,5) xRadius:1 yRadius:1] fill];
   }
   else if([name isEqualToString:@"ListView"]){
    for(int i=0;i<3;i++){
     [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(1.5,2.5+i*4.5,2,2)] fill];
     line(5.5,3.5+i*4.5,14,3.5+i*4.5);
    }
   }
   else if([name isEqualToString:@"ColumnView"]){
    strokePath([NSBezierPath bezierPathWithRect:NSMakeRect(1.5,2.5,13,11)]);
    line(6,2.5,6,13.5); line(10.5,2.5,10.5,13.5);
   }
   else if([name isEqualToString:@"FlowView"]){
    [[NSBezierPath bezierPathWithRect:NSMakeRect(5,3,6,9)] fill];
    strokePath([NSBezierPath bezierPathWithRect:NSMakeRect(1,5,3,5)]);
    strokePath([NSBezierPath bezierPathWithRect:NSMakeRect(12,5,3,5)]);
   }
   else if([name isEqualToString:@"Path"]){
    chevron(4,8,3,1); chevron(8,8,3,1); chevron(12,8,3,1);
   }
   else if([name isEqualToString:@"InvalidDataFreestanding"]){
    ring();
    line(8,5.5,8,6); line(8,8.5,8,11.5);
   }
   else if([name isEqualToString:@"LockLocked"])
    lock(YES);
   else if([name isEqualToString:@"LockUnlocked"])
    lock(NO);
   else if([name isEqualToString:@"GoRight"] || [name isEqualToString:@"GoForward"])
    chevron(8,8,5,1);
   else if([name isEqualToString:@"GoLeft"] || [name isEqualToString:@"GoBack"])
    chevron(8,8,5,-1);
   else if([name isEqualToString:@"RightFacingTriangle"])
    triangle(1);
   else if([name isEqualToString:@"LeftFacingTriangle"])
    triangle(-1);
   else if([name isEqualToString:@"RevealFreestanding"]){
    strokePath([NSBezierPath bezierPathWithOvalInRect:NSMakeRect(2,6,7.5,7.5)]);
    line(8.5,6.5,13.5,1.5);
   }
   else if([name isEqualToString:@"FollowLinkFreestanding"]){
    ring();
    line(5.5,5.5,10.5,10.5);
    line(10.5,10.5,6.5,10.5); line(10.5,10.5,10.5,6.5);
   }
   else if([name isEqualToString:@"EnterFullScreen"]){
    line(2,14,6,10); line(2,14,2,10.5); line(2,14,5.5,14);
    line(14,2,10,6); line(14,2,14,5.5); line(14,2,10.5,2);
   }
   else if([name isEqualToString:@"ExitFullScreen"]){
    line(2,14,6,10); line(6,10,6,13.5); line(6,10,2.5,10);
    line(14,2,10,6); line(10,6,10,2.5); line(10,6,13.5,6);
   }
   else if([name isEqualToString:@"StopProgress"]){
    line(4,4,12,12); line(4,12,12,4);
   }
   else if([name isEqualToString:@"StopProgressFreestanding"]){
    ring();
    line(5.5,5.5,10.5,10.5); line(5.5,10.5,10.5,5.5);
   }
   else if([name isEqualToString:@"Refresh"])
    circularArrow();
   else if([name isEqualToString:@"RefreshFreestanding"])
    circularArrow();
   else if([name isEqualToString:@"QuickLook"]){
    NSBezierPath *eye=[NSBezierPath bezierPath];

    [eye moveToPoint:NSMakePoint(1.5,8)];
    [eye curveToPoint:NSMakePoint(14.5,8) controlPoint1:NSMakePoint(4,13) controlPoint2:NSMakePoint(12,13)];
    [eye curveToPoint:NSMakePoint(1.5,8) controlPoint1:NSMakePoint(12,3) controlPoint2:NSMakePoint(4,3)];
    strokePath(eye);
    [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(6,6,4,4)] fill];
   }
   else if([name isEqualToString:@"Slideshow"]){
    NSBezierPath *play=[NSBezierPath bezierPath];

    [play moveToPoint:NSMakePoint(4,2.5)];
    [play lineToPoint:NSMakePoint(13,8)];
    [play lineToPoint:NSMakePoint(4,13.5)];
    [play closePath];
    [play fill];
   }
   else if([name isEqualToString:@"Home"]){
    NSBezierPath *house=[NSBezierPath bezierPath];

    [house moveToPoint:NSMakePoint(1.5,8)];
    [house lineToPoint:NSMakePoint(8,14)];
    [house lineToPoint:NSMakePoint(14.5,8)];
    strokePath(house);
    [[NSBezierPath bezierPathWithRect:NSMakeRect(4,2,8,7)] fill];
   }
   else if([name isEqualToString:@"Bookmarks"]){
    NSBezierPath *book=[NSBezierPath bezierPath];

    [book moveToPoint:NSMakePoint(4,14)];
    [book lineToPoint:NSMakePoint(12,14)];
    [book lineToPoint:NSMakePoint(12,2)];
    [book lineToPoint:NSMakePoint(8,5.5)];
    [book lineToPoint:NSMakePoint(4,2)];
    [book closePath];
    [book fill];
   }
   else if([name isEqualToString:@"Share"]){
    NSBezierPath *box=[NSBezierPath bezierPath];

    [box moveToPoint:NSMakePoint(5.5,10)];
    [box lineToPoint:NSMakePoint(3,10)];
    [box lineToPoint:NSMakePoint(3,1.5)];
    [box lineToPoint:NSMakePoint(13,1.5)];
    [box lineToPoint:NSMakePoint(13,10)];
    [box lineToPoint:NSMakePoint(10.5,10)];
    strokePath(box);
    line(8,5,8,14.5); line(8,14.5,5.5,12); line(8,14.5,10.5,12);
   }
   else if([name isEqualToString:@"MenuOnState"]){
    NSBezierPath *check=[NSBezierPath bezierPath];

    [check moveToPoint:NSMakePoint(3,8)];
    [check lineToPoint:NSMakePoint(6.5,4)];
    [check lineToPoint:NSMakePoint(13,12.5)];
    [check setLineWidth:2];
    [check setLineCapStyle:NSRoundLineCapStyle];
    [check setLineJoinStyle:NSRoundLineJoinStyle];
    [check stroke];
   }
   else if([name isEqualToString:@"MenuMixedState"])
    line(4,8,12,8);
   else
    return NO;
   return YES;
}

/* The status dots: a colored circle. */
static NSImage *statusImage(NSColor *color) {
   NSImage *image=[[NSImage alloc] initWithSize:NSMakeSize(SIDE,SIDE)];

   [image lockFocus];
   [color setFill];
   [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(3,3,10,10)] fill];
   [[NSColor colorWithCalibratedWhite:0 alpha:0.25] setStroke];
   [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(3.5,3.5,9,9)] stroke];
   [image unlockFocus];
   return [image autorelease];
}

/* The system icons' files for the others. */
static NSString *iconPath(NSString *name) {
   static NSDictionary *paths;

   if(paths==nil)
    paths=[[NSDictionary alloc] initWithObjectsAndKeys:
       @"status/dialog-warning.png",@"NSCaution",
       @"status/dialog-information.png",@"NSInfo",
       @"prefs/preferences-system-windows.png",@"NSAdvanced",
       @"apps/SystemPreferences.png",@"NSPreferencesGeneral",
       @"places/folder.png",@"NSFolder",
       @"places/folder.png",@"NSFolderBurnable",
       @"places/folder-saved-search.png",@"NSFolderSmart",
       @"system/user-trash.png",@"NSTrashEmpty",
       @"system/user-trash-full.png",@"NSTrashFull",
       @"devices/computer.png",@"NSComputer",
       @"devices/network-wired.png",@"NSNetwork",
       @"devices/network-wireless.png",@"NSBonjour",
       @"system/text-x-generic.png",@"NSMultipleDocuments",
       @"prefs/system-users.png",@"NSUser",
       @"prefs/system-users.png",@"NSUserGroup",
       @"prefs/system-users.png",@"NSEveryone",
       @"prefs/system-users.png",@"NSUserGuest",
       @"apps/ColorPicker.png",@"NSColorPanel",
       @"apps/Fonts.png",@"NSFontPanel",
       nil];
   NSString *relative=[paths objectForKey:name];

   return (relative!=nil)?[@"/System/Library/Icons" stringByAppendingPathComponent:relative]:nil;
}

/* The standard image of that name, made and named; nil if it is not one. */
NSImage *_NSStandardImageNamed(NSString *name) {
   NSImage *image=nil;

   if(![name hasPrefix:@"NS"])
    return nil;

   if([name hasSuffix:@"Template"] && [name length]>10){
    NSString *base=[name substringWithRange:NSMakeRange(2,[name length]-10)];

    image=[[[NSImage alloc] initWithSize:NSMakeSize(SIDE,SIDE)] autorelease];
    [image lockFocus];
    if(!drawTemplate(base)){
     [image unlockFocus];
     return nil;
    }
    [image unlockFocus];
    [image setTemplate:YES];
   }
   else if([name isEqualToString:@"NSStatusAvailable"])
    image=statusImage([NSColor systemGreenColor]);
   else if([name isEqualToString:@"NSStatusPartiallyAvailable"])
    image=statusImage([NSColor systemYellowColor]);
   else if([name isEqualToString:@"NSStatusUnavailable"])
    image=statusImage([NSColor systemRedColor]);
   else if([name isEqualToString:@"NSStatusNone"])
    image=statusImage([NSColor colorWithCalibratedWhite:0.85 alpha:1]);
   else {
    NSString *path=iconPath(name);

    if(path!=nil && [[NSFileManager defaultManager] fileExistsAtPath:path])
     image=[[[NSImage alloc] initWithContentsOfFile:path] autorelease];
   }

   if(image!=nil)
    [image setName:name];
   return image;
}
