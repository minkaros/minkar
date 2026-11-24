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

/* Files compressed by a thread a processor, as they are queued; finish
   waits for them and says how many failed (each told to failed). */
typedef void (*hfs_compress_failed_fn)(const char *path);
void	hfs_compress_start(hfs_compress_failed_fn failed);
void	hfs_compress_enqueue(const char *path);
int	hfs_compress_finish(void);

#endif
