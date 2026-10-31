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
#import <Foundation/NSRange.h>

@class NSNumber, NSString, NSURL;

@interface NSURLQueryItem : NSObject <NSCopying, NSSecureCoding>
- (instancetype)initWithName:(NSString *)name value:(NSString *)value;
+ (instancetype)queryItemWithName:(NSString *)name value:(NSString *)value;
@property (readonly) NSString *name;
@property (readonly) NSString *value;
@end

@interface NSURLComponents : NSObject <NSCopying>

- (instancetype)init;
- (instancetype)initWithURL:(NSURL *)url resolvingAgainstBaseURL:(BOOL)resolve;
+ (instancetype)componentsWithURL:(NSURL *)url resolvingAgainstBaseURL:(BOOL)resolve;
- (instancetype)initWithString:(NSString *)URLString;
+ (instancetype)componentsWithString:(NSString *)URLString;

@property (readonly, copy) NSURL *URL;
- (NSURL *)URLRelativeToURL:(NSURL *)baseURL;
@property (readonly, copy) NSString *string;

@property (copy) NSString *scheme;
@property (copy) NSString *user;
@property (copy) NSString *password;
@property (copy) NSString *host;
@property (copy) NSNumber *port;
@property (copy) NSString *path;
@property (copy) NSString *query;
@property (copy) NSString *fragment;

@property (copy) NSString *percentEncodedUser;
@property (copy) NSString *percentEncodedPassword;
@property (copy) NSString *percentEncodedHost;
@property (copy) NSString *percentEncodedPath;
@property (copy) NSString *percentEncodedQuery;
@property (copy) NSString *percentEncodedFragment;

@property (readonly) NSRange rangeOfScheme;
@property (readonly) NSRange rangeOfUser;
@property (readonly) NSRange rangeOfPassword;
@property (readonly) NSRange rangeOfHost;
@property (readonly) NSRange rangeOfPort;
@property (readonly) NSRange rangeOfPath;
@property (readonly) NSRange rangeOfQuery;
@property (readonly) NSRange rangeOfFragment;

@property (copy) NSArray<NSURLQueryItem *> *queryItems;
@property (copy) NSArray<NSURLQueryItem *> *percentEncodedQueryItems;

@end
