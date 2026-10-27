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

// The Foundation classes that adopt NSSecureCoding on macOS, for the
// secure-coding unarchiver (NSString, NSUUID and NSRegularExpression say
// so themselves). Minkar.

#import <Foundation/NSArray.h>
#import <Foundation/NSCharacterSet.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSError.h>
#import <Foundation/NSIndexSet.h>
#import <Foundation/NSLocale.h>
#import <Foundation/NSNull.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSTimeZone.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSValue.h>

#define SECURE(cls) \
    @implementation cls (NSSecureCodingSupport) \
    + (BOOL)supportsSecureCoding { return YES; } \
    @end

SECURE(NSArray)
SECURE(NSDictionary)
SECURE(NSSet)
SECURE(NSData)
SECURE(NSValue)
SECURE(NSDate)
SECURE(NSNull)
SECURE(NSURL)
SECURE(NSError)
SECURE(NSIndexSet)
SECURE(NSCharacterSet)
SECURE(NSTimeZone)
SECURE(NSLocale)
