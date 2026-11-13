/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSString *const ASAuthorizationErrorDomain;
FOUNDATION_EXPORT NSString *const ASWebAuthenticationSessionErrorDomain;

typedef NS_ENUM(NSInteger, ASAuthorizationError) {
    ASAuthorizationErrorUnknown = 1000,
    ASAuthorizationErrorCanceled = 1001,
    ASAuthorizationErrorInvalidResponse = 1002,
    ASAuthorizationErrorNotHandled = 1003,
    ASAuthorizationErrorFailed = 1004,
    ASAuthorizationErrorNotInteractive = 1005,
};

typedef NS_ENUM(NSInteger, ASWebAuthenticationSessionErrorCode) {
    ASWebAuthenticationSessionErrorCodeCanceledLogin = 1,
    ASWebAuthenticationSessionErrorCodePresentationContextNotProvided = 2,
    ASWebAuthenticationSessionErrorCodePresentationContextInvalid = 3,
};

@protocol ASAuthorizationCredential <NSObject, NSCopying, NSSecureCoding>
@end

@protocol ASAuthorizationProvider <NSObject>
@end

@interface ASAuthorizationRequest : NSObject <NSCopying, NSSecureCoding>
@property (nonatomic, readonly, strong) id<ASAuthorizationProvider> provider;
@end

@interface ASAuthorizationOpenIDRequest : ASAuthorizationRequest
@property (nonatomic, copy, nullable) NSArray<NSString *> *requestedScopes;
@property (nonatomic, copy, nullable) NSString *state;
@property (nonatomic, copy, nullable) NSString *nonce;
@end

@interface ASAuthorizationSingleSignOnRequest : ASAuthorizationOpenIDRequest
@property (nonatomic, copy) NSArray<NSURLQueryItem *> *authorizationOptions;
@property (nonatomic) BOOL enableEmbeddedAuthorizationViewController;
@property (nonatomic) BOOL userInterfaceEnabled;
@end

@interface ASAuthorizationSingleSignOnProvider : NSObject <ASAuthorizationProvider>
+ (instancetype)authorizationProviderWithIdentityProviderURL:(NSURL *)url;
- (ASAuthorizationSingleSignOnRequest *)createRequest;
@property (nonatomic, readonly) NSURL *url;
@property (nonatomic, readonly) BOOL canPerformAuthorization;
@end

@interface ASAuthorizationSingleSignOnCredential : NSObject <ASAuthorizationCredential>
@property (nonatomic, readonly, copy, nullable) NSString *state;
@property (nonatomic, readonly, copy, nullable) NSData *accessToken;
@property (nonatomic, readonly, copy, nullable) NSData *identityToken;
@property (nonatomic, readonly, copy) NSArray<NSString *> *authorizedScopes;
@property (nonatomic, readonly, copy, nullable) NSHTTPURLResponse *authenticatedResponse;
@end

@interface ASAuthorization : NSObject
@property (nonatomic, readonly, strong) id<ASAuthorizationProvider> provider;
@property (nonatomic, readonly, strong) id<ASAuthorizationCredential> credential;
@end

@class ASAuthorizationController;

@protocol ASAuthorizationControllerDelegate <NSObject>
@optional
- (void)authorizationController:(ASAuthorizationController *)controller didCompleteWithAuthorization:(ASAuthorization *)authorization;
- (void)authorizationController:(ASAuthorizationController *)controller didCompleteWithError:(NSError *)error;
@end

@protocol ASAuthorizationControllerPresentationContextProviding <NSObject>
@end

@interface ASAuthorizationController : NSObject
- (instancetype)initWithAuthorizationRequests:(NSArray<ASAuthorizationRequest *> *)authorizationRequests;
@property (nonatomic, readonly, strong) NSArray<ASAuthorizationRequest *> *authorizationRequests;
@property (nonatomic, weak, nullable) id<ASAuthorizationControllerDelegate> delegate;
@property (nonatomic, weak, nullable) id<ASAuthorizationControllerPresentationContextProviding> presentationContextProvider;
- (void)performRequests;
- (void)cancel;
@end

/* A web sign-in another app asked the default browser to do. */
@interface ASWebAuthenticationSessionRequest : NSObject <NSSecureCoding, NSCopying>
@property (nonatomic, readonly) NSUUID *UUID;
@property (nonatomic, readonly) NSURL *URL;
@property (nonatomic, readonly, nullable) NSString *callbackURLScheme;
@property (nonatomic, readonly) BOOL shouldUseEphemeralSession;
- (void)cancelWithError:(NSError *)error;
- (void)completeWithCallbackURL:(NSURL *)url;
@end

@protocol ASWebAuthenticationSessionWebBrowserSessionHandling <NSObject>
- (void)beginHandlingWebAuthenticationSessionRequest:(nullable ASWebAuthenticationSessionRequest *)request;
- (void)cancelWebAuthenticationSessionRequest:(nullable ASWebAuthenticationSessionRequest *)request;
@end

@interface ASWebAuthenticationSessionWebBrowserSessionManager : NSObject
@property (class, nonatomic, readonly) ASWebAuthenticationSessionWebBrowserSessionManager *sharedManager;
@property (nonatomic, strong) id<ASWebAuthenticationSessionWebBrowserSessionHandling> sessionHandler;
@property (nonatomic, readonly) BOOL wasLaunchedByAuthenticationServices;
@end

NS_ASSUME_NONNULL_END
