/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <LocalAuthentication/LocalAuthentication.h>

NSString *const LAErrorDomain = @"com.apple.LocalAuthentication";

@implementation LAContext {
    BOOL _invalidated;
}

/* Why a policy cannot be evaluated here: no biometric sensor is driven,
   and there is no dialog yet for the user's password. */
- (NSError *)_errorForPolicy:(LAPolicy)policy {
    LAError code;
    NSString *reason;
    if (_invalidated) {
        code = LAErrorInvalidContext;
        reason = @"The context has been invalidated.";
    } else if (policy == LAPolicyDeviceOwnerAuthentication) {
        code = LAErrorPasscodeNotSet;
        reason = @"No way to confirm the user's password is available.";
    } else {
        code = LAErrorBiometryNotAvailable;
        reason = @"No biometric sensor is available.";
    }
    return [NSError errorWithDomain:LAErrorDomain code:code
                           userInfo:@{ NSLocalizedDescriptionKey: reason }];
}

- (BOOL)canEvaluatePolicy:(LAPolicy)policy error:(NSError * __autoreleasing *)error {
    if (error != NULL)
        *error = [self _errorForPolicy:policy];
    return NO;
}

- (void)evaluatePolicy:(LAPolicy)policy localizedReason:(NSString *)localizedReason
                 reply:(void (^)(BOOL, NSError *))reply {
    NSError *error = [self _errorForPolicy:policy];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        reply(NO, error);
    });
}

- (void)invalidate {
    _invalidated = YES;
}

- (LABiometryType)biometryType {
    return LABiometryTypeNone;
}

- (NSData *)evaluatedPolicyDomainState {
    return nil;
}

@end
