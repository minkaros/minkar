/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Apple event descriptors: typed data, lists, records and Apple events
 * (records with attributes), with coercions between the common types.
 */

#ifndef __AE_AEDATAMODEL__
#define __AE_AEDATAMODEL__

#include <CoreFoundation/CoreFoundation.h>
#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef FourCharCode DescType;
typedef FourCharCode AEKeyword;
typedef FourCharCode AEEventClass;
typedef FourCharCode AEEventID;
typedef SInt16 AEReturnID;
typedef SInt32 AETransactionID;
typedef SInt32 AESendMode;
typedef SInt32 AESendPriority;

typedef struct OpaqueAEDataStorageType *AEDataStorageType;
typedef AEDataStorageType *AEDataStorage;

struct AEDesc {
    DescType descriptorType;
    AEDataStorage dataHandle;
};
typedef struct AEDesc AEDesc;
typedef AEDesc *AEDescPtr;
typedef AEDesc AEAddressDesc;
typedef AEDesc AEDescList;
typedef AEDesc AERecord;
typedef AEDesc AppleEvent;
typedef AppleEvent *AppleEventPtr;

typedef struct AEKeyDesc {
    AEKeyword descKey;
    AEDesc descContent;
} AEKeyDesc;

enum {
    typeBoolean = 'bool',
    typeChar = 'TEXT',
    typeSInt16 = 'shor',
    typeSInt32 = 'long',
    typeUInt16 = 'ushr',
    typeUInt32 = 'magn',
    typeSInt64 = 'comp',
    typeUInt64 = 'ucom',
    typeIEEE32BitFloatingPoint = 'sing',
    typeIEEE64BitFloatingPoint = 'doub',
    type128BitFloatingPoint = 'ldbl',
    typeDecimalStruct = 'decm',
    typeAEList = 'list',
    typeAERecord = 'reco',
    typeAppleEvent = 'aevt',
    typeEventRecord = 'evrc',
    typeTrue = 'true',
    typeFalse = 'fals',
    typeAlias = 'alis',
    typeEnumerated = 'enum',
    typeType = 'type',
    typeAppParameters = 'appa',
    typeProperty = 'prop',
    typeFSRef = 'fsrf',
    typeFileURL = 'furl',
    typeBookmarkData = 'bmrk',
    typeKeyword = 'keyw',
    typeSectionH = 'sect',
    typeWildCard = '****',
    typeApplSignature = 'sign',
    typeQDRectangle = 'qdrt',
    typeFixed = 'fixd',
    typeProcessSerialNumber = 'psn ',
    typeApplicationURL = 'aprl',
    typeNull = 'null',
    typeKernelProcessID = 'kpid',
    typeMachPort = 'port',
    typeAuditToken = 'tokn',
    typeApplicationBundleID = 'bund',
    typeUnicodeText = 'utxt',
    typeStyledUnicodeText = 'sutx',
    typeUTF8Text = 'utf8',
    typeUTF16ExternalRepresentation = 'ut16',
    typeData = 'tdta',
    typeLongDateTime = 'ldt ',
    typeVersion = 'vers',
    typeObjectSpecifier = 'obj '
};

enum {
    typeSMInt = typeSInt16,
    typeShortInteger = typeSInt16,
    typeInteger = typeSInt32,
    typeLongInteger = typeSInt32,
    typeMagnitude = typeUInt32,
    typeComp = typeSInt64,
    typeSMFloat = typeIEEE32BitFloatingPoint,
    typeShortFloat = typeIEEE32BitFloatingPoint,
    typeFloat = typeIEEE64BitFloatingPoint,
    typeLongFloat = typeIEEE64BitFloatingPoint,
    typeExtended = 'exte'
};

