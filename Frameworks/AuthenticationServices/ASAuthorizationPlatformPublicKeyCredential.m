/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/*
 * Platform public key credentials: provider, requests, descriptor and the
 * registration and assertion credentials. Minkar has no platform
 * authenticator yet, so ASAuthorizationController never returns these
 * credentials; the classes are complete so apps can build, copy and
 * archive requests.
 */

#import "ASAuthorizationInternal.h"

/* Archiving of the extension inputs a request carries. */

void _ASEncodeLargeBlobRegistrationInput(NSCoder *coder, ASAuthorizationPublicKeyCredentialLargeBlobRegistrationInput *input) {
    if (input == nil)
        return;
    [coder encodeBool:YES forKey:@"largeBlob"];
    [coder encodeInteger:input.supportRequirement forKey:@"largeBlobSupportRequirement"];
}

ASAuthorizationPublicKeyCredentialLargeBlobRegistrationInput *_ASDecodeLargeBlobRegistrationInput(NSCoder *coder) {
    if (![coder decodeBoolForKey:@"largeBlob"])
        return nil;
    return [[ASAuthorizationPublicKeyCredentialLargeBlobRegistrationInput alloc]
        initWithSupportRequirement:[coder decodeIntegerForKey:@"largeBlobSupportRequirement"]];
}

void _ASEncodeLargeBlobAssertionInput(NSCoder *coder, ASAuthorizationPublicKeyCredentialLargeBlobAssertionInput *input) {
    if (input == nil)
        return;
    [coder encodeBool:YES forKey:@"largeBlob"];
    [coder encodeInteger:input.operation forKey:@"largeBlobOperation"];
    [coder encodeObject:input.dataToWrite forKey:@"largeBlobDataToWrite"];
}

ASAuthorizationPublicKeyCredentialLargeBlobAssertionInput *_ASDecodeLargeBlobAssertionInput(NSCoder *coder) {
    if (![coder decodeBoolForKey:@"largeBlob"])
        return nil;
    ASAuthorizationPublicKeyCredentialLargeBlobAssertionInput *input =
        [[ASAuthorizationPublicKeyCredentialLargeBlobAssertionInput alloc]
            initWithOperation:[coder decodeIntegerForKey:@"largeBlobOperation"]];
    input.dataToWrite = [coder decodeObjectOfClass:[NSData class] forKey:@"largeBlobDataToWrite"];
    return input;
}

static void encodeSalts(NSCoder *coder, ASAuthorizationPublicKeyCredentialPRFAssertionInputValues *values, NSString *prefix) {
    if (values == nil)
        return;
    [coder encodeBool:YES forKey:prefix];
    [coder encodeObject:values.saltInput1 forKey:[prefix stringByAppendingString:@"Salt1"]];
    [coder encodeObject:values.saltInput2 forKey:[prefix stringByAppendingString:@"Salt2"]];
}

static ASAuthorizationPublicKeyCredentialPRFAssertionInputValues *decodeSalts(NSCoder *coder, NSString *prefix) {
    if (![coder decodeBoolForKey:prefix])
        return nil;
    NSData *salt1 = [coder decodeObjectOfClass:[NSData class] forKey:[prefix stringByAppendingString:@"Salt1"]];
    NSData *salt2 = [coder decodeObjectOfClass:[NSData class] forKey:[prefix stringByAppendingString:@"Salt2"]];
    return [[ASAuthorizationPublicKeyCredentialPRFAssertionInputValues alloc] initWithSaltInput1:salt1 saltInput2:salt2];
}

void _ASEncodePRFRegistrationInput(NSCoder *coder, ASAuthorizationPublicKeyCredentialPRFRegistrationInput *input) {
    if (input == nil)
        return;
    [coder encodeBool:YES forKey:@"prf"];
    encodeSalts(coder, input.inputValues, @"prfInput");
}

ASAuthorizationPublicKeyCredentialPRFRegistrationInput *_ASDecodePRFRegistrationInput(NSCoder *coder) {
    if (![coder decodeBoolForKey:@"prf"])
        return nil;
    return [[ASAuthorizationPublicKeyCredentialPRFRegistrationInput alloc] initWithInputValues:decodeSalts(coder, @"prfInput")];
}

