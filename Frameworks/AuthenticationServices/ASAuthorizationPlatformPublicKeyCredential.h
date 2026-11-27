/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/*
 * Platform public key credentials (passkeys kept by the system). The
 * provider makes registration and assertion requests for one relying
 * party; the registration and assertion classes are the credentials an
 * authorization returns.
 */

#import <AuthenticationServices/ASAuthorization.h>
#import <AuthenticationServices/ASAuthorizationPublicKeyCredential.h>

NS_ASSUME_NONNULL_BEGIN

@class ASAuthorizationPlatformPublicKeyCredentialRegistrationRequest;
@class ASAuthorizationPlatformPublicKeyCredentialAssertionRequest;

API_AVAILABLE(macos(12.0))
@interface ASAuthorizationPlatformPublicKeyCredentialDescriptor : NSObject <ASAuthorizationPublicKeyCredentialDescriptor>
- (instancetype)initWithCredentialID:(NSData *)credentialID NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

API_AVAILABLE(macos(12.0))
@interface ASAuthorizationPlatformPublicKeyCredentialProvider : NSObject <ASAuthorizationProvider>
- (instancetype)initWithRelyingPartyIdentifier:(NSString *)relyingPartyIdentifier NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
- (ASAuthorizationPlatformPublicKeyCredentialRegistrationRequest *)createCredentialRegistrationRequestWithChallenge:(NSData *)challenge
                                                                                                             name:(NSString *)name
                                                                                                           userID:(NSData *)userID;
- (ASAuthorizationPlatformPublicKeyCredentialAssertionRequest *)createCredentialAssertionRequestWithChallenge:(NSData *)challenge;
@property (nonatomic, readonly, copy) NSString *relyingPartyIdentifier;
@end

API_AVAILABLE(macos(12.0))
@interface ASAuthorizationPlatformPublicKeyCredentialRegistrationRequest : ASAuthorizationRequest <ASAuthorizationPublicKeyCredentialRegistrationRequest>
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic) BOOL shouldShowHybridTransport API_AVAILABLE(macos(13.5));
@property (nonatomic, nullable) ASAuthorizationPublicKeyCredentialLargeBlobRegistrationInput *largeBlob API_AVAILABLE(macos(14.0));
@property (nonatomic, nullable) ASAuthorizationPublicKeyCredentialPRFRegistrationInput *prf API_AVAILABLE(macos(15.0));
@end

API_AVAILABLE(macos(12.0))
@interface ASAuthorizationPlatformPublicKeyCredentialAssertionRequest : ASAuthorizationRequest <ASAuthorizationPublicKeyCredentialAssertionRequest>
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic, copy) NSArray<ASAuthorizationPlatformPublicKeyCredentialDescriptor *> *allowedCredentials;
@property (nonatomic) BOOL shouldShowHybridTransport API_AVAILABLE(macos(13.5));
@property (nonatomic, nullable) ASAuthorizationPublicKeyCredentialLargeBlobAssertionInput *largeBlob API_AVAILABLE(macos(14.0));
@property (nonatomic, nullable) ASAuthorizationPublicKeyCredentialPRFAssertionInput *prf API_AVAILABLE(macos(15.0));
@end

API_AVAILABLE(macos(12.0))
@interface ASAuthorizationPlatformPublicKeyCredentialRegistration : NSObject <ASAuthorizationPublicKeyCredentialRegistration>
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic, readonly) ASAuthorizationPublicKeyCredentialAttachment attachment API_AVAILABLE(macos(13.5));
@property (nonatomic, readonly, nullable) ASAuthorizationPublicKeyCredentialLargeBlobRegistrationOutput *largeBlob API_AVAILABLE(macos(14.0));
@property (nonatomic, readonly, nullable) ASAuthorizationPublicKeyCredentialPRFRegistrationOutput *prf API_AVAILABLE(macos(15.0));
@end

API_AVAILABLE(macos(12.0))
@interface ASAuthorizationPlatformPublicKeyCredentialAssertion : NSObject <ASAuthorizationPublicKeyCredentialAssertion>
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic, readonly) ASAuthorizationPublicKeyCredentialAttachment attachment API_AVAILABLE(macos(13.5));
@property (nonatomic, readonly, nullable) ASAuthorizationPublicKeyCredentialLargeBlobAssertionOutput *largeBlob API_AVAILABLE(macos(14.0));
@property (nonatomic, readonly, nullable) ASAuthorizationPublicKeyCredentialPRFAssertionOutput *prf API_AVAILABLE(macos(15.0));
@end

NS_ASSUME_NONNULL_END
