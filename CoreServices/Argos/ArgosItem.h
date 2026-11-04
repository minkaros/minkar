/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

/* One entry of a folder, as plain data. */
@interface ArgosItem : NSObject
@property (nonatomic, readonly, copy) NSURL *URL;
@property (nonatomic, readonly, copy) NSString *name;
@property (nonatomic, readonly) BOOL isFolder;       /* a folder browsed into */
@property (nonatomic, readonly) BOOL isApplication;  /* an .app bundle */
@property (nonatomic, readonly) unsigned long long size;
@property (nonatomic, readonly, nullable, copy) NSDate *modified;
@property (nonatomic, readonly, copy) NSString *kind;
@property (nonatomic, readonly) NSImage *icon;

- (instancetype)initWithURL:(NSURL *)url;
/* A folder's entries, folders first then by name; hidden ones left out. */
+ (NSArray<ArgosItem *> *)itemsInFolder:(NSURL *)folder error:(NSError **)error;
@end

/* The locale of the language Argos's texts are shown in (dates). */
NSLocale *ArgosDisplayLocale(void);

/* "12 KB", "1.4 MB": sizes in the reference platform's style (1000-based). */
NSString *ArgosSizeString(unsigned long long size);

NS_ASSUME_NONNULL_END
