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

#import <Foundation/NSObject.h>

#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>

@class NSDate, NSNumber, NSString, NSURL;

FOUNDATION_EXPORT NSString *const NSHTTPCookieName;
FOUNDATION_EXPORT NSString *const NSHTTPCookieValue;
FOUNDATION_EXPORT NSString *const NSHTTPCookieOriginURL;
FOUNDATION_EXPORT NSString *const NSHTTPCookieVersion;
FOUNDATION_EXPORT NSString *const NSHTTPCookieDomain;
FOUNDATION_EXPORT NSString *const NSHTTPCookiePath;
FOUNDATION_EXPORT NSString *const NSHTTPCookieSecure;
FOUNDATION_EXPORT NSString *const NSHTTPCookieExpires;
FOUNDATION_EXPORT NSString *const NSHTTPCookieComment;
FOUNDATION_EXPORT NSString *const NSHTTPCookieCommentURL;
FOUNDATION_EXPORT NSString *const NSHTTPCookieDiscard;
FOUNDATION_EXPORT NSString *const NSHTTPCookieMaximumAge;
FOUNDATION_EXPORT NSString *const NSHTTPCookiePort;
FOUNDATION_EXPORT NSString *const NSHTTPCookieSameSitePolicy;

typedef NSString *NSHTTPCookieStringPolicy;
FOUNDATION_EXPORT NSHTTPCookieStringPolicy const NSHTTPCookieSameSiteLax;
FOUNDATION_EXPORT NSHTTPCookieStringPolicy const NSHTTPCookieSameSiteStrict;

@interface NSHTTPCookie : NSObject <NSCopying>

+ (NSArray<NSHTTPCookie *> *)cookiesWithResponseHeaderFields:(NSDictionary<NSString *, NSString *> *)headerFields
                                                       forURL:(NSURL *)URL;
+ (NSDictionary<NSString *, NSString *> *)requestHeaderFieldsWithCookies:(NSArray<NSHTTPCookie *> *)cookies;

+ (instancetype)cookieWithProperties:(NSDictionary<NSString *, id> *)properties;
- (instancetype)initWithProperties:(NSDictionary<NSString *, id> *)properties;

@property (readonly, copy) NSDictionary<NSString *, id> *properties;
@property (readonly) NSUInteger version;
@property (readonly, copy) NSString *name;
@property (readonly, copy) NSString *value;
@property (readonly, copy) NSDate *expiresDate;
@property (readonly, getter=isSessionOnly) BOOL sessionOnly;
@property (readonly, copy) NSString *domain;
@property (readonly, copy) NSString *path;
@property (readonly, getter=isSecure) BOOL secure;
@property (readonly, getter=isHTTPOnly) BOOL HTTPOnly;
@property (readonly, copy) NSString *comment;
@property (readonly, copy) NSURL *commentURL;
@property (readonly, copy) NSArray<NSNumber *> *portList;
@property (readonly, copy) NSHTTPCookieStringPolicy sameSitePolicy;

@end
