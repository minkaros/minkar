/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * What scripting reads of the application, its windows and documents
 * (the standard suite's keys), and how they are specified, as on the
 * reference.
 */

#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSDocument.h>
#import <AppKit/NSWindowController.h>
#import <Foundation/Foundation.h>

@interface NSWindow (NSWindowScriptingInternal)
-(BOOL)_isInternalWindow;
@end

@implementation NSApplication (NSApplicationScripting)

/* The standard suite's name and version keys: the application's name and
   version from its bundle, or its process. */
-(id)valueForUndefinedKey:(NSString *)key {
   if([key isEqualToString:@"name"]){
    NSBundle *bundle=[NSBundle mainBundle];
    NSString *name=[bundle objectForInfoDictionaryKey:@"CFBundleDisplayName"];

    if(name==nil)
     name=[bundle objectForInfoDictionaryKey:@"CFBundleName"];
    return (name!=nil)?name:[[NSProcessInfo processInfo] processName];
   }
   if([key isEqualToString:@"version"]){
    NSBundle *bundle=[NSBundle mainBundle];
    NSString *version=[bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];

    return (version!=nil)?version:[bundle objectForInfoDictionaryKey:@"CFBundleVersion"];
   }
   return [super valueForUndefinedKey:key];
}

@end

@implementation NSWindow (NSWindowScripting)

/* Windows of AppKit's own (tool tips) are not the application's. */
-(BOOL)_isInternalWindow {
   return NO;
}

/* Its window number. */
-(NSNumber *)uniqueID {
   return [NSNumber numberWithInteger:[self windowNumber]];
}

/* Its place among the application's windows, from 1. */
-(NSNumber *)orderedIndex {
   NSUInteger index=[[NSApp orderedWindows] indexOfObjectIdenticalTo:self];

   return [NSNumber numberWithInteger:(index==NSNotFound)?0:(NSInteger)index+1];
}

/* "orderedWindows with an ID of ..." of the application. */
-(NSScriptObjectSpecifier *)objectSpecifier {
   NSScriptClassDescription *application=[NSScriptClassDescription classDescriptionForClass:[NSApplication class]];

   if(application==nil)
    return nil;
   return [[[NSUniqueIDSpecifier alloc] initWithContainerClassDescription:application containerSpecifier:nil
     key:@"orderedWindows" uniqueID:[self uniqueID]] autorelease];
}

-(NSDocument *)document {
   return [[self windowController] document];
}

-(BOOL)hasCloseBox { return ([self styleMask]&NSClosableWindowMask)?YES:NO; }
-(BOOL)hasTitleBar { return ([self styleMask]&NSTitledWindowMask)?YES:NO; }
-(BOOL)isFloatingPanel { return NO; }
-(BOOL)isModalPanel { return [NSApp modalWindow]==self; }

-(BOOL)isResizable { return ([self styleMask]&NSResizableWindowMask)?YES:NO; }
-(BOOL)isMiniaturizable { return ([self styleMask]&NSMiniaturizableWindowMask)?YES:NO; }
-(BOOL)isZoomable { return ([self styleMask]&NSResizableWindowMask) && ([self styleMask]&NSTitledWindowMask); }

@end

@implementation NSDocument (NSDocumentScripting)

/* Its name to scripting: the one set, or the name it shows. */
-(NSString *)lastComponentOfFileName {
   return (_lastComponentOfFileName!=nil)?_lastComponentOfFileName:[self displayName];
}

/* "orderedDocuments named ..." of the application. */
-(NSScriptObjectSpecifier *)objectSpecifier {
   NSScriptClassDescription *application=[NSScriptClassDescription classDescriptionForClass:[NSApplication class]];

   if(application==nil)
    return nil;
   return [[[NSNameSpecifier alloc] initWithContainerClassDescription:application containerSpecifier:nil
     key:@"orderedDocuments" name:[self lastComponentOfFileName]] autorelease];
}

@end
