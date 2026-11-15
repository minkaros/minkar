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
}

-(void)setShowsApplicationBadge:(BOOL)value {
   _showsApplicationBadge=value;
}

-(void)display {
}

@end
