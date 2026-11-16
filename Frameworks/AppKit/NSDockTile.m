#include <unistd.h>
#import <Foundation/NSDistributedNotificationCenter.h>
#import <AppKit/NSBitmapImageRep.h>
#import <AppKit/NSView.h>
#import <AppKit/NSDockTile.h>
#import <AppKit/NSApplication.h>
#import <Foundation/NSUserNotification.h>

/* Foundation's: the app's badge, through the notification center to the
   Dock (Atlas.md, 5.6, N2). */
@interface NSUserNotificationCenter (NSDockTileBadge)
-(BOOL)_setBadgeLabel:(NSString *)label;
@end

@implementation NSDockTile

-initWithOwner:owner {
   _owner=owner;
   _size=NSZeroSize;
   return self;
}

-(void)dealloc {
   [_badgeLabel release];
   [_contentView release];
   [super dealloc];
}

-(NSSize)size {
   return _size;
}

-owner {
   return _owner;
}

-(NSString *)badgeLabel {
   return _badgeLabel;
}

-(NSView *)contentView {
   return _contentView;
}

-(BOOL)showsApplicationBadge {
   return _showsApplicationBadge;
}

-(void)setBadgeLabel:(NSString *)value {
   BOOL changed=!(value==_badgeLabel || [value isEqualToString:_badgeLabel]);

   value=[value copy];
   [_badgeLabel release];
   _badgeLabel=value;
   /* The application's tile is the Dock's: its badge shows there. */
   if(changed && [_owner isKindOfClass:[NSApplication class]])
    [[NSUserNotificationCenter defaultUserNotificationCenter] _setBadgeLabel:value];
}

-(void)setContentView:(NSView *)view {
   view=[view retain];
   [_contentView release];
   _contentView=view;
   if(view==nil)
    [self display];
}

-(void)setShowsApplicationBadge:(BOOL)value {
   _showsApplicationBadge=value;
}

/* The application's tile drawn by its content view (D3): the view drawn
   into an image at its size, sent to the Dock, which shows it in place of
   the app's icon; with no content view the icon is back. */
-(void)display {
   if(![_owner isKindOfClass:[NSApplication class]])
    return;
   NSMutableDictionary *info=[NSMutableDictionary dictionaryWithObject:[NSNumber numberWithInt:getpid()] forKey:@"pid"];

   if(_contentView!=nil){
    NSRect bounds=[_contentView bounds];
    NSBitmapImageRep *rep=[_contentView bitmapImageRepForCachingDisplayInRect:bounds];

    if(rep!=nil){
     [_contentView cacheDisplayInRect:bounds toBitmapImageRep:rep];
     NSData *png=[rep representationUsingType:NSPNGFileType properties:[NSDictionary dictionary]];
     if(png!=nil)
      [info setObject:png forKey:@"png"];
    }
   }
   [[NSDistributedNotificationCenter defaultCenter] postNotificationName:@"org.minkar.dock.tile"
      object:[[NSBundle mainBundle] bundleIdentifier] userInfo:info deliverImmediately:YES];
}

@end
