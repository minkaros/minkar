/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Another application, reached through its scripting dictionary. */

#import <Foundation/Foundation.h>
#import <ScriptingBridge/SBObject.h>

NS_ASSUME_NONNULL_BEGIN

@protocol SBApplicationDelegate;

@interface SBApplication : SBObject <NSCoding>
+ (nullable instancetype)applicationWithBundleIdentifier:(NSString *)ident;
+ (nullable instancetype)applicationWithURL:(NSURL *)url;
+ (nullable instancetype)applicationWithProcessIdentifier:(pid_t)pid;
- (nullable instancetype)initWithBundleIdentifier:(NSString *)ident;
- (nullable instancetype)initWithURL:(NSURL *)url;
- (nullable instancetype)initWithProcessIdentifier:(pid_t)pid;
@property (readonly, getter=isRunning) BOOL running;
- (void)activate;
@property (nullable, weak) id <SBApplicationDelegate> delegate;
@property AESendMode sendMode;
@property long timeout;
@end

@protocol SBApplicationDelegate <NSObject>
- (nullable id)eventDidFail:(const void *)event withError:(NSError *)error;
@end

NS_ASSUME_NONNULL_END
