/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * PrintCore: print sessions, print settings, page formats, printers and
 * papers, and printing a document (drawn with Core Graphics, or a file).
 */

#ifndef PRINTCORE_PMCORE_H
#define PRINTCORE_PMCORE_H

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include "PMDefinitions.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Objects. */
OSStatus PMRetain(PMObject object);
OSStatus PMRelease(PMObject object);

/* Sessions. */
OSStatus PMCreateSession(PMPrintSession *printSession);
OSStatus PMSessionError(PMPrintSession printSession);
OSStatus PMSessionSetError(PMPrintSession printSession, OSStatus printError);
OSStatus PMSessionBeginCGDocumentNoDialog(PMPrintSession printSession, PMPrintSettings printSettings,
                                          PMPageFormat pageFormat);
OSStatus PMSessionEndDocumentNoDialog(PMPrintSession printSession);
OSStatus PMSessionBeginPageNoDialog(PMPrintSession printSession, PMPageFormat pageFormat, const PMRect *pageFrame);
OSStatus PMSessionEndPageNoDialog(PMPrintSession printSession);
OSStatus PMSessionGetCGGraphicsContext(PMPrintSession printSession, CGContextRef *context);
OSStatus PMSessionGetDestinationType(PMPrintSession printSession, PMPrintSettings printSettings,
                                     PMDestinationType *destTypeP);
OSStatus PMSessionCopyDestinationFormat(PMPrintSession printSession, PMPrintSettings printSettings,
                                        CFStringRef *destFormatP);
OSStatus PMSessionCopyDestinationLocation(PMPrintSession printSession, PMPrintSettings printSettings,
                                          CFURLRef *destLocationP);
OSStatus PMSessionSetDestination(PMPrintSession printSession, PMPrintSettings printSettings,
                                 PMDestinationType destType, CFStringRef destFormat, CFURLRef destLocation);
OSStatus PMSessionCopyOutputFormatList(PMPrintSession printSession, PMDestinationType destType,
                                       CFArrayRef *documentFormatP);
OSStatus PMSessionCreatePageFormatList(PMPrintSession printSession, PMPrinter printer, CFArrayRef *pageFormatList);
OSStatus PMSessionCreatePrinterList(PMPrintSession printSession, CFArrayRef *printerList, CFIndex *currentIndex,
                                    PMPrinter *currentPrinter);
OSStatus PMSessionGetCurrentPrinter(PMPrintSession printSession, PMPrinter *currentPrinter);
OSStatus PMSessionSetCurrentPMPrinter(PMPrintSession session, PMPrinter printer);
OSStatus PMSessionGetDataFromSession(PMPrintSession printSession, CFStringRef key, CFTypeRef *data);
OSStatus PMSessionSetDataInSession(PMPrintSession printSession, CFStringRef key, CFTypeRef data);
OSStatus PMSessionDefaultPageFormat(PMPrintSession printSession, PMPageFormat pageFormat);
OSStatus PMSessionDefaultPrintSettings(PMPrintSession printSession, PMPrintSettings printSettings);
OSStatus PMSessionValidatePageFormat(PMPrintSession printSession, PMPageFormat pageFormat, Boolean *changed);
OSStatus PMSessionValidatePrintSettings(PMPrintSession printSession, PMPrintSettings printSettings,
                                        Boolean *changed);

