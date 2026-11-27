/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The user's accessibility settings apps follow themselves.
 */

#import <Foundation/Foundation.h>

/* Whether the text insertion point should stay lit instead of blinking:
   the AXPrefersNonBlinkingTextInsertionIndicator key of the global
   preferences. Changes are announced through the default notification
   center and the distributed one. */
FOUNDATION_EXPORT BOOL AXPrefersNonBlinkingTextInsertionIndicator(void) API_AVAILABLE(macos(15.0));
FOUNDATION_EXPORT NSNotificationName const AXPrefersNonBlinkingTextInsertionIndicatorDidChangeNotification
    API_AVAILABLE(macos(15.0));