void _ASEncodePRFAssertionInput(NSCoder *coder, ASAuthorizationPublicKeyCredentialPRFAssertionInput *input) {
    if (input == nil)
        return;
    [coder encodeBool:YES forKey:@"prf"];
    encodeSalts(coder, input.inputValues, @"prfInput");
    NSDictionary<NSData *, ASAuthorizationPublicKeyCredentialPRFAssertionInputValues *> *perCredential = input.perCredentialInputValues;
    if (perCredential == nil)
        return;
    NSMutableArray<NSData *> *ids = [NSMutableArray arrayWithCapacity:perCredential.count];
    NSMutableArray<NSData *> *firsts = [NSMutableArray arrayWithCapacity:perCredential.count];
    NSMutableArray *seconds = [NSMutableArray arrayWithCapacity:perCredential.count];
    [perCredential enumerateKeysAndObjectsUsingBlock:^(NSData *credentialID, ASAuthorizationPublicKeyCredentialPRFAssertionInputValues *values, BOOL *stop) {
        [ids addObject:credentialID];
        [firsts addObject:values.saltInput1 ?: [NSData data]];
        [seconds addObject:values.saltInput2 ?: (id)[NSNull null]];
    }];
    [coder encodeObject:ids forKey:@"prfPerCredentialIDs"];
    [coder encodeObject:firsts forKey:@"prfPerCredentialSalt1"];
    [coder encodeObject:seconds forKey:@"prfPerCredentialSalt2"];
}

ASAuthorizationPublicKeyCredentialPRFAssertionInput *_ASDecodePRFAssertionInput(NSCoder *coder) {
    if (![coder decodeBoolForKey:@"prf"])
        return nil;
    NSSet *classes = [NSSet setWithObjects:[NSArray class], [NSData class], [NSNull class], nil];
    NSArray *ids = [coder decodeObjectOfClasses:classes forKey:@"prfPerCredentialIDs"];
    NSArray *firsts = [coder decodeObjectOfClasses:classes forKey:@"prfPerCredentialSalt1"];
    NSArray *seconds = [coder decodeObjectOfClasses:classes forKey:@"prfPerCredentialSalt2"];
    NSMutableDictionary *perCredential = nil;
    if (ids != nil && ids.count == firsts.count && ids.count == seconds.count) {
        perCredential = [NSMutableDictionary dictionaryWithCapacity:ids.count];
        for (NSUInteger i = 0; i < ids.count; i++) {
            id second = seconds[i];
            perCredential[ids[i]] = [[ASAuthorizationPublicKeyCredentialPRFAssertionInputValues alloc]
                initWithSaltInput1:firsts[i]
                        saltInput2:second == [NSNull null] ? nil : second];
        }
    }
    return [[ASAuthorizationPublicKeyCredentialPRFAssertionInput alloc] initWithInputValues:decodeSalts(coder, @"prfInput")
                                                                  perCredentialInputValues:perCredential];
}

@implementation ASAuthorizationPlatformPublicKeyCredentialDescriptor

@synthesize credentialID = _credentialID;

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCredentialID:(NSData *)credentialID {
    if ((self = [super init]) != nil)
        _credentialID = [credentialID copy];
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    return [[[self class] allocWithZone:zone] initWithCredentialID:_credentialID];
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_credentialID forKey:@"credentialID"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    NSData *credentialID = [coder decodeObjectOfClass:[NSData class] forKey:@"credentialID"];
    return [self initWithCredentialID:credentialID ?: [NSData data]];
}

- (BOOL)isEqual:(id)object {
    if (![object isKindOfClass:[ASAuthorizationPlatformPublicKeyCredentialDescriptor class]])
        return NO;
    return [((ASAuthorizationPlatformPublicKeyCredentialDescriptor *)object).credentialID isEqualToData:_credentialID];
}

- (NSUInteger)hash {
    return _credentialID.hash;
}

@end

@implementation ASAuthorizationPlatformPublicKeyCredentialProvider

- (instancetype)initWithRelyingPartyIdentifier:(NSString *)relyingPartyIdentifier {
    if ((self = [super init]) != nil)
        _relyingPartyIdentifier = [relyingPartyIdentifier copy];
    return self;
}

