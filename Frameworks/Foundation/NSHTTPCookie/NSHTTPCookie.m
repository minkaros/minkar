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
 * HTTP cookies (RFC 6265): made from properties or from a response's
 * Set-Cookie headers, and turned into a request's Cookie header.
 */

#import <Foundation/NSHTTPCookie.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSCharacterSet.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#include <string.h>
#include <time.h>
#include <xlocale.h>

NSString *const NSHTTPCookieName = @"Name";
NSString *const NSHTTPCookieValue = @"Value";
NSString *const NSHTTPCookieOriginURL = @"OriginURL";
NSString *const NSHTTPCookieVersion = @"Version";
NSString *const NSHTTPCookieDomain = @"Domain";
NSString *const NSHTTPCookiePath = @"Path";
NSString *const NSHTTPCookieSecure = @"Secure";
NSString *const NSHTTPCookieExpires = @"Expires";
NSString *const NSHTTPCookieComment = @"Comment";
NSString *const NSHTTPCookieCommentURL = @"CommentURL";
NSString *const NSHTTPCookieDiscard = @"Discard";
NSString *const NSHTTPCookieMaximumAge = @"Max-Age";
NSString *const NSHTTPCookiePort = @"Port";
NSString *const NSHTTPCookieSameSitePolicy = @"SameSite";
NSHTTPCookieStringPolicy const NSHTTPCookieSameSiteLax = @"lax";
NSHTTPCookieStringPolicy const NSHTTPCookieSameSiteStrict = @"strict";

// The HttpOnly attribute, kept in the properties under this key.
static NSString *const HTTPOnlyKey = @"HttpOnly";

