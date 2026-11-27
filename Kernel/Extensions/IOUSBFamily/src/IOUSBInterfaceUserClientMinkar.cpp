/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * IOUSBInterfaceUserClient: an app's way to a USB interface, through
 * IOUSBLib's IOUSBInterfaceInterface (the plug-in IOCFPlugInTypes names).
 * The calls and their arguments are the ones IOUSBLib makes
 * (IOUSBUserClient.h): opening and closing the interface, its pipes'
 * properties and status, control requests, bulk and interrupt reads and
 * writes, aborting and clearing a stall, the configuration descriptor.
 * They are synchronous; asynchronous transfers, isochronous pipes, frame
 * numbers, bandwidth and alternate settings other than the current one
 * answer kIOReturnUnsupported. CUPS's USB backend (printers) uses this.
 */

#include <IOKit/IOUserClient.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBUserClient.h>
#include <libkern/OSByteOrder.h>

#define super IOUserClient

/* IOUSBFamily's errors (iokit_usb_err), where this tree's USB.h lacks them. */
#ifndef kIOUSBUnknownPipeErr
#define kIOUSBUnknownPipeErr ((IOReturn)0xe0004061)
#endif
#ifndef kIOUSBEndpointNotFound
#define kIOUSBEndpointNotFound ((IOReturn)0xe0004057)
#endif
#ifndef kIOUSBConfigNotFound
#define kIOUSBConfigNotFound ((IOReturn)0xe0004056)
#endif
#define USBToHostWord(x) OSSwapLittleToHostInt16(x)

/* IOUSBLib's endpoint properties (USB.h on the user side), version 3. */
struct MinkarUSBEndpointProperties {
    UInt8  bVersion;
    UInt8  bAlternateSetting;
    UInt8  bDirection;
    UInt8  bEndpointNumber;
    UInt8  bTransferType;
    UInt8  bUsageType;
    UInt8  bSyncType;
    UInt8  bInterval;
    UInt16 wMaxPacketSize;
    UInt8  bMaxBurst;
    UInt8  bMaxStreams;
    UInt8  bMult;
    UInt16 wBytesPerInterval;
} __attribute__((packed));

class IOUSBInterfaceUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOUSBInterfaceUserClient)

    IOUSBInterface *fInterface;
    task_t          fTask;
    bool            fOpen;

    IOUSBPipe *pipe(uint64_t pipeRef);

public:
    static IOReturn sOpen(OSObject *, void *, IOExternalMethodArguments *);
    static IOReturn sClose(OSObject *, void *, IOExternalMethodArguments *);
    static IOReturn sGetDevice(OSObject *, void *, IOExternalMethodArguments *);
    static IOReturn sSetAlternateInterface(OSObject *, void *, IOExternalMethodArguments *);
    static IOReturn sUnsupported(OSObject *, void *, IOExternalMethodArguments *);
    static IOReturn sGetPipeStatus(OSObject *, void *, IOExternalMethodArguments *);
    static IOReturn sAbortPipe(OSObject *, void *, IOExternalMethodArguments *);
    static IOReturn sResetPipe(OSObject *, void *, IOExternalMethodArguments *);
    static IOReturn sClearPipeStall(OSObject *, void *, IOExternalMethodArguments *);
    static IOReturn sControlRequest(OSObject *, void *, IOExternalMethodArguments *);
    static IOReturn sReadPipe(OSObject *, void *, IOExternalMethodArguments *);
    static IOReturn sWritePipe(OSObject *, void *, IOExternalMethodArguments *);
    static IOReturn sGetPipeProperties(OSObject *, void *, IOExternalMethodArguments *);
    static IOReturn sGetPipePropertiesV3(OSObject *, void *, IOExternalMethodArguments *);
    static IOReturn sGetEndpointPropertiesV3(OSObject *, void *, IOExternalMethodArguments *);
    static IOReturn sGetConfigDescriptor(OSObject *, void *, IOExternalMethodArguments *);
    static IOReturn sSetAsyncPort(OSObject *, void *, IOExternalMethodArguments *);

    virtual bool initWithTask(task_t owningTask, void *securityID, UInt32 type) APPLE_KEXT_OVERRIDE;
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual IOReturn clientClose(void) APPLE_KEXT_OVERRIDE;
    virtual IOReturn clientDied(void) APPLE_KEXT_OVERRIDE;
    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *arguments,
                                    IOExternalMethodDispatch *dispatch, OSObject *target,
                                    void *reference) APPLE_KEXT_OVERRIDE;
};

