/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The generic USB printer class driver: the CFPlug-In CUPS's usb backend
 * loads for a printer interface (class 7) that names no vendor driver.
 * The backend hands it an IOUSBInterfaceInterface245 for the interface;
 * the driver opens it, finds its bulk pipes and moves the print data and
 * the printer's answers. The interface the backend calls through (the
 * classdriver structure below, which is also the object's data) is the
 * layout CUPS's backend declares.
 *
 * Requests of the USB printer class (its specification, 4.2):
 * GET_DEVICE_ID (0), GET_PORT_STATUS (1), SOFT_RESET (2).
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPlugInCOM.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <stdlib.h>
#include <string.h>

#define kUSBPrinterClassTypeID \
    CFUUIDGetConstantUUIDWithBytes(NULL, 0x06, 0x04, 0x7D, 0x16, 0x53, 0xA2, 0x11, 0xD6, 0x92, 0x06, 0x00, 0x30, \
                                   0x65, 0x52, 0x45, 0x92)
#define kUSBPrinterClassInterfaceID \
    CFUUIDGetConstantUUIDWithBytes(NULL, 0x03, 0x34, 0x6D, 0x74, 0x53, 0xA3, 0x11, 0xD6, 0x9E, 0xA1, 0x76, 0x30, \
                                   0x65, 0x52, 0x45, 0x92)
#define kGenericFactoryID \
    CFUUIDGetConstantUUIDWithBytes(NULL, 0xBF, 0xF6, 0x71, 0x18, 0x83, 0xE8, 0x47, 0xDE, 0xAA, 0x72, 0x2D, 0x9C, \
                                   0xA2, 0xB9, 0x30, 0x89)

enum {
    kPrintingProtocolNoOpen = 0,
    kPrintingProtocolUnidirectional = 1,
    kPrintingProtocolBidirectional = 2,
};

enum {
    kRequestGetDeviceID = 0,
    kRequestGetPortStatus = 1,
    kRequestSoftReset = 2,
};

/* How long a write may wait for the printer to take its data (a laser
   printer warming up refuses it for seconds), and a read for an answer
   (the backend asks again every 250 ms). */
enum {
    kWriteTimeoutMS = 60000,
    kReadTimeoutMS = 1000,
    kDeviceIDLength = 1024,
};

typedef IOUSBInterfaceInterface245 **printer_interface_t;

typedef struct {
    UInt8 requestType;
    UInt8 request;
    UInt16 value;
    UInt16 index;
    UInt16 length;
    void *buffer;
} iodevice_request_t;

typedef union {
    char b;
} centronics_status_t;

typedef struct classdriver_s classdriver_t;
struct classdriver_s {
    IUNKNOWN_C_GUTS;
    CFPlugInRef plugin;
    IUnknownVTbl **factory;
    void *vendorReference;
    UInt32 location;
    UInt8 interfaceNumber;
    UInt16 vendorID;
    UInt16 productID;
    printer_interface_t interface;
    UInt8 outpipe;
    UInt8 inpipe;

    kern_return_t (*DeviceRequest)(classdriver_t **printer, iodevice_request_t *iorequest, UInt16 timeout);
    kern_return_t (*GetString)(classdriver_t **printer, UInt8 whichString, UInt16 language, UInt16 timeout,
                               CFStringRef *result);
    kern_return_t (*SoftReset)(classdriver_t **printer, UInt16 timeout);
    kern_return_t (*GetCentronicsStatus)(classdriver_t **printer, centronics_status_t *result, UInt16 timeout);
    kern_return_t (*GetDeviceID)(classdriver_t **printer, CFStringRef *devid, UInt16 timeout);
    kern_return_t (*ReadPipe)(classdriver_t **printer, UInt8 *buffer, UInt32 *count);
    kern_return_t (*WritePipe)(classdriver_t **printer, UInt8 *buffer, UInt32 *count, Boolean eoj);
    kern_return_t (*Open)(classdriver_t **printer, UInt32 location, UInt8 protocol);
    kern_return_t (*Abort)(classdriver_t **printer);
    kern_return_t (*Close)(classdriver_t **printer);
    kern_return_t (*Initialize)(classdriver_t **printer, classdriver_t **baseclass);
    kern_return_t (*Terminate)(classdriver_t **printer);
};

/* The object: the classdriver_t the caller's pointer points to (functions
   and data in one), then the driver's own state. */
