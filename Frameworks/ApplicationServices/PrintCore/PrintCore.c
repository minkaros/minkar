/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * PrintCore. Printers are CUPS's (libcups: the destinations cupsd knows;
 * with no cupsd there are none, and a session's printer is the generic
 * one). A document drawn in a session (PMSessionBeginCGDocumentNoDialog) is
 * a PDF made by Core Graphics: written where the destination says (a
 * file), or to a temporary file then printed (a printer, through CUPS),
 * opened (preview) or handed to a workflow item (an app opens it, a folder
 * keeps it, a tool gets it).
 *
 * Geometry: a page format has a paper (its size and its imageable area's
 * margins), an orientation and a scale. The page rectangle is the
 * imageable area, from (0, 0); the paper rectangle is the whole sheet in
 * the same space (its top and left are minus the margins). The adjusted
 * rectangles follow the orientation and the scale; a page's graphics
 * context has its origin at the bottom left of the imageable area, in
 * points of the adjusted page.
 */

#include <PrintCore/PrintCore.h>
#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CFURLEnumerator.h>
#include <cups/cups.h>
#include <cups/ppd.h>
#include <pthread.h>
#include <spawn.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern char **environ;

/* ---- Objects ---------------------------------------------------------- */

enum { kSession = 1, kSettings, kFormat, kPrinter, kPaper, kPreset };

typedef struct {
    int kind;
    atomic_int refs;
} Header;

typedef struct OpaquePMPaper {
    Header h;
    CFStringRef paperID;
    CFStringRef name;
    CFStringRef printerID;
    double width, height;
    PMPaperMargins margins;
    Boolean custom;
} Paper;

typedef struct OpaquePMPrinter {
    Header h;
    CFStringRef printerID;          /* the CUPS name; empty for the generic printer */
    CFStringRef name;
    CFStringRef location;
    CFStringRef makeAndModel;
    CFStringRef deviceURI;
    PMPrinterState state;
    Boolean isDefault, remote, generic;
    PMResolution resolution;
} Printer;

typedef struct OpaquePMPageFormat {
    Header h;
    Paper *paper;
    PMOrientation orientation;
    double scale;                   /* percent */
    CFMutableDictionaryRef extended;
} Format;

typedef struct OpaquePMPrintSettings {
    Header h;
    CFMutableDictionaryRef values;
} Settings;

typedef struct OpaquePMPrintSession {
    Header h;
    OSStatus error;
    Printer *printer;
    CFMutableDictionaryRef data;
    /* The document being drawn. */
    CGContextRef context;
    Boolean inPage;
    CFURLRef output;                /* where the PDF goes */
    Boolean temporary;              /* output is ours, to hand over then delete */
    Settings *settings;
    Format *format;
} Session;

static void *newObject(size_t size, int kind) {
    Header *h = calloc(1, size);
    if (h != NULL) {
        h->kind = kind;
        atomic_init(&h->refs, 1);
    }
    return h;
}

static int kindOf(PMObject o) {
    return o != NULL ? ((const Header *)o)->kind : 0;
}

static void release(CFTypeRef cf) {
    if (cf != NULL)
        CFRelease(cf);
}

static void freePaper(Paper *p) {
    release(p->paperID);
    release(p->name);
    release(p->printerID);
}

static void freePrinter(Printer *p) {
    release(p->printerID);
    release(p->name);
    release(p->location);
    release(p->makeAndModel);
    release(p->deviceURI);
}

static void endDocument(Session *s);

static void destroy(Header *h) {
    switch (h->kind) {
    case kPaper:
        freePaper((Paper *)h);
        break;
    case kPrinter:
        freePrinter((Printer *)h);
        break;
    case kFormat:
        PMRelease(((Format *)h)->paper);
        release(((Format *)h)->extended);
        break;
    case kSettings:
        release(((Settings *)h)->values);
        break;
    case kSession: {
        Session *s = (Session *)h;
        if (s->context != NULL)
            endDocument(s);
        PMRelease(s->printer);
        release(s->data);
        break;
    }
    default:
        break;
    }
    h->kind = 0;
    free(h);
}

OSStatus PMRetain(PMObject object) {
    if (object == NULL)
        return kPMInvalidParameter;
    atomic_fetch_add(&((Header *)(uintptr_t)object)->refs, 1);
    return kPMNoError;
}

OSStatus PMRelease(PMObject object) {
    if (object == NULL)
        return kPMNoError;
    Header *h = (Header *)(uintptr_t)object;
    if (atomic_fetch_sub(&h->refs, 1) == 1)
        destroy(h);
    return kPMNoError;
}

/* Arrays of PrintCore objects hold them. */
static const void *retainValue(CFAllocatorRef allocator, const void *value) {
    (void)allocator;
    PMRetain(value);
    return value;
}

static void releaseValue(CFAllocatorRef allocator, const void *value) {
    (void)allocator;
    PMRelease(value);
}

static const CFArrayCallBacks objectCallBacks = { 0, retainValue, releaseValue, NULL, NULL };

/* Appends an object the caller made, the array now holding it. */
static void appendOwned(CFMutableArrayRef a, PMObject o) {
    CFArrayAppendValue(a, o);
    PMRelease(o);
}

/* ---- Papers ----------------------------------------------------------- */

static const struct {
    const char *paperID, *name;
    double width, height;
} standardPapers[] = {
    { "na-letter", "US Letter", 612, 792 },
    { "na-legal", "US Legal", 612, 1008 },
    { "iso-a4", "A4", 595.2756, 841.8898 },
    { "iso-a3", "A3", 841.8898, 1190.5512 },
    { "iso-a5", "A5", 419.5276, 595.2756 },
    { "jis-b5", "JB5", 515.9055, 728.5039 },
    { "na-ledger", "Tabloid", 792, 1224 },
    { "na-executive", "Executive", 522, 756 },
    { "na-number-10-envelope", "#10 Envelope", 297, 684 },
    { "iso-dl-envelope", "DL Envelope", 311.8110, 623.6220 },
};

static Paper *newPaper(CFStringRef paperID, CFStringRef name, CFStringRef printerID, double w, double h,
                       const PMPaperMargins *m, Boolean custom) {
    Paper *p = newObject(sizeof(Paper), kPaper);
    if (p == NULL)
        return NULL;
    p->paperID = CFRetain(paperID);
    p->name = CFRetain(name);
    p->printerID = printerID != NULL ? CFRetain(printerID) : CFSTR("");
    p->width = w;
    p->height = h;
    if (m != NULL)
        p->margins = *m;
    p->custom = custom;
    return p;
}

static Paper *standardPaper(size_t i, CFStringRef printerID, const PMPaperMargins *m) {
    CFStringRef pid = CFStringCreateWithCString(NULL, standardPapers[i].paperID, kCFStringEncodingUTF8);
    CFStringRef name = CFStringCreateWithCString(NULL, standardPapers[i].name, kCFStringEncodingUTF8);
    Paper *p = newPaper(pid, name, printerID, standardPapers[i].width, standardPapers[i].height, m, false);
    CFRelease(pid);
    CFRelease(name);
    return p;
}

/* The locale's paper: Letter where it measures in inches, else A4. */
static Paper *defaultPaper(void) {
    CFLocaleRef locale = CFLocaleCopyCurrent();
    CFBooleanRef metric = locale ? CFLocaleGetValue(locale, kCFLocaleUsesMetricSystem) : NULL;
    Boolean usesMetric = metric == NULL || CFBooleanGetValue(metric);
    release(locale);
    return standardPaper(usesMetric ? 2 : 0, NULL, NULL);
}

OSStatus PMPaperCreateCustom(PMPrinter printer, CFStringRef paperID, CFStringRef name, double width, double height,
                             const PMPaperMargins *margins, PMPaper *paperP) {
    if (paperP == NULL || paperID == NULL || name == NULL || width <= 0 || height <= 0)
        return kPMInvalidParameter;
    *paperP = newPaper(paperID, name, printer ? printer->printerID : NULL, width, height, margins, true);
    return *paperP != NULL ? kPMNoError : kPMGeneralError;
}

OSStatus PMPaperGetID(PMPaper paper, CFStringRef *paperID) {
    if (kindOf(paper) != kPaper || paperID == NULL)
        return kPMInvalidParameter;
    *paperID = paper->paperID;
    return kPMNoError;
}

OSStatus PMPaperGetWidth(PMPaper paper, double *w) {
    if (kindOf(paper) != kPaper || w == NULL)
        return kPMInvalidParameter;
    *w = paper->width;
    return kPMNoError;
}

OSStatus PMPaperGetHeight(PMPaper paper, double *h) {
    if (kindOf(paper) != kPaper || h == NULL)
        return kPMInvalidParameter;
    *h = paper->height;
    return kPMNoError;
}

OSStatus PMPaperGetMargins(PMPaper paper, PMPaperMargins *m) {
    if (kindOf(paper) != kPaper || m == NULL)
        return kPMInvalidParameter;
    *m = paper->margins;
    return kPMNoError;
}

