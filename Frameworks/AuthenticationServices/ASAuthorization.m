/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AuthenticationServices/AuthenticationServices.h>

NSString *const ASAuthorizationErrorDomain = @"com.apple.AuthenticationServices.AuthorizationError";
NSString *const ASWebAuthenticationSessionErrorDomain = @"com.apple.AuthenticationServices.WebAuthenticationSession";

@interface ASAuthorizationRequest ()
- (instancetype)_initWithProvider:(id<ASAuthorizationProvider>)provider;
@end

@implementation ASAuthorizationRequest

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)_initWithProvider:(id<ASAuthorizationProvider>)provider {
    if ((self = [super init]) != nil)
        _provider = provider;
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [super init];
}

@end

@implementation ASAuthorizationOpenIDRequest
@end

@implementation ASAuthorizationSingleSignOnRequest

- (instancetype)_initWithProvider:(id<ASAuthorizationProvider>)provider {
    if ((self = [super _initWithProvider:provider]) != nil) {
        _authorizationOptions = @[];
        _userInterfaceEnabled = YES;
    }
    return self;
}

@end

@implementation ASAuthorizationSingleSignOnProvider

+ (instancetype)authorizationProviderWithIdentityProviderURL:(NSURL *)url {
    ASAuthorizationSingleSignOnProvider *provider = [[self alloc] init];
    provider->_url = [url copy];
    return provider;
}

- (ASAuthorizationSingleSignOnRequest *)createRequest {
    return [[ASAuthorizationSingleSignOnRequest alloc] _initWithProvider:self];
}

/* No sign-on extension is installed for any identity provider. */
- (BOOL)canPerformAuthorization {
    return NO;
}

@end

@implementation ASAuthorizationSingleSignOnCredential

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (NSArray<NSString *> *)authorizedScopes {
    return @[];
}

- (id)copyWithZone:(NSZone *)zone {
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [super init];
}

@end

@implementation ASAuthorization
@end

@implementation ASAuthorizationController

- (instancetype)initWithAuthorizationRequests:(NSArray<ASAuthorizationRequest *> *)authorizationRequests {
    if ((self = [super init]) != nil)
        _authorizationRequests = [authorizationRequests copy];
    return self;
}

/* Nothing on the system handles these requests. */
- (void)performRequests {
    dispatch_async(dispatch_get_main_queue(), ^{
        id<ASAuthorizationControllerDelegate> delegate = self.delegate;
        if ([delegate respondsToSelector:@selector(authorizationController:didCompleteWithError:)])
            [delegate authorizationController:self didCompleteWithError:
                [NSError errorWithDomain:ASAuthorizationErrorDomain code:ASAuthorizationErrorNotHandled userInfo:nil]];
    });
}

- (void)cancel {
}

@end

@implementation ASWebAuthenticationSessionRequest

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (id)copyWithZone:(NSZone *)zone {
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [super init];
}

- (void)cancelWithError:(NSError *)error {
}

- (void)completeWithCallbackURL:(NSURL *)url {
}

@end

@implementation ASWebAuthenticationSessionWebBrowserSessionManager

+ (ASWebAuthenticationSessionWebBrowserSessionManager *)sharedManager {
    static ASWebAuthenticationSessionWebBrowserSessionManager *manager;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        manager = [[ASWebAuthenticationSessionWebBrowserSessionManager alloc] init];
    });
    return manager;
}

/* No other app's sign-in is ever handed to the browser on Minkar. */
- (BOOL)wasLaunchedByAuthenticationServices {
    return NO;
}

@end
