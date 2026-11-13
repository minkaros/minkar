/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSSharingService.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSMenuItem.h>
#import <AppKit/NSWorkspace.h>
#import <Foundation/Foundation.h>

NSSharingServiceName const NSSharingServiceNameComposeEmail = @"NSSharingServiceNameComposeEmail";
NSSharingServiceName const NSSharingServiceNameComposeMessage = @"NSSharingServiceNameComposeMessage";
NSSharingServiceName const NSSharingServiceNameSendViaAirDrop = @"NSSharingServiceNameSendViaAirDrop";
NSSharingServiceName const NSSharingServiceNameAddToSafariReadingList = @"NSSharingServiceNameAddToSafariReadingList";
NSSharingServiceName const NSSharingServiceNameAddToIPhoto = @"NSSharingServiceNameAddToIPhoto";
NSSharingServiceName const NSSharingServiceNameAddToAperture = @"NSSharingServiceNameAddToAperture";
NSSharingServiceName const NSSharingServiceNameUseAsDesktopPicture = @"NSSharingServiceNameUseAsDesktopPicture";
NSSharingServiceName const NSSharingServiceNameCloudSharing = @"NSSharingServiceNameCloudSharing";

@interface NSSharingService ()
- (instancetype)_initWithName:(NSString *)name title:(NSString *)title;
@end

/* What an item says in a message: its text, a link's address, a file's path. */
static NSString *textOfItem(id item) {
    if ([item isKindOfClass:[NSURL class]])
        return [item isFileURL] ? [item path] : [item absoluteString];
    if ([item isKindOfClass:[NSAttributedString class]])
        return [item string];
    if ([item isKindOfClass:[NSString class]])
        return item;
    return nil;
}

static NSString *mailQuery(NSString *text) {
    NSMutableCharacterSet *allowed = [[[NSCharacterSet URLQueryAllowedCharacterSet] mutableCopy] autorelease];
    [allowed removeCharactersInString:@"&=+?#"];
    return [text stringByAddingPercentEncodingWithAllowedCharacters:allowed];
}

@implementation NSSharingService

+ (NSSharingService *)sharingServiceNamed:(NSSharingServiceName)serviceName {
    if ([serviceName isEqualToString:NSSharingServiceNameComposeEmail])
        return [[[self alloc] _initWithName:serviceName title:@"Email"] autorelease];
    return nil;
}

+ (NSArray *)sharingServicesForItems:(NSArray *)items {
    NSSharingService *mail = [self sharingServiceNamed:NSSharingServiceNameComposeEmail];
    return [mail canPerformWithItems:items] ? @[ mail ] : @[];
}

- (instancetype)_initWithName:(NSString *)name title:(NSString *)title {
    if ((self = [super init]) != nil) {
        _name = [name copy];
        _title = [title copy];
        _menuItemTitle = [title copy];
    }
    return self;
}

- (instancetype)initWithTitle:(NSString *)title image:(NSImage *)image alternateImage:(NSImage *)alternateImage
                      handler:(void (^)(void))block {
    if ((self = [self _initWithName:nil title:title]) != nil) {
        _image = [image retain];
        _alternateImage = [alternateImage retain];
        _handler = [block copy];
    }
    return self;
}

- (void)dealloc {
    [_name release];
    [_title release];
    [_image release];
    [_alternateImage release];
    [_menuItemTitle release];
    [_recipients release];
    [_subject release];
    [_messageBody release];
    [_permanentLink release];
    [_accountName release];
    [_attachmentFileURLs release];
    [_handler release];
    [super dealloc];
}

- (id)delegate { return _delegate; }
- (void)setDelegate:(id)delegate { _delegate = delegate; }
- (NSString *)title { return _title; }
- (NSImage *)image { return _image; }
- (NSImage *)alternateImage { return _alternateImage; }
- (NSString *)menuItemTitle { return _menuItemTitle; }
- (void)setMenuItemTitle:(NSString *)title { title = [title copy]; [_menuItemTitle release]; _menuItemTitle = title; }
- (NSArray *)recipients { return _recipients; }
- (void)setRecipients:(NSArray *)recipients { recipients = [recipients copy]; [_recipients release]; _recipients = recipients; }
- (NSString *)subject { return _subject; }
- (void)setSubject:(NSString *)subject { subject = [subject copy]; [_subject release]; _subject = subject; }
- (NSString *)messageBody { return _messageBody; }
- (NSURL *)permanentLink { return _permanentLink; }
- (NSString *)accountName { return _accountName; }
- (NSArray *)attachmentFileURLs { return _attachmentFileURLs; }