typedef struct {
    classdriver_t *driver;
    classdriver_t storage;
    UInt32 references;
    Boolean opened;
    CFUUIDRef factoryID;
} GenericPrinter;

#define PRINTER(p) ((GenericPrinter *)(p))

static IOReturn controlRequest(printer_interface_t interface, UInt8 type, UInt8 request, UInt16 value, UInt16 index,
                               void *data, UInt16 length, UInt32 timeout, UInt32 *done)
{
    IOUSBDevRequestTO r;

    memset(&r, 0, sizeof(r));
    r.bmRequestType = type;
    r.bRequest = request;
    r.wValue = value;
    r.wIndex = index;
    r.wLength = length;
    r.pData = data;
    r.noDataTimeout = timeout;
    r.completionTimeout = timeout;
    IOReturn ret = (*interface)->ControlRequestTO(interface, 0, &r);
    if (done != NULL)
        *done = r.wLenDone;
    return ret;
}

static kern_return_t DeviceRequest(classdriver_t **printer, iodevice_request_t *iorequest, UInt16 timeout)
{
    if ((*printer)->interface == NULL || iorequest == NULL)
        return kIOReturnNotOpen;
    return controlRequest((*printer)->interface, iorequest->requestType, iorequest->request, iorequest->value,
                          iorequest->index, iorequest->buffer, iorequest->length, timeout, NULL);
}

/* A string descriptor, its UTF-16LE characters made a CFString. */
static kern_return_t GetString(classdriver_t **printer, UInt8 whichString, UInt16 language, UInt16 timeout,
                               CFStringRef *result)
{
    UInt8 descriptor[256];
    UInt32 done = 0;

    *result = NULL;
    if ((*printer)->interface == NULL)
        return kIOReturnNotOpen;
    if (whichString == 0)
        return kIOReturnBadArgument;
    memset(descriptor, 0, sizeof(descriptor));
    IOReturn ret = controlRequest((*printer)->interface, USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice),
                                  kUSBRqGetDescriptor, (kUSBStringDesc << 8) | whichString,
                                  language ? language : 0x0409, descriptor, sizeof(descriptor), timeout, &done);
    if (ret != kIOReturnSuccess)
        return ret;
    if (done < 2 || descriptor[1] != kUSBStringDesc)
        return kIOReturnError;
    UInt32 length = descriptor[0] < done ? descriptor[0] : done;
    *result = CFStringCreateWithBytes(kCFAllocatorDefault, descriptor + 2, length > 2 ? (length - 2) & ~1u : 0,
                                      kCFStringEncodingUTF16LE, false);
    return *result != NULL ? kIOReturnSuccess : kIOReturnNoMemory;
}

static UInt16 interfaceIndex(classdriver_t **printer)
{
    return (*printer)->interfaceNumber;
}

static kern_return_t SoftReset(classdriver_t **printer, UInt16 timeout)
{
    if ((*printer)->interface == NULL)
        return kIOReturnNotOpen;
    return controlRequest((*printer)->interface, USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface),
                          kRequestSoftReset, 0, interfaceIndex(printer), NULL, 0, timeout, NULL);
}

static kern_return_t GetCentronicsStatus(classdriver_t **printer, centronics_status_t *result, UInt16 timeout)
{
    UInt8 status = 0;

    if ((*printer)->interface == NULL)
        return kIOReturnNotOpen;
    IOReturn ret = controlRequest((*printer)->interface, USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface),
                                  kRequestGetPortStatus, 0, interfaceIndex(printer), &status, 1, timeout, NULL);
    if (ret == kIOReturnSuccess && result != NULL)
        result->b = (char)status;
    return ret;
}

/* The IEEE 1284 device ID: two bytes of length (big-endian, counting
   themselves), then the string. wValue is the configuration's index
   (from 0), wIndex the interface number and its alternate setting. */
