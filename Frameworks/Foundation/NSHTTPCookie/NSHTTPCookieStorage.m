/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * The cookie store. Each app has its own (the shared storage), kept in
 * ~/Library/Cookies/<bundle identifier>.cookies.plist; the cookies that
 * last past the session are written there after every change. A storage
 * made for an ephemeral URL session keeps its cookies in memory only.
 */

#import <Foundation/NSHTTPCookieStorage.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSBundle.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSHTTPCookie.h>
#import <Foundation/NSNotification.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSProcessInfo.h>
#import <Foundation/NSPropertyList.h>
#import <Foundation/NSSortDescriptor.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSURLSession.h>
#include <dispatch/dispatch.h>
#include <pthread.h>

NSString *const NSHTTPCookieManagerAcceptPolicyChangedNotification = @"NSHTTPCookieManagerAcceptPolicyChangedNotification";
NSString *const NSHTTPCookieManagerCookiesChangedNotification = @"NSHTTPCookieManagerCookiesChangedNotification";

FOUNDATION_EXPORT BOOL _NSHTTPCookieDomainMatches(NSString *host, NSString *domain);

@interface NSHTTPCookieStorage ()
- (instancetype)_initWithFile:(NSString *)path;
@end

@implementation NSHTTPCookieStorage {
    pthread_mutex_t _lock;
    NSMutableArray *_cookies;
    NSHTTPCookieAcceptPolicy _policy;
    NSString *_file;        // nil: in memory only
    BOOL _loaded;
}

static NSString *storeName(void) {
    NSString *identifier = [[NSBundle mainBundle] bundleIdentifier];
    return [identifier length] > 0 ? identifier : [[NSProcessInfo processInfo] processName];
}

+ (NSHTTPCookieStorage *)sharedHTTPCookieStorage {
    static NSHTTPCookieStorage *shared;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSString *directory = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Cookies"];
        NSString *file = [directory stringByAppendingPathComponent:
                                        [storeName() stringByAppendingString:@".cookies.plist"]];
        shared = [[NSHTTPCookieStorage alloc] _initWithFile:file];
    });
    return shared;
}

