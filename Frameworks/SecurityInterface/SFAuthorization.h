/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>
#import <Security/Authorization.h>

/* An authorization, as an object: Authorization's calls on its reference. */
@interface SFAuthorization : NSObject <NSCoding>
+ (instancetype)authorization;
+ (instancetype)authorizationWithFlags:(AuthorizationFlags)flags rights:(const AuthorizationRights *)rights
                           environment:(const AuthorizationEnvironment *)environment;
- (instancetype)initWithFlags:(AuthorizationFlags)flags rights:(const AuthorizationRights *)rights
                  environment:(const AuthorizationEnvironment *)environment;
- (AuthorizationRef)authorizationRef;
- (BOOL)obtainWithRight:(AuthorizationString)rightName flags:(AuthorizationFlags)flags error:(NSError **)error;
- (BOOL)obtainWithRights:(const AuthorizationRights *)rights flags:(AuthorizationFlags)flags
             environment:(const AuthorizationEnvironment *)environment
        authorizedRights:(AuthorizationRights **)authorizedRights error:(NSError **)error;
- (void)invalidateCredentials;
@end
