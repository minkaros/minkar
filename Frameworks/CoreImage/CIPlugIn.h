/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>
#import <CoreImage/CoreImageDefines.h>

NS_ASSUME_NONNULL_BEGIN

/* Image units: bundles (.plugin) in the Graphics/Image Units folders of
   the system, the computer and the user, each listing the filters it
   adds (CIPlugInFilterList in its Info.plist). Loaded, a unit's filters
   are registered with CIFilter by name. */
CORE_IMAGE_CLASS_EXPORT
@interface CIPlugIn : NSObject

+ (void)loadAllPlugIns;
+ (void)loadNonExecutablePlugIns;
+ (void)loadNonExecutablePlugIn:(NSURL *)url;
+ (void)loadPlugIn:(NSURL *)url allowExecutableCode:(BOOL)allowExecutableCode;
+ (void)loadPlugIn:(NSURL *)url allowNonExecutable:(BOOL)allowNonExecutable;

@end

NS_ASSUME_NONNULL_END