+ (NSHTTPCookieStorage *)sharedCookieStorageForGroupContainerIdentifier:(NSString *)identifier {
    static NSMutableDictionary *storages;
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    if ([identifier length] == 0)
        return [self sharedHTTPCookieStorage];
    pthread_mutex_lock(&lock);
    if (storages == nil)
        storages = [NSMutableDictionary dictionary];
    NSHTTPCookieStorage *storage = storages[identifier];
    if (storage == nil) {
        NSString *file = [[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Cookies"]
                             stringByAppendingPathComponent:[identifier stringByAppendingString:@".cookies.plist"]];
        storage = [[NSHTTPCookieStorage alloc] _initWithFile:file];
        storages[identifier] = storage;
    }
    pthread_mutex_unlock(&lock);
    return storage;
}

- (instancetype)init {
    return [self _initWithFile:nil];
}

- (instancetype)_initWithFile:(NSString *)path {
    if ((self = [super init]) == nil)
        return nil;
    pthread_mutex_init(&_lock, NULL);
    _cookies = [NSMutableArray array];
    _policy = NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
    _file = [path copy];
    return self;
}

#pragma mark Disk

// Under the lock.
- (void)_load {
    if (_loaded)
        return;
    _loaded = YES;
    if (_file == nil)
        return;
    NSData *data = [NSData dataWithContentsOfFile:_file];
    NSArray *list = data ? [NSPropertyListSerialization propertyListWithData:data options:0 format:NULL error:NULL] : nil;
    if (![list isKindOfClass:[NSArray class]])
        return;
    NSDate *now = [NSDate date];
    for (NSDictionary *properties in list) {
        if (![properties isKindOfClass:[NSDictionary class]])
            continue;
        NSHTTPCookie *cookie = [NSHTTPCookie cookieWithProperties:properties];
        if (cookie != nil && [[cookie expiresDate] compare:now] == NSOrderedDescending)
            [_cookies addObject:cookie];
    }
}

// Under the lock: the cookies that outlive the session.
- (void)_save {
    if (_file == nil)
        return;
    NSMutableArray *list = [NSMutableArray array];
    for (NSHTTPCookie *cookie in _cookies) {
        if ([cookie isSessionOnly])
            continue;
        NSMutableDictionary *properties = [NSMutableDictionary dictionary];
        for (NSString *key in [cookie properties]) {
            id value = [cookie properties][key];
            if ([value isKindOfClass:[NSURL class]])
                value = [value absoluteString];
            if ([value isKindOfClass:[NSString class]] || [value isKindOfClass:[NSDate class]] ||
                [value isKindOfClass:[NSNumber class]])
                properties[key] = value;
        }
        [list addObject:properties];
    }
    NSData *data = [NSPropertyListSerialization dataWithPropertyList:list format:NSPropertyListBinaryFormat_v1_0
                                                             options:0 error:NULL];
    if (data == nil)
        return;
    [[NSFileManager defaultManager] createDirectoryAtPath:[_file stringByDeletingLastPathComponent]
                              withIntermediateDirectories:YES attributes:nil error:NULL];
    [data writeToFile:_file atomically:YES];
}

- (void)_changed {
    [[NSNotificationCenter defaultCenter] postNotificationName:NSHTTPCookieManagerCookiesChangedNotification
                                                        object:self];
}

#pragma mark Cookies

- (NSHTTPCookieAcceptPolicy)cookieAcceptPolicy {
    pthread_mutex_lock(&_lock);
    NSHTTPCookieAcceptPolicy policy = _policy;
    pthread_mutex_unlock(&_lock);
    return policy;
}

- (void)setCookieAcceptPolicy:(NSHTTPCookieAcceptPolicy)policy {
    pthread_mutex_lock(&_lock);
    _policy = policy;
    pthread_mutex_unlock(&_lock);
    [[NSNotificationCenter defaultCenter] postNotificationName:NSHTTPCookieManagerAcceptPolicyChangedNotification
                                                        object:self];
}

// Under the lock; YES if something changed.
- (BOOL)_storeCookie:(NSHTTPCookie *)cookie {
    BOOL changed = NO;
    for (NSUInteger i = [_cookies count]; i > 0; i--) {
        NSHTTPCookie *old = _cookies[i - 1];
        if ([[old name] isEqualToString:[cookie name]] && [[old domain] isEqualToString:[cookie domain]] &&
            [[old path] isEqualToString:[cookie path]]) {
            [_cookies removeObjectAtIndex:i - 1];
            changed = YES;
        }
    }
    // An expired cookie only removes the one it replaces.
    NSDate *expires = [cookie expiresDate];
    if (expires == nil || [expires timeIntervalSinceNow] > 0) {
        [_cookies addObject:cookie];
        changed = YES;
    }
    return changed;
}

- (void)setCookie:(NSHTTPCookie *)cookie {
    if (cookie == nil)
        return;
    pthread_mutex_lock(&_lock);
    [self _load];
    BOOL changed = _policy != NSHTTPCookieAcceptPolicyNever && [self _storeCookie:cookie];
    if (changed)
        [self _save];
    pthread_mutex_unlock(&_lock);
    if (changed)
        [self _changed];
}

- (void)deleteCookie:(NSHTTPCookie *)cookie {
    pthread_mutex_lock(&_lock);
    [self _load];
    NSUInteger before = [_cookies count];
    for (NSUInteger i = [_cookies count]; i > 0; i--) {
        NSHTTPCookie *old = _cookies[i - 1];
        if ([[old name] isEqualToString:[cookie name]] && [[old domain] isEqualToString:[cookie domain]] &&
            [[old path] isEqualToString:[cookie path]])
            [_cookies removeObjectAtIndex:i - 1];
    }
    BOOL changed = [_cookies count] != before;
    if (changed)
        [self _save];
    pthread_mutex_unlock(&_lock);
    if (changed)
        [self _changed];
}

// Cookies remember when they were stored only as long as the process runs:
// this removes every cookie that has no earlier creation date known, that
// is, all of them.
- (void)removeCookiesSinceDate:(NSDate *)date {
    pthread_mutex_lock(&_lock);
    [self _load];
    BOOL changed = [_cookies count] > 0 && [date timeIntervalSinceNow] <= 0;
    if (changed) {
        [_cookies removeAllObjects];
        [self _save];
    }
    pthread_mutex_unlock(&_lock);
    if (changed)
        [self _changed];
}

- (NSArray *)cookies {
    pthread_mutex_lock(&_lock);
    [self _load];
    NSMutableArray *result = [NSMutableArray array];
    for (NSHTTPCookie *cookie in _cookies)
        if ([cookie expiresDate] == nil || [[cookie expiresDate] timeIntervalSinceNow] > 0)
            [result addObject:cookie];
    pthread_mutex_unlock(&_lock);
    return result;
}

static BOOL pathMatches(NSString *requestPath, NSString *cookiePath) {
    if ([requestPath length] == 0)
        requestPath = @"/";
    if ([requestPath isEqualToString:cookiePath])
        return YES;
    if (![requestPath hasPrefix:cookiePath])
        return NO;
    return [cookiePath hasSuffix:@"/"] || [requestPath characterAtIndex:[cookiePath length]] == '/';
}

// The cookies to send to a URL, the longest paths first.
- (NSArray *)cookiesForURL:(NSURL *)URL {
    NSString *host = [URL host];
    if (host == nil)
        return @[];
    BOOL secure = [[[URL scheme] lowercaseString] isEqualToString:@"https"];
    NSMutableArray *result = [NSMutableArray array];
    for (NSHTTPCookie *cookie in [self cookies]) {
        if ([cookie isSecure] && !secure)
            continue;
        if (!_NSHTTPCookieDomainMatches(host, [cookie domain]))
            continue;
        if (!pathMatches([URL path], [cookie path]))
            continue;
        [result addObject:cookie];
    }
    [result sortUsingComparator:^NSComparisonResult(NSHTTPCookie *a, NSHTTPCookie *b) {
        NSUInteger la = [[a path] length], lb = [[b path] length];
        return la > lb ? NSOrderedAscending : la < lb ? NSOrderedDescending : NSOrderedSame;
    }];
    return result;
}

// The last two labels of a host name: close enough to its registered
// domain for the main-document policy.
static NSString *siteOf(NSString *host) {
    NSArray *labels = [[host lowercaseString] componentsSeparatedByString:@"."];
    if ([labels count] <= 2)
        return [host lowercaseString];
    return [[labels subarrayWithRange:NSMakeRange([labels count] - 2, 2)] componentsJoinedByString:@"."];
}

- (void)_setCookies:(NSArray *)cookies forURL:(NSURL *)URL mainDocumentURL:(NSURL *)mainDocumentURL
             policy:(NSHTTPCookieAcceptPolicy)policy {
    if (policy == NSHTTPCookieAcceptPolicyNever || [cookies count] == 0)
        return;
    NSString *site = mainDocumentURL ? siteOf([mainDocumentURL host]) : nil;
    pthread_mutex_lock(&_lock);
    [self _load];
    BOOL changed = NO;
    for (NSHTTPCookie *cookie in cookies) {
        if (policy == NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain && site != nil &&
            !_NSHTTPCookieDomainMatches([mainDocumentURL host], [cookie domain]) &&
            ![siteOf([[cookie domain] hasPrefix:@"."] ? [[cookie domain] substringFromIndex:1] : [cookie domain]) isEqualToString:site])
            continue;
        changed |= [self _storeCookie:cookie];
    }
    if (changed)
        [self _save];
    pthread_mutex_unlock(&_lock);
    if (changed)
        [self _changed];
}

- (void)setCookies:(NSArray *)cookies forURL:(NSURL *)URL mainDocumentURL:(NSURL *)mainDocumentURL {
    [self _setCookies:cookies forURL:URL mainDocumentURL:mainDocumentURL policy:[self cookieAcceptPolicy]];
}

- (NSArray *)sortedCookiesUsingDescriptors:(NSArray *)sortOrder {
    return [[self cookies] sortedArrayUsingDescriptors:sortOrder];
}

- (void)storeCookies:(NSArray *)cookies forTask:(NSURLSessionTask *)task {
    NSURLRequest *request = [task currentRequest];
    [self setCookies:cookies forURL:[request URL] mainDocumentURL:[request mainDocumentURL]];
}

- (void)getCookiesForTask:(NSURLSessionTask *)task completionHandler:(void (^)(NSArray<NSHTTPCookie *> *))completionHandler {
    NSArray *cookies = [self cookiesForURL:[[task currentRequest] URL]];
    if (completionHandler)
        completionHandler(cookies);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<NSHTTPCookieStorage %p cookies count:%lu>", (__bridge void *)self,
                                      (unsigned long)[[self cookies] count]];
}

@end
