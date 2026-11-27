/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/*
 * Security-key public key credentials: provider, requests, descriptor and
 * the registration and assertion credentials. Minkar has no CTAP
 * transport (USB HID, NFC or Bluetooth) yet, so ASAuthorizationController
 * never returns these credentials; the classes are complete so apps can
 * build, copy and archive requests.
 */

#import "ASAuthorizationInternal.h"

ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport const ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportUSB = @"usb";
ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport const ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportNFC = @"nfc";
ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport const ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportBluetooth = @"ble";

@implementation ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor

@synthesize credentialID = _credentialID;

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCredentialID:(NSData *)credentialID
                          transports:(NSArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport> *)allowedTransports {
    if ((self = [super init]) != nil) {
        _credentialID = [credentialID copy];
        _transports = [allowedTransports copy] ?: @[];
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    return [[[self class] allocWithZone:zone] initWithCredentialID:_credentialID transports:_transports];
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_credentialID forKey:@"credentialID"];
    [coder encodeObject:_transports forKey:@"transports"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    NSData *credentialID = [coder decodeObjectOfClass:[NSData class] forKey:@"credentialID"];
    NSArray *transports = [coder decodeObjectOfClasses:[NSSet setWithObjects:[NSArray class], [NSString class], nil]
                                                forKey:@"transports"];
    return [self initWithCredentialID:credentialID ?: [NSData data] transports:transports ?: @[]];
}

- (BOOL)isEqual:(id)object {
    if (![object isKindOfClass:[ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor class]])
        return NO;
    ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor *other = object;
    return [other.credentialID isEqualToData:_credentialID] && [other.transports isEqualToArray:_transports];
}

- (NSUInteger)hash {
    return _credentialID.hash;
}

@end

@implementation ASAuthorizationSecurityKeyPublicKeyCredentialProvider

- (instancetype)initWithRelyingPartyIdentifier:(NSString *)relyingPartyIdentifier {
    if ((self = [super init]) != nil)
        _relyingPartyIdentifier = [relyingPartyIdentifier copy];
    return self;
}

- (ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest *)createCredentialRegistrationRequestWithChallenge:(NSData *)challenge
                                                                                                          displayName:(NSString *)displayName
                                                                                                                 name:(NSString *)name
                                                                                                               userID:(NSData *)userID {
    ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest *request =
        [[ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest alloc] _initWithProvider:self];
    request.challenge = challenge;
    request.displayName = displayName;
    request.name = name;
    request.userID = userID;
    return request;
}

- (ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest *)createCredentialAssertionRequestWithChallenge:(NSData *)challenge {
    ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest *request =
        [[ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest alloc] _initWithProvider:self];
    request.challenge = challenge;
    request.relyingPartyIdentifier = _relyingPartyIdentifier;
    return request;
}

@end

@implementation ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest

@synthesize userID = _userID;
@synthesize name = _name;
@synthesize displayName = _displayName;
@synthesize challenge = _challenge;
@synthesize userVerificationPreference = _userVerificationPreference;
@synthesize attestationPreference = _attestationPreference;

+ (BOOL)supportsSecureCoding {
    return YES;
}

/*
 * The WebAuthn specification's defaults; with no algorithm given, ES256
 * is asked for.
 */
- (instancetype)_initWithProvider:(id<ASAuthorizationProvider>)provider {
    if ((self = [super _initWithProvider:provider]) != nil) {
        _userID = [NSData data];
        _name = @"";
        _challenge = [NSData data];
        _userVerificationPreference = ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred;
        _attestationPreference = ASAuthorizationPublicKeyCredentialAttestationKindNone;
        _credentialParameters = @[ [[ASAuthorizationPublicKeyCredentialParameters alloc] initWithAlgorithm:ASCOSEAlgorithmIdentifierES256] ];
        _excludedCredentials = @[];
        _residentKeyPreference = ASAuthorizationPublicKeyCredentialResidentKeyPreferenceDiscouraged;
    }
    return self;
}

- (NSString *)relyingPartyIdentifier {
    return ((ASAuthorizationSecurityKeyPublicKeyCredentialProvider *)self.provider).relyingPartyIdentifier;
}

- (id)copyWithZone:(NSZone *)zone {
    ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest *copy =
        [[[self class] allocWithZone:zone] _initWithProvider:self.provider];
    copy.userID = _userID;
    copy.name = _name;
    copy.displayName = _displayName;
    copy.challenge = _challenge;
    copy.userVerificationPreference = _userVerificationPreference;
    copy.attestationPreference = _attestationPreference;
    copy.credentialParameters = _credentialParameters;
    copy.excludedCredentials = [[NSArray alloc] initWithArray:_excludedCredentials copyItems:YES];
    copy.residentKeyPreference = _residentKeyPreference;
    copy.prf = _prf;
    return copy;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.relyingPartyIdentifier forKey:@"relyingPartyIdentifier"];
    [coder encodeObject:_userID forKey:@"userID"];
    [coder encodeObject:_name forKey:@"name"];
    [coder encodeObject:_displayName forKey:@"displayName"];
    [coder encodeObject:_challenge forKey:@"challenge"];
    [coder encodeObject:_userVerificationPreference forKey:@"userVerificationPreference"];
    [coder encodeObject:_attestationPreference forKey:@"attestationPreference"];
    [coder encodeObject:_credentialParameters forKey:@"credentialParameters"];
    [coder encodeObject:_excludedCredentials forKey:@"excludedCredentials"];
    [coder encodeObject:_residentKeyPreference forKey:@"residentKeyPreference"];
    _ASEncodePRFRegistrationInput(coder, _prf);
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    NSString *rp = [coder decodeObjectOfClass:[NSString class] forKey:@"relyingPartyIdentifier"] ?: @"";
    ASAuthorizationSecurityKeyPublicKeyCredentialProvider *provider =
        [[ASAuthorizationSecurityKeyPublicKeyCredentialProvider alloc] initWithRelyingPartyIdentifier:rp];
    if ((self = [self _initWithProvider:provider]) == nil)
        return nil;
    _userID = [coder decodeObjectOfClass:[NSData class] forKey:@"userID"] ?: _userID;
    _name = [coder decodeObjectOfClass:[NSString class] forKey:@"name"] ?: _name;
    _displayName = [coder decodeObjectOfClass:[NSString class] forKey:@"displayName"];
    _challenge = [coder decodeObjectOfClass:[NSData class] forKey:@"challenge"] ?: _challenge;
    _userVerificationPreference = [coder decodeObjectOfClass:[NSString class] forKey:@"userVerificationPreference"] ?: _userVerificationPreference;
    _attestationPreference = [coder decodeObjectOfClass:[NSString class] forKey:@"attestationPreference"] ?: _attestationPreference;
    _credentialParameters = [coder decodeObjectOfClasses:[NSSet setWithObjects:[NSArray class], [ASAuthorizationPublicKeyCredentialParameters class], nil]
                                                  forKey:@"credentialParameters"] ?: _credentialParameters;
    _excludedCredentials = [coder decodeObjectOfClasses:[NSSet setWithObjects:[NSArray class], [ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor class], nil]
                                                 forKey:@"excludedCredentials"] ?: _excludedCredentials;
    _residentKeyPreference = [coder decodeObjectOfClass:[NSString class] forKey:@"residentKeyPreference"] ?: _residentKeyPreference;
    _prf = _ASDecodePRFRegistrationInput(coder);
    return self;
}