OSStatus PMPaperCreateLocalizedName(PMPaper paper, PMPrinter printer, CFStringRef *paperName) {
    (void)printer;
    if (kindOf(paper) != kPaper || paperName == NULL)
        return kPMInvalidParameter;
    *paperName = CFRetain(paper->name);
    return kPMNoError;
}

CFStringRef PMPaperGetPPDPaperName(PMPaper paper, CFStringRef *paperName) {
    if (kindOf(paper) != kPaper)
        return NULL;
    if (paperName != NULL)
        *paperName = paper->paperID;
    return paper->paperID;
}

OSStatus PMPaperGetPrinterID(PMPaper paper, CFStringRef *printerID) {
    if (kindOf(paper) != kPaper || printerID == NULL)
        return kPMInvalidParameter;
    *printerID = paper->printerID;
    return kPMNoError;
}

Boolean PMPaperIsCustom(PMPaper paper) {
    return kindOf(paper) == kPaper && paper->custom;
}

/* ---- Printers --------------------------------------------------------- */

static CFStringRef cfString(const char *s) {
    return CFStringCreateWithCString(NULL, s != NULL ? s : "", kCFStringEncodingUTF8);
}

static char *cString(CFStringRef s) {
    if (s == NULL)
        return strdup("");
    CFIndex size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s), kCFStringEncodingUTF8) + 1;
    char *c = malloc((size_t)size);
    if (c != NULL && !CFStringGetCString(s, c, size, kCFStringEncodingUTF8))
        c[0] = 0;
    return c;
}

static Printer *genericPrinter(void) {
    Printer *p = newObject(sizeof(Printer), kPrinter);
    if (p == NULL)
        return NULL;
    p->printerID = CFSTR("");
    p->name = CFSTR("Generic Printer");
    p->location = CFSTR("");
    p->makeAndModel = CFSTR("Generic PDF");
    p->deviceURI = NULL;
    p->state = kPMPrinterIdle;
    p->generic = true;
    p->resolution = (PMResolution){ 300, 300 };
    return p;
}

static Printer *printerFromDest(cups_dest_t *d) {
    Printer *p = newObject(sizeof(Printer), kPrinter);
    if (p == NULL)
        return NULL;
    const char *info = cupsGetOption("printer-info", d->num_options, d->options);
    const char *state = cupsGetOption("printer-state", d->num_options, d->options);
    const char *type = cupsGetOption("printer-type", d->num_options, d->options);
    const char *uri = cupsGetOption("device-uri", d->num_options, d->options);
    const char *res = cupsGetOption("printer-resolution-default", d->num_options, d->options);
    p->printerID = cfString(d->name);
    p->name = cfString(info != NULL && *info ? info : d->name);
    p->location = cfString(cupsGetOption("printer-location", d->num_options, d->options));
    p->makeAndModel = cfString(cupsGetOption("printer-make-and-model", d->num_options, d->options));
    if (uri != NULL && *uri) {
        CFStringRef u = cfString(uri);
        p->deviceURI = CFURLCreateWithString(NULL, u, NULL);
        CFRelease(u);
    }
    p->state = state != NULL ? (PMPrinterState)atoi(state) : kPMPrinterIdle;
    p->isDefault = d->is_default != 0;
    p->remote = type != NULL && (strtoul(type, NULL, 0) & CUPS_PRINTER_REMOTE) != 0;
    int dpi = res != NULL ? atoi(res) : 0;
    p->resolution = (PMResolution){ dpi > 0 ? dpi : 300, dpi > 0 ? dpi : 300 };
    return p;
}

/* CUPS's destinations, as printers; the default one's index. */
static CFArrayRef copyPrinters(CFIndex *defaultIndex) {
    cups_dest_t *dests = NULL;
    int count = cupsGetDests2(CUPS_HTTP_DEFAULT, &dests);
    CFMutableArrayRef list = CFArrayCreateMutable(NULL, 0, &objectCallBacks);

    if (defaultIndex != NULL)
        *defaultIndex = -1;
    for (int i = 0; i < count; i++) {
        if (dests[i].instance != NULL)
            continue;
        Printer *p = printerFromDest(&dests[i]);
        if (p == NULL)
            continue;
        if (p->isDefault && defaultIndex != NULL)
            *defaultIndex = CFArrayGetCount(list);
        appendOwned(list, p);
    }
    cupsFreeDests(count, dests);
    return list;
}

OSStatus PMServerCreatePrinterList(PMServer server, CFArrayRef *printerList) {
    if (printerList == NULL)
        return kPMInvalidParameter;
    if (server != kPMServerLocal)
        return kPMInvalidParameter;
    *printerList = copyPrinters(NULL);
    return kPMNoError;
}

PMPrinter PMPrinterCreateFromPrinterID(CFStringRef printerID) {
    if (printerID == NULL)
        return NULL;
    if (CFStringGetLength(printerID) == 0)
        return genericPrinter();
    CFArrayRef list = copyPrinters(NULL);
    Printer *found = NULL;
    for (CFIndex i = 0; i < CFArrayGetCount(list) && found == NULL; i++) {
        Printer *p = (Printer *)CFArrayGetValueAtIndex(list, i);
        if (CFEqual(p->printerID, printerID)) {
            PMRetain(p);
            found = p;
        }
    }
    CFRelease(list);
    return found;
}

OSStatus PMCreateGenericPrinter(PMPrinter *printer) {
    if (printer == NULL)
        return kPMInvalidParameter;
    *printer = genericPrinter();
    return *printer != NULL ? kPMNoError : kPMGeneralError;
}

CFStringRef PMPrinterGetID(PMPrinter printer) {
    return kindOf(printer) == kPrinter ? printer->printerID : NULL;
}

CFStringRef PMPrinterGetName(PMPrinter printer) {
    return kindOf(printer) == kPrinter ? printer->name : NULL;
}

CFStringRef PMPrinterGetLocation(PMPrinter printer) {
    return kindOf(printer) == kPrinter ? printer->location : NULL;
}

OSStatus PMPrinterGetState(PMPrinter printer, PMPrinterState *state) {
    if (kindOf(printer) != kPrinter || state == NULL)
        return kPMInvalidParameter;
    *state = printer->state;
    return kPMNoError;
}

OSStatus PMPrinterGetMakeAndModelName(PMPrinter printer, CFStringRef *makeAndModel) {
    if (kindOf(printer) != kPrinter || makeAndModel == NULL)
        return kPMInvalidParameter;
    *makeAndModel = printer->makeAndModel;
    return kPMNoError;
}

Boolean PMPrinterIsDefault(PMPrinter printer) {
    return kindOf(printer) == kPrinter && printer->isDefault;
}

/* The user's default (lpoptions), as CUPS keeps it. */
OSStatus PMPrinterSetDefault(PMPrinter printer) {
    if (kindOf(printer) != kPrinter || printer->generic)
        return kPMInvalidPrinter;
    cups_dest_t *dests = NULL;
    int count = cupsGetDests2(CUPS_HTTP_DEFAULT, &dests);
    char *name = cString(printer->printerID);
    for (int i = 0; i < count; i++)
        dests[i].is_default = strcmp(dests[i].name, name) == 0 && dests[i].instance == NULL;
    int ok = cupsSetDests2(CUPS_HTTP_DEFAULT, count, dests);
    cupsFreeDests(count, dests);
    free(name);
    if (ok != 0)
        return kPMGeneralError;
    printer->isDefault = true;
    return kPMNoError;
}

Boolean PMPrinterIsFavorite(PMPrinter printer) {
    return kindOf(printer) == kPrinter && !printer->generic;
}

Boolean PMPrinterIsRemote(PMPrinter printer, Boolean *isRemoteP) {
    Boolean remote = kindOf(printer) == kPrinter && printer->remote;
    if (isRemoteP != NULL)
        *isRemoteP = remote;
    return remote;
}

/* CUPS's filters take PostScript for any printer. */
Boolean PMPrinterIsPostScriptCapable(PMPrinter printer) {
    return kindOf(printer) == kPrinter && !printer->generic;
}

OSStatus PMPrinterCopyDeviceURI(PMPrinter printer, CFURLRef *deviceURI) {
    if (kindOf(printer) != kPrinter || deviceURI == NULL)
        return kPMInvalidParameter;
    if (printer->deviceURI == NULL)
        return kPMNoSuchEntry;
    *deviceURI = CFRetain(printer->deviceURI);
    return kPMNoError;
}

CFStringRef PMPrinterCopyHostName(PMPrinter printer, CFStringRef *hostNameP) {
    CFStringRef host = NULL;
    if (kindOf(printer) == kPrinter && printer->deviceURI != NULL)
        host = CFURLCopyHostName(printer->deviceURI);
    if (host == NULL)
        host = CFSTR("localhost");
    if (hostNameP != NULL)
        *hostNameP = host;
    return host;
}

