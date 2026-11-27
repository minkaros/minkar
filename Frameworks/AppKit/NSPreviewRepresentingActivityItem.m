/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Share items with a preview (NSPreviewRepresentingActivityItem.h). The
 * sharing services share the item inside (NSSharingService.m).
 */

#import <AppKit/NSPreviewRepresentingActivityItem.h>
#import <AppKit/NSImage.h>

@implementation NSPreviewRepresentingActivityItem

/* An image as a provider of it, as TIFF data. */
static NSItemProvider *providerOfImage(NSImage *image) {
    if (image == nil)
        return nil;
    NSItemProvider *provider = [[[NSItemProvider alloc] init] autorelease];
    [provider registerDataRepresentationForTypeIdentifier:@"public.tiff"
                                               visibility:NSItemProviderRepresentationVisibilityAll
                                              loadHandler:^NSProgress *(void (^completion)(NSData *, NSError *)) {
        completion([image TIFFRepresentation], nil);
        return nil;
    }];
    return provider;
}

- (instancetype)initWithItem:(id)item title:(NSString *)title image:(NSImage *)image icon:(NSImage *)icon {
    return [self initWithItem:item title:title imageProvider:providerOfImage(image) iconProvider:providerOfImage(icon)];
}

- (instancetype)initWithItem:(id)item title:(NSString *)title imageProvider:(NSItemProvider *)imageProvider
                iconProvider:(NSItemProvider *)iconProvider {
    if ((self = [super init]) != nil) {
        _item = [item retain];
        _title = [title copy];
        _imageProvider = [imageProvider retain];
        _iconProvider = [iconProvider retain];
    }
    return self;
}

- (void)dealloc {
    [_item release];
    [_title release];
    [_imageProvider release];
    [_iconProvider release];
    [super dealloc];
}

- (id)item { return _item; }
- (NSString *)title { return _title; }
- (NSItemProvider *)imageProvider { return _imageProvider; }
- (NSItemProvider *)iconProvider { return _iconProvider; }

@end
