/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Screen zoom. There is no screen zoom yet: it is never on, and focus
 * changes are accepted and have nothing to move.
 */

#ifndef __UNIVERSALACCESS__
#define __UNIVERSALACCESS__

#include <CoreGraphics/CoreGraphics.h>
#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef UInt32 UAZoomChangeFocusType;
enum {
    kUAZoomFocusTypeOther = 0,
    kUAZoomFocusTypeInsertionPoint = 1
};

Boolean UAZoomEnabled(void);
OSStatus UAZoomChangeFocus(const CGRect *inRect, const CGRect *inHighlightRect, UAZoomChangeFocusType inType);

#ifdef __cplusplus
}
#endif

#endif
