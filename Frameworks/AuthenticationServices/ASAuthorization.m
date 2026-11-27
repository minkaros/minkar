/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "ASAuthorizationInternal.h"

NSString *const ASAuthorizationErrorDomain = @"com.apple.AuthenticationServices.AuthorizationError";
NSString *const ASWebAuthenticationSessionErrorDomain = @"com.apple.AuthenticationServices.WebAuthenticationSession";

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

- (instancetype)_initWithProvider:(id<ASAuthorizationProvider>)provider
                       credential:(id<ASAuthorizationCredential>)credential {
    if ((self = [super init]) != nil) {
        _provider = provider;
        _credential = credential;
    }
    return self;
}

@end

@implementation ASAuthorizationController {
    /* Bumped by every performRequests and cancel; a pending completion
       whose number is no longer current has been superseded. */
    NSUInteger _generation;
    BOOL _inFlight;
}

- (instancetype)initWithAuthorizationRequests:(NSArray<ASAuthorizationRequest *> *)authorizationRequests {
    if ((self = [super init]) != nil)
        _authorizationRequests = [authorizationRequests copy] ?: @[];
    return self;
}

/* Tells the delegate, on the main queue, unless superseded meanwhile. */
- (void)_finishGeneration:(NSUInteger)generation withErrorCode:(ASAuthorizationError)code description:(NSString *)description {
    dispatch_async(dispatch_get_main_queue(), ^{
        @synchronized (self) {
            if (!self->_inFlight || self->_generation != generation)
                return;
            self->_inFlight = NO;
        }
        id<ASAuthorizationControllerDelegate> delegate = self.delegate;
        if ([delegate respondsToSelector:@selector(authorizationController:didCompleteWithError:)])
            [delegate authorizationController:self didCompleteWithError:
                [NSError errorWithDomain:ASAuthorizationErrorDomain code:code
                                userInfo:@{ NSLocalizedDescriptionKey: description }]];
    });
}

/*
 * Nothing on the system can satisfy a request yet: there are no sign-on
 * extensions, no platform authenticator (passkeys kept by the system) and
 * no CTAP transport to reach security keys over USB, NFC or Bluetooth.
 * Every set of requests therefore ends, asynchronously on the main queue,
 * with ASAuthorizationErrorNotHandled: the documented code for a request
 * that no authorizer on the system handled. A browser turns that into the
 * WebAuthn NotAllowedError, which is what the specification asks for when
 * no authenticator is available.
 */
- (void)performRequests {
    NSUInteger generation;
    @synchronized (self) {
        generation = ++_generation;
        _inFlight = YES;
    }
    NSString *description = _authorizationRequests.count == 0
        ? @"No authorization requests were given."
        : @"No credential provider or authenticator on this system can handle the authorization requests.";
    [self _finishGeneration:generation withErrorCode:ASAuthorizationErrorNotHandled description:description];
}

/* Ends a request still in flight with ASAuthorizationErrorCanceled. */
- (void)cancel {
    NSUInteger generation;
    @synchronized (self) {
        if (!_inFlight)
            return;
        generation = ++_generation;
    }
    [self _finishGeneration:generation withErrorCode:ASAuthorizationErrorCanceled
                description:@"The authorization requests were canceled."];
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
