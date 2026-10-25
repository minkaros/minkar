/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
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
 * libxpc's hooks into libSystem's initialization and fork(): what they owe
 * every process is its bootstrap port, the Mach port to launchd that
 * bootstrap_look_up(), launch_msg() and the rest of liblaunch talk to.
 * The kernel hands it over as the task's bootstrap special port, and a
 * forked child gets the one its parent set up for it (launchd gives each
 * job its own), so it is read again after fork().
 */

#include <xpc/xpc.h>

/* liblaunch (libbootstrap.c): sets bootstrap_port from the task. */
extern void bootstrap_init(void);

/* Called by libSystem's initializer, after libdispatch. */
void _libxpc_initializer(void);
void xpc_atfork_prepare(void);
void xpc_atfork_parent(void);
void xpc_atfork_child(void);

void
_libxpc_initializer(void)
{
	bootstrap_init();
}

void
xpc_atfork_prepare(void)
{
}

void
xpc_atfork_parent(void)
{
}

void
xpc_atfork_child(void)
{
	bootstrap_init();
}
