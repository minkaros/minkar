/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef _HFSCOMPRESS_H_
#define _HFSCOMPRESS_H_

/* A file HFS+-compressed in place: 1 compressed, 0 left as it is, -1 on an
   error (the file kept as it was). */
int	hfs_compress_file(const char *path);

#endif
