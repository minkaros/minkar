/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Finder's file types of clippings and Internet locations, as items'
 * Finder information holds them (LSCopyItemInfoForURL gives the type).
 */

#ifndef __FINDER__
#define __FINDER__

/* Clippings and Internet locations. */
enum {
    kClippingCreator = 'drag',
    kClippingPictureType = 'clpp',
    kClippingTextType = 'clpt',
    kClippingSoundType = 'clps',
    kClippingUnknownType = 'clpu',
    kInternetLocationCreator = 'drag',
    kInternetLocationHTTP = 'ilht',
    kInternetLocationFTP = 'ilft',
    kInternetLocationFile = 'ilfi',
    kInternetLocationMail = 'ilma',
    kInternetLocationNNTP = 'ilnw',
    kInternetLocationAFP = 'ilaf',
    kInternetLocationAppleTalk = 'ilat',
    kInternetLocationNSL = 'ilns',
    kInternetLocationGeneric = 'ilge',
};

#endif /* __FINDER__ */