OSDefineMetaClassAndStructors(IOUSBInterfaceUserClient, IOUserClient)

bool IOUSBInterfaceUserClient::initWithTask(task_t owningTask, void *securityID, UInt32 type)
{
    if (!super::initWithTask(owningTask, securityID, type))
        return false;
    fTask = owningTask;
    fOpen = false;
    fInterface = NULL;
    return true;
}

bool IOUSBInterfaceUserClient::start(IOService *provider)
{
    fInterface = OSDynamicCast(IOUSBInterface, provider);
    if (fInterface == NULL || !super::start(provider))
        return false;
    return true;
}

IOReturn IOUSBInterfaceUserClient::clientClose(void)
{
    if (fOpen && fInterface != NULL) {
        fInterface->close(this);
        fOpen = false;
    }
    terminate();
    return kIOReturnSuccess;
}

IOReturn IOUSBInterfaceUserClient::clientDied(void)
{
    return clientClose();
}

/* Pipes 1 to N: the interface's, in the order the interface keeps them
   (its endpoints by number, OUT then IN). Pipe 0, the device's control
   pipe, is reached through the device's requests (controlRequest). */
IOUSBPipe *IOUSBInterfaceUserClient::pipe(uint64_t pipeRef)
{
    IOUSBFindEndpointRequest any = { kUSBAnyType, kUSBAnyDirn, 0, 0 };
    IOUSBPipe *p = NULL;

    if (fInterface == NULL || pipeRef == 0 || pipeRef > kUSBMaxPipes)
        return NULL;
    for (uint64_t i = 0; i < pipeRef; i++) {
        p = fInterface->FindNextPipe(p, &any);
        if (p == NULL)
            return NULL;
    }
    return p;
}

#define THIS ((IOUSBInterfaceUserClient *)target)

IOReturn IOUSBInterfaceUserClient::sOpen(OSObject *target, void *, IOExternalMethodArguments *args)
{
    if (THIS->fOpen)
        return kIOReturnExclusiveAccess;
    if (!THIS->fInterface->open(THIS))
        return kIOReturnExclusiveAccess;
    THIS->fOpen = true;
    return kIOReturnSuccess;
}

IOReturn IOUSBInterfaceUserClient::sClose(OSObject *target, void *, IOExternalMethodArguments *args)
{
    if (!THIS->fOpen)
        return kIOReturnNotOpen;
    THIS->fInterface->close(THIS);
    THIS->fOpen = false;
    return kIOReturnSuccess;
}

/* The device, as an io_object_t in the caller's task (an object port,
   whose properties IOUSBLib reads). exportObjectToClient consumes a
   reference. */
IOReturn IOUSBInterfaceUserClient::sGetDevice(OSObject *target, void *, IOExternalMethodArguments *args)
{
    IOUSBDevice *device = THIS->fInterface->GetDevice();
    io_object_t name = 0;

    if (device == NULL)
        return kIOReturnNoDevice;
    device->retain();
    IOReturn ret = THIS->exportObjectToClient(THIS->fTask, device, &name);
    if (ret != kIOReturnSuccess)
        return ret;
    args->scalarOutput[0] = (uint64_t)name;
    return kIOReturnSuccess;
}

IOReturn IOUSBInterfaceUserClient::sSetAlternateInterface(OSObject *target, void *, IOExternalMethodArguments *args)
{
    if (!THIS->fOpen)
        return kIOReturnNotOpen;
    return args->scalarInput[0] == THIS->fInterface->GetAlternateSetting() ? kIOReturnSuccess : kIOReturnUnsupported;
}

IOReturn IOUSBInterfaceUserClient::sUnsupported(OSObject *, void *, IOExternalMethodArguments *)
{
    return kIOReturnUnsupported;
}

IOReturn IOUSBInterfaceUserClient::sGetPipeStatus(OSObject *target, void *, IOExternalMethodArguments *args)
{
    IOUSBPipe *p = THIS->pipe(args->scalarInput[0]);
    if (args->scalarInput[0] == 0)
        return kIOReturnSuccess;
    return p != NULL ? p->GetPipeStatus() : kIOUSBUnknownPipeErr;
}

