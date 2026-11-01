/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSObject.h>

FOUNDATION_EXPORT NSString *const NSURLProtectionSpaceHTTP;
FOUNDATION_EXPORT NSString *const NSURLProtectionSpaceHTTPS;
FOUNDATION_EXPORT NSString *const NSURLProtectionSpaceFTP;
FOUNDATION_EXPORT NSString *const NSURLProtectionSpaceHTTPProxy;
FOUNDATION_EXPORT NSString *const NSURLProtectionSpaceHTTPSProxy;
FOUNDATION_EXPORT NSString *const NSURLProtectionSpaceFTPProxy;
FOUNDATION_EXPORT NSString *const NSURLProtectionSpaceSOCKSProxy;

FOUNDATION_EXPORT NSString *const NSURLAuthenticationMethodDefault;
FOUNDATION_EXPORT NSString *const NSURLAuthenticationMethodHTTPBasic;
FOUNDATION_EXPORT NSString *const NSURLAuthenticationMethodHTTPDigest;
FOUNDATION_EXPORT NSString *const NSURLAuthenticationMethodHTMLForm;
FOUNDATION_EXPORT NSString *const NSURLAuthenticationMethodNTLM;
FOUNDATION_EXPORT NSString *const NSURLAuthenticationMethodNegotiate;
FOUNDATION_EXPORT NSString *const NSURLAuthenticationMethodClientCertificate;
FOUNDATION_EXPORT NSString *const NSURLAuthenticationMethodServerTrust;

@interface NSURLProtectionSpace : NSObject <NSCopying> {
    NSString *_host;
    int _port;
    NSString *_protocol;
    NSString *_realm;
    NSString *_authenticationMethod;
    BOOL _isProxy;
}

- initWithHost:(NSString *)host port:(int)port protocol:(NSString *)protocol realm:(NSString *)realm authenticationMethod:(NSString *)authenticationMethod;
- initWithProxyHost:(NSString *)host port:(int)port protocol:(NSString *)protocol realm:(NSString *)realm authenticationMethod:(NSString *)authenticationMethod;

- (NSString *)host;
- (int)port;
- (NSString *)protocol;
- (NSString *)realm;
- (NSString *)authenticationMethod;

- (NSString *)proxyType;

- (BOOL)receivesCredentialsSecurely;
- (BOOL)isProxy;

@end
