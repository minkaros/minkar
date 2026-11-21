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

#import <ScriptingBridge/SBApplication.h>
#import <Foundation/NSRaise.h>

/* Minkar: no application can be reached without an Open Scripting
 * Architecture (Phase 6): the constructors say so once and return nil,
 * as for an application that is not there. */
@implementation SBApplication

@synthesize delegate = _delegate;
@synthesize sendMode = _sendMode;
@synthesize timeout = _timeout;

+ (instancetype)applicationWithBundleIdentifier:(NSString *)ident {
    NSUnimplementedMethod();
    return nil;
}

+ (instancetype)applicationWithURL:(NSURL *)url {
    NSUnimplementedMethod();
    return nil;
}

+ (instancetype)applicationWithProcessIdentifier:(pid_t)pid {
    NSUnimplementedMethod();
    return nil;
}

- (instancetype)initWithBundleIdentifier:(NSString *)ident {
    NSUnimplementedMethod();
    return nil;
}

- (instancetype)initWithURL:(NSURL *)url {
    NSUnimplementedMethod();
    return nil;
}

- (instancetype)initWithProcessIdentifier:(pid_t)pid {
    NSUnimplementedMethod();
    return nil;
}

- (BOOL)isRunning {
    return NO;
}

- (void)activate {
    NSUnimplementedMethod();
}

@end
