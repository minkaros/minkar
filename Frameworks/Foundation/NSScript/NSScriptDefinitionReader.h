// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/Foundation.h>

/* An sdef (scripting definition) as suites of the dictionary format
   (.scriptSuite): each suite's Name, AppleEventCode, Classes and
   Commands. A class is keyed by its sdef name, its Cocoa class its
   ImplementationClassName. */
NSArray<NSDictionary *> *NSScriptSuitesFromDefinition(NSData *data);