OSStatus PMPrinterGetPaperList(PMPrinter printer, CFArrayRef *paperList) {
    if (kindOf(printer) != kPrinter || paperList == NULL)
        return kPMInvalidParameter;
    /* The standard sizes (a printer's own list comes with its PPD). */
    CFMutableArrayRef list = CFArrayCreateMutable(NULL, 0, &objectCallBacks);
    for (size_t i = 0; i < sizeof(standardPapers) / sizeof(standardPapers[0]); i++) {
        Paper *p = standardPaper(i, printer->printerID, NULL);
        if (p != NULL)
            appendOwned(list, p);
    }
    /* The list stays with the printer, as the getter's name says. */
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    static CFMutableDictionaryRef kept;
    pthread_mutex_lock(&lock);
    if (kept == NULL)
        kept = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(kept, printer->printerID, list);
    pthread_mutex_unlock(&lock);
    *paperList = list;
    CFRelease(list);
    return kPMNoError;
}

OSStatus PMPrinterGetOutputResolution(PMPrinter printer, PMPrintSettings printSettings, PMResolution *res) {
    (void)printSettings;
    if (kindOf(printer) != kPrinter || res == NULL)
        return kPMInvalidParameter;
    *res = printer->resolution;
    return kPMNoError;
}

OSStatus PMPrinterGetPrinterResolutionCount(PMPrinter printer, UInt32 *countP) {
    if (kindOf(printer) != kPrinter || countP == NULL)
        return kPMInvalidParameter;
    *countP = 1;
    return kPMNoError;
}

OSStatus PMPrinterGetIndexedPrinterResolution(PMPrinter printer, UInt32 index, PMResolution *res) {
    if (kindOf(printer) != kPrinter || res == NULL)
        return kPMInvalidParameter;
    if (index != 1)
        return kPMValueOutOfRange;
    *res = printer->resolution;
    return kPMNoError;
}

OSStatus PMPrinterCopyPresets(PMPrinter printer, CFArrayRef *presetList) {
    if (kindOf(printer) != kPrinter || presetList == NULL)
        return kPMInvalidParameter;
    *presetList = CFArrayCreate(NULL, NULL, 0, NULL);
    return kPMNoError;
}

/* The printer's PPD (kPMPPDDescriptionType), as CUPS gives it. */
OSStatus PMPrinterCopyDescriptionURL(PMPrinter printer, CFStringRef descriptionType, CFURLRef *fileURL) {
    if (kindOf(printer) != kPrinter || fileURL == NULL)
        return kPMInvalidParameter;
    if (descriptionType == NULL || !CFEqual(descriptionType, CFSTR("PPD")))
        return kPMInvalidParameter;
    if (printer->generic)
        return kPMNoSuchEntry;
    char *name = cString(printer->printerID);
    const char *path = cupsGetPPD2(CUPS_HTTP_DEFAULT, name);
    free(name);
    if (path == NULL)
        return kPMNoSuchEntry;
    *fileURL = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, (CFIndex)strlen(path), false);
    return kPMNoError;
}

OSStatus PMPrinterGetMimeTypes(PMPrinter printer, PMPrintSettings settings, CFArrayRef *mimeTypes) {
    (void)settings;
    if (kindOf(printer) != kPrinter || mimeTypes == NULL)
        return kPMInvalidParameter;
    const void *types[] = { kPMDocumentFormatPDF, kPMDocumentFormatPostScript, CFSTR("image/jpeg"),
                            CFSTR("image/png"), CFSTR("text/plain") };
    static CFArrayRef all, pdfOnly;
    if (all == NULL) {
        all = CFArrayCreate(NULL, types, 5, &kCFTypeArrayCallBacks);
        pdfOnly = CFArrayCreate(NULL, types, 1, &kCFTypeArrayCallBacks);
    }
    *mimeTypes = printer->generic ? pdfOnly : all;
    return kPMNoError;
}

/* ---- Page formats ----------------------------------------------------- */

OSStatus PMCreatePageFormatWithPMPaper(PMPageFormat *pageFormat, PMPaper paper) {
    if (pageFormat == NULL || kindOf(paper) != kPaper)
        return kPMInvalidParameter;
    Format *f = newObject(sizeof(Format), kFormat);
    if (f == NULL)
        return kPMGeneralError;
    PMRetain(paper);
    f->paper = paper;
    f->orientation = kPMPortrait;
    f->scale = 100;
    f->extended = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    *pageFormat = f;
    return kPMNoError;
}

OSStatus PMCreatePageFormat(PMPageFormat *pageFormat) {
    Paper *paper = defaultPaper();
    OSStatus err = PMCreatePageFormatWithPMPaper(pageFormat, paper);
    PMRelease(paper);
    return err;
}

OSStatus PMCopyPageFormat(PMPageFormat src, PMPageFormat dest) {
    if (kindOf(src) != kFormat || kindOf(dest) != kFormat)
        return kPMInvalidPageFormat;
    if (src == dest)
        return kPMNoError;
    PMRetain(src->paper);
    PMRelease(dest->paper);
    dest->paper = src->paper;
    dest->orientation = src->orientation;
    dest->scale = src->scale;
    CFRelease(dest->extended);
    dest->extended = CFDictionaryCreateMutableCopy(NULL, 0, src->extended);
    return kPMNoError;
}

static void setNumber(CFMutableDictionaryRef d, CFStringRef key, double v) {
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberDoubleType, &v);
    CFDictionarySetValue(d, key, n);
    CFRelease(n);
}

static double getNumber(CFDictionaryRef d, CFStringRef key, double fallback) {
    CFTypeRef n = CFDictionaryGetValue(d, key);
    double v = fallback;
    if (n != NULL && CFGetTypeID(n) == CFNumberGetTypeID())
        CFNumberGetValue(n, kCFNumberDoubleType, &v);
    return v;
}

static CFStringRef getString(CFDictionaryRef d, CFStringRef key, CFStringRef fallback) {
    CFTypeRef s = CFDictionaryGetValue(d, key);
    return s != NULL && CFGetTypeID(s) == CFStringGetTypeID() ? s : fallback;
}

static CFDataRef dataOf(CFDictionaryRef d, PMDataFormat format) {
    (void)format;   /* every form is the XML property list */
    return CFPropertyListCreateData(NULL, d, kCFPropertyListXMLFormat_v1_0, 0, NULL);
}

static CFDictionaryRef dictionaryOf(CFDataRef data) {
    if (data == NULL)
        return NULL;
    CFPropertyListRef p = CFPropertyListCreateWithData(NULL, data, kCFPropertyListImmutable, NULL, NULL);
    if (p != NULL && CFGetTypeID(p) != CFDictionaryGetTypeID()) {
        CFRelease(p);
        p = NULL;
    }
    return p;
}

#define FMT(k) CFSTR("com.apple.print.PageFormat." k)

OSStatus PMPageFormatCreateDataRepresentation(PMPageFormat f, CFDataRef *data, PMDataFormat format) {
    if (kindOf(f) != kFormat || data == NULL)
        return kPMInvalidPageFormat;
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                         &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(d, FMT("PMPaperID"), f->paper->paperID);
    CFDictionarySetValue(d, FMT("PMPaperName"), f->paper->name);
    CFDictionarySetValue(d, FMT("PMPrinterID"), f->paper->printerID);
    setNumber(d, FMT("PMPaperWidth"), f->paper->width);
    setNumber(d, FMT("PMPaperHeight"), f->paper->height);
    setNumber(d, FMT("PMMarginTop"), f->paper->margins.top);
    setNumber(d, FMT("PMMarginLeft"), f->paper->margins.left);
    setNumber(d, FMT("PMMarginBottom"), f->paper->margins.bottom);
    setNumber(d, FMT("PMMarginRight"), f->paper->margins.right);
    setNumber(d, FMT("PMCustomPaper"), f->paper->custom);
    setNumber(d, FMT("PMOrientation"), f->orientation);
    setNumber(d, FMT("PMScaling"), f->scale);
    CFDictionarySetValue(d, FMT("PMExtendedData"), f->extended);
    *data = dataOf(d, format);
    CFRelease(d);
    return *data != NULL ? kPMNoError : kPMGeneralError;
}

