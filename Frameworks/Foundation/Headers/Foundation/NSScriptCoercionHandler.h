// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSObject.h>

NS_ASSUME_NONNULL_BEGIN

/* Converts scripting values from one class to another by the coercers
   registered for the pair. */
@interface NSScriptCoercionHandler : NSObject

+ (NSScriptCoercionHandler *)sharedCoercionHandler;
- (nullable id)coerceValue:(id)value toClass:(Class)toClass;
- (void)registerCoercer:(id)coercer selector:(SEL)selector toConvertFromClass:(Class)fromClass toClass:(Class)toClass;

@end

NS_ASSUME_NONNULL_END