static kern_return_t GetDeviceID(classdriver_t **printer, CFStringRef *devid, UInt16 timeout)
{
    printer_interface_t interface = (*printer)->interface;
    UInt8 configuration = 1, alternate = 0;
    UInt32 done = 0;

    *devid = NULL;
    if (interface == NULL)
        return kIOReturnNotOpen;
    (*interface)->GetConfigurationValue(interface, &configuration);
    (*interface)->GetAlternateSetting(interface, &alternate);
    UInt8 *buffer = calloc(1, kDeviceIDLength);
    if (buffer == NULL)
        return kIOReturnNoMemory;
    IOReturn ret = controlRequest(interface, USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface),
                                  kRequestGetDeviceID, configuration > 0 ? configuration - 1 : 0,
                                  (UInt16)((interfaceIndex(printer) << 8) | alternate), buffer, kDeviceIDLength,
                                  timeout, &done);
    if (ret == kIOReturnSuccess) {
        UInt32 length = done >= 2 ? ((UInt32)buffer[0] << 8) | buffer[1] : 0;
        if (length > done)
            length = done;
        if (length > 2)
            *devid = CFStringCreateWithBytes(kCFAllocatorDefault, buffer + 2, length - 2, kCFStringEncodingUTF8,
                                             false);
        if (*devid == NULL && length > 2)
            *devid = CFStringCreateWithBytes(kCFAllocatorDefault, buffer + 2, length - 2,
                                             kCFStringEncodingISOLatin1, false);
        if (*devid == NULL)
            ret = kIOReturnError;
    }
    free(buffer);
    return ret;
}

/* An answer from the printer, if it has one within kReadTimeoutMS: a
   timeout comes back as kIOUSBTransactionTimeout, which the backend takes
   as "nothing yet". After a stall the kernel has already cleared the pipe
   at both ends. */
static kern_return_t ReadPipe(classdriver_t **printer, UInt8 *buffer, UInt32 *count)
{
    printer_interface_t interface = (*printer)->interface;

    if (interface == NULL || !PRINTER(printer)->opened) {
        *count = 0;
        return kIOReturnNotOpen;
    }
    if ((*printer)->inpipe == 0) {
        *count = 0;
        return kIOReturnSuccess;
    }
    IOReturn ret = (*interface)->ReadPipeTO(interface, (*printer)->inpipe, buffer, count, kReadTimeoutMS,
                                            kReadTimeoutMS);
    if (ret != kIOReturnSuccess)
        *count = 0;
    return ret;
}

/* The print data, all of it or nothing: *count is what the printer took
   (0 on a timeout, which the backend sends again). eoj, the job's end,
   needs nothing more here: a transfer ends with its last packet. */
static kern_return_t WritePipe(classdriver_t **printer, UInt8 *buffer, UInt32 *count, Boolean eoj)
{
    printer_interface_t interface = (*printer)->interface;

    if (interface == NULL || !PRINTER(printer)->opened || (*printer)->outpipe == 0) {
        *count = 0;
        return kIOReturnNotOpen;
    }
    if (*count == 0)
        return kIOReturnSuccess;
    IOReturn ret = (*interface)->WritePipeTO(interface, (*printer)->outpipe, buffer, *count, 0, kWriteTimeoutMS);
    if (ret != kIOReturnSuccess)
        *count = 0;
    return ret;
}

/* The interface opened and its bulk pipes found. A bidirectional open
   needs an interface of protocol 2 or 3 and a bulk IN pipe; the backend
   asks for a unidirectional one when it fails. */
static kern_return_t Open(classdriver_t **printer, UInt32 location, UInt8 protocol)
{
    printer_interface_t interface = (*printer)->interface;
    UInt8 interfaceProtocol = 0, endpoints = 0, number = 0;

    if (interface == NULL)
        return kIOReturnNotOpen;
    if (protocol == kPrintingProtocolNoOpen)
        return kIOReturnSuccess;
    if (!PRINTER(printer)->opened) {
        IOReturn ret = (*interface)->USBInterfaceOpen(interface);
        if (ret != kIOReturnSuccess)
            return ret;
        PRINTER(printer)->opened = true;
    }
    (*printer)->location = location;
    (*printer)->outpipe = 0;
    (*printer)->inpipe = 0;
    (*interface)->GetInterfaceNumber(interface, &number);
    (*printer)->interfaceNumber = number;
    (*interface)->GetDeviceVendor(interface, &(*printer)->vendorID);
    (*interface)->GetDeviceProduct(interface, &(*printer)->productID);
    (*interface)->GetInterfaceProtocol(interface, &interfaceProtocol);
    (*interface)->GetNumEndpoints(interface, &endpoints);
    for (UInt8 ref = 1; ref <= endpoints; ref++) {
        UInt8 direction = 0, pipeNumber = 0, type = 0, interval = 0;
        UInt16 maxPacket = 0;
        if ((*interface)->GetPipeProperties(interface, ref, &direction, &pipeNumber, &type, &maxPacket, &interval) !=
            kIOReturnSuccess)
            continue;
        if (type != kUSBBulk)
            continue;
        if (direction == kUSBOut && (*printer)->outpipe == 0)
            (*printer)->outpipe = ref;
        else if (direction == kUSBIn && (*printer)->inpipe == 0)
            (*printer)->inpipe = ref;
    }
    if ((*printer)->outpipe == 0)
        goto fail;
    if (protocol == kPrintingProtocolBidirectional) {
        if (interfaceProtocol < 2 || (*printer)->inpipe == 0)
            goto fail;
    } else
        (*printer)->inpipe = 0;
    return kIOReturnSuccess;

fail:
    (*interface)->USBInterfaceClose(interface);
    PRINTER(printer)->opened = false;
    (*printer)->outpipe = 0;
    (*printer)->inpipe = 0;
    return kIOReturnNotFound;
}