IOReturn IOUSBInterfaceUserClient::sAbortPipe(OSObject *target, void *, IOExternalMethodArguments *args)
{
    IOUSBPipe *p = THIS->pipe(args->scalarInput[0]);
    if (!THIS->fOpen)
        return kIOReturnNotOpen;
    return p != NULL ? p->Abort() : kIOUSBUnknownPipeErr;
}

IOReturn IOUSBInterfaceUserClient::sResetPipe(OSObject *target, void *, IOExternalMethodArguments *args)
{
    IOUSBPipe *p = THIS->pipe(args->scalarInput[0]);
    if (!THIS->fOpen)
        return kIOReturnNotOpen;
    return p != NULL ? p->Reset() : kIOUSBUnknownPipeErr;
}

IOReturn IOUSBInterfaceUserClient::sClearPipeStall(OSObject *target, void *, IOExternalMethodArguments *args)
{
    IOUSBPipe *p = THIS->pipe(args->scalarInput[0]);
    if (!THIS->fOpen)
        return kIOReturnNotOpen;
    return p != NULL ? p->ClearPipeStall(args->scalarInput[1] != 0) : kIOUSBUnknownPipeErr;
}

/* A memory descriptor for the call's data: the one IOKit made for a large
   buffer, else a kernel buffer the inline bytes are copied to or from.
   *made is set when the caller must release it. */
static IOMemoryDescriptor *dataDescriptor(IOExternalMethodArguments *args, IODirection direction, uint32_t length,
                                          bool *made)
{
    IOMemoryDescriptor *given = direction == kIODirectionOut ? args->structureInputDescriptor
                                                             : args->structureOutputDescriptor;
    *made = false;
    if (given != NULL) {
        given->retain();
        *made = true;
        return given;
    }
    IOBufferMemoryDescriptor *buffer = IOBufferMemoryDescriptor::withCapacity(length > 0 ? length : 1, direction);
    if (buffer == NULL)
        return NULL;
    buffer->setLength(length);
    /* Into the buffer's own memory: writeBytes refuses a descriptor made
       for output only. */
    if (direction == kIODirectionOut && length > 0 && args->structureInput != NULL)
        bcopy(args->structureInput, buffer->getBytesNoCopy(), length);
    *made = true;
    return buffer;
}

/* The bytes read, into the caller's inline buffer when it gave one. */
static void returnData(IOExternalMethodArguments *args, IOMemoryDescriptor *data, IOByteCount count)
{
    if (args->structureOutputDescriptor == NULL) {
        if (count > args->structureOutputSize)
            count = args->structureOutputSize;
        if (count > 0 && args->structureOutput != NULL)
            data->readBytes(0, args->structureOutput, count);
        args->structureOutputSize = (uint32_t)count;
    } else
        args->structureOutputDescriptorSize = (uint32_t)count;
}

/* in: pipeRef, bmRequestType, bRequest, wValue, wIndex, wLength, pData
   (the caller's own pointer, not used here), noDataTimeout,
   completionTimeout; data in the structure, out or in by direction. */
IOReturn IOUSBInterfaceUserClient::sControlRequest(OSObject *target, void *reference, IOExternalMethodArguments *args)
{
    bool in = (reference != NULL);
    IOUSBPipe *p = args->scalarInput[0] != 0 ? THIS->pipe(args->scalarInput[0]) : NULL;
    IOUSBDevice *device = THIS->fInterface->GetDevice();
    uint32_t length = (uint32_t)args->scalarInput[5];
    bool made = false;

    if (args->scalarInput[0] == 0 ? device == NULL : p == NULL)
        return kIOUSBUnknownPipeErr;
    if (args->scalarInput[0] != 0 && !THIS->fOpen)
        return kIOReturnNotOpen;
    IOMemoryDescriptor *data = length > 0 ? dataDescriptor(args, in ? kIODirectionIn : kIODirectionOut, length, &made) : NULL;
    if (length > 0 && data == NULL)
        return kIOReturnNoMemory;
    if (data != NULL && data->prepare() != kIOReturnSuccess) {
        if (made)
            data->release();
        return kIOReturnVMError;
    }

    IOUSBDevRequestDesc request;
    request.bmRequestType = (UInt8)args->scalarInput[1];
    request.bRequest = (UInt8)args->scalarInput[2];
    request.wValue = (UInt16)args->scalarInput[3];
    request.wIndex = (UInt16)args->scalarInput[4];
    request.wLength = (UInt16)length;
    request.pData = data;
    request.wLenDone = 0;
    IOReturn ret = p != NULL ? p->ControlRequest(&request, (UInt32)args->scalarInput[7], (UInt32)args->scalarInput[8], NULL)
                             : device->DeviceRequest(&request, (UInt32)args->scalarInput[7], (UInt32)args->scalarInput[8],
                                                     (IOUSBCompletion *)NULL);

    /* Fewer bytes than asked is how a control read ends early, not an
       error (a descriptor shorter than the buffer). */
    if (ret == kIOReturnUnderrun)
        ret = kIOReturnSuccess;
    if (in && data != NULL && ret == kIOReturnSuccess)
        returnData(args, data, request.wLenDone);
    /* The one output: whether the device sent more than was asked for
       (IOUSBLib then answers kIOReturnOverrun). */
    if (args->scalarOutputCount > 0)
        args->scalarOutput[0] = 0;
    if (data != NULL) {
        data->complete();
        if (made)
            data->release();
    }
    return ret;
}

