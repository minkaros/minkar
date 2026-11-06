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

#include <stddef.h>
#include "LSSession.h"

/* Reads the application folders again when one of them changed. */
void LSDBRefresh(void);
/* The applications that open a file, best first (at most max), and the
 * file's type identifier. Returns how many. */
unsigned LSDBCopyHandlers(const LSSessionFile *file, char uti[128], LSSessionPath *apps,
    unsigned max);
/* The applications for a type identifier with the roles (0: any), best
 * first (interface version 5). */
unsigned LSDBCopyHandlersForType(const char *uti, unsigned roles, LSSessionPath *apps,
    unsigned max);
/* The applications for a URL scheme (CFBundleURLTypes). */
unsigned LSDBCopyHandlersForScheme(const char *scheme, LSSessionPath *apps, unsigned max);
/* Whether a type is another or conforms to it. */
int LSDBConformsTo(const char *uti, const char *other);
/* A type's declaration as four lines: what it conforms to, its
 * extensions, its MIME types (space-separated) and its description;
 * 0 when it is known. */
int LSDBCopyDeclaration(const char *uti, char *buffer, size_t size);
/* The types with a tag of a class (0: filename extension, 1: MIME type),
 * conforming to a type (or any: ""), one per line, built-in first. 0 when
 * there is one. */
int LSDBCopyTypesForTag(unsigned tagClass, const char *tag, const char *conforming,
    char *buffer, size_t size);
/* The application with a bundle identifier; 0 when there is one. */
int LSDBAppForBundleIdentifier(const char *bundleIdentifier, char path[1024]);
/* Reads an application again, or adds it; 0 when it is one. */
int LSDBRegister(const char *path);

#endif
