/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Gestalt: the system version (the compatibility version programs compare
 * against), memory and the architecture.
 */

#ifndef __GESTALT__
#define __GESTALT__

#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef OSErr (*SelectorFunctionProcPtr)(OSType selector, SInt32 *response);
typedef SelectorFunctionProcPtr SelectorFunctionUPP;

enum {
    gestaltSystemVersion = 'sysv',
    gestaltSystemVersionMajor = 'sys1',
    gestaltSystemVersionMinor = 'sys2',
    gestaltSystemVersionBugFix = 'sys3',
    gestaltPhysicalRAMSize = 'ram ',
    gestaltPhysicalRAMSizeInMegabytes = 'ramm',
    gestaltLogicalRAMSize = 'lram',
    gestaltSysArchitecture = 'sysa',
    gestaltNativeCPUtype = 'cput',
    gestaltNativeCPUfamily = 'cpuf',
    gestaltProcClkSpeed = 'pclk',
    gestaltProcClkSpeedMHz = 'mclk',
    gestaltCountOfCPUs = 'cpus',
    gestaltLogicalPageSize = 'pgsz',
    gestaltAddressingModeAttr = 'addr'
};

enum {
    gestalt68k = 1,
    gestaltPowerPC = 2,
    gestaltIntel = 10
};

enum {
    gestaltCPUPentium4 = 0x0405
};

OSErr Gestalt(OSType selector, SInt32 *response);
OSErr NewGestaltValue(OSType selector, SInt32 newValue);
OSErr ReplaceGestaltValue(OSType selector, SInt32 replacementValue);
OSErr SetGestaltValue(OSType selector, SInt32 newValue);
OSErr DeleteGestaltValue(OSType selector);

#ifdef __cplusplus
}
#endif

#endif
