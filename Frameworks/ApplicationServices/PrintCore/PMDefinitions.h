/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * PrintCore's types, values and errors.
 */

#ifndef PRINTCORE_PMDEFINITIONS_H
#define PRINTCORE_PMDEFINITIONS_H

#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef const void *PMObject;
typedef struct OpaquePMDialog *PMDialog;
typedef struct OpaquePMPrintSettings *PMPrintSettings;
typedef struct OpaquePMPageFormat *PMPageFormat;
typedef struct OpaquePMPrintSession *PMPrintSession;
typedef struct OpaquePMPrinter *PMPrinter;
typedef struct OpaquePMServer *PMServer;
typedef struct OpaquePMPreset *PMPreset;
typedef struct OpaquePMPaper *PMPaper;

#define kPMServerLocal ((PMServer)NULL)

enum { kPMCancel = 0x0080 };

typedef UInt16 PMDestinationType;
enum {
    kPMDestinationInvalid = 0,
    kPMDestinationPrinter = 1,
    kPMDestinationFile = 2,
    kPMDestinationFax = 3,
    kPMDestinationPreview = 4,
    kPMDestinationProcessPDF = 5,
};
#define kPMDestinationTypeDefault kPMDestinationPrinter

typedef UInt16 PMOrientation;
enum {
    kPMPortrait = 1,
    kPMLandscape = 2,
    kPMReversePortrait = 3,
    kPMReverseLandscape = 4,
};

typedef UInt16 PMPrinterState;
enum {
    kPMPrinterIdle = 3,
    kPMPrinterProcessing = 4,
    kPMPrinterStopped = 5,
};

typedef UInt32 PMDuplexMode;
enum {
    kPMDuplexNone = 0x0001,
    kPMDuplexNoTumble = 0x0002,
    kPMDuplexTumble = 0x0003,
    kPMSimplexTumble = 0x0004,
};

typedef UInt32 PMDataFormat;
enum {
    kPMDataFormatXMLDefault = 0,
    kPMDataFormatXMLMinimal = 1,
    kPMDataFormatXMLCompressed = 2,
};

typedef UInt32 PMPPDDomain;
enum {
    kAllPPDDomains = 1,
    kSystemPPDDomain = 2,
    kLocalPPDDomain = 3,
    kNetworkPPDDomain = 4,
    kUserPPDDomain = 5,
    kCUPSPPDDomain = 6,
};

enum { kPMPrintAllPages = (UInt32)-1 };
enum { kPMUnlocked = false };

typedef struct {
    double top;
    double left;
    double bottom;
    double right;
} PMRect;

typedef struct {
    double hRes;
    double vRes;
} PMResolution;

typedef PMRect PMPaperMargins;

/* Errors. */
enum {
    kPMNoError = 0,
    kPMGeneralError = -30870,
    kPMOutOfScope = -30871,
    kPMInvalidParameter = -50,
    kPMNoDefaultPrinter = -30872,
    kPMNotImplemented = -30873,
    kPMNoSuchEntry = -30874,
    kPMInvalidPrintSettings = -30875,
    kPMInvalidPageFormat = -30876,
    kPMValueOutOfRange = -30877,
    kPMLockIgnored = -30878,
    kPMInvalidPrintSession = -30879,
    kPMInvalidPrinter = -30880,
    kPMObjectInUse = -30881,
    kPMInvalidPreset = -30899,
    kPMPrintAllPagesError = -30882,
    kPMCreateMessageFailed = -9620,
    kPMInvalidFileType = -9626,
    kPMInvalidPaper = -30897,
};

/* Document and graphics formats. */
#define kPMDocumentFormatDefault        CFSTR("com.apple.documentformat.default")
#define kPMDocumentFormatPDF            CFSTR("application/pdf")
#define kPMDocumentFormatPostScript     CFSTR("application/postscript")
#define kPMGraphicsContextDefault       CFSTR("com.apple.graphicscontext.default")
#define kPMGraphicsContextCoreGraphics  CFSTR("com.apple.graphicscontext.coregraphics")
#define kPMDataFormatPDF                kPMDocumentFormatPDF
#define kPMDataFormatPostScript         kPMDocumentFormatPostScript

/* Preset attributes. */
#define kPMPresetGraphicsTypeKey        CFSTR("com.apple.print.preset.graphicsType")
#define kPMPresetGraphicsTypePhoto      CFSTR("Photo")
#define kPMPresetGraphicsTypeAll        CFSTR("All")
#define kPMPresetGraphicsTypeGeneral    CFSTR("General")
#define kPMPresetGraphicsTypeNone       CFSTR("None")

#ifdef __cplusplus
}
#endif

#endif
