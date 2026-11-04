/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * launchservicesd's database: the applications installed, the document
 * types they declare and the type identifiers it knows (LSDatabase.c).
 */

#ifndef LSDATABASE_H
#define LSDATABASE_H

#include "LSSession.h"

/* Reads the application folders again when one of them changed. */
void LSDBRefresh(void);
/* The applications that open a file, best first (at most max), and the
 * file's type identifier. Returns how many. */
unsigned LSDBCopyHandlers(const LSSessionFile *file, char uti[128], LSSessionPath *apps,
    unsigned max);
/* The application with a bundle identifier; 0 when there is one. */
int LSDBAppForBundleIdentifier(const char *bundleIdentifier, char path[1024]);
/* Reads an application again, or adds it; 0 when it is one. */
int LSDBRegister(const char *path);

#endif