static NSString *trim(NSString *s) {
    return [s stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
}

static BOOL flagValue(id value) {
    if (value == nil)
        return NO;
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    NSString *s = [[value description] uppercaseString];
    return !([s isEqualToString:@"FALSE"] || [s isEqualToString:@"NO"] || [s isEqualToString:@"0"]);
}

// The cookie date formats in use (RFC 1123, RFC 850, asctime, and the
// Netscape draft with dashes).
static NSDate *parseCookieDate(NSString *string) {
    static const char *formats[] = {
        "%a, %d %b %Y %H:%M:%S", "%a, %d-%b-%Y %H:%M:%S", "%A, %d-%b-%y %H:%M:%S",
        "%a, %d %b %y %H:%M:%S", "%a %b %d %H:%M:%S %Y", "%a, %d-%b-%y %H:%M:%S",
    };
    const char *text = [trim(string) UTF8String];
    for (size_t i = 0; text != NULL && i < sizeof(formats) / sizeof(formats[0]); i++) {
        struct tm tm;
        memset(&tm, 0, sizeof(tm));
        if (strptime_l(text, formats[i], &tm, NULL) != NULL) {
            if (tm.tm_year < 70)
                tm.tm_year += 100;
            return [NSDate dateWithTimeIntervalSince1970:(NSTimeInterval)timegm(&tm)];
        }
    }
    return nil;
}

// The default path of a cookie set by a URL: the directory of its path.
static NSString *defaultPath(NSURL *url) {
    NSString *path = [url path];
    if ([path length] == 0 || ![path hasPrefix:@"/"])
        return @"/";
    NSRange slash = [path rangeOfString:@"/" options:NSBackwardsSearch];
    return slash.location == 0 ? @"/" : [path substringToIndex:slash.location];
}

// host domain-matches domain (".example.com" or "example.com").
FOUNDATION_EXPORT BOOL _NSHTTPCookieDomainMatches(NSString *host, NSString *domain);
BOOL _NSHTTPCookieDomainMatches(NSString *host, NSString *domain) {
    host = [host lowercaseString];
    domain = [domain lowercaseString];
    NSString *bare = [domain hasPrefix:@"."] ? [domain substringFromIndex:1] : domain;
    if ([host isEqualToString:bare])
        return YES;
    return [domain hasPrefix:@"."] && [host hasSuffix:domain];
}

@implementation NSHTTPCookie {
    NSDictionary *_properties;
    NSString *_name, *_value, *_domain, *_path, *_comment;
    NSDate *_expiresDate;
    NSURL *_commentURL;
    NSArray *_portList;
    NSUInteger _version;
    BOOL _secure, _HTTPOnly, _sessionOnly;
    NSHTTPCookieStringPolicy _sameSitePolicy;
}

+ (instancetype)cookieWithProperties:(NSDictionary *)properties {
    return [[self alloc] initWithProperties:properties];
}

// nil without a name, a value, and a domain (or an origin URL).
- (instancetype)initWithProperties:(NSDictionary *)properties {
    if ((self = [super init]) == nil)
        return nil;
    id name = properties[NSHTTPCookieName], value = properties[NSHTTPCookieValue];
    if (![name isKindOfClass:[NSString class]] || [name length] == 0 || value == nil)
        return nil;
    id origin = properties[NSHTTPCookieOriginURL];
    NSURL *originURL = [origin isKindOfClass:[NSURL class]] ? origin
                     : [origin isKindOfClass:[NSString class]] ? [NSURL URLWithString:origin] : nil;
    NSString *domain = properties[NSHTTPCookieDomain];
    if ([domain length] == 0)
        domain = [originURL host];
    if ([domain length] == 0)
        return nil;
    NSString *path = properties[NSHTTPCookiePath];
    if ([path length] == 0)
        path = originURL ? defaultPath(originURL) : @"/";

    _name = [name copy];
    _value = [[value description] copy];
    _domain = [domain lowercaseString];
    _path = [path copy];
    _version = [properties[NSHTTPCookieVersion] integerValue] > 0 ? 1 : 0;
    _secure = flagValue(properties[NSHTTPCookieSecure]);
    _HTTPOnly = flagValue(properties[HTTPOnlyKey]);
    _comment = [properties[NSHTTPCookieComment] copy];
    id commentURL = properties[NSHTTPCookieCommentURL];
    _commentURL = [commentURL isKindOfClass:[NSURL class]] ? commentURL
                : commentURL ? [NSURL URLWithString:[commentURL description]] : nil;

    id maxAge = properties[NSHTTPCookieMaximumAge];
    id expires = properties[NSHTTPCookieExpires];
    if (maxAge != nil)
        _expiresDate = [NSDate dateWithTimeIntervalSinceNow:[[maxAge description] doubleValue]];
    else if ([expires isKindOfClass:[NSDate class]])
        _expiresDate = expires;
    else if ([expires isKindOfClass:[NSString class]])
        _expiresDate = parseCookieDate(expires);
    _sessionOnly = _expiresDate == nil || (_version == 1 && flagValue(properties[NSHTTPCookieDiscard]));

    NSString *ports = properties[NSHTTPCookiePort];
    if ([ports length] > 0) {
        NSMutableArray *list = [NSMutableArray array];
        for (NSString *port in [[ports description] componentsSeparatedByString:@","])
            if ([trim(port) integerValue] > 0)
                [list addObject:@([trim(port) integerValue])];
        _portList = list;
    }
    NSString *sameSite = [[properties[NSHTTPCookieSameSitePolicy] description] lowercaseString];
    if ([sameSite isEqualToString:NSHTTPCookieSameSiteLax])
        _sameSitePolicy = NSHTTPCookieSameSiteLax;
    else if ([sameSite isEqualToString:NSHTTPCookieSameSiteStrict])
        _sameSitePolicy = NSHTTPCookieSameSiteStrict;

    NSMutableDictionary *all = [properties mutableCopy];
    all[NSHTTPCookieDomain] = _domain;
    all[NSHTTPCookiePath] = _path;
    if (_expiresDate != nil)
        all[NSHTTPCookieExpires] = _expiresDate;
    [all removeObjectForKey:NSHTTPCookieMaximumAge];
    _properties = all;
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    return self;
}

- (NSDictionary *)properties { return _properties; }
- (NSUInteger)version { return _version; }
- (NSString *)name { return _name; }
- (NSString *)value { return _value; }
- (NSDate *)expiresDate { return _expiresDate; }
- (BOOL)isSessionOnly { return _sessionOnly; }
- (NSString *)domain { return _domain; }
- (NSString *)path { return _path; }
- (BOOL)isSecure { return _secure; }
- (BOOL)isHTTPOnly { return _HTTPOnly; }
- (NSString *)comment { return _comment; }
- (NSURL *)commentURL { return _commentURL; }
- (NSArray *)portList { return _portList; }
- (NSHTTPCookieStringPolicy)sameSitePolicy { return _sameSitePolicy; }

- (BOOL)isEqual:(id)other {
    if (other == self)
        return YES;
    if (![other isKindOfClass:[NSHTTPCookie class]])
        return NO;
    NSHTTPCookie *cookie = other;
    return [_name isEqualToString:cookie->_name] && [_domain isEqualToString:cookie->_domain] &&
           [_path isEqualToString:cookie->_path] && [_value isEqualToString:cookie->_value];
}

- (NSUInteger)hash {
    return [_name hash] ^ [_domain hash] ^ [_path hash];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<NSHTTPCookie version:%lu name:%@ value:%@ expiresDate:%@ "
                                      @"sessionOnly:%d domain:%@ path:%@ isSecure:%d isHTTPOnly:%d>",
                                      (unsigned long)_version, _name, _value, _expiresDate, _sessionOnly,
                                      _domain, _path, _secure, _HTTPOnly];
}

#pragma mark Header fields

