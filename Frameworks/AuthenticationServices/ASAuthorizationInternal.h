/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/*
 * Initializers the framework uses to build requests, extension outputs,
 * credentials and authorizations. Not installed: the public classes keep
 * these objects read-only for apps.
 */

/*
 * The framework implements every API level it declares, so its own
 * sources use the annotated classes without availability checks.
 */
#pragma clang diagnostic ignored "-Wunguarded-availability"
#pragma clang diagnostic ignored "-Wunguarded-availability-new"

#import <AuthenticationServices/AuthenticationServices.h>

NS_ASSUME_NONNULL_BEGIN

@interface ASAuthorizationRequest ()
- (instancetype)_initWithProvider:(id<ASAuthorizationProvider>)provider;
@end

@interface ASAuthorization ()
- (instancetype)_initWithProvider:(id<ASAuthorizationProvider>)provider
                       credential:(id<ASAuthorizationCredential>)credential;
@end

@interface ASAuthorizationPublicKeyCredentialLargeBlobRegistrationOutput ()
- (instancetype)_initWithSupported:(BOOL)isSupported;
@end

@interface ASAuthorizationPublicKeyCredentialLargeBlobAssertionOutput ()
- (instancetype)_initWithReadData:(nullable NSData *)readData didWrite:(BOOL)didWrite;
@end

@interface ASAuthorizationPublicKeyCredentialPRFAssertionOutput ()
- (instancetype)_initWithFirst:(NSData *)first second:(nullable NSData *)second;
@end

@interface ASAuthorizationPublicKeyCredentialPRFRegistrationOutput ()
- (instancetype)_initWithSupported:(BOOL)isSupported first:(nullable NSData *)first second:(nullable NSData *)second;
@end

@interface ASAuthorizationPlatformPublicKeyCredentialRegistration ()
- (instancetype)_initWithRawClientDataJSON:(NSData *)rawClientDataJSON
                              credentialID:(NSData *)credentialID
                      rawAttestationObject:(nullable NSData *)rawAttestationObject
                                attachment:(ASAuthorizationPublicKeyCredentialAttachment)attachment
                                 largeBlob:(nullable ASAuthorizationPublicKeyCredentialLargeBlobRegistrationOutput *)largeBlob
                                       prf:(nullable ASAuthorizationPublicKeyCredentialPRFRegistrationOutput *)prf;
@end

@interface ASAuthorizationPlatformPublicKeyCredentialAssertion ()
- (instancetype)_initWithRawClientDataJSON:(NSData *)rawClientDataJSON
                              credentialID:(NSData *)credentialID
                      rawAuthenticatorData:(NSData *)rawAuthenticatorData
                                    userID:(NSData *)userID
                                 signature:(NSData *)signature
                                attachment:(ASAuthorizationPublicKeyCredentialAttachment)attachment
                                 largeBlob:(nullable ASAuthorizationPublicKeyCredentialLargeBlobAssertionOutput *)largeBlob
                                       prf:(nullable ASAuthorizationPublicKeyCredentialPRFAssertionOutput *)prf;
@end

@interface ASAuthorizationSecurityKeyPublicKeyCredentialRegistration ()
- (instancetype)_initWithRawClientDataJSON:(NSData *)rawClientDataJSON
                              credentialID:(NSData *)credentialID
                      rawAttestationObject:(nullable NSData *)rawAttestationObject
                                transports:(NSArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport> *)transports
                                       prf:(nullable ASAuthorizationPublicKeyCredentialPRFRegistrationOutput *)prf;
@end

@interface ASAuthorizationSecurityKeyPublicKeyCredentialAssertion ()
- (instancetype)_initWithRawClientDataJSON:(NSData *)rawClientDataJSON
                              credentialID:(NSData *)credentialID
                      rawAuthenticatorData:(NSData *)rawAuthenticatorData
                                    userID:(NSData *)userID
                                 signature:(NSData *)signature
                                     appID:(BOOL)appID
                                       prf:(nullable ASAuthorizationPublicKeyCredentialPRFAssertionOutput *)prf;
@end

@interface ASAuthorizationWebBrowserPlatformPublicKeyCredential ()
- (instancetype)_initWithName:(NSString *)name
                  customTitle:(nullable NSString *)customTitle
                 relyingParty:(NSString *)relyingParty
                 credentialID:(NSData *)credentialID
                   userHandle:(NSData *)userHandle
                 providerName:(nullable NSString *)providerName;
@end

/* Archiving of the extension inputs a request carries (nil is skipped). */
#define AS_HIDDEN __attribute__((visibility("hidden")))
AS_HIDDEN void _ASEncodeLargeBlobRegistrationInput(NSCoder *coder, ASAuthorizationPublicKeyCredentialLargeBlobRegistrationInput *_Nullable input);
AS_HIDDEN ASAuthorizationPublicKeyCredentialLargeBlobRegistrationInput *_Nullable _ASDecodeLargeBlobRegistrationInput(NSCoder *coder);
AS_HIDDEN void _ASEncodeLargeBlobAssertionInput(NSCoder *coder, ASAuthorizationPublicKeyCredentialLargeBlobAssertionInput *_Nullable input);
AS_HIDDEN ASAuthorizationPublicKeyCredentialLargeBlobAssertionInput *_Nullable _ASDecodeLargeBlobAssertionInput(NSCoder *coder);
AS_HIDDEN void _ASEncodePRFRegistrationInput(NSCoder *coder, ASAuthorizationPublicKeyCredentialPRFRegistrationInput *_Nullable input);
AS_HIDDEN ASAuthorizationPublicKeyCredentialPRFRegistrationInput *_Nullable _ASDecodePRFRegistrationInput(NSCoder *coder);
AS_HIDDEN void _ASEncodePRFAssertionInput(NSCoder *coder, ASAuthorizationPublicKeyCredentialPRFAssertionInput *_Nullable input);
AS_HIDDEN ASAuthorizationPublicKeyCredentialPRFAssertionInput *_Nullable _ASDecodePRFAssertionInput(NSCoder *coder);

NS_ASSUME_NONNULL_END
