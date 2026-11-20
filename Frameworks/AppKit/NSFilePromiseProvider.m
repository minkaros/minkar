// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <AppKit/NSFilePromiseProvider.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>

/* The pasteboard types of a file promise. */
static NSString *const filePromiseType = @"com.apple.pasteboard.promised-file-url";
static NSString *const filePromiseContentType = @"com.apple.pasteboard.promised-file-content-type";

@implementation NSFilePromiseProvider {
    NSString *_fileType;
    id _delegate;
    id _userInfo;
}

/* A delegate that cannot write the file is refused, as on the
   reference. */
- (instancetype)initWithFileType:(NSString *)fileType delegate:(id<NSFilePromiseProviderDelegate>)delegate {
    if (![delegate respondsToSelector:@selector(filePromiseProvider:writePromiseToURL:completionHandler:)]) {
        [self release];
        [NSException raise:NSInvalidArgumentException format:@"NSFilePromiseProviderDelegate is missing a -filePromiseProvider:writePromiseToURL:completionHandler: method"];
        return nil;
    }
    if ((self = [super init]) != nil) {
        _fileType = [fileType copy];
        _delegate = delegate;
    }
    return self;
}

- (instancetype)init {
    return [super init];
}

- (void)dealloc {
    [_fileType release];
    [_userInfo release];
    [super dealloc];
}

- (NSString *)fileType {
    return _fileType;
}

- (void)setFileType:(NSString *)fileType {
    fileType = [fileType copy];
    [_fileType release];
    _fileType = fileType;
}

- (id<NSFilePromiseProviderDelegate>)delegate {
    return _delegate;
}

- (void)setDelegate:(id<NSFilePromiseProviderDelegate>)delegate {
    _delegate = delegate;
}

- (id)userInfo {
    return _userInfo;
}

- (void)setUserInfo:(id)userInfo {
    [userInfo retain];
    [_userInfo release];
    _userInfo = userInfo;
}

/* Promised only on a drag's pasteboard, as on the reference (none on the
   general one). */
- (NSArray<NSPasteboardType> *)writableTypesForPasteboard:(NSPasteboard *)pasteboard {
    if (_fileType == nil || ![[pasteboard name] isEqualToString:NSPasteboardNameDrag])
        return [NSArray array];
    return [NSArray arrayWithObjects:filePromiseType, filePromiseContentType, nil];
}

- (NSPasteboardWritingOptions)writingOptionsForType:(NSPasteboardType)type pasteboard:(NSPasteboard *)pasteboard {
    return [type isEqualToString:filePromiseType] ? NSPasteboardWritingPromised : 0;
}

- (id)pasteboardPropertyListForType:(NSPasteboardType)type {
    if ([type isEqualToString:filePromiseContentType])
        return _fileType;
    return nil;
}

@end
