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
 * NSURLComponents and NSURLQueryItem: a URL taken apart (RFC 3986) and put
 * together again. Each part is kept percent-encoded; the plain accessors
 * decode it and encode what they are given.
 */

#import <Foundation/NSURLComponents.h>
#import <Foundation/NSCharacterSet.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSException.h>
#include <dispatch/dispatch.h>

@implementation NSURLQueryItem {
    NSString *_name;
    NSString *_value;
}

- (instancetype)initWithName:(NSString *)name value:(NSString *)value {
    if ((self = [super init]) != nil) {
        _name = [name copy] ?: @"";
        _value = [value copy];
    }
    return self;
}

+ (instancetype)queryItemWithName:(NSString *)name value:(NSString *)value {
    return [[self alloc] initWithName:name value:value];
}

- (NSString *)name { return _name; }
- (NSString *)value { return _value; }

- (id)copyWithZone:(NSZone *)zone {
    return self;
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self initWithName:[coder decodeObjectOfClass:[NSString class] forKey:@"NS.name"]
                        value:[coder decodeObjectOfClass:[NSString class] forKey:@"NS.value"]];
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_name forKey:@"NS.name"];
    [coder encodeObject:_value forKey:@"NS.value"];
}

- (BOOL)isEqual:(id)other {
    if (![other isKindOfClass:[NSURLQueryItem class]])
        return NO;
    NSURLQueryItem *item = other;
    return [_name isEqualToString:item->_name] &&
           (_value == item->_value || [_value isEqualToString:item->_value]);
}

- (NSUInteger)hash {
    return [_name hash] ^ [_value hash];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<NSURLQueryItem %p> {name = %@, value = %@}", (__bridge void *)self,
                                      _name, _value ?: @"(null)"];
}

@end

static NSCharacterSet *queryItemAllowed(void) {
    static NSCharacterSet *set;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSMutableCharacterSet *chars = [[NSCharacterSet URLQueryAllowedCharacterSet] mutableCopy];
        [chars removeCharactersInString:@"&=#"];
        set = [chars copy];
    });
    return set;
}

static NSString *encode(NSString *string, NSCharacterSet *allowed) {
    return string ? [string stringByAddingPercentEncodingWithAllowedCharacters:allowed] : nil;
}

static NSString *decode(NSString *string) {
    return string ? ([string stringByRemovingPercentEncoding] ?: string) : nil;
}

// The characters a URL string may hold: the reserved and unreserved ones,
// and "%" of an escape.
static BOOL validURLCharacters(NSString *string) {
    static NSCharacterSet *invalid;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        invalid = [[NSCharacterSet characterSetWithCharactersInString:
            @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~:/?#[]@!$&'()*+,;=%"] invertedSet];
    });
    return [string rangeOfCharacterFromSet:invalid].location == NSNotFound;
}

@implementation NSURLComponents {
    NSString *_scheme;
    NSString *_user, *_password, *_host, *_path, *_query, *_fragment;    // encoded
    NSNumber *_port;
}

- (instancetype)init {
    if ((self = [super init]) != nil)
        _path = @"";
    return self;
}

+ (instancetype)componentsWithString:(NSString *)URLString {
    return [[self alloc] initWithString:URLString];
}

+ (instancetype)componentsWithURL:(NSURL *)url resolvingAgainstBaseURL:(BOOL)resolve {
    return [[self alloc] initWithURL:url resolvingAgainstBaseURL:resolve];
}

- (instancetype)initWithURL:(NSURL *)url resolvingAgainstBaseURL:(BOOL)resolve {
    NSString *string = resolve ? [url absoluteString] : [url relativeString];
    return string ? [self initWithString:string] : nil;
}