- (BOOL)canPerformWithItems:(NSArray *)items {
    if (_handler != nil)
        return YES;
    if ([items count] == 0)
        return NO;
    for (id item in items)
        if (textOfItem(item) == nil)
            return NO;
    return YES;
}

- (void)performWithItems:(NSArray *)items {
    if ([_delegate respondsToSelector:@selector(sharingService:willShareItems:)])
        [_delegate sharingService:self willShareItems:items];
    if (_handler != nil) {
        _handler();
        if ([_delegate respondsToSelector:@selector(sharingService:didShareItems:)])
            [_delegate sharingService:self didShareItems:items];
        return;
    }

    NSMutableArray *lines = [NSMutableArray array];
    for (id item in items) {
        NSString *text = textOfItem(item);
        if (text != nil)
            [lines addObject:text];
    }
    NSString *body = [lines componentsJoinedByString:@"\n"];
    NSMutableString *link = [NSMutableString stringWithString:@"mailto:"];
    [link appendString:mailQuery([_recipients componentsJoinedByString:@","] ?: @"")];
    [link appendFormat:@"?body=%@", mailQuery(body)];
    if (_subject != nil)
        [link appendFormat:@"&subject=%@", mailQuery(_subject)];

    NSURL *url = [NSURL URLWithString:link];
    if (url != nil && [[NSWorkspace sharedWorkspace] openURL:url]) {
        [_messageBody release];
        _messageBody = [body copy];
        if ([_delegate respondsToSelector:@selector(sharingService:didShareItems:)])
            [_delegate sharingService:self didShareItems:items];
    } else if ([_delegate respondsToSelector:@selector(sharingService:didFailToShareItems:error:)]) {
        NSError *error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil];
        [_delegate sharingService:self didFailToShareItems:items error:error];
    }
}

@end

@implementation NSSharingServicePicker

- (instancetype)initWithItems:(NSArray *)items {
    if ((self = [super init]) != nil)
        _items = [items copy];
    return self;
}

- (void)dealloc {
    [_items release];
    [super dealloc];
}

- (id)delegate { return _delegate; }
- (void)setDelegate:(id)delegate { _delegate = delegate; }

- (NSArray *)_services {
    NSArray *services = [NSSharingService sharingServicesForItems:_items];
    if ([_delegate respondsToSelector:@selector(sharingServicePicker:sharingServicesForItems:proposedSharingServices:)])
        services = [_delegate sharingServicePicker:self sharingServicesForItems:_items proposedSharingServices:services];
    return services;
}

- (void)_choose:(NSMenuItem *)item {
    NSSharingService *service = [item representedObject];
    if ([_delegate respondsToSelector:@selector(sharingServicePicker:delegateForSharingService:)])
        [service setDelegate:[_delegate sharingServicePicker:self delegateForSharingService:service]];
    if ([_delegate respondsToSelector:@selector(sharingServicePicker:didChooseSharingService:)])
        [_delegate sharingServicePicker:self didChooseSharingService:service];
    [service performWithItems:_items];
}

- (void)showRelativeToRect:(NSRect)rect ofView:(NSView *)view preferredEdge:(NSRectEdge)preferredEdge {
    NSMenu *menu = [[[NSMenu alloc] initWithTitle:@""] autorelease];
    for (NSSharingService *service in [self _services]) {
        NSMenuItem *item = [menu addItemWithTitle:[service menuItemTitle] action:@selector(_choose:) keyEquivalent:@""];
        [item setTarget:self];
        [item setImage:[service image]];
        [item setRepresentedObject:service];
    }
    if ([menu numberOfItems] == 0) {
        if ([_delegate respondsToSelector:@selector(sharingServicePicker:didChooseSharingService:)])
            [_delegate sharingServicePicker:self didChooseSharingService:nil];
        return;
    }
    NSPoint at = NSMakePoint(NSMinX(rect), [view isFlipped] ? NSMaxY(rect) : NSMinY(rect));
    [menu popUpMenuPositioningItem:nil atLocation:at inView:view];
}

- (void)close {
}

@end
