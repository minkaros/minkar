/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/*
 * Requests to loginwindow (WindowServer plan, section 10.3): a plain Mach
 * message to its service, sent without waiting. Apps send them from the
 * system menu; loginwindow asks every app to quit, then ends the session
 * and logs in again, restarts or shuts down. Root and the console user may
 * send them.
 */

#ifndef LOGINWINDOW_REQUEST_H
#define LOGINWINDOW_REQUEST_H

#include <mach/mach.h>
#include <stdint.h>

#define kLoginWindowServiceName	"org.minkar.loginwindow"
#define kLoginWindowRequestID	0x4c57524b	/* 'LWRK' */

enum {
	kLoginWindowLogOut	= 1,
	kLoginWindowRestart	= 2,
	kLoginWindowShutDown	= 3,
};

typedef struct LoginWindowRequest {
	mach_msg_header_t	header;
	uint32_t		request;	/* kLoginWindow* */
} LoginWindowRequest;

#endif
