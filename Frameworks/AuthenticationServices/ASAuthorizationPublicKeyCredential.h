/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/*
 * Public key credentials (WebAuthn passkeys and security keys): the
 * preference constants, the protocols that platform and security-key
 * requests, descriptors and credentials share, the algorithm parameters,
 * and the large blob and PRF extension inputs and outputs.
 */

#import <Foundation/Foundation.h>
#import <AuthenticationServices/ASAuthorization.h>
#include <os/availability.h>

NS_ASSUME_NONNULL_BEGIN

/* Preference strings. They carry the WebAuthn specification's values. */

typedef NSString *ASAuthorizationPublicKeyCredentialUserVerificationPreference NS_TYPED_EXTENSIBLE_ENUM;
FOUNDATION_EXPORT ASAuthorizationPublicKeyCredentialUserVerificationPreference const ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred API_AVAILABLE(macos(12.0));
FOUNDATION_EXPORT ASAuthorizationPublicKeyCredentialUserVerificationPreference const ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired API_AVAILABLE(macos(12.0));
FOUNDATION_EXPORT ASAuthorizationPublicKeyCredentialUserVerificationPreference const ASAuthorizationPublicKeyCredentialUserVerificationPreferenceDiscouraged API_AVAILABLE(macos(12.0));

typedef NSString *ASAuthorizationPublicKeyCredentialAttestationKind NS_TYPED_EXTENSIBLE_ENUM;
FOUNDATION_EXPORT ASAuthorizationPublicKeyCredentialAttestationKind const ASAuthorizationPublicKeyCredentialAttestationKindNone API_AVAILABLE(macos(12.0));
FOUNDATION_EXPORT ASAuthorizationPublicKeyCredentialAttestationKind const ASAuthorizationPublicKeyCredentialAttestationKindDirect API_AVAILABLE(macos(12.0));
FOUNDATION_EXPORT ASAuthorizationPublicKeyCredentialAttestationKind const ASAuthorizationPublicKeyCredentialAttestationKindIndirect API_AVAILABLE(macos(12.0));
FOUNDATION_EXPORT ASAuthorizationPublicKeyCredentialAttestationKind const ASAuthorizationPublicKeyCredentialAttestationKindEnterprise API_AVAILABLE(macos(12.0));

typedef NSString *ASAuthorizationPublicKeyCredentialResidentKeyPreference NS_TYPED_EXTENSIBLE_ENUM;
FOUNDATION_EXPORT ASAuthorizationPublicKeyCredentialResidentKeyPreference const ASAuthorizationPublicKeyCredentialResidentKeyPreferenceDiscouraged API_AVAILABLE(macos(12.0));
FOUNDATION_EXPORT ASAuthorizationPublicKeyCredentialResidentKeyPreference const ASAuthorizationPublicKeyCredentialResidentKeyPreferencePreferred API_AVAILABLE(macos(12.0));
FOUNDATION_EXPORT ASAuthorizationPublicKeyCredentialResidentKeyPreference const ASAuthorizationPublicKeyCredentialResidentKeyPreferenceRequired API_AVAILABLE(macos(12.0));

/* COSE algorithm and curve identifiers (RFC 9053). */
typedef NSInteger ASCOSEAlgorithmIdentifier NS_TYPED_EXTENSIBLE_ENUM;
static ASCOSEAlgorithmIdentifier const ASCOSEAlgorithmIdentifierES256 = -7;

typedef NSInteger ASCOSEEllipticCurveIdentifier NS_TYPED_EXTENSIBLE_ENUM;
static ASCOSEEllipticCurveIdentifier const ASCOSEEllipticCurveIdentifierP256 = 1;

/* Which kind of authenticator produced a credential. */
typedef NS_ENUM(NSInteger, ASAuthorizationPublicKeyCredentialAttachment) {
    ASAuthorizationPublicKeyCredentialAttachmentPlatform,
    ASAuthorizationPublicKeyCredentialAttachmentCrossPlatform,
} API_AVAILABLE(macos(13.5));

/* Shared protocols. */

API_AVAILABLE(macos(12.0))
@protocol ASPublicKeyCredential <ASAuthorizationCredential>
@property (nonatomic, readonly, copy) NSData *rawClientDataJSON;
@property (nonatomic, readonly, copy) NSData *credentialID;
@end

API_AVAILABLE(macos(12.0))
@protocol ASAuthorizationPublicKeyCredentialDescriptor <NSObject, NSSecureCoding, NSCopying>
@property (nonatomic, copy) NSData *credentialID;
@end

API_AVAILABLE(macos(12.0))
@protocol ASAuthorizationPublicKeyCredentialRegistrationRequest <NSObject, NSSecureCoding, NSCopying>
@property (nonatomic, readonly, copy) NSString *relyingPartyIdentifier;
@property (nonatomic, copy) NSData *userID;
@property (nonatomic, copy) NSString *name;
@property (nonatomic, copy, nullable) NSString *displayName;
@property (nonatomic, copy) NSData *challenge;
@property (nonatomic, copy) ASAuthorizationPublicKeyCredentialUserVerificationPreference userVerificationPreference;
@property (nonatomic, copy) ASAuthorizationPublicKeyCredentialAttestationKind attestationPreference;
@end