@end

@implementation ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest

@synthesize challenge = _challenge;
@synthesize relyingPartyIdentifier = _relyingPartyIdentifier;
@synthesize allowedCredentials = _allowedCredentials;
@synthesize userVerificationPreference = _userVerificationPreference;

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)_initWithProvider:(id<ASAuthorizationProvider>)provider {
    if ((self = [super _initWithProvider:provider]) != nil) {
        _challenge = [NSData data];
        _relyingPartyIdentifier = [((ASAuthorizationSecurityKeyPublicKeyCredentialProvider *)provider).relyingPartyIdentifier copy] ?: @"";
        _allowedCredentials = @[];
        _userVerificationPreference = ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred;
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest *copy =
        [[[self class] allocWithZone:zone] _initWithProvider:self.provider];
    copy.challenge = _challenge;
    copy.relyingPartyIdentifier = _relyingPartyIdentifier;
    copy.allowedCredentials = [[NSArray alloc] initWithArray:_allowedCredentials copyItems:YES];
    copy.userVerificationPreference = _userVerificationPreference;
    copy.appID = _appID;
    copy.prf = _prf;
    return copy;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_challenge forKey:@"challenge"];
    [coder encodeObject:_relyingPartyIdentifier forKey:@"relyingPartyIdentifier"];
    [coder encodeObject:_allowedCredentials forKey:@"allowedCredentials"];
    [coder encodeObject:_userVerificationPreference forKey:@"userVerificationPreference"];
    [coder encodeObject:_appID forKey:@"appID"];
    _ASEncodePRFAssertionInput(coder, _prf);
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    NSString *rp = [coder decodeObjectOfClass:[NSString class] forKey:@"relyingPartyIdentifier"] ?: @"";
    ASAuthorizationSecurityKeyPublicKeyCredentialProvider *provider =
        [[ASAuthorizationSecurityKeyPublicKeyCredentialProvider alloc] initWithRelyingPartyIdentifier:rp];
    if ((self = [self _initWithProvider:provider]) == nil)
        return nil;
    _challenge = [coder decodeObjectOfClass:[NSData class] forKey:@"challenge"] ?: _challenge;
    _allowedCredentials = [coder decodeObjectOfClasses:[NSSet setWithObjects:[NSArray class], [ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor class], nil]
                                                forKey:@"allowedCredentials"] ?: _allowedCredentials;
    _userVerificationPreference = [coder decodeObjectOfClass:[NSString class] forKey:@"userVerificationPreference"] ?: _userVerificationPreference;
    _appID = [coder decodeObjectOfClass:[NSString class] forKey:@"appID"];
    _prf = _ASDecodePRFAssertionInput(coder);
    return self;
}