OSStatus PMPageFormatCreateWithDataRepresentation(CFDataRef data, PMPageFormat *pageFormat) {
    if (pageFormat == NULL)
        return kPMInvalidParameter;
    CFDictionaryRef d = dictionaryOf(data);
    if (d == NULL)
        return kPMInvalidParameter;
    PMPaperMargins m = { getNumber(d, FMT("PMMarginTop"), 0), getNumber(d, FMT("PMMarginLeft"), 0),
                         getNumber(d, FMT("PMMarginBottom"), 0), getNumber(d, FMT("PMMarginRight"), 0) };
    Paper *paper = newPaper(getString(d, FMT("PMPaperID"), CFSTR("iso-a4")), getString(d, FMT("PMPaperName"), CFSTR("A4")),
                            getString(d, FMT("PMPrinterID"), CFSTR("")), getNumber(d, FMT("PMPaperWidth"), 595.2756),
                            getNumber(d, FMT("PMPaperHeight"), 841.8898), &m, getNumber(d, FMT("PMCustomPaper"), 0) != 0);
    OSStatus err = PMCreatePageFormatWithPMPaper(pageFormat, paper);
    PMRelease(paper);
    if (err == kPMNoError) {
        (*pageFormat)->orientation = (PMOrientation)getNumber(d, FMT("PMOrientation"), kPMPortrait);
        (*pageFormat)->scale = getNumber(d, FMT("PMScaling"), 100);
        CFTypeRef ext = CFDictionaryGetValue(d, FMT("PMExtendedData"));
        if (ext != NULL && CFGetTypeID(ext) == CFDictionaryGetTypeID()) {
            CFRelease((*pageFormat)->extended);
            (*pageFormat)->extended = CFDictionaryCreateMutableCopy(NULL, 0, ext);
        }
    }
    CFRelease(d);
    return err;
}

static CFStringRef keyForDataID(OSType dataID) {
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("%08x"), (unsigned)dataID);
}

/* An app's own data in the page format, by its four-character ID; a NULL
   buffer asks for the size. */
OSStatus PMGetPageFormatExtendedData(PMPageFormat f, OSType dataID, UInt32 *size, void *extendedData) {
    if (kindOf(f) != kFormat || size == NULL)
        return kPMInvalidPageFormat;
    CFStringRef key = keyForDataID(dataID);
    CFDataRef d = CFDictionaryGetValue(f->extended, key);
    CFRelease(key);
    if (d == NULL)
        return kPMNoSuchEntry;
    UInt32 length = (UInt32)CFDataGetLength(d);
    if (extendedData == NULL) {
        *size = length;
        return kPMNoError;
    }
    if (*size < length)
        return kPMValueOutOfRange;
    memcpy(extendedData, CFDataGetBytePtr(d), length);
    *size = length;
    return kPMNoError;
}

OSStatus PMSetPageFormatExtendedData(PMPageFormat f, OSType dataID, UInt32 size, void *extendedData) {
    if (kindOf(f) != kFormat || (size > 0 && extendedData == NULL))
        return kPMInvalidPageFormat;
    CFStringRef key = keyForDataID(dataID);
    CFDataRef d = CFDataCreate(NULL, extendedData, size);
    CFDictionarySetValue(f->extended, key, d);
    CFRelease(d);
    CFRelease(key);
    return kPMNoError;
}

OSStatus PMPageFormatGetPrinterID(PMPageFormat f, CFStringRef *printerID) {
    if (kindOf(f) != kFormat || printerID == NULL)
        return kPMInvalidPageFormat;
    *printerID = f->paper->printerID;
    return kPMNoError;
}

OSStatus PMGetPageFormatPaper(PMPageFormat f, PMPaper *paper) {
    if (kindOf(f) != kFormat || paper == NULL)
        return kPMInvalidPageFormat;
    *paper = f->paper;
    return kPMNoError;
}

static Boolean isLandscape(PMOrientation o) {
    return o == kPMLandscape || o == kPMReverseLandscape;
}

/* The sheet and its margins as the orientation turns them, in points. */
static void orientedPaper(Format *f, double *w, double *h, PMPaperMargins *m) {
    PMPaperMargins p = f->paper->margins;
    if (isLandscape(f->orientation)) {
        *w = f->paper->height;
        *h = f->paper->width;
        *m = (PMPaperMargins){ p.left, p.bottom, p.right, p.top };
    } else {
        *w = f->paper->width;
        *h = f->paper->height;
        *m = p;
    }
}

static void rects(Format *f, Boolean adjusted, PMRect *page, PMRect *paper) {
    double w = f->paper->width, h = f->paper->height, s = 1;
    PMPaperMargins m = f->paper->margins;
    if (adjusted) {
        orientedPaper(f, &w, &h, &m);
        s = f->scale > 0 ? f->scale / 100 : 1;
    }
    double pw = (w - m.left - m.right) / s, ph = (h - m.top - m.bottom) / s;
    if (page != NULL)
        *page = (PMRect){ 0, 0, ph, pw };
    if (paper != NULL)
        *paper = (PMRect){ -m.top / s, -m.left / s, (h - m.top) / s, (w - m.left) / s };
}

OSStatus PMGetAdjustedPageRect(PMPageFormat f, PMRect *pageRect) {
    if (kindOf(f) != kFormat || pageRect == NULL)
        return kPMInvalidPageFormat;
    rects(f, true, pageRect, NULL);
    return kPMNoError;
}

OSStatus PMGetAdjustedPaperRect(PMPageFormat f, PMRect *paperRect) {
    if (kindOf(f) != kFormat || paperRect == NULL)
        return kPMInvalidPageFormat;
    rects(f, true, NULL, paperRect);
    return kPMNoError;
}

OSStatus PMGetUnadjustedPageRect(PMPageFormat f, PMRect *pageRect) {
    if (kindOf(f) != kFormat || pageRect == NULL)
        return kPMInvalidPageFormat;
    rects(f, false, pageRect, NULL);
    return kPMNoError;
}

OSStatus PMGetUnadjustedPaperRect(PMPageFormat f, PMRect *paperRect) {
    if (kindOf(f) != kFormat || paperRect == NULL)
        return kPMInvalidPageFormat;
    rects(f, false, NULL, paperRect);
    return kPMNoError;
}

OSStatus PMGetOrientation(PMPageFormat f, PMOrientation *orientation) {
    if (kindOf(f) != kFormat || orientation == NULL)
        return kPMInvalidPageFormat;
    *orientation = f->orientation;
    return kPMNoError;
}

OSStatus PMSetOrientation(PMPageFormat f, PMOrientation orientation, Boolean lock) {
    (void)lock;
    if (kindOf(f) != kFormat)
        return kPMInvalidPageFormat;
    if (orientation < kPMPortrait || orientation > kPMReverseLandscape)
        return kPMValueOutOfRange;
    f->orientation = orientation;
    return kPMNoError;
}

OSStatus PMGetScale(PMPageFormat f, double *scale) {
    if (kindOf(f) != kFormat || scale == NULL)
        return kPMInvalidPageFormat;
    *scale = f->scale;
    return kPMNoError;
}

OSStatus PMSetScale(PMPageFormat f, double scale) {
    if (kindOf(f) != kFormat)
        return kPMInvalidPageFormat;
    if (scale <= 0)
        return kPMValueOutOfRange;
    f->scale = scale;
    return kPMNoError;
}

/* ---- Print settings --------------------------------------------------- */

#define kJobNameKey      CFSTR("com.apple.print.JobInfo.PMJobName")
#define kFirstPageKey    CFSTR("com.apple.print.PrintSettings.PMFirstPage")
#define kLastPageKey     CFSTR("com.apple.print.PrintSettings.PMLastPage")
#define kPageRangeKey    CFSTR("com.apple.print.PrintSettings.PMPageRange")

OSStatus PMCreatePrintSettings(PMPrintSettings *printSettings) {
    if (printSettings == NULL)
        return kPMInvalidParameter;
    Settings *s = newObject(sizeof(Settings), kSettings);
    if (s == NULL)
        return kPMGeneralError;
    s->values = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    *printSettings = s;
    return kPMNoError;
}

OSStatus PMCopyPrintSettings(PMPrintSettings source, PMPrintSettings dest) {
    if (kindOf(source) != kSettings || kindOf(dest) != kSettings)
        return kPMInvalidPrintSettings;
    if (source == dest)
        return kPMNoError;
    CFRelease(dest->values);
    dest->values = CFDictionaryCreateMutableCopy(NULL, 0, source->values);
    return kPMNoError;
}

OSStatus PMPrintSettingsCreateDataRepresentation(PMPrintSettings s, CFDataRef *data, PMDataFormat format) {
    if (kindOf(s) != kSettings || data == NULL)
        return kPMInvalidPrintSettings;
    *data = dataOf(s->values, format);
    return *data != NULL ? kPMNoError : kPMGeneralError;
}

OSStatus PMPrintSettingsCreateWithDataRepresentation(CFDataRef data, PMPrintSettings *printSettings) {
    CFDictionaryRef d = dictionaryOf(data);
    if (d == NULL || printSettings == NULL) {
        release(d);
        return kPMInvalidParameter;
    }
    OSStatus err = PMCreatePrintSettings(printSettings);
    if (err == kPMNoError) {
        CFRelease((*printSettings)->values);
        (*printSettings)->values = CFDictionaryCreateMutableCopy(NULL, 0, d);
    }
    CFRelease(d);
    return err;
}

OSStatus PMPrintSettingsCopyAsDictionary(PMPrintSettings s, CFDictionaryRef *dictionary) {
    if (kindOf(s) != kSettings || dictionary == NULL)
        return kPMInvalidPrintSettings;
    *dictionary = CFDictionaryCreateCopy(NULL, s->values);
    return kPMNoError;
}