/* in: pipeRef, noDataTimeout, completionTimeout, buffer, size (the last
   two for asynchronous calls only); the data in the structure. */
IOReturn IOUSBInterfaceUserClient::sReadPipe(OSObject *target, void *, IOExternalMethodArguments *args)
{
    IOUSBPipe *p = THIS->pipe(args->scalarInput[0]);
    uint32_t length = args->structureOutputDescriptor != NULL ? args->structureOutputDescriptorSize
                                                              : args->structureOutputSize;
    bool made = false;

    if (args->asyncWakePort != MACH_PORT_NULL)
        return kIOReturnUnsupported;
    if (!THIS->fOpen)
        return kIOReturnNotOpen;
    if (p == NULL || args->scalarInput[0] == 0)
        return kIOUSBUnknownPipeErr;
    IOMemoryDescriptor *data = dataDescriptor(args, kIODirectionIn, length, &made);
    if (data == NULL)
        return kIOReturnNoMemory;
    if (data->prepare() != kIOReturnSuccess) {
        data->release();
        return kIOReturnVMError;
    }
    IOByteCount count = 0;
    IOReturn ret = p->Read(data, (UInt32)args->scalarInput[1], (UInt32)args->scalarInput[2], length, (IOUSBCompletion *)NULL,
                           &count);
    if (ret == kIOReturnSuccess || ret == kIOReturnOverrun)
        returnData(args, data, count);
    data->complete();
    data->release();
    return ret;
}

IOReturn IOUSBInterfaceUserClient::sWritePipe(OSObject *target, void *, IOExternalMethodArguments *args)
{
    IOUSBPipe *p = THIS->pipe(args->scalarInput[0]);
    uint32_t length = args->structureInputDescriptor != NULL ? (uint32_t)args->structureInputDescriptor->getLength()
                                                             : args->structureInputSize;
    bool made = false;

    if (args->asyncWakePort != MACH_PORT_NULL)
        return kIOReturnUnsupported;
    if (!THIS->fOpen)
        return kIOReturnNotOpen;
    if (p == NULL || args->scalarInput[0] == 0)
        return kIOUSBUnknownPipeErr;
    IOMemoryDescriptor *data = dataDescriptor(args, kIODirectionOut, length, &made);
    if (data == NULL)
        return kIOReturnNoMemory;
    if (data->prepare() != kIOReturnSuccess) {
        data->release();
        return kIOReturnVMError;
    }
    IOReturn ret = p->Write(data, (UInt32)args->scalarInput[1], (UInt32)args->scalarInput[2], length,
                            (IOUSBCompletion *)NULL);
    data->complete();
    data->release();
    return ret;
}

static void fillProperties(IOUSBPipe *p, UInt8 alternateSetting, MinkarUSBEndpointProperties *props)
{
    const IOUSBEndpointDescriptor *ed = p->GetEndpointDescriptor();

    bzero(props, sizeof(*props));
    props->bVersion = 3;
    props->bAlternateSetting = alternateSetting;
    props->bDirection = p->GetDirection();
    props->bEndpointNumber = p->GetEndpointNumber();
    props->bTransferType = p->GetType();
    props->bInterval = p->GetInterval();
    props->wMaxPacketSize = p->GetMaxPacketSize();
    if (ed != NULL)
        props->bMult = (UInt8)((USBToHostWord(ed->wMaxPacketSize) >> 11) & 3);
    props->wBytesPerInterval = props->wMaxPacketSize;
}

/* in: pipeRef; out: direction, number, transfer type, max packet size,
   interval. */