@end

@implementation ASAuthorizationSecurityKeyPublicKeyCredentialRegistration

@synthesize rawClientDataJSON = _rawClientDataJSON;
@synthesize credentialID = _credentialID;
@synthesize rawAttestationObject = _rawAttestationObject;

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)_initWithRawClientDataJSON:(NSData *)rawClientDataJSON
                              credentialID:(NSData *)credentialID
                      rawAttestationObject:(NSData *)rawAttestationObject
                                transports:(NSArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransport> *)transports
                                       prf:(ASAuthorizationPublicKeyCredentialPRFRegistrationOutput *)prf {
    if ((self = [super init]) != nil) {
        _rawClientDataJSON = [rawClientDataJSON copy] ?: [NSData data];
        _credentialID = [credentialID copy] ?: [NSData data];
        _rawAttestationObject = [rawAttestationObject copy];
        _transports = [transports copy] ?: @[];
        _prf = prf;
    }
    return self;
}

/* Immutable. */
- (id)copyWithZone:(NSZone *)zone {
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_rawClientDataJSON forKey:@"rawClientDataJSON"];
    [coder encodeObject:_credentialID forKey:@"credentialID"];
    [coder encodeObject:_rawAttestationObject forKey:@"rawAttestationObject"];
    [coder encodeObject:_transports forKey:@"transports"];
    if (_prf != nil) {
        [coder encodeBool:YES forKey:@"prf"];
        [coder encodeBool:_prf.isSupported forKey:@"prfSupported"];
        [coder encodeObject:_prf.first forKey:@"prfFirst"];
        [coder encodeObject:_prf.second forKey:@"prfSecond"];
    }
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    ASAuthorizationPublicKeyCredentialPRFRegistrationOutput *prf = nil;
    if ([coder decodeBoolForKey:@"prf"])
        prf = [[ASAuthorizationPublicKeyCredentialPRFRegistrationOutput alloc]
            _initWithSupported:[coder decodeBoolForKey:@"prfSupported"]
                         first:[coder decodeObjectOfClass:[NSData class] forKey:@"prfFirst"]
                        second:[coder decodeObjectOfClass:[NSData class] forKey:@"prfSecond"]];
    NSArray *transports = [coder decodeObjectOfClasses:[NSSet setWithObjects:[NSArray class], [NSString class], nil]
                                                forKey:@"transports"];
    return [self _initWithRawClientDataJSON:[coder decodeObjectOfClass:[NSData class] forKey:@"rawClientDataJSON"]
                               credentialID:[coder decodeObjectOfClass:[NSData class] forKey:@"credentialID"]
                       rawAttestationObject:[coder decodeObjectOfClass:[NSData class] forKey:@"rawAttestationObject"]
                                 transports:transports ?: @[]
                                        prf:prf];
}