OSStatus PMPrintSettingsCopyKeys(PMPrintSettings s, CFArrayRef *keys) {
    if (kindOf(s) != kSettings || keys == NULL)
        return kPMInvalidPrintSettings;
    CFIndex n = CFDictionaryGetCount(s->values);
    const void **k = malloc(sizeof(void *) * (size_t)(n > 0 ? n : 1));
    CFDictionaryGetKeysAndValues(s->values, k, NULL);
    *keys = CFArrayCreate(NULL, k, n, &kCFTypeArrayCallBacks);
    free(k);
    return kPMNoError;
}

OSStatus PMPrintSettingsGetValue(PMPrintSettings s, CFStringRef key, CFTypeRef *value) {
    if (kindOf(s) != kSettings || key == NULL || value == NULL)
        return kPMInvalidPrintSettings;
    *value = CFDictionaryGetValue(s->values, key);
    return *value != NULL ? kPMNoError : kPMNoSuchEntry;
}

OSStatus PMPrintSettingsSetValue(PMPrintSettings s, CFStringRef key, CFTypeRef value, Boolean locked) {
    (void)locked;
    if (kindOf(s) != kSettings || key == NULL)
        return kPMInvalidPrintSettings;
    if (value == NULL)
        CFDictionaryRemoveValue(s->values, key);
    else
        CFDictionarySetValue(s->values, key, value);
    return kPMNoError;
}

OSStatus PMPrintSettingsGetJobName(PMPrintSettings s, CFStringRef *name) {
    if (kindOf(s) != kSettings || name == NULL)
        return kPMInvalidPrintSettings;
    *name = getString(s->values, kJobNameKey, CFSTR("Untitled"));
    return kPMNoError;
}

OSStatus PMPrintSettingsSetJobName(PMPrintSettings s, CFStringRef name) {
    return PMPrintSettingsSetValue(s, kJobNameKey, name, false);
}

static UInt32 getUInt(Settings *s, CFStringRef key, UInt32 fallback) {
    double v = getNumber(s->values, key, -1);
    return v < 0 ? fallback : (UInt32)v;
}

static OSStatus setUInt(PMPrintSettings s, CFStringRef key, UInt32 v) {
    if (kindOf(s) != kSettings)
        return kPMInvalidPrintSettings;
    long long n = v;
    CFNumberRef num = CFNumberCreate(NULL, kCFNumberLongLongType, &n);
    CFDictionarySetValue(s->values, key, num);
    CFRelease(num);
    return kPMNoError;
}

OSStatus PMGetCopies(PMPrintSettings s, UInt32 *copies) {
    if (kindOf(s) != kSettings || copies == NULL)
        return kPMInvalidPrintSettings;
    *copies = getUInt(s, kPMCopiesKey, 1);
    return kPMNoError;
}

OSStatus PMSetCopies(PMPrintSettings s, UInt32 copies, Boolean lock) {
    (void)lock;
    if (copies == 0)
        return kPMValueOutOfRange;
    return setUInt(s, kPMCopiesKey, copies);
}

OSStatus PMGetCollate(PMPrintSettings s, Boolean *collate) {
    if (kindOf(s) != kSettings || collate == NULL)
        return kPMInvalidPrintSettings;
    CFTypeRef v = CFDictionaryGetValue(s->values, kPMCopyCollateKey);
    *collate = v == NULL || (CFGetTypeID(v) == CFBooleanGetTypeID() && CFBooleanGetValue(v));
    return kPMNoError;
}

OSStatus PMSetCollate(PMPrintSettings s, Boolean collate) {
    return PMPrintSettingsSetValue(s, kPMCopyCollateKey, collate ? kCFBooleanTrue : kCFBooleanFalse, false);
}

OSStatus PMGetFirstPage(PMPrintSettings s, UInt32 *first) {
    if (kindOf(s) != kSettings || first == NULL)
        return kPMInvalidPrintSettings;
    *first = getUInt(s, kFirstPageKey, 1);
    return kPMNoError;
}

OSStatus PMSetFirstPage(PMPrintSettings s, UInt32 first, Boolean lock) {
    (void)lock;
    if (first == 0)
        return kPMValueOutOfRange;
    return setUInt(s, kFirstPageKey, first);
}

OSStatus PMGetLastPage(PMPrintSettings s, UInt32 *last) {
    if (kindOf(s) != kSettings || last == NULL)
        return kPMInvalidPrintSettings;
    *last = getUInt(s, kLastPageKey, kPMPrintAllPages);
    return kPMNoError;
}

OSStatus PMSetLastPage(PMPrintSettings s, UInt32 last, Boolean lock) {
    (void)lock;
    if (last == 0)
        return kPMValueOutOfRange;
    return setUInt(s, kLastPageKey, last);
}

/* The pages the user may choose from (the document's). */
OSStatus PMGetPageRange(PMPrintSettings s, UInt32 *minPage, UInt32 *maxPage) {
    if (kindOf(s) != kSettings || minPage == NULL || maxPage == NULL)
        return kPMInvalidPrintSettings;
    CFTypeRef r = CFDictionaryGetValue(s->values, kPageRangeKey);
    *minPage = 1;
    *maxPage = kPMPrintAllPages;
    if (r != NULL && CFGetTypeID(r) == CFArrayGetTypeID() && CFArrayGetCount(r) == 2) {
        long long a = 1, b = kPMPrintAllPages;
        CFNumberGetValue(CFArrayGetValueAtIndex(r, 0), kCFNumberLongLongType, &a);
        CFNumberGetValue(CFArrayGetValueAtIndex(r, 1), kCFNumberLongLongType, &b);
        *minPage = (UInt32)a;
        *maxPage = (UInt32)b;
    }
    return kPMNoError;
}

OSStatus PMSetPageRange(PMPrintSettings s, UInt32 minPage, UInt32 maxPage) {
    if (kindOf(s) != kSettings)
        return kPMInvalidPrintSettings;
    if (minPage == 0 || maxPage < minPage)
        return kPMValueOutOfRange;
    long long a = minPage, b = maxPage;
    CFNumberRef n[2] = { CFNumberCreate(NULL, kCFNumberLongLongType, &a), CFNumberCreate(NULL, kCFNumberLongLongType, &b) };
    CFArrayRef r = CFArrayCreate(NULL, (const void **)n, 2, &kCFTypeArrayCallBacks);
    CFDictionarySetValue(s->values, kPageRangeKey, r);
    CFRelease(r);
    CFRelease(n[0]);
    CFRelease(n[1]);
    /* The pages chosen stay within the range. */
    if (getUInt(s, kFirstPageKey, 1) < minPage)
        setUInt(s, kFirstPageKey, minPage);
    if (maxPage != kPMPrintAllPages && getUInt(s, kLastPageKey, kPMPrintAllPages) > maxPage)
        setUInt(s, kLastPageKey, maxPage);
    return kPMNoError;
}

OSStatus PMGetDuplex(PMPrintSettings s, PMDuplexMode *duplex) {
    if (kindOf(s) != kSettings || duplex == NULL)
        return kPMInvalidPrintSettings;
    *duplex = getUInt(s, kPMDuplexingKey, kPMDuplexNone);
    return kPMNoError;
}

OSStatus PMSetDuplex(PMPrintSettings s, PMDuplexMode duplex) {
    if (duplex < kPMDuplexNone || duplex > kPMSimplexTumble)
        return kPMValueOutOfRange;
    return setUInt(s, kPMDuplexingKey, duplex);
}

/* The settings as CUPS job options ("copies=2 page-ranges=1-3 ..."). */
static CFStringRef copyOptions(Settings *s, Format *f) {
    CFMutableStringRef o = CFStringCreateMutable(NULL, 0);
    UInt32 copies = getUInt(s, kPMCopiesKey, 1), first = getUInt(s, kFirstPageKey, 1),
           last = getUInt(s, kLastPageKey, kPMPrintAllPages);
    Boolean collate = true;
    PMDuplexMode duplex = kPMDuplexNone;
    PMGetCollate(s, &collate);
    PMGetDuplex(s, &duplex);
    CFStringAppendFormat(o, NULL, CFSTR("copies=%u"), (unsigned)copies);
    if (first > 1 || last != kPMPrintAllPages) {
        if (last == kPMPrintAllPages)
            CFStringAppendFormat(o, NULL, CFSTR(" page-ranges=%u-"), (unsigned)first);
        else
            CFStringAppendFormat(o, NULL, CFSTR(" page-ranges=%u-%u"), (unsigned)first, (unsigned)last);
    }
    if (copies > 1)
        CFStringAppend(o, collate ? CFSTR(" multiple-document-handling=separate-documents-collated-copies")
                                  : CFSTR(" multiple-document-handling=separate-documents-uncollated-copies"));
    CFStringAppend(o, duplex == kPMDuplexNoTumble ? CFSTR(" sides=two-sided-long-edge")
                      : duplex == kPMDuplexTumble ? CFSTR(" sides=two-sided-short-edge")
                                                  : CFSTR(" sides=one-sided"));
    if (f != NULL) {
        CFStringAppendFormat(o, NULL, CFSTR(" media=%@"), f->paper->paperID);
        if (isLandscape(f->orientation))
            CFStringAppend(o, CFSTR(" landscape"));
    }
    return o;
}