// nil for a string that is not a URL.
- (instancetype)initWithString:(NSString *)string {
    if ((self = [self init]) == nil || string == nil || !validURLCharacters(string))
        return nil;
    NSString *rest = string;

    NSRange hash = [rest rangeOfString:@"#"];
    if (hash.location != NSNotFound) {
        _fragment = [rest substringFromIndex:NSMaxRange(hash)];
        rest = [rest substringToIndex:hash.location];
    }
    NSRange question = [rest rangeOfString:@"?"];
    if (question.location != NSNotFound) {
        _query = [rest substringFromIndex:NSMaxRange(question)];
        rest = [rest substringToIndex:question.location];
    }

    // A scheme: a letter, then letters, digits, "+", "-", "." before ":".
    NSRange colon = [rest rangeOfString:@":"];
    NSRange slash = [rest rangeOfString:@"/"];
    if (colon.location != NSNotFound && colon.location > 0 &&
        (slash.location == NSNotFound || colon.location < slash.location)) {
        NSString *scheme = [rest substringToIndex:colon.location];
        NSCharacterSet *schemeChars = [NSCharacterSet characterSetWithCharactersInString:
            @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-."];
        unichar first = [scheme characterAtIndex:0];
        if (((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z')) &&
            [scheme rangeOfCharacterFromSet:[schemeChars invertedSet]].location == NSNotFound) {
            _scheme = scheme;
            rest = [rest substringFromIndex:NSMaxRange(colon)];
        }
    }

    if ([rest hasPrefix:@"//"]) {
        rest = [rest substringFromIndex:2];
        NSRange end = [rest rangeOfString:@"/"];
        NSString *authority = end.location == NSNotFound ? rest : [rest substringToIndex:end.location];
        rest = end.location == NSNotFound ? @"" : [rest substringFromIndex:end.location];

        NSRange at = [authority rangeOfString:@"@" options:NSBackwardsSearch];
        if (at.location != NSNotFound) {
            NSString *userinfo = [authority substringToIndex:at.location];
            authority = [authority substringFromIndex:NSMaxRange(at)];
            NSRange c = [userinfo rangeOfString:@":"];
            if (c.location != NSNotFound) {
                _user = [userinfo substringToIndex:c.location];
                _password = [userinfo substringFromIndex:NSMaxRange(c)];
            } else {
                _user = userinfo;
            }
        }
        // A host, an IPv6 literal in brackets, then the port.
        NSString *hostPart = authority, *portPart = nil;
        if ([authority hasPrefix:@"["]) {
            NSRange close = [authority rangeOfString:@"]"];
            if (close.location == NSNotFound)
                return nil;
            hostPart = [authority substringToIndex:NSMaxRange(close)];
            NSString *after = [authority substringFromIndex:NSMaxRange(close)];
            if ([after hasPrefix:@":"])
                portPart = [after substringFromIndex:1];
            else if ([after length] > 0)
                return nil;
        } else {
            NSRange c = [authority rangeOfString:@":" options:NSBackwardsSearch];
            if (c.location != NSNotFound) {
                hostPart = [authority substringToIndex:c.location];
                portPart = [authority substringFromIndex:NSMaxRange(c)];
            }
        }
        _host = hostPart;
        if ([portPart length] > 0) {
            NSCharacterSet *digits = [[NSCharacterSet decimalDigitCharacterSet] invertedSet];
            if ([portPart rangeOfCharacterFromSet:digits].location != NSNotFound)
                return nil;
            _port = @([portPart integerValue]);
        }
    }
    _path = rest;
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    NSURLComponents *copy = [[NSURLComponents alloc] init];
    copy->_scheme = _scheme;
    copy->_user = _user;
    copy->_password = _password;
    copy->_host = _host;
    copy->_port = _port;
    copy->_path = _path;
    copy->_query = _query;
    copy->_fragment = _fragment;
    return copy;
}

#pragma mark The whole

// nil when the parts cannot make a URL (a path not starting with "/"
// after an authority, or one starting with "//" without one).
- (NSString *)string {
    BOOL authority = _host != nil || _user != nil || _password != nil || _port != nil;
    if (authority && [_path length] > 0 && ![_path hasPrefix:@"/"])
        return nil;
    if (!authority && [_path hasPrefix:@"//"])
        return nil;
    NSMutableString *string = [NSMutableString string];
    if (_scheme)
        [string appendFormat:@"%@:", _scheme];
    if (authority) {
        [string appendString:@"//"];
        if (_user || _password) {
            [string appendString:_user ?: @""];
            if (_password)
                [string appendFormat:@":%@", _password];
            [string appendString:@"@"];
        }
        [string appendString:_host ?: @""];
        if (_port)
            [string appendFormat:@":%@", _port];
    }
    [string appendString:_path ?: @""];
    if (_query)
        [string appendFormat:@"?%@", _query];
    if (_fragment)
        [string appendFormat:@"#%@", _fragment];
    return string;
}

- (NSURL *)URL {
    NSString *string = [self string];
    return string ? [NSURL URLWithString:string] : nil;
}

- (NSURL *)URLRelativeToURL:(NSURL *)baseURL {
    NSString *string = [self string];
    return string ? [NSURL URLWithString:string relativeToURL:baseURL] : nil;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<NSURLComponents %p> {scheme = %@, user = %@, password = %@, host = %@, "
                                      @"port = %@, path = %@, query = %@, fragment = %@}",
                                      (__bridge void *)self, _scheme, _user, _password, _host, _port, _path,
                                      _query, _fragment];
}

#pragma mark Parts

- (NSString *)scheme { return _scheme; }

- (void)setScheme:(NSString *)scheme {
    if (scheme != nil && [scheme length] == 0)
        [NSException raise:NSInvalidArgumentException format:@"*** -[NSURLComponents setScheme:]: invalid characters in scheme"];
    _scheme = [scheme copy];
}

- (NSNumber *)port { return _port; }

- (void)setPort:(NSNumber *)port {
    if (port != nil && [port integerValue] < 0)
        [NSException raise:NSInvalidArgumentException format:@"*** -[NSURLComponents setPort:]: negative port"];
    _port = [port copy];
}

- (NSString *)user { return decode(_user); }
- (void)setUser:(NSString *)user { _user = encode(user, [NSCharacterSet URLUserAllowedCharacterSet]); }
- (NSString *)password { return decode(_password); }
- (void)setPassword:(NSString *)password { _password = encode(password, [NSCharacterSet URLPasswordAllowedCharacterSet]); }
- (NSString *)host { return decode(_host); }
- (void)setHost:(NSString *)host { _host = encode(host, [NSCharacterSet URLHostAllowedCharacterSet]); }
- (NSString *)path { return decode(_path) ?: @""; }
- (void)setPath:(NSString *)path { _path = encode(path, [NSCharacterSet URLPathAllowedCharacterSet]) ?: @""; }
- (NSString *)query { return decode(_query); }
- (void)setQuery:(NSString *)query { _query = encode(query, [NSCharacterSet URLQueryAllowedCharacterSet]); }
- (NSString *)fragment { return decode(_fragment); }
- (void)setFragment:(NSString *)fragment { _fragment = encode(fragment, [NSCharacterSet URLFragmentAllowedCharacterSet]); }

- (NSString *)percentEncodedUser { return _user; }
- (void)setPercentEncodedUser:(NSString *)user { _user = [user copy]; }
- (NSString *)percentEncodedPassword { return _password; }
- (void)setPercentEncodedPassword:(NSString *)password { _password = [password copy]; }
- (NSString *)percentEncodedHost { return _host; }
- (void)setPercentEncodedHost:(NSString *)host { _host = [host copy]; }
- (NSString *)percentEncodedPath { return _path ?: @""; }
- (void)setPercentEncodedPath:(NSString *)path { _path = [path copy] ?: @""; }
- (NSString *)percentEncodedQuery { return _query; }
- (void)setPercentEncodedQuery:(NSString *)query { _query = [query copy]; }
- (NSString *)percentEncodedFragment { return _fragment; }
- (void)setPercentEncodedFragment:(NSString *)fragment { _fragment = [fragment copy]; }

#pragma mark Query items

- (NSArray *)_queryItemsDecoded:(BOOL)decoded {
    if (_query == nil)
        return nil;
    NSMutableArray *items = [NSMutableArray array];
    if ([_query length] == 0)
        return items;
    for (NSString *pair in [_query componentsSeparatedByString:@"&"]) {
        NSRange equal = [pair rangeOfString:@"="];
        NSString *name = equal.location == NSNotFound ? pair : [pair substringToIndex:equal.location];
        NSString *value = equal.location == NSNotFound ? nil : [pair substringFromIndex:NSMaxRange(equal)];
        [items addObject:[NSURLQueryItem queryItemWithName:decoded ? decode(name) : name
                                                     value:decoded ? decode(value) : value]];
    }
    return items;
}

- (void)_setQueryItems:(NSArray *)items encode:(BOOL)shouldEncode {
    if (items == nil) {
        _query = nil;
        return;
    }
    NSMutableArray *pairs = [NSMutableArray array];
    for (NSURLQueryItem *item in items) {
        NSString *name = shouldEncode ? encode([item name], queryItemAllowed()) : [item name];
        NSString *value = shouldEncode ? encode([item value], queryItemAllowed()) : [item value];
        [pairs addObject:value ? [NSString stringWithFormat:@"%@=%@", name, value] : name];
    }
    _query = [pairs componentsJoinedByString:@"&"];
}

- (NSArray *)queryItems { return [self _queryItemsDecoded:YES]; }
- (void)setQueryItems:(NSArray *)items { [self _setQueryItems:items encode:YES]; }
- (NSArray *)percentEncodedQueryItems { return [self _queryItemsDecoded:NO]; }
- (void)setPercentEncodedQueryItems:(NSArray *)items { [self _setQueryItems:items encode:NO]; }

#pragma mark Ranges

// The range of a part in -string, or {NSNotFound, 0}.
- (NSRange)_rangeOfPart:(int)part {
    NSString *string = [self string];
    if (string == nil)
        return NSMakeRange(NSNotFound, 0);
    NSUInteger at = 0;
    BOOL authority = _host != nil || _user != nil || _password != nil || _port != nil;
    NSRange result = NSMakeRange(NSNotFound, 0);
#define PART(n, value, before, after) \
    if (value != nil) { at += before; if (part == n) result = NSMakeRange(at, [value length]); at += [value length] + after; }
    NSString *port = [_port description];
    PART(0, _scheme, 0, 1)
    if (authority) at += 2;
    PART(1, _user, 0, 0)
    PART(2, _password, (NSUInteger)(_password ? 1 : 0), 0)
    if (_user || _password) at += 1;
    PART(3, _host, 0, 0)
    PART(4, port, 1, 0)
    PART(5, _path, 0, 0)
    PART(6, _query, 1, 0)
    PART(7, _fragment, 1, 0)
#undef PART
    return result;
}

- (NSRange)rangeOfScheme { return [self _rangeOfPart:0]; }
- (NSRange)rangeOfUser { return [self _rangeOfPart:1]; }
- (NSRange)rangeOfPassword { return [self _rangeOfPart:2]; }
- (NSRange)rangeOfHost { return [self _rangeOfPart:3]; }
- (NSRange)rangeOfPort { return [self _rangeOfPart:4]; }
- (NSRange)rangeOfPath { return [self _rangeOfPart:5]; }
- (NSRange)rangeOfQuery { return [self _rangeOfPart:6]; }
- (NSRange)rangeOfFragment { return [self _rangeOfPart:7]; }

@end
