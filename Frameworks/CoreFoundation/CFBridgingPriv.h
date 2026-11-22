/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Private bridging for frameworks whose CF types are also Objective-C
 * classes (OpenDirectory's ODNode, ODQuery...): instances of a CF type made
 * after the call get the class named, once Foundation has loaded it.
 */

#ifndef __COREFOUNDATION_CFBRIDGINGPRIV__
#define __COREFOUNDATION_CFBRIDGINGPRIV__

#include <CoreFoundation/CFBase.h>
/* Their CF types are CF runtime classes. */
#include <CoreFoundation/CFRuntime.h>

CF_EXTERN_C_BEGIN

/* The Objective-C class named for the type's instances; nothing when no
   class of that name is loaded. */
CF_EXPORT void _CFRuntimeBridgeClasses(CFTypeID typeID, const char *className);
/* CFRuntime.c: whether obj is an Objective-C object of the type's family
   that is not one of its CF instances. */
CF_EXPORT Boolean _CFIsObjC(CFTypeID typeID, void *obj);

CF_EXTERN_C_END

/* A CF function's body sent as a message to an object of the type's
   Objective-C family that is not a CF instance (a subclass of the bridged
   class): CF_OBJC_FUNCDISPATCH returns the message's result, CF_OBJC_CALL
   stores it in var. Integer and pointer arguments only. */
#include <objc/message.h>
#include <objc/runtime.h>

#define CF_IS_OBJC(typeID, obj) _CFIsObjC((typeID), (void *)(obj))
#define CF_OBJC_CALL(rettype, var, obj, sel, ...) \
	((var) = ((rettype (*)(const void *, SEL, ...))objc_msgSend)((const void *)(obj), \
	    sel_registerName(sel), ##__VA_ARGS__))
#define CF_OBJC_FUNCDISPATCH(typeID, rettype, obj, sel, ...) do { \
	if (CF_IS_OBJC(typeID, obj)) \
		return ((rettype (*)(const void *, SEL, ...))objc_msgSend)((const void *)(obj), \
		    sel_registerName(sel), ##__VA_ARGS__); \
	} while (0)

CF_EXTERN_C_BEGIN

CF_EXTERN_C_END

#endif