OSStatus PMPrintSettingsToOptions(PMPrintSettings s, char **options) {
    if (kindOf(s) != kSettings || options == NULL)
        return kPMInvalidPrintSettings;
    CFStringRef o = copyOptions(s, NULL);
    *options = cString(o);
    CFRelease(o);
    return kPMNoError;
}

OSStatus PMPrintSettingsToOptionsWithPrinterAndPageFormat(PMPrintSettings s, PMPrinter printer, PMPageFormat f,
                                                          CFStringRef *options) {
    (void)printer;
    if (kindOf(s) != kSettings || options == NULL)
        return kPMInvalidPrintSettings;
    *options = copyOptions(s, kindOf(f) == kFormat ? f : NULL);
    return kPMNoError;
}

/* ---- Presets ---------------------------------------------------------- */

OSStatus PMPresetCopyName(PMPreset preset, CFStringRef *name) {
    (void)name;
    return kindOf(preset) == kPreset ? kPMNoError : kPMInvalidPreset;
}

OSStatus PMPresetCreatePrintSettings(PMPreset preset, PMPrintSession session, PMPrintSettings *printSettings) {
    (void)session;
    (void)printSettings;
    return kindOf(preset) == kPreset ? kPMNoError : kPMInvalidPreset;
}

OSStatus PMPresetGetAttributes(PMPreset preset, CFDictionaryRef *attributes) {
    (void)attributes;
    return kindOf(preset) == kPreset ? kPMNoError : kPMInvalidPreset;
}

/* ---- Sessions --------------------------------------------------------- */

OSStatus PMCreateSession(PMPrintSession *printSession) {
    if (printSession == NULL)
        return kPMInvalidParameter;
    Session *s = newObject(sizeof(Session), kSession);
    if (s == NULL)
        return kPMGeneralError;
    s->data = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    *printSession = s;
    return kPMNoError;
}

OSStatus PMSessionError(PMPrintSession s) {
    return kindOf(s) == kSession ? s->error : kPMInvalidPrintSession;
}

OSStatus PMSessionSetError(PMPrintSession s, OSStatus printError) {
    if (kindOf(s) != kSession)
        return kPMInvalidPrintSession;
    s->error = printError;
    return kPMNoError;
}

/* The session's printer: the one chosen, else CUPS's default, else the
   generic printer. */
OSStatus PMSessionGetCurrentPrinter(PMPrintSession s, PMPrinter *currentPrinter) {
    if (kindOf(s) != kSession || currentPrinter == NULL)
        return kPMInvalidPrintSession;
    if (s->printer == NULL) {
        CFIndex def = -1;
        CFArrayRef list = copyPrinters(&def);
        if (def >= 0) {
            s->printer = (PMPrinter)CFArrayGetValueAtIndex(list, def);
            PMRetain(s->printer);
        }
        CFRelease(list);
        if (s->printer == NULL)
            s->printer = genericPrinter();
    }
    *currentPrinter = s->printer;
    return s->printer != NULL ? kPMNoError : kPMNoDefaultPrinter;
}

OSStatus PMSessionSetCurrentPMPrinter(PMPrintSession s, PMPrinter printer) {
    if (kindOf(s) != kSession)
        return kPMInvalidPrintSession;
    if (kindOf(printer) != kPrinter)
        return kPMInvalidPrinter;
    PMRetain(printer);
    PMRelease(s->printer);
    s->printer = printer;
    return kPMNoError;
}

OSStatus PMSessionCreatePrinterList(PMPrintSession s, CFArrayRef *printerList, CFIndex *currentIndex,
                                    PMPrinter *currentPrinter) {
    if (kindOf(s) != kSession || printerList == NULL)
        return kPMInvalidPrintSession;
    PMPrinter current = NULL;
    PMSessionGetCurrentPrinter(s, &current);
    CFArrayRef raw = copyPrinters(NULL);
    CFMutableArrayRef names = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFIndex index = -1;
    for (CFIndex i = 0; i < CFArrayGetCount(raw); i++) {
        Printer *p = (Printer *)CFArrayGetValueAtIndex(raw, i);
        if (current != NULL && CFEqual(p->printerID, current->printerID))
            index = i;
        CFArrayAppendValue(names, p->name);
    }
    CFRelease(raw);
    *printerList = names;
    if (currentIndex != NULL)
        *currentIndex = index;
    if (currentPrinter != NULL)
        *currentPrinter = current;
    return kPMNoError;
}

OSStatus PMSessionCreatePageFormatList(PMPrintSession s, PMPrinter printer, CFArrayRef *pageFormatList) {
    if (kindOf(s) != kSession || pageFormatList == NULL)
        return kPMInvalidPrintSession;
    if (printer == NULL)
        PMSessionGetCurrentPrinter(s, &printer);
    CFArrayRef papers = NULL;
    OSStatus err = PMPrinterGetPaperList(printer, &papers);
    if (err != kPMNoError)
        return err;
    CFMutableArrayRef formats = CFArrayCreateMutable(NULL, 0, &objectCallBacks);
    for (CFIndex i = 0; i < CFArrayGetCount(papers); i++) {
        PMPageFormat f = NULL;
        if (PMCreatePageFormatWithPMPaper(&f, (PMPaper)CFArrayGetValueAtIndex(papers, i)) == kPMNoError)
            appendOwned(formats, f);
    }
    *pageFormatList = formats;
    return kPMNoError;
}

OSStatus PMSessionGetDataFromSession(PMPrintSession s, CFStringRef key, CFTypeRef *data) {
    if (kindOf(s) != kSession || key == NULL || data == NULL)
        return kPMInvalidPrintSession;
    *data = CFDictionaryGetValue(s->data, key);
    return *data != NULL ? kPMNoError : kPMNoSuchEntry;
}

OSStatus PMSessionSetDataInSession(PMPrintSession s, CFStringRef key, CFTypeRef data) {
    if (kindOf(s) != kSession || key == NULL)
        return kPMInvalidPrintSession;
    if (data == NULL)
        CFDictionaryRemoveValue(s->data, key);
    else
        CFDictionarySetValue(s->data, key, data);
    return kPMNoError;
}

/* The defaults: the locale's paper for the session's printer, portrait,
   at full size; one copy, every page, collated. */
OSStatus PMSessionDefaultPageFormat(PMPrintSession s, PMPageFormat f) {
    if (kindOf(s) != kSession)
        return kPMInvalidPrintSession;
    if (kindOf(f) != kFormat)
        return kPMInvalidPageFormat;
    PMPrinter printer = NULL;
    PMSessionGetCurrentPrinter(s, &printer);
    Paper *d = defaultPaper();
    Paper *p = newPaper(d->paperID, d->name, printer ? printer->printerID : NULL, d->width, d->height, NULL, false);
    PMRelease(d);
    PMRelease(f->paper);
    f->paper = p;
    f->orientation = kPMPortrait;
    f->scale = 100;
    return kPMNoError;
}

OSStatus PMSessionDefaultPrintSettings(PMPrintSession s, PMPrintSettings settings) {
    if (kindOf(s) != kSession)
        return kPMInvalidPrintSession;
    if (kindOf(settings) != kSettings)
        return kPMInvalidPrintSettings;
    CFDictionaryRemoveAllValues(settings->values);
    setUInt(settings, kPMCopiesKey, 1);
    setUInt(settings, kFirstPageKey, 1);
    setUInt(settings, kLastPageKey, kPMPrintAllPages);
    PMSetCollate(settings, true);
    setUInt(settings, kPMDestinationTypeKey, kPMDestinationPrinter);
    return kPMNoError;
}

OSStatus PMSessionValidatePageFormat(PMPrintSession s, PMPageFormat f, Boolean *changed) {
    if (kindOf(s) != kSession)
        return kPMInvalidPrintSession;
    if (kindOf(f) != kFormat)
        return kPMInvalidPageFormat;
    Boolean c = false;
    if (f->scale <= 0) {
        f->scale = 100;
        c = true;
    }
    if (f->orientation < kPMPortrait || f->orientation > kPMReverseLandscape) {
        f->orientation = kPMPortrait;
        c = true;
    }
    if (changed != NULL)
        *changed = c;
    return kPMNoError;
}