- (ASAuthorizationPlatformPublicKeyCredentialRegistrationRequest *)createCredentialRegistrationRequestWithChallenge:(NSData *)challenge
                                                                                                             name:(NSString *)name
                                                                                                           userID:(NSData *)userID {
    ASAuthorizationPlatformPublicKeyCredentialRegistrationRequest *request =
        [[ASAuthorizationPlatformPublicKeyCredentialRegistrationRequest alloc] _initWithProvider:self];
    request.challenge = challenge;
    request.name = name;
    request.userID = userID;
    return request;
}

- (ASAuthorizationPlatformPublicKeyCredentialAssertionRequest *)createCredentialAssertionRequestWithChallenge:(NSData *)challenge {
    ASAuthorizationPlatformPublicKeyCredentialAssertionRequest *request =
        [[ASAuthorizationPlatformPublicKeyCredentialAssertionRequest alloc] _initWithProvider:self];
    request.challenge = challenge;
    request.relyingPartyIdentifier = _relyingPartyIdentifier;
    return request;
}

@end

@implementation ASAuthorizationPlatformPublicKeyCredentialRegistrationRequest

@synthesize userID = _userID;
@synthesize name = _name;
@synthesize displayName = _displayName;
@synthesize challenge = _challenge;
@synthesize userVerificationPreference = _userVerificationPreference;
@synthesize attestationPreference = _attestationPreference;

+ (BOOL)supportsSecureCoding {
    return YES;
}

/* The WebAuthn specification's defaults. */
- (instancetype)_initWithProvider:(id<ASAuthorizationProvider>)provider {
    if ((self = [super _initWithProvider:provider]) != nil) {
        _userID = [NSData data];
        _name = @"";
        _challenge = [NSData data];
        _userVerificationPreference = ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred;
        _attestationPreference = ASAuthorizationPublicKeyCredentialAttestationKindNone;
        _shouldShowHybridTransport = YES;
    }
    return self;
}

- (NSString *)relyingPartyIdentifier {
    return ((ASAuthorizationPlatformPublicKeyCredentialProvider *)self.provider).relyingPartyIdentifier;
}

- (id)copyWithZone:(NSZone *)zone {
    ASAuthorizationPlatformPublicKeyCredentialRegistrationRequest *copy =
        [[[self class] allocWithZone:zone] _initWithProvider:self.provider];
    copy.userID = _userID;
    copy.name = _name;
    copy.displayName = _displayName;
    copy.challenge = _challenge;
    copy.userVerificationPreference = _userVerificationPreference;
    copy.attestationPreference = _attestationPreference;
    copy.shouldShowHybridTransport = _shouldShowHybridTransport;
    copy.largeBlob = _largeBlob;
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
    [coder encodeBool:_shouldShowHybridTransport forKey:@"shouldShowHybridTransport"];
    _ASEncodeLargeBlobRegistrationInput(coder, _largeBlob);
    _ASEncodePRFRegistrationInput(coder, _prf);
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    NSString *rp = [coder decodeObjectOfClass:[NSString class] forKey:@"relyingPartyIdentifier"] ?: @"";
    ASAuthorizationPlatformPublicKeyCredentialProvider *provider =
        [[ASAuthorizationPlatformPublicKeyCredentialProvider alloc] initWithRelyingPartyIdentifier:rp];
    if ((self = [self _initWithProvider:provider]) == nil)
        return nil;
    _userID = [coder decodeObjectOfClass:[NSData class] forKey:@"userID"] ?: _userID;
    _name = [coder decodeObjectOfClass:[NSString class] forKey:@"name"] ?: _name;
    _displayName = [coder decodeObjectOfClass:[NSString class] forKey:@"displayName"];
    _challenge = [coder decodeObjectOfClass:[NSData class] forKey:@"challenge"] ?: _challenge;
    _userVerificationPreference = [coder decodeObjectOfClass:[NSString class] forKey:@"userVerificationPreference"] ?: _userVerificationPreference;
    _attestationPreference = [coder decodeObjectOfClass:[NSString class] forKey:@"attestationPreference"] ?: _attestationPreference;
    if ([coder containsValueForKey:@"shouldShowHybridTransport"])
        _shouldShowHybridTransport = [coder decodeBoolForKey:@"shouldShowHybridTransport"];
    _largeBlob = _ASDecodeLargeBlobRegistrationInput(coder);
    _prf = _ASDecodePRFRegistrationInput(coder);
    return self;
}

