/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Item providers (API 19): representations registered by type identifier,
 * each a handler that loads the data when asked. Types match exactly (and
 * public.data matches all). ARC.
 */

#import <Foundation/NSItemProvider.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSMutableArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSData.h>
#import <Foundation/NSError.h>
#import <Foundation/NSProgress.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSUUID.h>
#import <Foundation/NSPathUtilities.h>
#include <dispatch/dispatch.h>

NSString * const NSItemProviderPreferredImageSizeKey = @"NSItemProviderPreferredImageSizeKey";
NSString * const NSItemProviderErrorDomain = @"NSItemProviderErrorDomain";

typedef NSProgress *(^DataLoader)(void (^)(NSData *, NSError *));

@interface _NSItemRepresentation : NSObject
@property (copy) NSString *type;
@property (copy) DataLoader data;
@property (copy) NSProgress *(^file)(void (^)(NSURL *, BOOL, NSError *));
@property (strong) id item;
@end

@implementation _NSItemRepresentation
@end

static NSError *unavailable(NSString *type) {
    return [NSError errorWithDomain:NSItemProviderErrorDomain code:NSItemProviderItemUnavailableError
        userInfo:@{ NSLocalizedDescriptionKey: [NSString stringWithFormat:@"No item of type %@", type] }];
}

static BOOL conforms(NSString *type, NSString *wanted) {
    return [type isEqualToString:wanted] || [wanted isEqualToString:@"public.data"] || [wanted isEqualToString:@"public.item"];
}

@implementation NSItemProvider

- (instancetype)init {
    return [self initWithItem:nil typeIdentifier:nil];
}

- (instancetype)initWithItem:(id<NSSecureCoding>)item typeIdentifier:(NSString *)typeIdentifier {
    if ((self = [super init]) == nil)
        return nil;
    _representations = [NSMutableArray array];
    if (item != nil && typeIdentifier != nil) {
        _NSItemRepresentation *r = [_NSItemRepresentation new];
        r.type = typeIdentifier;
        r.item = item;
        [_representations addObject:r];
    }
    return self;
}

- (instancetype)initWithContentsOfURL:(NSURL *)fileURL {
    NSData *data = [NSData dataWithContentsOfURL:fileURL];
    if (data == nil)
        return nil;
    self = [self initWithItem:nil typeIdentifier:nil];
    NSString *type = [[fileURL pathExtension] length] > 0 ? [@"public." stringByAppendingString:[fileURL pathExtension]] : @"public.data";
    [self registerDataRepresentationForTypeIdentifier:type visibility:NSItemProviderRepresentationVisibilityAll
        loadHandler:^NSProgress *(void (^done)(NSData *, NSError *)) { done(data, nil); return nil; }];
    self.suggestedName = [[fileURL lastPathComponent] stringByDeletingPathExtension];
    return self;
}

- (instancetype)initWithObject:(id<NSItemProviderWriting>)object {
    if ((self = [self initWithItem:nil typeIdentifier:nil]) == nil)
        return nil;
    [self registerObject:object visibility:NSItemProviderRepresentationVisibilityAll];
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    NSItemProvider *copy = [[NSItemProvider alloc] init];
    [copy->_representations addObjectsFromArray:_representations];
    copy.suggestedName = self.suggestedName;
    return copy;
}

- (void)registerDataRepresentationForTypeIdentifier:(NSString *)typeIdentifier visibility:(NSItemProviderRepresentationVisibility)visibility loadHandler:(NSProgress *(^)(void (^)(NSData *, NSError *)))loadHandler {
    _NSItemRepresentation *r = [_NSItemRepresentation new];
    r.type = typeIdentifier;
    r.data = loadHandler;
    @synchronized(self) { [_representations addObject:r]; }
}

- (void)registerFileRepresentationForTypeIdentifier:(NSString *)typeIdentifier fileOptions:(NSItemProviderFileOptions)fileOptions visibility:(NSItemProviderRepresentationVisibility)visibility loadHandler:(NSProgress *(^)(void (^)(NSURL *, BOOL, NSError *)))loadHandler {
    _NSItemRepresentation *r = [_NSItemRepresentation new];
    r.type = typeIdentifier;
    r.file = loadHandler;
    @synchronized(self) { [_representations addObject:r]; }
}

