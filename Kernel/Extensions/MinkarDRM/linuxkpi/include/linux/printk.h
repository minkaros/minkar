/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _LKPI_LINUX_PRINTK_H
#define _LKPI_LINUX_PRINTK_H

#include "lkpi_os.h"

/* Levels are not prefixed to the text here: printk logs at info. */
#define KERN_EMERG	""
#define KERN_ALERT	""
#define KERN_CRIT	""
#define KERN_ERR	""
#define KERN_WARNING	""
#define KERN_NOTICE	""
#define KERN_INFO	""
#define KERN_DEBUG	""
#define KERN_CONT	""

#define pr_err(...)	lkpi_log(3, __VA_ARGS__)
#define pr_warn(...)	lkpi_log(4, __VA_ARGS__)
#define pr_info(...)	lkpi_log(6, __VA_ARGS__)
#define pr_cont(...)	lkpi_log(6, __VA_ARGS__)
#define pr_debug(...)	lkpi_log(7, __VA_ARGS__)
#define printk(...)	lkpi_log(6, __VA_ARGS__)

#endif
