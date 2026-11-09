/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * HTML to an attributed string (private to AppKit).
 */

#import <Foundation/NSObject.h>

@class NSAttributedString, NSData, NSDictionary, NSURL;

/* Marks the character an anchor (an id, or <a name>) of the page is at. */
extern NSString *const NSHTMLAnchorAttributeName;

@interface NSHTMLReader : NSObject
+ (NSAttributedString *)attributedStringWithHTML:(NSData *)data baseURL:(NSURL *)baseURL
    documentAttributes:(NSDictionary **)documentAttributes;
@end
