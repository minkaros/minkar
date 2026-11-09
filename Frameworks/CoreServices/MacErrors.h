/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Error codes of the classic managers still in use.
 */

#ifndef __MACERRORS__
#define __MACERRORS__

enum {
    paramErr = -50,
    unimpErr = -4,
    memFullErr = -108,
    nilHandleErr = -109,
    memWZErr = -111,
    memLockedErr = -117,
    fnfErr = -43,
    dirNFErr = -120,
    ioErr = -36,
    eofErr = -39,
    permErr = -54,
    wrPermErr = -61,
    nsvErr = -35,
    bdNamErr = -37,
    tmfoErr = -42,
    opWrErr = -49,
    fnOpnErr = -38,
    resNotFound = -192,
    resFNotFound = -193,
    addResFailed = -194,
    rmvResFailed = -196,
    resAttrErr = -198,
    mapReadErr = -199,
    gestaltUnknownErr = -5550,
    gestaltUndefSelectorErr = -5551,
    gestaltDupSelectorErr = -5552,
    gestaltLocationErr = -5553,
    afpAccessDenied = -5000,
    procNotFound = -600,
    connectionInvalid = -609,
    diffVolErr = -1303,
    errFSRefsDifferent = -1420
};

#endif
