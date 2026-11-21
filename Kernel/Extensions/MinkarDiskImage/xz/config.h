/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * liblzma's configuration for the kernel's copy of its xz decoder (LZMA
 * images, ULMO): the decoders and checks an xz stream may use, no
 * encoders, no threads, and no CPU-specific CRC routines (the kernel's
 * code keeps to general registers).
 */

#define HAVE_DECODERS 1
#define HAVE_DECODER_LZMA1 1
#define HAVE_DECODER_LZMA2 1
#define HAVE_DECODER_DELTA 1
#define HAVE_DECODER_X86 1
#define HAVE_CHECK_CRC32 1
#define HAVE_CHECK_CRC64 1
#define HAVE_CHECK_SHA256 1
#define HAVE_STDBOOL_H 1
#define HAVE__BOOL 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_LIMITS_H 1
#define HAVE_STRING_H 1
#define HAVE_STDLIB_H 1
#define SIZEOF_SIZE_T 8
#define TUKLIB_SYMBOL_PREFIX lzma_
