/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/*
 * The browser's view of saved platform passkeys. Minkar keeps no platform
 * passkeys yet and has no consent prompt for this access, so browsers are
 * always told they are denied and every relying party has no credentials.
 */

#import "ASAuthorizationInternal.h"

@implementation ASAuthorizationWebBrowserPlatformPublicKeyCredential

- (instancetype)_initWithName:(NSString *)name
                  customTitle:(NSString *)customTitle
                 relyingParty:(NSString *)relyingParty
                 credentialID:(NSData *)credentialID
                   userHandle:(NSData *)userHandle
                 providerName:(NSString *)providerName {
    if ((self = [super init]) != nil) {
        _name = [name copy] ?: @"";
        _customTitle = [customTitle copy];
        _relyingParty = [relyingParty copy] ?: @"";
        _credentialID = [credentialID copy] ?: [NSData data];
        _userHandle = [userHandle copy] ?: [NSData data];
        _providerName = [providerName copy];
    }
    return self;
}

@end

@implementation ASAuthorizationWebBrowserPublicKeyCredentialManager

- (instancetype)init {
    return [super init];
}

- (ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationState)authorizationStateForPlatformCredentials {
    return ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationStateDenied;
}

/* The handler runs on the main queue, never inside this call. */
- (void)requestAuthorizationForPublicKeyCredentials:(void (^)(ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationState))completionHandler {
    if (completionHandler == nil)
        return;
    ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationState state = self.authorizationStateForPlatformCredentials;
    dispatch_async(dispatch_get_main_queue(), ^{
        completionHandler(state);
    });
}

- (void)platformCredentialsForRelyingParty:(NSString *)relyingParty
                         completionHandler:(void (^)(NSArray<ASAuthorizationWebBrowserPlatformPublicKeyCredential *> *))completionHandler {
    if (completionHandler == nil)
        return;
    dispatch_async(dispatch_get_main_queue(), ^{
        completionHandler(@[]);
    });
}

@end
