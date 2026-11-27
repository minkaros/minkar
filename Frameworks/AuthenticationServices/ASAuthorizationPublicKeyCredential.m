/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/*
 * Public key credential constants, algorithm parameters and the large
 * blob and PRF extension inputs and outputs.
 */

#import "ASAuthorizationInternal.h"

ASAuthorizationPublicKeyCredentialUserVerificationPreference const ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred = @"preferred";
ASAuthorizationPublicKeyCredentialUserVerificationPreference const ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired = @"required";
ASAuthorizationPublicKeyCredentialUserVerificationPreference const ASAuthorizationPublicKeyCredentialUserVerificationPreferenceDiscouraged = @"discouraged";

ASAuthorizationPublicKeyCredentialAttestationKind const ASAuthorizationPublicKeyCredentialAttestationKindNone = @"none";
ASAuthorizationPublicKeyCredentialAttestationKind const ASAuthorizationPublicKeyCredentialAttestationKindDirect = @"direct";
ASAuthorizationPublicKeyCredentialAttestationKind const ASAuthorizationPublicKeyCredentialAttestationKindIndirect = @"indirect";
ASAuthorizationPublicKeyCredentialAttestationKind const ASAuthorizationPublicKeyCredentialAttestationKindEnterprise = @"enterprise";

ASAuthorizationPublicKeyCredentialResidentKeyPreference const ASAuthorizationPublicKeyCredentialResidentKeyPreferenceDiscouraged = @"discouraged";
ASAuthorizationPublicKeyCredentialResidentKeyPreference const ASAuthorizationPublicKeyCredentialResidentKeyPreferencePreferred = @"preferred";
ASAuthorizationPublicKeyCredentialResidentKeyPreference const ASAuthorizationPublicKeyCredentialResidentKeyPreferenceRequired = @"required";

@implementation ASAuthorizationPublicKeyCredentialParameters

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithAlgorithm:(ASCOSEAlgorithmIdentifier)algorithm {
    if ((self = [super init]) != nil)
        _algorithm = algorithm;
    return self;
}

/* Immutable. */
- (id)copyWithZone:(NSZone *)zone {
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeInteger:_algorithm forKey:@"algorithm"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self initWithAlgorithm:[coder decodeIntegerForKey:@"algorithm"]];
}

- (BOOL)isEqual:(id)object {
    if (![object isKindOfClass:[ASAuthorizationPublicKeyCredentialParameters class]])
        return NO;
    return ((ASAuthorizationPublicKeyCredentialParameters *)object).algorithm == _algorithm;
}

- (NSUInteger)hash {
    return (NSUInteger)_algorithm;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@: %p; algorithm = %ld>", [self class], self, (long)_algorithm];
}

@end

@implementation ASAuthorizationPublicKeyCredentialLargeBlobRegistrationInput

- (instancetype)initWithSupportRequirement:(ASAuthorizationPublicKeyCredentialLargeBlobSupportRequirement)requirement {
    if ((self = [super init]) != nil)
        _supportRequirement = requirement;
    return self;
}

@end

@implementation ASAuthorizationPublicKeyCredentialLargeBlobRegistrationOutput

- (instancetype)_initWithSupported:(BOOL)isSupported {
    if ((self = [super init]) != nil)
        _isSupported = isSupported;
    return self;
}

@end

@implementation ASAuthorizationPublicKeyCredentialLargeBlobAssertionInput

- (instancetype)initWithOperation:(ASAuthorizationPublicKeyCredentialLargeBlobAssertionOperation)operation {
    if ((self = [super init]) != nil)
        _operation = operation;
    return self;
}

@end

@implementation ASAuthorizationPublicKeyCredentialLargeBlobAssertionOutput

- (instancetype)_initWithReadData:(NSData *)readData didWrite:(BOOL)didWrite {
    if ((self = [super init]) != nil) {
        _readData = [readData copy];
        _didWrite = didWrite;
    }
    return self;
}

@end

@implementation ASAuthorizationPublicKeyCredentialPRFAssertionInputValues

- (instancetype)initWithSaltInput1:(NSData *)saltInput1 saltInput2:(NSData *)saltInput2 {
    if ((self = [super init]) != nil) {
        _saltInput1 = [saltInput1 copy];
        _saltInput2 = [saltInput2 copy];
    }
    return self;
}

@end

@implementation ASAuthorizationPublicKeyCredentialPRFAssertionInput

- (instancetype)initWithInputValues:(ASAuthorizationPublicKeyCredentialPRFAssertionInputValues *)inputValues
           perCredentialInputValues:(NSDictionary<NSData *, ASAuthorizationPublicKeyCredentialPRFAssertionInputValues *> *)perCredentialInputValues {
    if ((self = [super init]) != nil) {
        _inputValues = inputValues;
        _perCredentialInputValues = [perCredentialInputValues copy];
    }
    return self;
}

@end

@implementation ASAuthorizationPublicKeyCredentialPRFAssertionOutput

- (instancetype)_initWithFirst:(NSData *)first second:(NSData *)second {
    if ((self = [super init]) != nil) {
        _first = [first copy];
        _second = [second copy];
    }
    return self;
}

@end

@implementation ASAuthorizationPublicKeyCredentialPRFRegistrationInput

+ (instancetype)checkForSupport {
    return [[self alloc] initWithInputValues:nil];
}

/*
 * Support is always reported; the salts, when given, are evaluated too
 * if the new credential can.
 */
- (instancetype)initWithInputValues:(ASAuthorizationPublicKeyCredentialPRFAssertionInputValues *)inputValues {
    if ((self = [super init]) != nil) {
        _shouldCheckForSupport = YES;
        _inputValues = inputValues;
    }
    return self;
}

@end

@implementation ASAuthorizationPublicKeyCredentialPRFRegistrationOutput

- (instancetype)_initWithSupported:(BOOL)isSupported first:(NSData *)first second:(NSData *)second {
    if ((self = [super init]) != nil) {
        _isSupported = isSupported;
        _first = [first copy];
        _second = [second copy];
    }
    return self;
}

@end