IOReturn IOUSBInterfaceUserClient::sGetPipeProperties(OSObject *target, void *, IOExternalMethodArguments *args)
{
    IOUSBPipe *p = THIS->pipe(args->scalarInput[0]);

    if (p == NULL)
        return kIOUSBUnknownPipeErr;
    args->scalarOutput[0] = p->GetDirection();
    args->scalarOutput[1] = p->GetEndpointNumber();
    args->scalarOutput[2] = p->GetType();
    args->scalarOutput[3] = p->GetMaxPacketSize();
    args->scalarOutput[4] = p->GetInterval();
    return kIOReturnSuccess;
}

/* in: pipeRef, version (3); out: the properties. */
IOReturn IOUSBInterfaceUserClient::sGetPipePropertiesV3(OSObject *target, void *, IOExternalMethodArguments *args)
{
    IOUSBPipe *p = THIS->pipe(args->scalarInput[0]);
    MinkarUSBEndpointProperties props;

    if (p == NULL)
        return kIOUSBUnknownPipeErr;
    if (args->scalarInput[1] != 3 || args->structureOutputSize < sizeof(props))
        return kIOReturnBadArgument;
    fillProperties(p, THIS->fInterface->GetAlternateSetting(), &props);
    bcopy(&props, args->structureOutput, sizeof(props));
    args->structureOutputSize = sizeof(props);
    return kIOReturnSuccess;
}

/* in and out: the properties, its alternate setting, direction and
   endpoint number naming the endpoint (the current setting's only). */
IOReturn IOUSBInterfaceUserClient::sGetEndpointPropertiesV3(OSObject *target, void *, IOExternalMethodArguments *args)
{
    MinkarUSBEndpointProperties props;

    if (args->structureInputSize < sizeof(props) || args->structureOutputSize < sizeof(props))
        return kIOReturnBadArgument;
    bcopy(args->structureInput, &props, sizeof(props));
    if (props.bAlternateSetting != THIS->fInterface->GetAlternateSetting())
        return kIOReturnUnsupported;
    for (uint64_t ref = 1; ref <= kUSBMaxPipes; ref++) {
        IOUSBPipe *p = THIS->pipe(ref);
        if (p == NULL)
            break;
        if (p->GetEndpointNumber() == props.bEndpointNumber && p->GetDirection() == props.bDirection) {
            fillProperties(p, props.bAlternateSetting, &props);
            bcopy(&props, args->structureOutput, sizeof(props));
            args->structureOutputSize = sizeof(props);
            return kIOReturnSuccess;
        }
    }
    return kIOUSBEndpointNotFound;
}

/* in: the configuration's index; out: its whole descriptor, or as much
   as fits. */
IOReturn IOUSBInterfaceUserClient::sGetConfigDescriptor(OSObject *target, void *, IOExternalMethodArguments *args)
{
    IOUSBDevice *device = THIS->fInterface->GetDevice();
    const IOUSBConfigurationDescriptor *config =
        device != NULL ? device->GetFullConfigurationDescriptor((UInt8)args->scalarInput[0]) : NULL;

    if (config == NULL)
        return kIOUSBConfigNotFound;
    uint32_t total = USBToHostWord(config->wTotalLength);
    if (args->structureOutputDescriptor != NULL) {
        uint32_t n = total < args->structureOutputDescriptorSize ? total : args->structureOutputDescriptorSize;
        args->structureOutputDescriptor->writeBytes(0, config, n);
        args->structureOutputDescriptorSize = n;
    } else {
        uint32_t n = total < args->structureOutputSize ? total : args->structureOutputSize;
        bcopy(config, args->structureOutput, n);
        args->structureOutputSize = n;
    }
    return kIOReturnSuccess;
}

/* Kept for IOUSBLib, which sets it as it opens the interface; nothing is
   sent there (no asynchronous transfers yet). */
IOReturn IOUSBInterfaceUserClient::sSetAsyncPort(OSObject *, void *, IOExternalMethodArguments *)
{
    return kIOReturnSuccess;
}