/* Page formats. */
OSStatus PMCreatePageFormat(PMPageFormat *pageFormat);
OSStatus PMCreatePageFormatWithPMPaper(PMPageFormat *pageFormat, PMPaper paper);
OSStatus PMCopyPageFormat(PMPageFormat formatSrc, PMPageFormat formatDest);
OSStatus PMPageFormatCreateDataRepresentation(PMPageFormat pageFormat, CFDataRef *data, PMDataFormat format);
OSStatus PMPageFormatCreateWithDataRepresentation(CFDataRef data, PMPageFormat *pageFormat);
OSStatus PMGetPageFormatExtendedData(PMPageFormat pageFormat, OSType dataID, UInt32 *size, void *extendedData);
OSStatus PMSetPageFormatExtendedData(PMPageFormat pageFormat, OSType dataID, UInt32 size, void *extendedData);
OSStatus PMPageFormatGetPrinterID(PMPageFormat pageFormat, CFStringRef *printerID);
OSStatus PMGetPageFormatPaper(PMPageFormat format, PMPaper *paper);
OSStatus PMGetAdjustedPageRect(PMPageFormat pageFormat, PMRect *pageRect);
OSStatus PMGetAdjustedPaperRect(PMPageFormat pageFormat, PMRect *paperRect);
OSStatus PMGetUnadjustedPageRect(PMPageFormat pageFormat, PMRect *pageRect);
OSStatus PMGetUnadjustedPaperRect(PMPageFormat pageFormat, PMRect *paperRect);
OSStatus PMGetOrientation(PMPageFormat pageFormat, PMOrientation *orientation);
OSStatus PMSetOrientation(PMPageFormat pageFormat, PMOrientation orientation, Boolean lock);
OSStatus PMGetScale(PMPageFormat pageFormat, double *scale);
OSStatus PMSetScale(PMPageFormat pageFormat, double scale);

/* Print settings. */
OSStatus PMCreatePrintSettings(PMPrintSettings *printSettings);
OSStatus PMCopyPrintSettings(PMPrintSettings source, PMPrintSettings dest);
OSStatus PMPrintSettingsCreateDataRepresentation(PMPrintSettings printSettings, CFDataRef *data, PMDataFormat format);
OSStatus PMPrintSettingsCreateWithDataRepresentation(CFDataRef data, PMPrintSettings *printSettings);
OSStatus PMPrintSettingsCopyAsDictionary(PMPrintSettings printSettings, CFDictionaryRef *settingsDictionary);
OSStatus PMPrintSettingsCopyKeys(PMPrintSettings printSettings, CFArrayRef *settingsKeys);
OSStatus PMPrintSettingsGetValue(PMPrintSettings printSettings, CFStringRef key, CFTypeRef *value);
OSStatus PMPrintSettingsSetValue(PMPrintSettings printSettings, CFStringRef key, CFTypeRef value, Boolean locked);
OSStatus PMPrintSettingsGetJobName(PMPrintSettings printSettings, CFStringRef *name);
OSStatus PMPrintSettingsSetJobName(PMPrintSettings printSettings, CFStringRef name);
OSStatus PMGetCopies(PMPrintSettings printSettings, UInt32 *copies);
OSStatus PMSetCopies(PMPrintSettings printSettings, UInt32 copies, Boolean lock);
OSStatus PMGetCollate(PMPrintSettings printSettings, Boolean *collate);
OSStatus PMSetCollate(PMPrintSettings printSettings, Boolean collate);
OSStatus PMGetFirstPage(PMPrintSettings printSettings, UInt32 *first);
OSStatus PMSetFirstPage(PMPrintSettings printSettings, UInt32 first, Boolean lock);
OSStatus PMGetLastPage(PMPrintSettings printSettings, UInt32 *last);
OSStatus PMSetLastPage(PMPrintSettings printSettings, UInt32 last, Boolean lock);
OSStatus PMGetPageRange(PMPrintSettings printSettings, UInt32 *minPage, UInt32 *maxPage);
OSStatus PMSetPageRange(PMPrintSettings printSettings, UInt32 minPage, UInt32 maxPage);
OSStatus PMGetDuplex(PMPrintSettings printSettings, PMDuplexMode *duplexSetting);
OSStatus PMSetDuplex(PMPrintSettings printSettings, PMDuplexMode duplexSetting);
OSStatus PMPrintSettingsToOptions(PMPrintSettings printSettings, char **options);
OSStatus PMPrintSettingsToOptionsWithPrinterAndPageFormat(PMPrintSettings printSettings, PMPrinter printer,
                                                          PMPageFormat pageFormat, CFStringRef *options);

