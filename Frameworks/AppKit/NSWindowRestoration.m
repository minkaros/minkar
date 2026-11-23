/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Window restoration (Atlas.md, W2). When an app quits keeping its windows
 * (NSQuitAlwaysKeepsWindows, the General pane's Close windows when
 * quitting an app off, or Quit and Keep Windows, Option-Command-Q), each
 * restorable window with a restoration class and an identifier is saved:
 * its class, identifier, frame and its state (its encodeRestorableState-
 * WithCoder:, which asks its delegate, and the app delegate's), front to
 * back, in ~/Library/Saved Application State/<bundle id>.savedState. At
 * the next launch each window is made again by its restoration class,
 * given its state and frame, and shown in its place; then no untitled
 * document is opened. Quitting closing the windows removes the state;
 * ApplePersistenceIgnoreState skips it.
 */

#import <AppKit/NSWindowRestoration.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSScreen.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

static char kIdentifierKey;

/* For NSApplication.m. */
BOOL _NSQuitKeepsWindows(BOOL other);
NSData *_NSEncodeRestorableState(BOOL keep);
void _NSWriteRestorableState(NSData *state);
NSUInteger _NSRestoreWindows(void);

@implementation NSWindow (NSUserInterfaceItemIdentification)

-(NSUserInterfaceItemIdentifier)identifier {
   return objc_getAssociatedObject(self,&kIdentifierKey);
}

-(void)setIdentifier:(NSUserInterfaceItemIdentifier)identifier {
   objc_setAssociatedObject(self,&kIdentifierKey,identifier,OBJC_ASSOCIATION_COPY);
}

-(NSUserInterfaceItemIdentifier)userInterfaceItemIdentifier {
   return [self identifier];
}

-(void)setUserInterfaceItemIdentifier:(NSUserInterfaceItemIdentifier)identifier {
   [self setIdentifier:identifier];
}

@end

/* NSWindowDelegate's and NSApplicationDelegate's calls. */
@protocol NSRestorableStateDelegate <NSObject>
@optional
-(void)window:(NSWindow *)window willEncodeRestorableState:(NSCoder *)state;
-(void)window:(NSWindow *)window didDecodeRestorableState:(NSCoder *)state;
-(void)application:(NSApplication *)app willEncodeRestorableState:(NSCoder *)state;
-(void)application:(NSApplication *)app didDecodeRestorableState:(NSCoder *)state;
@end

@implementation NSWindow (NSRestorableState)

/* Its own (its frame, as a string), its restorable key paths (NSResponder),
   then its delegate's. */
-(void)encodeRestorableStateWithCoder:(NSCoder *)coder {
   id delegate=[self delegate];

   [coder encodeObject:NSStringFromRect([self frame]) forKey:@"NSWindowFrame"];
   [coder encodeBool:[self isMiniaturized] forKey:@"NSWindowMiniaturized"];
   for(NSString *path in [[self class] restorableStateKeyPaths])
    [coder encodeObject:[self valueForKeyPath:path] forKey:path];
   if([delegate respondsToSelector:@selector(window:willEncodeRestorableState:)])
    [delegate window:self willEncodeRestorableState:coder];
}

-(void)restoreStateWithCoder:(NSCoder *)coder {
   NSString *frame=[coder decodeObjectForKey:@"NSWindowFrame"];
   id        delegate=[self delegate];

   if(frame!=nil)
    [self setFrame:NSRectFromString(frame) display:NO];
   for(NSString *path in [[self class] restorableStateKeyPaths]){
    id value=[coder decodeObjectForKey:path];

    if(value!=nil)
     [self setValue:value forKeyPath:path];
   }
   if([delegate respondsToSelector:@selector(window:didDecodeRestorableState:)])
    [delegate window:self didDecodeRestorableState:coder];
}

@end

@implementation NSApplication (NSWindowRestoration)

-(BOOL)restoreWindowWithIdentifier:(NSUserInterfaceItemIdentifier)identifier state:(NSCoder *)state
   completionHandler:(void (^)(NSWindow *,NSError *))completionHandler {
   return NO;
}

@end

/* The app's saved state's folder. */
static NSString *savedStatePath(void) {
   NSString *bundleID=[[NSBundle mainBundle] bundleIdentifier];

   if([bundleID length]==0)
    return nil;
   return [[[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Saved Application State"]
      stringByAppendingPathComponent:[bundleID stringByAppendingPathExtension:@"savedState"]]
      stringByAppendingPathComponent:@"windows.plist"];
}

/* The windows to keep, front to back: shown (or minimized), restorable,
   with a restoration class that answers and an identifier. */
static NSArray *windowsToKeep(void) {
   NSMutableArray *result=[NSMutableArray array];
   NSArray        *ordered=[NSApp respondsToSelector:@selector(orderedWindows)] ? [NSApp orderedWindows] : [NSApp windows];

   for(NSWindow *window in ordered){
    Class restoration=[window restorationClass];

    if(!([window isVisible] || [window isMiniaturized]) || ![window isRestorable] || [window isSheet])
     continue;
    if(restoration==Nil || ![restoration respondsToSelector:@selector(restoreWindowWithIdentifier:state:completionHandler:)])
     continue;
    if([[window identifier] length]==0)
     continue;
    [result addObject:window];
   }
   /* A tab behind its group's shown one is kept too (after it). */
   for(NSWindow *window in [NSApp windows])
    if(![result containsObject:window] && [window restorationClass]!=Nil && [[window identifier] length]>0 &&
       [window isRestorable] && [[window tabbedWindows] count]>1 && [[window restorationClass]
       respondsToSelector:@selector(restoreWindowWithIdentifier:state:completionHandler:)])
     [result addObject:window];
   return result;
}

/* Whether quitting keeps the windows: the user's choice, or the other one
   for Quit and Keep Windows (Quit and Close All Windows). */
BOOL _NSQuitKeepsWindows(BOOL other) {
   BOOL keeps=[[NSUserDefaults standardUserDefaults] boolForKey:@"NSQuitAlwaysKeepsWindows"];

   return other ? !keeps : keeps;
}

/* The state to save, from -[NSApplication terminate:], before the
   documents close; nil to forget it. */
NSData *_NSEncodeRestorableState(BOOL keep) {
   NSMutableArray *windows=[NSMutableArray array];

   if(!keep)
    return nil;
   for(NSWindow *window in windowsToKeep()){
    NSKeyedArchiver *coder=[[[NSKeyedArchiver alloc] initRequiringSecureCoding:NO] autorelease];

    [window encodeRestorableStateWithCoder:coder];
    [coder finishEncoding];
    [windows addObject:@{
       @"class":NSStringFromClass([window restorationClass]),
       @"identifier":[window identifier],
       @"frame":NSStringFromRect([window frame]),
       @"key":[NSNumber numberWithBool:(window==[NSApp keyWindow])],
       @"state":[coder encodedData] }];
   }
   NSKeyedArchiver *app=[[[NSKeyedArchiver alloc] initRequiringSecureCoding:NO] autorelease];
   id               delegate=[NSApp delegate];

   if([delegate respondsToSelector:@selector(application:willEncodeRestorableState:)])
    [delegate application:NSApp willEncodeRestorableState:app];
   [app finishEncoding];
   return [NSPropertyListSerialization dataWithPropertyList:@{ @"windows":windows, @"application":[app encodedData] }
      format:NSPropertyListBinaryFormat_v1_0 options:0 error:NULL];
}

/* As the app quits: the state written, or removed. */
void _NSWriteRestorableState(NSData *state) {
   NSString *path=savedStatePath();

   if(path==nil)
    return;
   if(state==nil){
    [[NSFileManager defaultManager] removeItemAtPath:[path stringByDeletingLastPathComponent] error:NULL];
    return;
   }
   [[NSFileManager defaultManager] createDirectoryAtPath:[path stringByDeletingLastPathComponent]
      withIntermediateDirectories:YES attributes:@{ NSFilePosixPermissions:@0700 } error:NULL];
   [state writeToFile:path atomically:YES];
}

/* At launch (-[NSApplication finishLaunching]): the saved windows made
   again and shown, back to front, the key one last; how many. */
NSUInteger _NSRestoreWindows(void) {
   NSString     *path=savedStatePath();
   NSData       *data;
   NSDictionary *saved;
   NSUInteger    count=0;
   __block NSWindow *key=nil;

   if(path==nil || [[NSUserDefaults standardUserDefaults] boolForKey:@"ApplePersistenceIgnoreState"])
    return 0;
   if((data=[NSData dataWithContentsOfFile:path])==nil)
    return 0;
   saved=[NSPropertyListSerialization propertyListWithData:data options:0 format:NULL error:NULL];
   if(![saved isKindOfClass:[NSDictionary class]])
    return 0;

   NSArray        *windows=[saved objectForKey:@"windows"];
   NSMutableArray *restored=[NSMutableArray array];

   for(NSDictionary *entry in [windows reverseObjectEnumerator]){
    Class      restoration=NSClassFromString([entry objectForKey:@"class"]);
    NSData    *state=[entry objectForKey:@"state"];
    NSString  *identifier=[entry objectForKey:@"identifier"];
    BOOL       wasKey=[[entry objectForKey:@"key"] boolValue];
    NSKeyedUnarchiver *coder;

    if(restoration==Nil || ![restoration respondsToSelector:@selector(restoreWindowWithIdentifier:state:completionHandler:)] ||
       ![state isKindOfClass:[NSData class]] || ![identifier isKindOfClass:[NSString class]])
     continue;
    coder=[[[NSKeyedUnarchiver alloc] initForReadingWithData:state] autorelease];
    [coder setRequiresSecureCoding:NO];
    [restoration restoreWindowWithIdentifier:identifier state:coder completionHandler:^(NSWindow *window,NSError *error){
     if(window==nil)
      return;
     if([[window identifier] length]==0)
      [window setIdentifier:identifier];
     [window restoreStateWithCoder:coder];
     [window setFrame:NSRectFromString([entry objectForKey:@"frame"]) display:NO];
     [restored addObject:window];
     if(wasKey)
      key=window;
    }];
   }
   /* Back to front: the front one ordered in last, the key one key. */
   for(NSWindow *window in restored){
    [window orderFront:nil];
    count++;
   }
   [(key ?: [restored lastObject]) makeKeyAndOrderFront:nil];

   NSData *app=[saved objectForKey:@"application"];
   id      delegate=[NSApp delegate];
   if([app isKindOfClass:[NSData class]] && [delegate respondsToSelector:@selector(application:didDecodeRestorableState:)]){
    NSKeyedUnarchiver *coder=[[[NSKeyedUnarchiver alloc] initForReadingWithData:app] autorelease];

    [coder setRequiresSecureCoding:NO];
    [delegate application:NSApp didDecodeRestorableState:coder];
   }
   return count;
}