// Set-Cookie headers folded into one (", "): a comma starts a new cookie
// unless it is inside an Expires date.
static NSArray *splitSetCookie(NSString *header) {
    NSMutableArray *result = [NSMutableArray array];
    NSUInteger start = 0, length = [header length];
    for (NSUInteger i = 0; i < length; i++) {
        if ([header characterAtIndex:i] != ',')
            continue;
        NSString *before = [header substringWithRange:NSMakeRange(start, i - start)];
        NSRange semicolon = [before rangeOfString:@";" options:NSBackwardsSearch];
        NSString *attribute = trim(semicolon.location == NSNotFound ? before
                                   : [before substringFromIndex:NSMaxRange(semicolon)]);
        if ([[attribute lowercaseString] hasPrefix:@"expires="] &&
            [[attribute substringFromIndex:8] rangeOfString:@" "].location == NSNotFound)
            continue;     // "Expires=Wed," : the day of the week
        NSString *rest = [header substringFromIndex:i + 1];
        NSRange equal = [rest rangeOfString:@"="];
        NSRange semi = [rest rangeOfString:@";"];
        if (equal.location == NSNotFound || (semi.location != NSNotFound && semi.location < equal.location))
            continue;
        [result addObject:before];
        start = i + 1;
    }
    [result addObject:[header substringFromIndex:start]];
    return result;
}

static NSHTTPCookie *parseSetCookie(NSString *string, NSURL *url) {
    NSArray *parts = [string componentsSeparatedByString:@";"];
    NSString *pair = [parts count] > 0 ? parts[0] : @"";
    NSRange equal = [pair rangeOfString:@"="];
    if (equal.location == NSNotFound)
        return nil;
    NSString *name = trim([pair substringToIndex:equal.location]);
    NSString *value = trim([pair substringFromIndex:NSMaxRange(equal)]);
    if ([name length] == 0)
        return nil;
    if ([value length] >= 2 && [value hasPrefix:@"\""] && [value hasSuffix:@"\""])
        value = [value substringWithRange:NSMakeRange(1, [value length] - 2)];

    NSMutableDictionary *properties = [NSMutableDictionary dictionary];
    properties[NSHTTPCookieName] = name;
    properties[NSHTTPCookieValue] = value;
    properties[NSHTTPCookieOriginURL] = url;
    for (NSUInteger i = 1; i < [parts count]; i++) {
        NSString *attribute = trim(parts[i]);
        NSRange eq = [attribute rangeOfString:@"="];
        NSString *key = [trim(eq.location == NSNotFound ? attribute : [attribute substringToIndex:eq.location]) lowercaseString];
        NSString *val = eq.location == NSNotFound ? @"" : trim([attribute substringFromIndex:NSMaxRange(eq)]);
        if ([key isEqualToString:@"domain"] && [val length] > 0) {
            NSString *domain = [[val lowercaseString] hasPrefix:@"."] ? [val lowercaseString]
                             : [@"." stringByAppendingString:[val lowercaseString]];
            properties[NSHTTPCookieDomain] = domain;
        } else if ([key isEqualToString:@"path"] && [val hasPrefix:@"/"]) {
            properties[NSHTTPCookiePath] = val;
        } else if ([key isEqualToString:@"expires"]) {
            NSDate *date = parseCookieDate(val);
            if (date) properties[NSHTTPCookieExpires] = date;
        } else if ([key isEqualToString:@"max-age"]) {
            properties[NSHTTPCookieMaximumAge] = val;
        } else if ([key isEqualToString:@"secure"]) {
            properties[NSHTTPCookieSecure] = @"TRUE";
        } else if ([key isEqualToString:@"httponly"]) {
            properties[HTTPOnlyKey] = @"TRUE";
        } else if ([key isEqualToString:@"samesite"]) {
            properties[NSHTTPCookieSameSitePolicy] = val;
        } else if ([key isEqualToString:@"version"]) {
            properties[NSHTTPCookieVersion] = val;
        } else if ([key isEqualToString:@"comment"]) {
            properties[NSHTTPCookieComment] = val;
        } else if ([key isEqualToString:@"port"]) {
            properties[NSHTTPCookiePort] = [val stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"\""]];
        }
    }
    // A server may only set cookies for its own domain.
    NSString *domain = properties[NSHTTPCookieDomain];
    if (domain != nil && ![[domain substringFromIndex:1] containsString:@"."] &&
        ![[domain substringFromIndex:1] isEqualToString:@"localhost"])
        return nil;
    if (domain != nil && !_NSHTTPCookieDomainMatches([url host], domain))
        return nil;
    return [NSHTTPCookie cookieWithProperties:properties];
}

+ (NSArray *)cookiesWithResponseHeaderFields:(NSDictionary *)headerFields forURL:(NSURL *)URL {
    NSMutableArray *cookies = [NSMutableArray array];
    if ([URL host] == nil)
        return cookies;
    for (NSString *key in headerFields) {
        if ([key caseInsensitiveCompare:@"Set-Cookie"] != NSOrderedSame)
            continue;
        for (NSString *one in splitSetCookie([headerFields[key] description])) {
            NSHTTPCookie *cookie = parseSetCookie(one, URL);
            if (cookie != nil)
                [cookies addObject:cookie];
        }
    }
    return cookies;
}

+ (NSDictionary *)requestHeaderFieldsWithCookies:(NSArray *)cookies {
    if ([cookies count] == 0)
        return @{};
    NSMutableArray *pairs = [NSMutableArray array];
    for (NSHTTPCookie *cookie in cookies)
        [pairs addObject:[NSString stringWithFormat:@"%@=%@", [cookie name], [cookie value]]];
    return @{ @"Cookie": [pairs componentsJoinedByString:@"; "] };
}

@end