API_AVAILABLE(macos(12.0))
@protocol ASAuthorizationPublicKeyCredentialAssertionRequest <NSObject, NSSecureCoding, NSCopying>
@property (nonatomic, copy) NSData *challenge;
@property (nonatomic, copy) NSString *relyingPartyIdentifier;
@property (nonatomic, copy) NSArray<id<ASAuthorizationPublicKeyCredentialDescriptor>> *allowedCredentials;
@property (nonatomic, copy) ASAuthorizationPublicKeyCredentialUserVerificationPreference userVerificationPreference;
@end

API_AVAILABLE(macos(12.0))
@protocol ASAuthorizationPublicKeyCredentialRegistration <ASPublicKeyCredential>
@property (nonatomic, readonly, copy, nullable) NSData *rawAttestationObject;
@end

API_AVAILABLE(macos(12.0))
@protocol ASAuthorizationPublicKeyCredentialAssertion <ASPublicKeyCredential>
@property (nonatomic, readonly, copy) NSData *rawAuthenticatorData;
@property (nonatomic, readonly, copy) NSData *userID;
@property (nonatomic, readonly, copy) NSData *signature;
@end

/* One entry of a registration's list of acceptable algorithms. */
API_AVAILABLE(macos(12.0))
@interface ASAuthorizationPublicKeyCredentialParameters : NSObject <NSSecureCoding, NSCopying>
- (instancetype)initWithAlgorithm:(ASCOSEAlgorithmIdentifier)algorithm NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic, readonly) ASCOSEAlgorithmIdentifier algorithm;
@end

/* largeBlob extension. */

typedef NS_ENUM(NSInteger, ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirement) {
    ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirementRequired,
    ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirementPreferred,
} API_AVAILABLE(macos(14.0));

typedef NS_ENUM(NSInteger, ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperation) {
    ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperationRead,
    ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperationWrite,
} API_AVAILABLE(macos(14.0));

API_AVAILABLE(macos(14.0))
@interface ASAuthorizationPublicKeyCredentialLargeBlobRegistrationInput : NSObject
- (instancetype)initWithSupportRequirement:(ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirement)requirement NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic) ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirement supportRequirement;
@end

API_AVAILABLE(macos(14.0))
@interface ASAuthorizationPublicKeyCredentialLargeBlobRegistrationOutput : NSObject
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic, readonly) BOOL isSupported;
@end

API_AVAILABLE(macos(14.0))
@interface ASAuthorizationPublicKeyCredentialLargeBlobAssertionInput : NSObject
- (instancetype)initWithOperation:(ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperation)operation NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic, readonly) ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperation operation;
/* Only used by the write operation. */
@property (nonatomic, copy, nullable) NSData *dataToWrite;
@end

API_AVAILABLE(macos(14.0))
@interface ASAuthorizationPublicKeyCredentialLargeBlobAssertionOutput : NSObject
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
/* The blob, after a read operation. */
@property (nonatomic, readonly, copy, nullable) NSData *readData;
/* Whether a write operation stored the blob. */
@property (nonatomic, readonly) BOOL didWrite;
@end

/* prf extension (hmac-secret). */

API_AVAILABLE(macos(15.0))
@interface ASAuthorizationPublicKeyCredentialPRFAssertionInputValues : NSObject
- (instancetype)initWithSaltInput1:(NSData *)saltInput1 saltInput2:(nullable NSData *)saltInput2 NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic, readonly, copy) NSData *saltInput1;
@property (nonatomic, readonly, copy, nullable) NSData *saltInput2;
@end

API_AVAILABLE(macos(15.0))
@interface ASAuthorizationPublicKeyCredentialPRFAssertionInput : NSObject
- (instancetype)initWithInputValues:(nullable ASAuthorizationPublicKeyCredentialPRFAssertionInputValues *)inputValues
           perCredentialInputValues:(nullable NSDictionary<NSData *, ASAuthorizationPublicKeyCredentialPRFAssertionInputValues *> *)perCredentialInputValues NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic, readonly, nullable) ASAuthorizationPublicKeyCredentialPRFAssertionInputValues *inputValues;
@property (nonatomic, readonly, nullable) NSDictionary<NSData *, ASAuthorizationPublicKeyCredentialPRFAssertionInputValues *> *perCredentialInputValues;
@end

API_AVAILABLE(macos(15.0))
@interface ASAuthorizationPublicKeyCredentialPRFAssertionOutput : NSObject
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic, readonly, copy) NSData *first;
@property (nonatomic, readonly, copy, nullable) NSData *second;
@end

API_AVAILABLE(macos(15.0))
@interface ASAuthorizationPublicKeyCredentialPRFRegistrationInput : NSObject
/* Asks only whether the new credential supports the extension. */
+ (instancetype)checkForSupport;
/* Asks for support and, if the authenticator can, evaluates the salts. */
- (instancetype)initWithInputValues:(nullable ASAuthorizationPublicKeyCredentialPRFAssertionInputValues *)inputValues NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic, readonly) BOOL shouldCheckForSupport;
@property (nonatomic, readonly, nullable) ASAuthorizationPublicKeyCredentialPRFAssertionInputValues *inputValues;
@end

API_AVAILABLE(macos(15.0))
@interface ASAuthorizationPublicKeyCredentialPRFRegistrationOutput : NSObject
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic, readonly) BOOL isSupported;
@property (nonatomic, readonly, copy, nullable) NSData *first;
@property (nonatomic, readonly, copy, nullable) NSData *second;
@end

NS_ASSUME_NONNULL_END
