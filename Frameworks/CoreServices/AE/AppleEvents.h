/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Apple event handlers and sending. Events sent to this process go to its
 * handlers; there is no transport to other processes yet, and sending
 * there fails with procNotFound (-600).
 */

#ifndef __AE_APPLEEVENTS__
#define __AE_APPLEEVENTS__

#include "AEDataModel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef OSErr (*AEEventHandlerProcPtr)(const AppleEvent *theAppleEvent, AppleEvent *reply, SRefCon handlerRefcon);
typedef AEEventHandlerProcPtr AEEventHandlerUPP;

AEEventHandlerUPP NewAEEventHandlerUPP(AEEventHandlerProcPtr userRoutine);
void DisposeAEEventHandlerUPP(AEEventHandlerUPP userUPP);
OSErr InvokeAEEventHandlerUPP(const AppleEvent *theAppleEvent, AppleEvent *reply, SRefCon handlerRefcon, AEEventHandlerUPP userUPP);

OSErr AEInstallEventHandler(AEEventClass theAEEventClass, AEEventID theAEEventID, AEEventHandlerUPP handler,
                            SRefCon handlerRefcon, Boolean isSysHandler);
OSErr AERemoveEventHandler(AEEventClass theAEEventClass, AEEventID theAEEventID, AEEventHandlerUPP handler, Boolean isSysHandler);
OSErr AEGetEventHandler(AEEventClass theAEEventClass, AEEventID theAEEventID, AEEventHandlerUPP *handler,
                        SRefCon *handlerRefcon, Boolean isSysHandler);

/* Sends the event: one addressed to this process is handled here and its
   reply filled in. */
OSStatus AESendMessage(const AppleEvent *event, AppleEvent *reply, AESendMode sendMode, long timeOutInTicks);
/* Hands an event to this process's handler for it. */
OSErr AEDispatchAppleEvent(const AppleEvent *theAppleEvent, AppleEvent *reply);

/* Keys of the editor suite's events (a document's position and the text
   searched for in it). */
enum {
    keyAEPosition = 'kpos',
    keyAESearchText = 'stxt',
};

#ifdef __cplusplus
}
#endif

#endif