@end

@implementation ASAuthorizationPlatformPublicKeyCredentialAssertionRequest

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
        _relyingPartyIdentifier = [((ASAuthorizationPlatformPublicKeyCredentialProvider *)provider).relyingPartyIdentifier copy] ?: @"";
        _allowedCredentials = @[];
        _userVerificationPreference = ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred;
        _shouldShowHybridTransport = YES;
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    ASAuthorizationPlatformPublicKeyCredentialAssertionRequest *copy =
        [[[self class] allocWithZone:zone] _initWithProvider:self.provider];
    copy.challenge = _challenge;
    copy.relyingPartyIdentifier = _relyingPartyIdentifier;
    copy.allowedCredentials = [[NSArray alloc] initWithArray:_allowedCredentials copyItems:YES];
    copy.userVerificationPreference = _userVerificationPreference;
    copy.shouldShowHybridTransport = _shouldShowHybridTransport;
    copy.largeBlob = _largeBlob;
    copy.prf = _prf;
    return copy;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_challenge forKey:@"challenge"];
    [coder encodeObject:_relyingPartyIdentifier forKey:@"relyingPartyIdentifier"];
    [coder encodeObject:_allowedCredentials forKey:@"allowedCredentials"];
    [coder encodeObject:_userVerificationPreference forKey:@"userVerificationPreference"];
    [coder encodeBool:_shouldShowHybridTransport forKey:@"shouldShowHybridTransport"];
    _ASEncodeLargeBlobAssertionInput(coder, _largeBlob);
    _ASEncodePRFAssertionInput(coder, _prf);
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    NSString *rp = [coder decodeObjectOfClass:[NSString class] forKey:@"relyingPartyIdentifier"] ?: @"";
    ASAuthorizationPlatformPublicKeyCredentialProvider *provider =
        [[ASAuthorizationPlatformPublicKeyCredentialProvider alloc] initWithRelyingPartyIdentifier:rp];
    if ((self = [self _initWithProvider:provider]) == nil)
        return nil;
    _challenge = [coder decodeObjectOfClass:[NSData class] forKey:@"challenge"] ?: _challenge;
    NSSet *classes = [NSSet setWithObjects:[NSArray class], [ASAuthorizationPlatformPublicKeyCredentialDescriptor class], nil];
    _allowedCredentials = [coder decodeObjectOfClasses:classes forKey:@"allowedCredentials"] ?: _allowedCredentials;
    _userVerificationPreference = [coder decodeObjectOfClass:[NSString class] forKey:@"userVerificationPreference"] ?: _userVerificationPreference;
    if ([coder containsValueForKey:@"shouldShowHybridTransport"])
        _shouldShowHybridTransport = [coder decodeBoolForKey:@"shouldShowHybridTransport"];
    _largeBlob = _ASDecodeLargeBlobAssertionInput(coder);
    _prf = _ASDecodePRFAssertionInput(coder);
    return self;
}

@end

@implementation ASAuthorizationPlatformPublicKeyCredentialRegistration

