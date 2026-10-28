#include "MinkarXHCIUSBBus.h"
#include "MinkarXHCIPort.h"
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLib.h>

#define super IOUSBController

OSDefineMetaClassAndStructors(MinkarXHCIUSBBus, IOUSBController)

bool MinkarXHCIUSBBus::initWithPort(MinkarXHCIPort *port)
{
    if (!port || !init(NULL)) return false;
    fPort = port;
    return true;
}

IOReturn MinkarXHCIUSBBus::UIMOpenPipe(USBDeviceAddress address, UInt8 speed, Endpoint *endpoint)
{
    UInt32 slotId = address;
    if (slotId >= 64 || !endpoint || !fPort) return kIOReturnBadArgument;

    switch (endpoint->transferType) {
        case kUSBControl:
            /* Endpoint 0 is already set up by addressDevice() at slot
             * address time - nothing to do. */
            return kIOReturnSuccess;

        case kUSBBulk:
            /* One bulk ring per direction per slot (see the header); each
             * direction is configured when its pipe is opened, and asked
             * of the port, which forgets them when the device goes. */
            if (endpoint->number == 0 || endpoint->number > 15)
                return kIOReturnBadArgument;
            if (fPort->bulkEndpointConfigured(slotId, endpoint->direction == kUSBIn))
                return kIOReturnSuccess;
            if (endpoint->direction == kUSBIn ?
                !fPort->configureBulkEndpoints(slotId, endpoint->number, endpoint->maxPacketSize, 0, 0) :
                !fPort->configureBulkEndpoints(slotId, 0, 0, endpoint->number, endpoint->maxPacketSize))
                return kIOReturnError;
            return kIOReturnSuccess;

        case kUSBInterrupt:
            if (endpoint->direction != kUSBIn)
                return kIOReturnUnsupported; /* no interrupt-OUT support in the UIM */
            if (endpoint->number == 0 || endpoint->number > 15)
                return kIOReturnBadArgument;
            /* Asked of the port, endpoint by endpoint: a device with several
             * HID interfaces has an interrupt-IN endpoint for each, and the
             * port forgets them when the device is unplugged. */
            if (fPort->interruptInEndpointConfigured(slotId, endpoint->number))
                return kIOReturnSuccess;
            if (!fPort->configureInterruptInEndpoint(slotId, endpoint->number,
                                                       endpoint->maxPacketSize, endpoint->interval))
                return kIOReturnError;
            return kIOReturnSuccess;

        default:
            return kIOReturnUnsupported; /* isoc: no UIM support */
    }
}

IOReturn MinkarXHCIUSBBus::UIMClosePipe(USBDeviceAddress address, Endpoint *endpoint)
{
    /* No per-endpoint teardown primitive in MinkarXHCIPort short of tearing
     * down the whole slot - treat as a no-op rather than disturb sibling
     * endpoints sharing the slot's single bulk/interrupt ring pair. */
    return kIOReturnSuccess;
}

IOReturn MinkarXHCIUSBBus::UIMAbortPipe(USBDeviceAddress address, Endpoint *endpoint)
{
    return kIOReturnSuccess;
}

IOReturn MinkarXHCIUSBBus::UIMClearPipeStall(USBDeviceAddress address, Endpoint *endpoint)
{
    return kIOReturnSuccess;
}

IOReturn MinkarXHCIUSBBus::UIMDeviceRequest(IOUSBDevRequest *request, USBDeviceAddress address)
{
    UInt32 slotId = address;
    if (slotId >= 64 || !request || !fPort) return kIOReturnBadArgument;

    USBSetupPacket setup;
    setup.bmRequestType = request->bmRequestType;
    setup.bRequest      = request->bRequest;
    setup.wValue        = request->wValue;
    setup.wIndex        = request->wIndex;
    setup.wLength        = request->wLength;

    bool in = (request->bmRequestType & 0x80) != 0;
    bool ok = fPort->controlTransfer(slotId, setup, request->pData, request->wLength, in);
    if (ok) request->wLenDone = request->wLength;
    return ok ? kIOReturnSuccess : kIOReturnError;
}

IOReturn MinkarXHCIUSBBus::UIMReadWrite(IOMemoryDescriptor *buffer, USBDeviceAddress address,
                                       Endpoint *endpoint, bool isWrite)
{
    UInt32 slotId = address;
    if (slotId >= 64 || !endpoint || !buffer || !fPort) return kIOReturnBadArgument;
    if (endpoint->transferType != kUSBBulk && endpoint->transferType != kUSBInterrupt)
        return kIOReturnUnsupported;

    UInt32 len = (UInt32)buffer->getLength();
    if (!len) return kIOReturnSuccess;

    if (endpoint->transferType == kUSBInterrupt) {
        if (isWrite)
            return kIOReturnUnsupported;
        return fPort->interruptTransfer(slotId, endpoint->number, buffer, len, 20);
    }

    IOBufferMemoryDescriptor *bounce =
        IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIODirectionInOut, len);
    if (!bounce) return kIOReturnNoMemory;

    if (isWrite)
        buffer->readBytes(0, bounce->getBytesNoCopy(), len);

    bool ok = fPort->bulkTransfer(slotId, endpoint->number, !isWrite, bounce, len, 5000);

    if (ok && !isWrite)
        buffer->writeBytes(0, bounce->getBytesNoCopy(), len);

    bounce->release();
    return ok ? kIOReturnSuccess : kIOReturnError;
}
