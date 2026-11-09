/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CoreText's objects are Objective-C objects with a CF type each:
 * CFGetTypeID asks the object (-_cfTypeID) and gets the type registered
 * for its class.
 */

#import <CoreFoundation/CoreFoundation.h>

extern CFTypeID _CFRuntimeRegisterClass(const void *runtimeClass);

#define CT_DEFINE_TYPE(name, className) \
   static const struct { CFIndex version; const char *cls; void *functions[16]; } name##RuntimeClass={ 0, className, { NULL } }; \
   CFTypeID name##GetTypeID(void) { \
      static CFTypeID typeID; \
      if(typeID==0) \
       typeID=_CFRuntimeRegisterClass(&name##RuntimeClass); \
      return typeID; \
   } \
   @interface name (CFType) \
   @end \
   @implementation name (CFType) \
   -(CFTypeID)_cfTypeID { return name##GetTypeID(); } \
   @end
