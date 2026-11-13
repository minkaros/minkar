/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Sharing: services that send items (text, links, files) somewhere, and a
 * picker that offers them. Minkar's service is email: a new message in the
 * mail app, through a mailto: link. A service the system does not have is
 * not returned.
 */

#import <Foundation/NSObject.h>
#import <Foundation/NSGeometry.h>
#import <AppKit/NSView.h>

@class NSArray, NSImage, NSString, NSURL, NSWindow, NSError;
@protocol NSSharingServiceDelegate, NSSharingServicePickerDelegate;

typedef NSString *NSSharingServiceName;

APPKIT_EXPORT NSSharingServiceName const NSSharingServiceNameComposeEmail;
APPKIT_EXPORT NSSharingServiceName const NSSharingServiceNameComposeMessage;
APPKIT_EXPORT NSSharingServiceName const NSSharingServiceNameSendViaAirDrop;
APPKIT_EXPORT NSSharingServiceName const NSSharingServiceNameAddToSafariReadingList;
APPKIT_EXPORT NSSharingServiceName const NSSharingServiceNameAddToIPhoto;
APPKIT_EXPORT NSSharingServiceName const NSSharingServiceNameAddToAperture;
APPKIT_EXPORT NSSharingServiceName const NSSharingServiceNameUseAsDesktopPicture;
APPKIT_EXPORT NSSharingServiceName const NSSharingServiceNameCloudSharing;

@interface NSSharingService : NSObject {
    NSString *_name;
    NSString *_title;
    NSImage *_image;
    NSImage *_alternateImage;
    NSString *_menuItemTitle;
    id _delegate;
    NSArray *_recipients;
    NSString *_subject;
    NSString *_messageBody;
    NSURL *_permanentLink;
    NSString *_accountName;
    NSArray *_attachmentFileURLs;
    void (^_handler)(void);
}

+ (NSSharingService *)sharingServiceNamed:(NSSharingServiceName)serviceName;
+ (NSArray<NSSharingService *> *)sharingServicesForItems:(NSArray *)items;

- (instancetype)initWithTitle:(NSString *)title image:(NSImage *)image alternateImage:(NSImage *)alternateImage
                      handler:(void (^)(void))block;

@property (assign) id<NSSharingServiceDelegate> delegate;
@property (readonly, copy) NSString *title;
@property (readonly, strong) NSImage *image;
@property (readonly, strong) NSImage *alternateImage;
@property (copy) NSString *menuItemTitle;
@property (copy) NSArray<NSString *> *recipients;
@property (copy) NSString *subject;
@property (readonly, copy) NSString *messageBody;
@property (readonly, copy) NSURL *permanentLink;
@property (readonly, copy) NSString *accountName;
@property (readonly, copy) NSArray<NSURL *> *attachmentFileURLs;

- (BOOL)canPerformWithItems:(NSArray *)items;
- (void)performWithItems:(NSArray *)items;

@end

@protocol NSSharingServiceDelegate <NSObject>
@optional
- (void)sharingService:(NSSharingService *)sharingService willShareItems:(NSArray *)items;
- (void)sharingService:(NSSharingService *)sharingService didFailToShareItems:(NSArray *)items error:(NSError *)error;
- (void)sharingService:(NSSharingService *)sharingService didShareItems:(NSArray *)items;
- (NSWindow *)sharingService:(NSSharingService *)sharingService sourceWindowForShareItems:(NSArray *)items
              sharingContentScope:(NSInteger *)sharingContentScope;
@end

@interface NSSharingServicePicker : NSObject {
    NSArray *_items;
    id _delegate;
}

- (instancetype)initWithItems:(NSArray *)items;
@property (assign) id<NSSharingServicePickerDelegate> delegate;
/* A menu of the services for the items, below the rect in view. */
- (void)showRelativeToRect:(NSRect)rect ofView:(NSView *)view preferredEdge:(NSRectEdge)preferredEdge;
- (void)close;

@end

@protocol NSSharingServicePickerDelegate <NSObject>
@optional
- (NSArray<NSSharingService *> *)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker
                              sharingServicesForItems:(NSArray *)items
                              proposedSharingServices:(NSArray<NSSharingService *> *)proposedServices;
- (id<NSSharingServiceDelegate>)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker
                           delegateForSharingService:(NSSharingService *)sharingService;
- (void)sharingServicePicker:(NSSharingServicePicker *)sharingServicePicker didChooseSharingService:(NSSharingService *)service;
@end
