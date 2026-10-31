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

/*
 * <minkar/stub.h>: noisy stubs. A function or method that is not
 * implemented yet says so the first time it is called: one line in the
 * system log (subsystem org.minkar.stubs), and on stderr in DT mode
 * (OS_ACTIVITY_DT_MODE). The log then tells which missing pieces real
 * programs use, in the order they use them.
 */

#ifndef _MINKAR_STUB_H_
#define _MINKAR_STUB_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

/*
 * what: the stub's name, "-[Class method]" or "function()"; caller: an
 * address in the code that called it (its image is named in the log).
 */
void _minkar_stub(const char *what, const void *caller);

__END_DECLS

/* In a C function that is a stub; after the first call it costs a load. */
#define MINKAR_STUB() do {						\
	static int _minkar_stub_said;					\
	if (!__atomic_load_n(&_minkar_stub_said, __ATOMIC_RELAXED)) {	\
		__atomic_store_n(&_minkar_stub_said, 1, __ATOMIC_RELAXED); \
		_minkar_stub(__func__, __builtin_return_address(0));	\
	}								\
} while (0)

#endif /* !_MINKAR_STUB_H_ */
