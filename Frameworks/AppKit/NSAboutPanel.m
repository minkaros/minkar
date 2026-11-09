/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The standard About window (-[NSApplication orderFrontStandardAboutPanel:]),
 * Atlas's, made in code: the app's icon, its name, its version, the
 * credits when it has any, and its copyright. What the options give
 * replaces what the app's Info.plist and resources say.
 */

#import <AppKit/NSApplication.h>
#import <AppKit/NSAttributedString.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSImageView.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSScrollView.h>
#import <AppKit/NSTextField.h>
#import <AppKit/NSTextStorage.h>
#import <AppKit/NSTextView.h>
#import <Foundation/NSBundle.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSProcessInfo.h>
#import "NSAboutPanel.h"

NSAboutPanelOptionKey const NSAboutPanelOptionCredits=@"Credits";
NSAboutPanelOptionKey const NSAboutPanelOptionApplicationName=@"ApplicationName";
NSAboutPanelOptionKey const NSAboutPanelOptionApplicationIcon=@"ApplicationIcon";
NSAboutPanelOptionKey const NSAboutPanelOptionVersion=@"Version";
NSAboutPanelOptionKey const NSAboutPanelOptionApplicationVersion=@"ApplicationVersion";

/* The reference platform's measures: 285 points wide, a 64-point icon,
   the name in bold 13. */
#define WIDTH 285
#define ICON 64
#define MARGIN 16
#define CREDITS_HEIGHT 120

static NSString *localized(NSString *string) {
   return [[NSBundle bundleForClass:[NSAboutPanel class]] localizedStringForKey:string value:string table:nil];
}

static NSString *infoString(NSString *key) {
   NSBundle *main=[NSBundle mainBundle];
   id        value=[main objectForInfoDictionaryKey:key];

   return [value isKindOfClass:[NSString class]]?value:nil;
}

/* The credits in the app's resources: Credits.html, .rtf or .rtfd. */
static NSAttributedString *bundleCredits(void) {
   NSBundle *main=[NSBundle mainBundle];
   NSString *path;

   if((path=[main pathForResource:@"Credits" ofType:@"html"])!=nil){
    NSData *data=[NSData dataWithContentsOfFile:path];

    if(data!=nil)
     return [[[NSAttributedString alloc] initWithHTML:data documentAttributes:NULL] autorelease];
   }
   if((path=[main pathForResource:@"Credits" ofType:@"rtf"])!=nil){
    NSData *data=[NSData dataWithContentsOfFile:path];

    if(data!=nil)
     return [[[NSAttributedString alloc] initWithRTF:data documentAttributes:NULL] autorelease];
   }
   if((path=[main pathForResource:@"Credits" ofType:@"rtfd"])!=nil)
    return [[[NSAttributedString alloc] initWithPath:path documentAttributes:NULL] autorelease];
   return nil;
}

static NSTextField *label(NSString *string,NSFont *font,NSColor *color) {
   NSTextField *field=[[[NSTextField alloc] initWithFrame:NSMakeRect(0,0,WIDTH-2*MARGIN,20)] autorelease];

   [field setStringValue:(string!=nil)?string:@""];
   [field setFont:font];
   [field setTextColor:color];
   [field setAlignment:NSCenterTextAlignment];
   [field setBezeled:NO];
   [field setBordered:NO];
   [field setDrawsBackground:NO];
   [field setEditable:NO];
   [field setSelectable:YES];
   return field;
}

@implementation NSAboutPanel

+(NSAboutPanel *)sharedAboutPanel {
   static NSAboutPanel *shared;

   if(shared==nil)
    shared=[[NSAboutPanel alloc] init];
   return shared;
}

-(void)dealloc {
   [_window release];
   [super dealloc];
}

/* "Version 1.2 (34)", the build left out when it is the same. */
+(NSString *)versionStringWithApplicationVersion:(NSString *)applicationVersion version:(NSString *)version {
   if([applicationVersion length]==0 && [version length]==0)
    return @"";
   if([applicationVersion length]==0)
    return [NSString stringWithFormat:localized(@"Version %@"),version];
   if([version length]==0 || [version isEqualToString:applicationVersion])
    return [NSString stringWithFormat:localized(@"Version %@"),applicationVersion];
   return [NSString stringWithFormat:localized(@"Version %@ (%@)"),applicationVersion,version];
}

