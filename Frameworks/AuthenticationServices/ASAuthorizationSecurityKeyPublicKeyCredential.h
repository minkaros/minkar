/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/*
 * Security-key public key credentials (roaming authenticators reached
 * over USB, NFC or Bluetooth). The provider makes registration and
 * assertion requests for one relying party; the registration and
 * assertion classes are the credentials an authorization returns.
 */

#import <AuthenticationServices/ASAuthorization.h>
#import <AuthenticationServices/ASAuthorizationPublicKeyCredential.h>

NS_ASSUME_NONNULL_BEGIN

/* Transport names, with the WebAuthn specification's values. */
typedef NSString *ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport NS_TYPED_EXTENSIBLE_ENUM;
FOUNDATION_EXPORT ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport const ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportUSB API_AVAILABLE(macos(12.0));
FOUNDATION_EXPORT ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport const ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportNFC API_AVAILABLE(macos(12.0));
FOUNDATION_EXPORT ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport const ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportBluetooth API_AVAILABLE(macos(12.0));

@class ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest;
@class ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest;

API_AVAILABLE(macos(12.0))
@interface ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor : NSObject <ASAuthorizationPublicKeyCredentialDescriptor>
- (instancetype)initWithCredentialID:(NSData *)credentialID
                          transports:(NSArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport> *)allowedTransports NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic, copy) NSArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport> *transports;
@end

API_AVAILABLE(macos(12.0))
@interface ASAuthorizationSecurityKeyPublicKeyCredentialProvider : NSObject <ASAuthorizationProvider>
- (instancetype)initWithRelyingPartyIdentifier:(NSString *)relyingPartyIdentifier NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
- (ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest *)createCredentialRegistrationRequestWithChallenge:(NSData *)challenge
                                                                                                          displayName:(NSString *)displayName
                                                                                                                 name:(NSString *)name
                                                                                                               userID:(NSData *)userID;
- (ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest *)createCredentialAssertionRequestWithChallenge:(NSData *)challenge;
@property (nonatomic, readonly, copy) NSString *relyingPartyIdentifier;
@end

API_AVAILABLE(macos(12.0))
@interface ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest : ASAuthorizationRequest <ASAuthorizationPublicKeyCredentialRegistrationRequest>
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic, copy) NSArray<ASAuthorizationPublicKeyCredentialParameters *> *credentialParameters;
@property (nonatomic, copy) NSArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor *> *excludedCredentials;
@property (nonatomic, copy) ASAuthorizationPublicKeyCredentialResidentKeyPreference residentKeyPreference;
@property (nonatomic, nullable) ASAuthorizationPublicKeyCredentialPRFRegistrationInput *prf API_AVAILABLE(macos(26.4));
@end

API_AVAILABLE(macos(12.0))
@interface ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest : ASAuthorizationRequest <ASAuthorizationPublicKeyCredentialAssertionRequest>
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic, copy) NSArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor *> *allowedCredentials;
/* The legacy FIDO U2F application identifier (the appid extension). */
@property (nonatomic, copy, nullable) NSString *appID API_AVAILABLE(macos(14.5));
@property (nonatomic, nullable) ASAuthorizationPublicKeyCredentialPRFAssertionInput *prf API_AVAILABLE(macos(26.4));
@end

API_AVAILABLE(macos(12.0))
@interface ASAuthorizationSecurityKeyPublicKeyCredentialRegistration : NSObject <ASAuthorizationPublicKeyCredentialRegistration>
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic, readonly, copy) NSArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport> *transports API_AVAILABLE(macos(14.5));
@property (nonatomic, readonly, nullable) ASAuthorizationPublicKeyCredentialPRFRegistrationOutput *prf API_AVAILABLE(macos(26.4));
@end

API_AVAILABLE(macos(12.0))
@interface ASAuthorizationSecurityKeyPublicKeyCredentialAssertion : NSObject <ASAuthorizationPublicKeyCredentialAssertion>
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
/* Whether the authenticator signed with the appid extension's identifier. */
@property (nonatomic, readonly) BOOL appID API_AVAILABLE(macos(14.5));
@property (nonatomic, readonly, nullable) ASAuthorizationPublicKeyCredentialPRFAssertionOutput *prf API_AVAILABLE(macos(26.4));
@end

NS_ASSUME_NONNULL_END