OSStatus PMSessionValidatePrintSettings(PMPrintSession s, PMPrintSettings settings, Boolean *changed) {
    if (kindOf(s) != kSession)
        return kPMInvalidPrintSession;
    if (kindOf(settings) != kSettings)
        return kPMInvalidPrintSettings;
    Boolean c = false;
    if (getUInt(settings, kPMCopiesKey, 1) == 0) {
        setUInt(settings, kPMCopiesKey, 1);
        c = true;
    }
    UInt32 first = getUInt(settings, kFirstPageKey, 1), last = getUInt(settings, kLastPageKey, kPMPrintAllPages);
    if (first == 0 || last < first) {
        setUInt(settings, kFirstPageKey, 1);
        setUInt(settings, kLastPageKey, kPMPrintAllPages);
        c = true;
    }
    if (changed != NULL)
        *changed = c;
    return kPMNoError;
}

/* ---- Destinations ----------------------------------------------------- */

OSStatus PMSessionSetDestination(PMPrintSession s, PMPrintSettings settings, PMDestinationType destType,
                                 CFStringRef destFormat, CFURLRef destLocation) {
    if (kindOf(s) != kSession)
        return kPMInvalidPrintSession;
    if (kindOf(settings) != kSettings)
        return kPMInvalidPrintSettings;
    if (destType < kPMDestinationPrinter || destType > kPMDestinationProcessPDF || destType == kPMDestinationFax)
        return kPMValueOutOfRange;
    setUInt(settings, kPMDestinationTypeKey, destType);
    PMPrintSettingsSetValue(settings, kPMDesiredFormatKey, destFormat, false);
    CFStringRef path = destLocation != NULL ? CFURLCopyFileSystemPath(destLocation, kCFURLPOSIXPathStyle) : NULL;
    PMPrintSettingsSetValue(settings, kPMOutputFilenameKey, path, false);
    release(path);
    return kPMNoError;
}

OSStatus PMSessionGetDestinationType(PMPrintSession s, PMPrintSettings settings, PMDestinationType *destType) {
    if (kindOf(s) != kSession)
        return kPMInvalidPrintSession;
    if (kindOf(settings) != kSettings || destType == NULL)
        return kPMInvalidPrintSettings;
    *destType = (PMDestinationType)getUInt(settings, kPMDestinationTypeKey, kPMDestinationPrinter);
    return kPMNoError;
}

OSStatus PMSessionCopyDestinationFormat(PMPrintSession s, PMPrintSettings settings, CFStringRef *destFormat) {
    if (kindOf(s) != kSession)
        return kPMInvalidPrintSession;
    if (kindOf(settings) != kSettings || destFormat == NULL)
        return kPMInvalidPrintSettings;
    *destFormat = CFRetain(getString(settings->values, kPMDesiredFormatKey, kPMDocumentFormatPDF));
    return kPMNoError;
}

OSStatus PMSessionCopyDestinationLocation(PMPrintSession s, PMPrintSettings settings, CFURLRef *destLocation) {
    if (kindOf(s) != kSession)
        return kPMInvalidPrintSession;
    if (kindOf(settings) != kSettings || destLocation == NULL)
        return kPMInvalidPrintSettings;
    CFStringRef path = getString(settings->values, kPMOutputFilenameKey, NULL);
    *destLocation = path != NULL ? CFURLCreateWithFileSystemPath(NULL, path, kCFURLPOSIXPathStyle, false) : NULL;
    return kPMNoError;
}

OSStatus PMSessionCopyOutputFormatList(PMPrintSession s, PMDestinationType destType, CFArrayRef *formats) {
    if (kindOf(s) != kSession || formats == NULL)
        return kPMInvalidPrintSession;
    const void *pdf[] = { kPMDocumentFormatPDF };
    *formats = destType == kPMDestinationInvalid ? CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks)
                                                 : CFArrayCreate(NULL, pdf, 1, &kCFTypeArrayCallBacks);
    return kPMNoError;
}

/* ---- Printing a document ---------------------------------------------- */

static CFURLRef temporaryPDF(void) {
    char dir[1024], path[1100];
    if (confstr(_CS_DARWIN_USER_TEMP_DIR, dir, sizeof(dir)) == 0)
        strcpy(dir, "/tmp/");
    snprintf(path, sizeof(path), "%sprint-XXXXXX.pdf", dir);
    int fd = mkstemps(path, 4);
    if (fd < 0)
        return NULL;
    close(fd);
    return CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, (CFIndex)strlen(path), false);
}

OSStatus PMSessionBeginCGDocumentNoDialog(PMPrintSession s, PMPrintSettings settings, PMPageFormat f) {
    if (kindOf(s) != kSession)
        return kPMInvalidPrintSession;
    if (kindOf(settings) != kSettings)
        return kPMInvalidPrintSettings;
    if (kindOf(f) != kFormat)
        return kPMInvalidPageFormat;
    if (s->context != NULL)
        return kPMObjectInUse;
    PMDestinationType type = kPMDestinationPrinter;
    PMSessionGetDestinationType(s, settings, &type);
    CFStringRef format = getString(settings->values, kPMDesiredFormatKey, kPMDocumentFormatPDF);
    if (!CFEqual(format, kPMDocumentFormatPDF) && !CFEqual(format, kPMDocumentFormatDefault))
        return kPMInvalidFileType;
    CFURLRef output = NULL;
    Boolean temporary = false;
    if (type == kPMDestinationFile) {
        PMSessionCopyDestinationLocation(s, settings, &output);
        if (output == NULL)
            return kPMInvalidParameter;
    } else {
        output = temporaryPDF();
        temporary = true;
        if (output == NULL)
            return kPMGeneralError;
    }
    double w, h;
    PMPaperMargins m;
    orientedPaper(f, &w, &h, &m);
    CGRect media = CGRectMake(0, 0, w, h);
    CFStringRef title = NULL;
    PMPrintSettingsGetJobName(settings, &title);
    const void *k[] = { kCGPDFContextTitle, kCGPDFContextCreator };
    const void *v[] = { title, CFSTR("Minkar PrintCore") };
    CFDictionaryRef info = CFDictionaryCreate(NULL, k, v, 2, &kCFTypeDictionaryKeyCallBacks,
                                              &kCFTypeDictionaryValueCallBacks);
    s->context = CGPDFContextCreateWithURL(output, &media, info);
    CFRelease(info);
    if (s->context == NULL) {
        CFRelease(output);
        s->error = kPMGeneralError;
        return kPMGeneralError;
    }
    s->output = output;
    s->temporary = temporary;
    PMRetain(settings);
    PMRetain(f);
    s->settings = settings;
    s->format = f;
    s->inPage = false;
    s->error = kPMNoError;
    return kPMNoError;
}

OSStatus PMSessionBeginPageNoDialog(PMPrintSession s, PMPageFormat f, const PMRect *pageFrame) {
    (void)pageFrame;   /* must be NULL: the page format gives the page */
    if (kindOf(s) != kSession)
        return kPMInvalidPrintSession;
    if (s->context == NULL || s->inPage)
        return kPMOutOfScope;
    Format *format = kindOf(f) == kFormat ? f : s->format;
    double w, h;
    PMPaperMargins m;
    orientedPaper(format, &w, &h, &m);
    CGRect media = CGRectMake(0, 0, w, h);
    CFDataRef box = CFDataCreate(NULL, (const UInt8 *)&media, sizeof(media));
    const void *k[] = { kCGPDFContextMediaBox };
    const void *v[] = { box };
    CFDictionaryRef info = CFDictionaryCreate(NULL, k, v, 1, &kCFTypeDictionaryKeyCallBacks,
                                              &kCFTypeDictionaryValueCallBacks);
    CGPDFContextBeginPage(s->context, info);
    CFRelease(info);
    CFRelease(box);
    CGContextSaveGState(s->context);
    /* The origin at the bottom left of the imageable area, at the scale. */
    CGContextTranslateCTM(s->context, m.left, m.bottom);
    double scale = format->scale > 0 ? format->scale / 100 : 1;
    CGContextScaleCTM(s->context, scale, scale);
    s->inPage = true;
    return kPMNoError;
}

OSStatus PMSessionEndPageNoDialog(PMPrintSession s) {
    if (kindOf(s) != kSession)
        return kPMInvalidPrintSession;
    if (!s->inPage)
        return kPMOutOfScope;
    CGContextRestoreGState(s->context);
    CGPDFContextEndPage(s->context);
    s->inPage = false;
    return kPMNoError;
}

OSStatus PMSessionGetCGGraphicsContext(PMPrintSession s, CGContextRef *context) {
    if (kindOf(s) != kSession || context == NULL)
        return kPMInvalidPrintSession;
    *context = s->inPage ? s->context : NULL;
    return s->inPage ? kPMNoError : kPMOutOfScope;
}

/* The finished PDF to a printer, through CUPS. */
static OSStatus printFile(Printer *p, Settings *settings, Format *f, const char *path, CFStringRef mimeType) {
    if (p == NULL || p->generic)
        return kPMInvalidPrinter;
    char *name = cString(p->printerID);
    CFStringRef optionString = copyOptions(settings, f);
    char *opts = cString(optionString);
    CFRelease(optionString);
    cups_option_t *options = NULL;
    int count = cupsParseOptions(opts, 0, &options);
    if (mimeType != NULL && !CFEqual(mimeType, kPMDocumentFormatDefault)) {
        char *mime = cString(mimeType);
        count = cupsAddOption("document-format", mime, count, &options);
        free(mime);
    }
    CFStringRef title = NULL;
    PMPrintSettingsGetJobName(settings, &title);
    char *t = cString(title);
    int job = cupsPrintFile2(CUPS_HTTP_DEFAULT, name, path, t, count, options);
    cupsFreeOptions(count, options);
    free(t);
    free(opts);
    free(name);
    return job > 0 ? kPMNoError : kPMGeneralError;
}

