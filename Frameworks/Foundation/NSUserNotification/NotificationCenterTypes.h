/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The notification center (Atlas.md, 5.6, N0): notificationd, a session
 * service, takes the notifications apps deliver (NSUserNotificationCenter,
 * and UserNotifications through it) and hands them to the presenter,
 * SystemUIServer, which shows a banner and plays the sound; a click on it
 * goes back through notificationd to the app.
 *
 * A notification is a binary property list, a dictionary with the NCKey
 * keys below. notificationd adds the app's process (NCKeyPID). Its
 * messages to an app and to the presenter are plain Mach messages, an
 * NCMessage with a byte payload.
 */

#ifndef _NOTIFICATIONCENTERTYPES_H_
#define _NOTIFICATIONCENTERTYPES_H_

#include <mach/message.h>
#include <stdint.h>

/* MIG's byte arrays. */
typedef char *nc_bytes_t;

#define kNCServiceName		"org.minkar.notificationd"
#define kNCMaxPayload		16384

enum {
	kNCSuccess = 0,
	kNCNoService = -1,
	kNCIllegalArgument = -2,
	kNCNotAllowed = -3,
	kNCNoPresenter = -4,
};

/* The dictionary's keys. */
#define kNCKey			"key"		/* the delivery's own, unique in the app */
#define kNCKeyIdentifier	"identifier"	/* the app's, may be absent */
#define kNCKeyTitle		"title"
#define kNCKeySubtitle		"subtitle"
#define kNCKeyText		"informativeText"
#define kNCKeySound		"sound"		/* a sound's name, "default", or absent */
#define kNCKeyBundleIdentifier	"bundleIdentifier"
#define kNCKeyAppName		"appName"
#define kNCKeyAppPath		"appPath"	/* its bundle, for the icon */
#define kNCKeyPID		"pid"		/* set by notificationd */
#define kNCKeyDate		"date"		/* set by the presenter: when it came */

/* The settings (N1), in the user's preferences domain kNCSettingsDomain:
   kNCSettingDoNotDisturb, and for each app that has posted one, under
   kNCSettingAppPrefix plus its bundle identifier (else its name), a
   dictionary of the keys below. System Preferences' Notifications pane
   writes them and posts kNCSettingsChanged (notify(3)); SystemUIServer
   adds the apps it has not seen, and follows them. */
#define kNCSettingsDomain	"org.minkar.notificationcenter"
#define kNCSettingsChanged	"org.minkar.notificationcenter.prefs"
#define kNCSettingDoNotDisturb	"doNotDisturb"
#define kNCSettingAppPrefix	"app."
#define kNCAppName		"name"
#define kNCAppPath		"path"
#define kNCAppAllowed		"allowed"	/* no: not even listed */
#define kNCAppStyle		"style"		/* "none", "banner" (the default), "alert" */
#define kNCAppSound		"sound"		/* no: never heard */

/* notificationd's messages. */
enum {
	kNCMessageActivated = 29650,	/* to an app: payload, the key; arg, the activation type */
	kNCMessageShow = 29651,		/* to the presenter: payload, the notification */
	kNCMessageRemove = 29652,	/* to the presenter: payload, the key; arg, the pid */
};

typedef struct {
	mach_msg_header_t header;
	int32_t arg;
	uint32_t length;
	char payload[kNCMaxPayload];
} NCMessage;

/* A message received: room for the trailer. */
typedef struct {
	NCMessage message;
	mach_msg_max_trailer_t trailer;
} NCReceivedMessage;

#endif
