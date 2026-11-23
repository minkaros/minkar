/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/AppKit.h>
#import <Security/Authorization.h>
#import <SecurityInterface/SFAuthorization.h>

typedef NS_ENUM(NSInteger, SFAuthorizationViewState) {
    SFAuthorizationStartupState,
    SFAuthorizationViewLockedState,
    SFAuthorizationViewInProgressState,
    SFAuthorizationViewUnlockedState,
};

@class SFAuthorizationView;

@protocol SFAuthorizationViewDelegate <NSObject>
@optional
- (void)authorizationViewDidAuthorize:(SFAuthorizationView *)view;
- (void)authorizationViewDidDeauthorize:(SFAuthorizationView *)view;
- (BOOL)authorizationViewShouldDeauthorize:(SFAuthorizationView *)view;
- (void)authorizationViewCreatedAuthorization:(SFAuthorizationView *)view;
- (void)authorizationViewReleasedAuthorization:(SFAuthorizationView *)view;
- (void)authorizationViewDidHide:(SFAuthorizationView *)view;
@end

/* The lock that guards a window's settings: closed, a click asks for the
   right (an administrator's name and password, by the authorization
   agent); open, a click gives it up. */
@interface SFAuthorizationView : NSView
- (void)setString:(AuthorizationString)authorizationString;
- (void)setAuthorizationRights:(const AuthorizationRights *)authorizationRights;
- (AuthorizationRights *)authorizationRights;
- (SFAuthorization *)authorization;
- (BOOL)updateStatus:(id)inSender;
- (void)setAutoupdate:(BOOL)autoupdate;
- (void)setAutoupdate:(BOOL)autoupdate interval:(NSTimeInterval)interval;
- (void)setDelegate:(id)delegate;
- (id)delegate;
- (SFAuthorizationViewState)authorizationState;
- (void)setEnabled:(BOOL)enabled;
- (BOOL)isEnabled;
- (BOOL)authorize:(id)inSender;
- (BOOL)deauthorize:(id)inSender;
- (void)setFlags:(AuthorizationFlags)flags;
@end