/* Printers. */
OSStatus PMServerCreatePrinterList(PMServer server, CFArrayRef *printerList);
PMPrinter PMPrinterCreateFromPrinterID(CFStringRef printerID);
OSStatus PMCreateGenericPrinter(PMPrinter *printer);
CFStringRef PMPrinterGetID(PMPrinter printer);
CFStringRef PMPrinterGetName(PMPrinter printer);
CFStringRef PMPrinterGetLocation(PMPrinter printer);
OSStatus PMPrinterGetState(PMPrinter printer, PMPrinterState *state);
OSStatus PMPrinterGetMakeAndModelName(PMPrinter printer, CFStringRef *makeAndModel);
Boolean PMPrinterIsDefault(PMPrinter printer);
OSStatus PMPrinterSetDefault(PMPrinter printer);
Boolean PMPrinterIsFavorite(PMPrinter printer);
Boolean PMPrinterIsRemote(PMPrinter printer, Boolean *isRemoteP);
Boolean PMPrinterIsPostScriptCapable(PMPrinter printer);
OSStatus PMPrinterCopyDeviceURI(PMPrinter printer, CFURLRef *deviceURI);
CFStringRef PMPrinterCopyHostName(PMPrinter printer, CFStringRef *hostNameP);
OSStatus PMPrinterGetPaperList(PMPrinter printer, CFArrayRef *paperList);
OSStatus PMPrinterGetOutputResolution(PMPrinter printer, PMPrintSettings printSettings, PMResolution *resolutionP);
OSStatus PMPrinterGetPrinterResolutionCount(PMPrinter printer, UInt32 *countP);
OSStatus PMPrinterGetIndexedPrinterResolution(PMPrinter printer, UInt32 index, PMResolution *resolutionP);
OSStatus PMPrinterCopyPresets(PMPrinter printer, CFArrayRef *presetList);
OSStatus PMPrinterCopyDescriptionURL(PMPrinter printer, CFStringRef descriptionType, CFURLRef *fileURL);
OSStatus PMPrinterPrintWithFile(PMPrinter printer, PMPrintSettings settings, PMPageFormat format,
                                CFStringRef mimeType, CFURLRef fileURL);
OSStatus PMPrinterPrintWithProvider(PMPrinter printer, PMPrintSettings settings, PMPageFormat format,
                                    CFStringRef mimeType, CGDataProviderRef provider);
OSStatus PMPrinterGetMimeTypes(PMPrinter printer, PMPrintSettings settings, CFArrayRef *mimeTypes);

/* Papers. */
OSStatus PMPaperCreateCustom(PMPrinter printer, CFStringRef id, CFStringRef name, double width, double height,
                             const PMPaperMargins *margins, PMPaper *paperP);
OSStatus PMPaperGetID(PMPaper paper, CFStringRef *paperID);
OSStatus PMPaperGetWidth(PMPaper paper, double *paperWidth);
OSStatus PMPaperGetHeight(PMPaper paper, double *paperHeight);
OSStatus PMPaperGetMargins(PMPaper paper, PMPaperMargins *paperMargins);
OSStatus PMPaperCreateLocalizedName(PMPaper paper, PMPrinter printer, CFStringRef *paperName);
CFStringRef PMPaperGetPPDPaperName(PMPaper paper, CFStringRef *paperName);
OSStatus PMPaperGetPrinterID(PMPaper paper, CFStringRef *printerID);
Boolean PMPaperIsCustom(PMPaper paper);

/* Presets. */
OSStatus PMPresetCopyName(PMPreset preset, CFStringRef *name);
OSStatus PMPresetCreatePrintSettings(PMPreset preset, PMPrintSession session, PMPrintSettings *printSettings);
OSStatus PMPresetGetAttributes(PMPreset preset, CFDictionaryRef *attributes);

/* Workflows (what to do with a PDF: an app opens it, a folder keeps it). */
OSStatus PMWorkflowCopyItems(CFArrayRef *workflowItems);
OSStatus PMWorkflowSubmitPDFWithOptions(CFURLRef workflowItem, CFStringRef title, const char *options,
                                        CFURLRef pdfFile);
OSStatus PMWorkflowSubmitPDFWithSettings(CFURLRef workflowItem, PMPrintSettings settings, CFURLRef pdfFile);

#ifdef __cplusplus
}
#endif

#endif