@end

@implementation ASAuthorizationSecurityKeyPublicKeyCredentialAssertion

@synthesize rawClientDataJSON = _rawClientDataJSON;
@synthesize credentialID = _credentialID;
@synthesize rawAuthenticatorData = _rawAuthenticatorData;
@synthesize userID = _userID;
@synthesize signature = _signature;

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)_initWithRawClientDataJSON:(NSData *)rawClientDataJSON
                              credentialID:(NSData *)credentialID
                      rawAuthenticatorData:(NSData *)rawAuthenticatorData
                                    userID:(NSData *)userID
                                 signature:(NSData *)signature
                                     appID:(BOOL)appID
                                       prf:(ASAuthorizationPublicKeyCredentialPRFAssertionOutput *)prf {
    if ((self = [super init]) != nil) {
        _rawClientDataJSON = [rawClientDataJSON copy] ?: [NSData data];
        _credentialID = [credentialID copy] ?: [NSData data];
        _rawAuthenticatorData = [rawAuthenticatorData copy] ?: [NSData data];
        _userID = [userID copy] ?: [NSData data];
        _signature = [signature copy] ?: [NSData data];
        _appID = appID;
        _prf = prf;
    }
    return self;
}

/* Immutable. */
- (id)copyWithZone:(NSZone *)zone {
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_rawClientDataJSON forKey:@"rawClientDataJSON"];
    [coder encodeObject:_credentialID forKey:@"credentialID"];
    [coder encodeObject:_rawAuthenticatorData forKey:@"rawAuthenticatorData"];
    [coder encodeObject:_userID forKey:@"userID"];
    [coder encodeObject:_signature forKey:@"signature"];
    [coder encodeBool:_appID forKey:@"appID"];
    if (_prf != nil) {
        [coder encodeBool:YES forKey:@"prf"];
        [coder encodeObject:_prf.first forKey:@"prfFirst"];
        [coder encodeObject:_prf.second forKey:@"prfSecond"];
    }
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    ASAuthorizationPublicKeyCredentialPRFAssertionOutput *prf = nil;
    if ([coder decodeBoolForKey:@"prf"])
        prf = [[ASAuthorizationPublicKeyCredentialPRFAssertionOutput alloc]
            _initWithFirst:[coder decodeObjectOfClass:[NSData class] forKey:@"prfFirst"] ?: [NSData data]
                    second:[coder decodeObjectOfClass:[NSData class] forKey:@"prfSecond"]];
    return [self _initWithRawClientDataJSON:[coder decodeObjectOfClass:[NSData class] forKey:@"rawClientDataJSON"]
                               credentialID:[coder decodeObjectOfClass:[NSData class] forKey:@"credentialID"]
                       rawAuthenticatorData:[coder decodeObjectOfClass:[NSData class] forKey:@"rawAuthenticatorData"]
                                     userID:[coder decodeObjectOfClass:[NSData class] forKey:@"userID"]
                                  signature:[coder decodeObjectOfClass:[NSData class] forKey:@"signature"]
                                      appID:[coder decodeBoolForKey:@"appID"]
                                        prf:prf];
}

@end
