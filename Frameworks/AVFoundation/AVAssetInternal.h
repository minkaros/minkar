/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * What the reader and the player need of an asset: its file, opened again
 * for a decoding session of their own, and its description.
 */

#import <AVFoundation/AVAsset.h>
#import "MDClient.h"

@interface AVAsset (Internal)
/* Loads the description once; NO, and the error, when it cannot be read. */
- (BOOL)_loadReturningError:(NSError **)outError;
/* The file's description, from a session of its own (subclasses read
   other sources). */
- (NSDictionary *)_copyInfoReturningError:(NSError **)outError;
/* A new decoding session on the asset's file, or nil and the error. */
- (MDSessionRef)_createSessionReturningError:(NSError **)outError;
@end

/* An NSError for a media decoder result. */
NSError *_AVErrorForDecoderResult(int32_t result, NSURL *URL);