static kern_return_t Abort(classdriver_t **printer)
{
    printer_interface_t interface = (*printer)->interface;

    if (interface == NULL || !PRINTER(printer)->opened)
        return kIOReturnNotOpen;
    if ((*printer)->outpipe != 0)
        (*interface)->AbortPipe(interface, (*printer)->outpipe);
    if ((*printer)->inpipe != 0)
        (*interface)->AbortPipe(interface, (*printer)->inpipe);
    return kIOReturnSuccess;
}

static kern_return_t Close(classdriver_t **printer)
{
    printer_interface_t interface = (*printer)->interface;

    if (interface != NULL && PRINTER(printer)->opened)
        (*interface)->USBInterfaceClose(interface);
    PRINTER(printer)->opened = false;
    (*printer)->outpipe = 0;
    (*printer)->inpipe = 0;
    return kIOReturnSuccess;
}

static kern_return_t Initialize(classdriver_t **printer, classdriver_t **baseclass)
{
    return kIOReturnSuccess;
}

static kern_return_t Terminate(classdriver_t **printer)
{
    return Close(printer);
}

static ULONG AddRef(void *self)
{
    return ++PRINTER(self)->references;
}

/* The last reference closes the interface and lets the backend's
   IOUSBInterfaceInterface go (it handed it over in load_classdriver). */
static ULONG Release(void *self)
{
    GenericPrinter *printer = PRINTER(self);

    if (--printer->references > 0)
        return printer->references;
    Close((classdriver_t **)self);
    if (printer->storage.interface != NULL)
        (*printer->storage.interface)->Release(printer->storage.interface);
    CFPlugInRemoveInstanceForFactory(printer->factoryID);
    CFRelease(printer->factoryID);
    free(printer);
    return 0;
}

static HRESULT QueryInterface(void *self, REFIID iid, LPVOID *out)
{
    CFUUIDRef wanted = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, iid);
    Boolean ours = CFEqual(wanted, IUnknownUUID) || CFEqual(wanted, kUSBPrinterClassInterfaceID);

    CFRelease(wanted);
    if (!ours) {
        *out = NULL;
        return E_NOINTERFACE;
    }
    AddRef(self);
    *out = self;
    return S_OK;
}

void *USBGenericPrintingClassFactory(CFAllocatorRef allocator, CFUUIDRef typeID);

void *USBGenericPrintingClassFactory(CFAllocatorRef allocator, CFUUIDRef typeID)
{
    if (!CFEqual(typeID, kUSBPrinterClassTypeID))
        return NULL;
    GenericPrinter *printer = calloc(1, sizeof(GenericPrinter));
    if (printer == NULL)
        return NULL;
    classdriver_t *d = &printer->storage;
    d->QueryInterface = QueryInterface;
    d->AddRef = AddRef;
    d->Release = Release;
    d->DeviceRequest = DeviceRequest;
    d->GetString = GetString;
    d->SoftReset = SoftReset;
    d->GetCentronicsStatus = GetCentronicsStatus;
    d->GetDeviceID = GetDeviceID;
    d->ReadPipe = ReadPipe;
    d->WritePipe = WritePipe;
    d->Open = Open;
    d->Abort = Abort;
    d->Close = Close;
    d->Initialize = Initialize;
    d->Terminate = Terminate;
    printer->driver = d;
    printer->references = 1;
    printer->factoryID = CFRetain(kGenericFactoryID);
    CFPlugInAddInstanceForFactory(printer->factoryID);
    return printer;
}