enum {
    keyTransactionIDAttr = 'tran',
    keyReturnIDAttr = 'rtid',
    keyEventClassAttr = 'evcl',
    keyEventIDAttr = 'evid',
    keyAddressAttr = 'addr',
    keyOptionalKeywordAttr = 'optk',
    keyTimeoutAttr = 'timo',
    keyInteractLevelAttr = 'inte',
    keyEventSourceAttr = 'esrc',
    keyMissedKeywordAttr = 'miss',
    keyOriginalAddressAttr = 'from',
    keyAcceptTimeoutAttr = 'actm',
    keyReplyRequestedAttr = 'repq',
    keySenderEUIDAttr = 'seid',
    keySenderEGIDAttr = 'sgid',
    keySenderUIDAttr = 'uids',
    keySenderGIDAttr = 'gids',
    keySenderPIDAttr = 'spid',
    keySenderAuditTokenAttr = 'tokn',
    keyDirectObject = '----',
    keyErrorNumber = 'errn',
    keyErrorString = 'errs',
    keyProcessSerialNumber = 'psn ',
    keyPreDispatch = 'phac',
    keySelectProc = 'selh',
    keyAERecorderCount = 'recr',
    keyAEVersion = 'vers'
};

enum {
    kAutoGenerateReturnID = -1,
    kAnyTransactionID = 0
};

enum {
    kAENoReply = 0x00000001,
    kAEQueueReply = 0x00000002,
    kAEWaitReply = 0x00000003,
    kAEDontReconnect = 0x00000080,
    kAEWantReceipt = 0x00000200,
    kAENeverInteract = 0x00000010,
    kAECanInteract = 0x00000020,
    kAEAlwaysInteract = 0x00000030,
    kAECanSwitchLayer = 0x00000040,
    kAEDontRecord = 0x00001000,
    kAEDontExecute = 0x00002000,
    kAEProcessNonReplyEvents = 0x00008000,
    kAEDoNotAutomaticallyAddAnnotationsToEvent = 0x00010000
};

enum {
    kAEDefaultTimeout = -1,
    kNoTimeOut = -2
};

enum {
    kCoreEventClass = 'aevt',
    kAEOpenApplication = 'oapp',
    kAEOpenDocuments = 'odoc',
    kAEPrintDocuments = 'pdoc',
    kAEOpenContents = 'ocon',
    kAEQuitApplication = 'quit',
    kAEAnswer = 'ansr',
    kAEApplicationDied = 'obit',
    kAEShowPreferences = 'pref',
    kAEReopenApplication = 'rapp',
    kInternetEventClass = 'GURL',
    kAEGetURL = 'GURL',
    kAEFetchURL = 'FURL',
    keyAEPropData = 'prdt',
    keyAEFile = 'kfil'
};

enum {
    errAECoercionFail = -1700,
    errAEDescNotFound = -1701,
    errAECorruptData = -1702,
    errAEWrongDataType = -1703,
    errAENotAEDesc = -1704,
    errAEBadListItem = -1705,
    errAENewerVersion = -1706,
    errAENotAppleEvent = -1707,
    errAEEventNotHandled = -1708,
    errAEReplyNotValid = -1709,
    errAEUnknownSendMode = -1710,
    errAEWaitCanceled = -1711,
    errAETimeout = -1712,
    errAENoUserInteraction = -1713,
    errAENotASpecialFunction = -1714,
    errAEParamMissed = -1715,
    errAEUnknownAddressType = -1716,
    errAEHandlerNotFound = -1717,
    errAEReplyNotArrived = -1718,
    errAEIllegalIndex = -1719,
    errAEEventFailed = -10000,
    errAEEventWouldRequireUserConsent = -1744
};

/* Descriptors. */
void AEInitializeDesc(AEDesc *desc);
#define AEInitializeDescInline(d) do { (d)->descriptorType = typeNull; (d)->dataHandle = NULL; } while (0)
OSErr AECreateDesc(DescType typeCode, const void *dataPtr, Size dataSize, AEDesc *result);
OSErr AEDisposeDesc(AEDesc *theAEDesc);
OSErr AEDuplicateDesc(const AEDesc *theAEDesc, AEDesc *result);
Size AEGetDescDataSize(const AEDesc *theAEDesc);
OSErr AEGetDescData(const AEDesc *theAEDesc, void *dataPtr, Size maximumSize);
OSErr AEReplaceDescData(DescType typeCode, const void *dataPtr, Size dataSize, AEDesc *theAEDesc);
Boolean AECheckIsRecord(const AEDesc *theDesc);

/* Coercions. */
OSErr AECoercePtr(DescType typeCode, const void *dataPtr, Size dataSize, DescType toType, AEDesc *result);
OSErr AECoerceDesc(const AEDesc *theAEDesc, DescType toType, AEDesc *result);

