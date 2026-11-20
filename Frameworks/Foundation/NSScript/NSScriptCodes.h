// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSString.h>
#import <MacTypes.h>

/* A four-character code from a suite's string ("capp"; "ID  " kept as
   is), or from a number; 0 for none. */
static inline FourCharCode NSScriptFourCharCode(id value) {
    if ([value isKindOfClass:[NSNumber class]])
        return (FourCharCode)[value unsignedIntValue];
    if (![value isKindOfClass:[NSString class]])
        return 0;
    NSData *bytes = [value dataUsingEncoding:NSMacOSRomanStringEncoding allowLossyConversion:YES];
    if (bytes.length < 4)
        return 0;
    const unsigned char *b = bytes.bytes;
    return ((FourCharCode)b[0] << 24) | ((FourCharCode)b[1] << 16) | ((FourCharCode)b[2] << 8) | b[3];
}

/* The first four characters as a code, the next four as another (a
   command's "coreclos"). */
static inline FourCharCode NSScriptFourCharCodeAt(NSString *value, NSUInteger offset) {
    if (value.length < offset + 4)
        return 0;
    return NSScriptFourCharCode([value substringWithRange:NSMakeRange(offset, 4)]);
}