-(void)orderFrontWithOptions:(NSDictionary *)options {
   NSString           *name=[options objectForKey:NSAboutPanelOptionApplicationName];
   NSImage            *icon=[options objectForKey:NSAboutPanelOptionApplicationIcon];
   NSString           *applicationVersion=[options objectForKey:NSAboutPanelOptionApplicationVersion];
   NSString           *version=[options objectForKey:NSAboutPanelOptionVersion];
   NSString           *copyright=[options objectForKey:@"Copyright"];
   NSAttributedString *credits=[options objectForKey:NSAboutPanelOptionCredits];
   NSBundle           *main=[NSBundle mainBundle];
   NSMutableArray     *rows=[NSMutableArray array];
   NSImageView        *iconView;
   NSView             *content;
   CGFloat             height,y;

   if(name==nil){
    name=[main localizedStringForKey:@"CFBundleName" value:infoString(@"CFBundleName") table:@"InfoPlist"];
    if([name length]==0)
     name=[[NSProcessInfo processInfo] processName];
   }
   if(icon==nil)
    icon=[NSApp applicationIconImage];
   if(icon==nil)
    icon=[NSImage imageNamed:NSImageNameApplicationIcon];
   if(applicationVersion==nil && ![options objectForKey:NSAboutPanelOptionApplicationVersion])
    applicationVersion=infoString(@"CFBundleShortVersionString");
   if(version==nil && ![options objectForKey:NSAboutPanelOptionVersion])
    version=infoString(@"CFBundleVersion");
   if(copyright==nil)
    copyright=[main localizedStringForKey:@"NSHumanReadableCopyright"
       value:infoString(@"NSHumanReadableCopyright") table:@"InfoPlist"];
   if(credits==nil)
    credits=bundleCredits();

   /* The rows, top to bottom. */
   [rows addObject:label(name,[NSFont boldSystemFontOfSize:13],[NSColor labelColor])];
   [rows addObject:label([NSAboutPanel versionStringWithApplicationVersion:applicationVersion version:version],
      [NSFont systemFontOfSize:11],[NSColor secondaryLabelColor])];
   if([credits length]>0){
    NSScrollView *scroll=[[[NSScrollView alloc] initWithFrame:NSMakeRect(0,0,WIDTH-2*MARGIN,CREDITS_HEIGHT)] autorelease];
    NSTextView   *text=[[[NSTextView alloc] initWithFrame:NSMakeRect(0,0,WIDTH-2*MARGIN-16,CREDITS_HEIGHT)] autorelease];

    [text setEditable:NO];
    [text setSelectable:YES];
    [[text textStorage] setAttributedString:credits];
    [scroll setHasVerticalScroller:YES];
    [scroll setBorderType:NSNoBorder];
    [scroll setDocumentView:text];
    [rows addObject:scroll];
   }
   if([copyright length]>0)
    [rows addObject:label(copyright,[NSFont systemFontOfSize:10],[NSColor secondaryLabelColor])];

   height=MARGIN+ICON+8;
   for(NSView *row in rows)
    height+=NSHeight([row frame])+6;
   height+=MARGIN-6;

   [_window close];
   [_window release];
   _window=[[NSPanel alloc] initWithContentRect:NSMakeRect(0,0,WIDTH,height)
      styleMask:NSTitledWindowMask|NSClosableWindowMask backing:NSBackingStoreBuffered defer:NO];
   [_window setTitle:@""];
   [_window setReleasedWhenClosed:NO];
   [_window setHidesOnDeactivate:NO];
   content=[_window contentView];

   y=height-MARGIN-ICON;
   iconView=[[[NSImageView alloc] initWithFrame:NSMakeRect((WIDTH-ICON)/2,y,ICON,ICON)] autorelease];
   [iconView setImageScaling:NSImageScaleProportionallyUpOrDown];
   [iconView setImage:icon];
   [content addSubview:iconView];
   y-=8;

   for(NSView *row in rows){
    NSRect frame=[row frame];

    y-=NSHeight(frame);
    frame.origin=NSMakePoint(MARGIN,y);
    [row setFrame:frame];
    [content addSubview:row];
    y-=6;
   }

   [_window center];
   [_window makeKeyAndOrderFront:nil];
}

-(NSWindow *)window {
   return _window;
}

@end