@synthesize rawClientDataJSON = _rawClientDataJSON;
@synthesize credentialID = _credentialID;
@synthesize rawAttestationObject = _rawAttestationObject;

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)_initWithRawClientDataJSON:(NSData *)rawClientDataJSON
                              credentialID:(NSData *)credentialID
                      rawAttestationObject:(NSData *)rawAttestationObject
                                attachment:(ASAuthorizationPublicKeyCredentialAttachment)attachment
                                 largeBlob:(ASAuthorizationPublicKeyCredentialLargeBlobRegistrationOutput *)largeBlob
                                       prf:(ASAuthorizationPublicKeyCredentialPRFRegistrationOutput *)prf {
    if ((self = [super init]) != nil) {
        _rawClientDataJSON = [rawClientDataJSON copy] ?: [NSData data];
        _credentialID = [credentialID copy] ?: [NSData data];
        _rawAttestationObject = [rawAttestationObject copy];
        _attachment = attachment;
        _largeBlob = largeBlob;
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
    [coder encodeInteger:_attachment forKey:@"attachment"];
    if (_largeBlob != nil) {
        [coder encodeBool:YES forKey:@"largeBlob"];
        [coder encodeBool:_largeBlob.isSupported forKey:@"largeBlobSupported"];
    }
    if (_prf != nil) {
        [coder encodeBool:YES forKey:@"prf"];
        [coder encodeBool:_prf.isSupported forKey:@"prfSupported"];
        [coder encodeObject:_prf.first forKey:@"prfFirst"];
        [coder encodeObject:_prf.second forKey:@"prfSecond"];
    }
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    ASAuthorizationPublicKeyCredentialLargeBlobRegistrationOutput *largeBlob = nil;
    if ([coder decodeBoolForKey:@"largeBlob"])
        largeBlob = [[ASAuthorizationPublicKeyCredentialLargeBlobRegistrationOutput alloc]
            _initWithSupported:[coder decodeBoolForKey:@"largeBlobSupported"]];
    ASAuthorizationPublicKeyCredentialPRFRegistrationOutput *prf = nil;
    if ([coder decodeBoolForKey:@"prf"])
        prf = [[ASAuthorizationPublicKeyCredentialPRFRegistrationOutput alloc]
            _initWithSupported:[coder decodeBoolForKey:@"prfSupported"]
                         first:[coder decodeObjectOfClass:[NSData class] forKey:@"prfFirst"]
                        second:[coder decodeObjectOfClass:[NSData class] forKey:@"prfSecond"]];
    return [self _initWithRawClientDataJSON:[coder decodeObjectOfClass:[NSData class] forKey:@"rawClientDataJSON"]
                               credentialID:[coder decodeObjectOfClass:[NSData class] forKey:@"credentialID"]
                       rawAttestationObject:[coder decodeObjectOfClass:[NSData class] forKey:@"rawAttestationObject"]
                                 attachment:[coder decodeIntegerForKey:@"attachment"]
                                  largeBlob:largeBlob
                                        prf:prf];
}

@end

@implementation ASAuthorizationPlatformPublicKeyCredentialAssertion

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
                                attachment:(ASAuthorizationPublicKeyCredentialAttachment)attachment
                                 largeBlob:(ASAuthorizationPublicKeyCredentialLargeBlobAssertionOutput *)largeBlob
                                       prf:(ASAuthorizationPublicKeyCredentialPRFAssertionOutput *)prf {
    if ((self = [super init]) != nil) {
        _rawClientDataJSON = [rawClientDataJSON copy] ?: [NSData data];
        _credentialID = [credentialID copy] ?: [NSData data];
        _rawAuthenticatorData = [rawAuthenticatorData copy] ?: [NSData data];
        _userID = [userID copy] ?: [NSData data];
        _signature = [signature copy] ?: [NSData data];
        _attachment = attachment;
        _largeBlob = largeBlob;
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
    [coder encodeInteger:_attachment forKey:@"attachment"];
    if (_largeBlob != nil) {
        [coder encodeBool:YES forKey:@"largeBlob"];
        [coder encodeObject:_largeBlob.readData forKey:@"largeBlobReadData"];
        [coder encodeBool:_largeBlob.didWrite forKey:@"largeBlobDidWrite"];
    }
    if (_prf != nil) {
        [coder encodeBool:YES forKey:@"prf"];
        [coder encodeObject:_prf.first forKey:@"prfFirst"];
        [coder encodeObject:_prf.second forKey:@"prfSecond"];
    }
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    ASAuthorizationPublicKeyCredentialLargeBlobAssertionOutput *largeBlob = nil;
    if ([coder decodeBoolForKey:@"largeBlob"])
        largeBlob = [[ASAuthorizationPublicKeyCredentialLargeBlobAssertionOutput alloc]
            _initWithReadData:[coder decodeObjectOfClass:[NSData class] forKey:@"largeBlobReadData"]
                     didWrite:[coder decodeBoolForKey:@"largeBlobDidWrite"]];
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
                                 attachment:[coder decodeIntegerForKey:@"attachment"]
                                  largeBlob:largeBlob
                                        prf:prf];
}

@end