/* Lists and records. */
OSErr AECreateList(const void *factoringPtr, Size factoredSize, Boolean isRecord, AEDescList *resultList);
OSErr AECountItems(const AEDescList *theAEDescList, long *theCount);
OSErr AEPutPtr(AEDescList *theAEDescList, long index, DescType typeCode, const void *dataPtr, Size dataSize);
OSErr AEPutDesc(AEDescList *theAEDescList, long index, const AEDesc *theAEDesc);
OSErr AEGetNthPtr(const AEDescList *theAEDescList, long index, DescType desiredType, AEKeyword *theAEKeyword,
                  DescType *typeCode, void *dataPtr, Size maximumSize, Size *actualSize);
OSErr AEGetNthDesc(const AEDescList *theAEDescList, long index, DescType desiredType, AEKeyword *theAEKeyword, AEDesc *result);
OSErr AESizeOfNthItem(const AEDescList *theAEDescList, long index, DescType *typeCode, Size *dataSize);
OSErr AEDeleteItem(AEDescList *theAEDescList, long index);

OSErr AEPutKeyPtr(AERecord *theAERecord, AEKeyword theAEKeyword, DescType typeCode, const void *dataPtr, Size dataSize);
OSErr AEPutKeyDesc(AERecord *theAERecord, AEKeyword theAEKeyword, const AEDesc *theAEDesc);
OSErr AEGetKeyPtr(const AERecord *theAERecord, AEKeyword theAEKeyword, DescType desiredType, DescType *typeCode,
                  void *dataPtr, Size maximumSize, Size *actualSize);
OSErr AEGetKeyDesc(const AERecord *theAERecord, AEKeyword theAEKeyword, DescType desiredType, AEDesc *result);
OSErr AESizeOfKeyDesc(const AERecord *theAERecord, AEKeyword theAEKeyword, DescType *typeCode, Size *dataSize);
OSErr AEDeleteKeyDesc(AERecord *theAERecord, AEKeyword theAEKeyword);

/* Apple events: parameters (keyed items) and attributes. */
OSErr AECreateAppleEvent(AEEventClass theAEEventClass, AEEventID theAEEventID, const AEAddressDesc *target,
                         AEReturnID returnID, AETransactionID transactionID, AppleEvent *result);
OSErr AEPutParamPtr(AppleEvent *theAppleEvent, AEKeyword theAEKeyword, DescType typeCode, const void *dataPtr, Size dataSize);
OSErr AEPutParamDesc(AppleEvent *theAppleEvent, AEKeyword theAEKeyword, const AEDesc *theAEDesc);
OSErr AEGetParamPtr(const AppleEvent *theAppleEvent, AEKeyword theAEKeyword, DescType desiredType, DescType *actualType,
                    void *dataPtr, Size maximumSize, Size *actualSize);
OSErr AEGetParamDesc(const AppleEvent *theAppleEvent, AEKeyword theAEKeyword, DescType desiredType, AEDesc *result);
OSErr AESizeOfParam(const AppleEvent *theAppleEvent, AEKeyword theAEKeyword, DescType *typeCode, Size *dataSize);
OSErr AEDeleteParam(AppleEvent *theAppleEvent, AEKeyword theAEKeyword);
OSErr AEPutAttributePtr(AppleEvent *theAppleEvent, AEKeyword theAEKeyword, DescType typeCode, const void *dataPtr, Size dataSize);
OSErr AEPutAttributeDesc(AppleEvent *theAppleEvent, AEKeyword theAEKeyword, const AEDesc *theAEDesc);
OSErr AEGetAttributePtr(const AppleEvent *theAppleEvent, AEKeyword theAEKeyword, DescType desiredType, DescType *typeCode,
                        void *dataPtr, Size maximumSize, Size *actualSize);
OSErr AEGetAttributeDesc(const AppleEvent *theAppleEvent, AEKeyword theAEKeyword, DescType desiredType, AEDesc *result);
OSErr AESizeOfAttribute(const AppleEvent *theAppleEvent, AEKeyword theAEKeyword, DescType *typeCode, Size *dataSize);

#ifdef __cplusplus
}
#endif

#endif