static void endDocument(Session *s) {
    if (s->inPage)
        PMSessionEndPageNoDialog(s);
    CGPDFContextClose(s->context);
    CGContextRelease(s->context);
    s->context = NULL;
    OSStatus err = kPMNoError;
    if (s->temporary) {
        PMDestinationType type = kPMDestinationPrinter;
        PMSessionGetDestinationType(s, s->settings, &type);
        char path[1024];
        CFURLGetFileSystemRepresentation(s->output, true, (UInt8 *)path, sizeof(path));
        if (type == kPMDestinationPrinter) {
            PMPrinter printer = NULL;
            PMSessionGetCurrentPrinter(s, &printer);
            err = printFile(printer, s->settings, s->format, path, kPMDocumentFormatPDF);
            unlink(path);
        } else if (type == kPMDestinationPreview) {
            /* Opened by the app for PDFs; the file stays for it. */
            err = LSOpenCFURLRef(s->output, NULL) == noErr ? kPMNoError : kPMGeneralError;
        } else if (type == kPMDestinationProcessPDF) {
            CFURLRef item = NULL;
            PMSessionCopyDestinationLocation(s, s->settings, &item);
            err = item != NULL ? PMWorkflowSubmitPDFWithSettings(item, s->settings, s->output) : kPMInvalidParameter;
            release(item);
            unlink(path);
        }
    }
    if (err != kPMNoError)
        s->error = err;
    CFRelease(s->output);
    s->output = NULL;
    PMRelease(s->settings);
    PMRelease(s->format);
    s->settings = NULL;
    s->format = NULL;
}

OSStatus PMSessionEndDocumentNoDialog(PMPrintSession s) {
    if (kindOf(s) != kSession)
        return kPMInvalidPrintSession;
    if (s->context == NULL)
        return kPMOutOfScope;
    endDocument(s);
    return s->error;
}

/* A file printed as it is: CUPS takes PDF, PostScript, images and text. */
OSStatus PMPrinterPrintWithFile(PMPrinter printer, PMPrintSettings settings, PMPageFormat format,
                                CFStringRef mimeType, CFURLRef fileURL) {
    if (kindOf(printer) != kPrinter)
        return kPMInvalidPrinter;
    if (kindOf(settings) != kSettings)
        return kPMInvalidPrintSettings;
    if (fileURL == NULL)
        return kPMInvalidParameter;
    char path[1024];
    if (!CFURLGetFileSystemRepresentation(fileURL, true, (UInt8 *)path, sizeof(path)))
        return kPMInvalidParameter;
    return printFile(printer, settings, kindOf(format) == kFormat ? format : NULL, path, mimeType);
}

OSStatus PMPrinterPrintWithProvider(PMPrinter printer, PMPrintSettings settings, PMPageFormat format,
                                    CFStringRef mimeType, CGDataProviderRef provider) {
    if (provider == NULL)
        return kPMInvalidParameter;
    CFDataRef data = CGDataProviderCopyData(provider);
    CFURLRef tmp = temporaryPDF();
    if (data == NULL || tmp == NULL) {
        release(data);
        release(tmp);
        return kPMGeneralError;
    }
    char path[1024];
    CFURLGetFileSystemRepresentation(tmp, true, (UInt8 *)path, sizeof(path));
    FILE *out = fopen(path, "wb");
    if (out != NULL) {
        fwrite(CFDataGetBytePtr(data), 1, (size_t)CFDataGetLength(data), out);
        fclose(out);
    }
    OSStatus err = PMPrinterPrintWithFile(printer, settings, format, mimeType, tmp);
    unlink(path);
    CFRelease(tmp);
    CFRelease(data);
    return err;
}

/* ---- Workflows -------------------------------------------------------- */

/* What the PDF services folders hold: each item's URL and name. */
OSStatus PMWorkflowCopyItems(CFArrayRef *workflowItems) {
    if (workflowItems == NULL)
        return kPMInvalidParameter;
    CFMutableArrayRef items = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    const char *home = getenv("HOME");
    char dirs[2][1024];
    snprintf(dirs[0], sizeof(dirs[0]), "%s/Library/PDF Services", home != NULL ? home : "");
    snprintf(dirs[1], sizeof(dirs[1]), "/Library/PDF Services");
    for (int i = 0; i < 2; i++) {
        CFURLRef dir = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)dirs[i], (CFIndex)strlen(dirs[i]), true);
        CFURLEnumeratorRef e = CFURLEnumeratorCreateForDirectoryURL(NULL, dir, kCFURLEnumeratorDefaultBehavior, NULL);
        CFURLRef url = NULL;
        while (e != NULL && CFURLEnumeratorGetNextURL(e, &url, NULL) == kCFURLEnumeratorSuccess) {
            CFStringRef name = CFURLCopyLastPathComponent(url);
            const void *k[] = { CFSTR("itemURL"), CFSTR("displayName") };
            const void *v[] = { url, name };
            CFDictionaryRef d = CFDictionaryCreate(NULL, k, v, 2, &kCFTypeDictionaryKeyCallBacks,
                                                   &kCFTypeDictionaryValueCallBacks);
            CFArrayAppendValue(items, d);
            CFRelease(d);
            CFRelease(name);
        }
        release(e);
        CFRelease(dir);
    }
    *workflowItems = items;
    return kPMNoError;
}

/* An app opens the PDF; a folder gets a copy named after the title; a
   tool runs with the title, the options and the file. */
static OSStatus submit(CFURLRef item, CFStringRef title, const char *options, CFURLRef pdf) {
    char itemPath[1024], pdfPath[1024];
    struct stat st;
    if (!CFURLGetFileSystemRepresentation(item, true, (UInt8 *)itemPath, sizeof(itemPath)) ||
        !CFURLGetFileSystemRepresentation(pdf, true, (UInt8 *)pdfPath, sizeof(pdfPath)) || stat(itemPath, &st) != 0)
        return kPMInvalidParameter;
    size_t n = strlen(itemPath);
    if (n > 4 && strcmp(itemPath + n - 4, ".app") == 0) {
        CFURLRef urls[] = { pdf };
        CFArrayRef list = CFArrayCreate(NULL, (const void **)urls, 1, &kCFTypeArrayCallBacks);
        LSLaunchURLSpec spec = { item, list, NULL, kLSLaunchDefaults, NULL };
        OSStatus err = LSOpenFromURLSpec(&spec, NULL);
        CFRelease(list);
        return err == noErr ? kPMNoError : kPMGeneralError;
    }
    char *t = cString(title);
    if (S_ISDIR(st.st_mode)) {
        char dest[2048];
        snprintf(dest, sizeof(dest), "%s/%s.pdf", itemPath, *t ? t : "Untitled");
        free(t);
        FILE *in = fopen(pdfPath, "rb"), *out = in != NULL ? fopen(dest, "wb") : NULL;
        char buf[65536];
        size_t r;
        while (in != NULL && out != NULL && (r = fread(buf, 1, sizeof(buf), in)) > 0)
            fwrite(buf, 1, r, out);
        if (in != NULL)
            fclose(in);
        if (out != NULL)
            fclose(out);
        return out != NULL ? kPMNoError : kPMGeneralError;
    }
    char *argv[] = { itemPath, t, (char *)(options != NULL ? options : ""), pdfPath, NULL };
    pid_t pid;
    int rc = posix_spawn(&pid, itemPath, NULL, NULL, argv, environ);
    free(t);
    return rc == 0 ? kPMNoError : kPMGeneralError;
}

OSStatus PMWorkflowSubmitPDFWithOptions(CFURLRef workflowItem, CFStringRef title, const char *options,
                                        CFURLRef pdfFile) {
    if (workflowItem == NULL || pdfFile == NULL)
        return kPMInvalidParameter;
    return submit(workflowItem, title != NULL ? title : CFSTR("Untitled"), options, pdfFile);
}

OSStatus PMWorkflowSubmitPDFWithSettings(CFURLRef workflowItem, PMPrintSettings settings, CFURLRef pdfFile) {
    if (workflowItem == NULL || pdfFile == NULL)
        return kPMInvalidParameter;
    CFStringRef title = CFSTR("Untitled");
    char *options = NULL;
    if (kindOf(settings) == kSettings) {
        PMPrintSettingsGetJobName(settings, &title);
        PMPrintSettingsToOptions(settings, &options);
    }
    OSStatus err = submit(workflowItem, title, options, pdfFile);
    free(options);
    return err;
}