- (NSArray *)registeredTypeIdentifiers {
    NSMutableArray *types = [NSMutableArray array];
    @synchronized(self) {
        for (_NSItemRepresentation *r in _representations)
            if (![types containsObject:r.type])
                [types addObject:r.type];
    }
    return types;
}

- (NSArray *)registeredTypeIdentifiersWithFileOptions:(NSItemProviderFileOptions)fileOptions {
    return [self registeredTypeIdentifiers];
}

- (_NSItemRepresentation *)_representationFor:(NSString *)type {
    @synchronized(self) {
        for (_NSItemRepresentation *r in _representations)
            if (conforms(r.type, type))
                return r;
    }
    return nil;
}

- (BOOL)hasItemConformingToTypeIdentifier:(NSString *)typeIdentifier {
    return [self _representationFor:typeIdentifier] != nil;
}

- (BOOL)hasRepresentationConformingToTypeIdentifier:(NSString *)typeIdentifier fileOptions:(NSItemProviderFileOptions)fileOptions {
    return [self hasItemConformingToTypeIdentifier:typeIdentifier];
}

/* The data of a representation, however it was registered. */
- (NSProgress *)loadDataRepresentationForTypeIdentifier:(NSString *)typeIdentifier completionHandler:(void (^)(NSData *, NSError *))completionHandler {
    NSProgress *progress = [NSProgress discreteProgressWithTotalUnitCount:1];
    _NSItemRepresentation *r = [self _representationFor:typeIdentifier];
    void (^finish)(NSData *, NSError *) = ^(NSData *data, NSError *error) {
        progress.completedUnitCount = 1;
        completionHandler(data, data == nil && error == nil ? unavailable(typeIdentifier) : error);
    };
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        if (r == nil)
            finish(nil, unavailable(typeIdentifier));
        else if (r.data != nil)
            r.data(finish);
        else if (r.file != nil)
            r.file(^(NSURL *url, BOOL coordinated, NSError *error) { finish(url ? [NSData dataWithContentsOfURL:url] : nil, error); });
        else if ([r.item isKindOfClass:[NSData class]])
            finish(r.item, nil);
        else if ([r.item isKindOfClass:[NSString class]])
            finish([r.item dataUsingEncoding:NSUTF8StringEncoding], nil);
        else if ([r.item isKindOfClass:[NSURL class]] && [r.item isFileURL])
            finish([NSData dataWithContentsOfURL:r.item], nil);
        else
            finish(nil, unavailable(typeIdentifier));
    });
    return progress;
}

/* A file: the representation's own, else its data in a temporary file. */
- (NSProgress *)loadFileRepresentationForTypeIdentifier:(NSString *)typeIdentifier completionHandler:(void (^)(NSURL *, NSError *))completionHandler {
    _NSItemRepresentation *r = [self _representationFor:typeIdentifier];
    if (r.file != nil) {
        NSProgress *progress = [NSProgress discreteProgressWithTotalUnitCount:1];
        dispatch_async(dispatch_get_global_queue(0, 0), ^{
            r.file(^(NSURL *url, BOOL coordinated, NSError *error) {
                progress.completedUnitCount = 1;
                completionHandler(url, error);
            });
        });
        return progress;
    }
    return [self loadDataRepresentationForTypeIdentifier:typeIdentifier completionHandler:^(NSData *data, NSError *error) {
        if (data == nil) {
            completionHandler(nil, error);
            return;
        }
        NSString *name = [NSString stringWithFormat:@"%@-%@", self.suggestedName ?: @"item", [[NSUUID UUID] UUIDString]];
        NSURL *url = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:name]];
        [data writeToURL:url atomically:YES];
        completionHandler(url, nil);
        [[NSFileManager defaultManager] removeItemAtURL:url error:NULL];
    }];
}

- (NSProgress *)loadInPlaceFileRepresentationForTypeIdentifier:(NSString *)typeIdentifier completionHandler:(void (^)(NSURL *, BOOL, NSError *))completionHandler {
    return [self loadFileRepresentationForTypeIdentifier:typeIdentifier completionHandler:^(NSURL *url, NSError *error) {
        completionHandler(url, NO, error);
    }];
}