#define VAR kIOUCVariableStructureSize
static const IOExternalMethodDispatch sMethods[kIOUSBLibInterfaceUserClientNumCommands] = {
    /* Open */                    { IOUSBInterfaceUserClient::sOpen, 1, 0, 0, 0 },
    /* Close */                   { IOUSBInterfaceUserClient::sClose, 0, 0, 0, 0 },
    /* GetDevice */               { IOUSBInterfaceUserClient::sGetDevice, 0, 0, 1, 0 },
    /* SetAlternateInterface */   { IOUSBInterfaceUserClient::sSetAlternateInterface, 1, 0, 0, 0 },
    /* GetFrameNumber */          { IOUSBInterfaceUserClient::sUnsupported, 0, 0, 0, VAR },
    /* GetPipeProperties */       { IOUSBInterfaceUserClient::sGetPipeProperties, 1, 0, 5, 0 },
    /* ReadPipe */                { IOUSBInterfaceUserClient::sReadPipe, 5, 0, 0, VAR },
    /* WritePipe */               { IOUSBInterfaceUserClient::sWritePipe, 5, VAR, 0, 0 },
    /* GetPipeStatus */           { IOUSBInterfaceUserClient::sGetPipeStatus, 1, 0, 0, 0 },
    /* AbortPipe */               { IOUSBInterfaceUserClient::sAbortPipe, 1, 0, 0, 0 },
    /* ResetPipe */               { IOUSBInterfaceUserClient::sResetPipe, 1, 0, 0, 0 },
    /* ClearPipeStall */          { IOUSBInterfaceUserClient::sClearPipeStall, 2, 0, 0, 0 },
    /* ControlRequestOut */       { IOUSBInterfaceUserClient::sControlRequest, 9, VAR, 0, 0 },
    /* ControlRequestIn */        { IOUSBInterfaceUserClient::sControlRequest, 9, 0, 1, VAR },
    /* SetPipePolicy */           { IOUSBInterfaceUserClient::sUnsupported, 3, 0, 0, 0 },
    /* GetBandwidthAvailable */   { IOUSBInterfaceUserClient::sUnsupported, 0, 0, 1, 0 },
    /* GetEndpointProperties */   { IOUSBInterfaceUserClient::sUnsupported, 3, 0, 3, 0 },
    /* LowLatencyPrepareBuffer */ { IOUSBInterfaceUserClient::sUnsupported, 7, 0, 1, 0 },
    /* LowLatencyReleaseBuffer */ { IOUSBInterfaceUserClient::sUnsupported, 7, 0, 0, 0 },
    /* GetMicroFrameNumber */     { IOUSBInterfaceUserClient::sUnsupported, 0, 0, 0, VAR },
    /* GetFrameListTime */        { IOUSBInterfaceUserClient::sUnsupported, 0, 0, 1, 0 },
    /* GetFrameNumberWithTime */  { IOUSBInterfaceUserClient::sUnsupported, 0, 0, 0, VAR },
    /* SetAsyncPort */            { IOUSBInterfaceUserClient::sSetAsyncPort, 0, 0, 0, 0 },
    /* ReadIsochPipe */           { IOUSBInterfaceUserClient::sUnsupported, 6, 0, 0, 0 },
    /* WriteIsochPipe */          { IOUSBInterfaceUserClient::sUnsupported, 6, 0, 0, 0 },
    /* LowLatencyReadIsochPipe */ { IOUSBInterfaceUserClient::sUnsupported, 9, 0, 0, 0 },
    /* LowLatencyWriteIsochPipe */{ IOUSBInterfaceUserClient::sUnsupported, 9, 0, 0, 0 },
    /* GetConfigDescriptor */     { IOUSBInterfaceUserClient::sGetConfigDescriptor, 1, 0, 0, VAR },
    /* GetPipePropertiesV2 */     { IOUSBInterfaceUserClient::sUnsupported, 1, 0, 0, VAR },
    /* GetPipePropertiesV3 */     { IOUSBInterfaceUserClient::sGetPipePropertiesV3, 2, 0, 0, VAR },
    /* GetEndpointPropertiesV3 */ { IOUSBInterfaceUserClient::sGetEndpointPropertiesV3, 0, VAR, 0, VAR },
};

IOReturn IOUSBInterfaceUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *arguments,
                                                  IOExternalMethodDispatch *dispatch, OSObject *target,
                                                  void *reference)
{
    if (selector >= kIOUSBLibInterfaceUserClientNumCommands)
        return kIOReturnUnsupported;
    dispatch = (IOExternalMethodDispatch *)&sMethods[selector];
    /* The two control requests share a function: in or out. */
    reference = (selector == kUSBInterfaceUserClientControlRequestIn) ? (void *)1 : NULL;
    return super::externalMethod(selector, arguments, dispatch, this, reference);
}