- (void)registerObject:(id<NSItemProviderWriting>)object visibility:(NSItemProviderRepresentationVisibility)visibility {
    NSArray *types = [object respondsToSelector:@selector(writableTypeIdentifiersForItemProvider)] ?
        [(id)object writableTypeIdentifiersForItemProvider] : [[object class] writableTypeIdentifiersForItemProvider];
    for (NSString *type in types)
        [self registerDataRepresentationForTypeIdentifier:type visibility:visibility loadHandler:^NSProgress *(void (^done)(NSData *, NSError *)) {
            return [object loadDataWithTypeIdentifier:type forItemProviderCompletionHandler:done];
        }];
}

- (void)registerObjectOfClass:(Class<NSItemProviderWriting>)aClass visibility:(NSItemProviderRepresentationVisibility)visibility loadHandler:(NSProgress *(^)(void (^)(id<NSItemProviderWriting>, NSError *)))loadHandler {
    for (NSString *type in [aClass writableTypeIdentifiersForItemProvider])
        [self registerDataRepresentationForTypeIdentifier:type visibility:visibility loadHandler:^NSProgress *(void (^done)(NSData *, NSError *)) {
            return loadHandler(^(id<NSItemProviderWriting> object, NSError *error) {
                if (object == nil)
                    done(nil, error);
                else
                    [object loadDataWithTypeIdentifier:type forItemProviderCompletionHandler:done];
            });
        }];
}

- (BOOL)canLoadObjectOfClass:(Class<NSItemProviderReading>)aClass {
    for (NSString *type in [aClass readableTypeIdentifiersForItemProvider])
        if ([self hasItemConformingToTypeIdentifier:type])
            return YES;
    return NO;
}

- (NSProgress *)loadObjectOfClass:(Class<NSItemProviderReading>)aClass completionHandler:(void (^)(id<NSItemProviderReading>, NSError *))completionHandler {
    for (NSString *type in [aClass readableTypeIdentifiersForItemProvider])
        if ([self hasItemConformingToTypeIdentifier:type])
            return [self loadDataRepresentationForTypeIdentifier:type completionHandler:^(NSData *data, NSError *error) {
                NSError *made = nil;
                id object = data ? [aClass objectWithItemProviderData:data typeIdentifier:type error:&made] : nil;
                completionHandler(object, object ? nil : (error ?: made));
            }];
    NSProgress *progress = [NSProgress discreteProgressWithTotalUnitCount:1];
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        completionHandler(nil, [NSError errorWithDomain:NSItemProviderErrorDomain code:NSItemProviderUnavailableCoercionError userInfo:nil]);
    });
    return progress;
}

- (void)registerItemForTypeIdentifier:(NSString *)typeIdentifier loadHandler:(NSItemProviderLoadHandler)loadHandler {
    [self registerDataRepresentationForTypeIdentifier:typeIdentifier visibility:NSItemProviderRepresentationVisibilityAll
        loadHandler:^NSProgress *(void (^done)(NSData *, NSError *)) {
            loadHandler(^(id item, NSError *error) {
                if ([(id)item isKindOfClass:[NSData class]])
                    done((NSData *)item, error);
                else if ([(id)item isKindOfClass:[NSString class]])
                    done([(NSString *)item dataUsingEncoding:NSUTF8StringEncoding], error);
                else
                    done(nil, error ?: unavailable(typeIdentifier));
            }, [NSData class], nil);
            return nil;
        }];
}

- (void)loadItemForTypeIdentifier:(NSString *)typeIdentifier options:(NSDictionary *)options completionHandler:(NSItemProviderCompletionHandler)completionHandler {
    _NSItemRepresentation *r = [self _representationFor:typeIdentifier];
    if (r.item != nil) {
        id item = r.item;
        if (completionHandler != nil)
            dispatch_async(dispatch_get_global_queue(0, 0), ^{ completionHandler(item, nil); });
        return;
    }
    [self loadDataRepresentationForTypeIdentifier:typeIdentifier completionHandler:^(NSData *data, NSError *error) {
        if (completionHandler != nil)
            completionHandler(data, error);
    }];
}

@end
