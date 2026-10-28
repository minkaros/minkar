/*
 * MinkarXHCIUSBBus: concrete IOUSBController backed directly by
 * MinkarXHCIPort's existing, already-working per-slot transfer primitives
 * (controlTransfer/bulkTransfer/configureBulkEndpoints/
 * configureInterruptInEndpoint). Lets IOUSBFamily's IOUSBDevice/
 * IOUSBInterface/IOUSBPipe - and anything that matches on them, like
 * IOUSBCompositeDriver - work against xHCI devices MinkarXHCIPort doesn't
 * already special-case as hub/mass-storage/keyboard.
 *
 * Scope limit inherited from MinkarXHCIPort: configureBulkEndpoints()
 * configures a single bulk-in/out ring pair per slot (not indexed by
 * endpoint number). Interrupt-IN endpoints have a ring each, so a
 * composite HID device (a keyboard with media keys, a KVM switch's
 * keyboard and mouse) gets all of them. So this bus supports one bulk IN +
 * one bulk OUT and any number of interrupt-IN endpoints per device. USBDeviceAddress == the device's xHCI slot ID directly (no
 * separate bus-address abstraction - we own both ends).
 */

#ifndef _MINKAR_XHCI_USB_BUS_H
#define _MINKAR_XHCI_USB_BUS_H

#include <IOKit/usb/IOUSBController.h>

class MinkarXHCIPort;

class MinkarXHCIUSBBus : public IOUSBController
{
    OSDeclareDefaultStructors(MinkarXHCIUSBBus)

public:
    bool initWithPort(MinkarXHCIPort *port);

    virtual IOReturn UIMOpenPipe(USBDeviceAddress address, UInt8 speed, Endpoint *endpoint) APPLE_KEXT_OVERRIDE;
    virtual IOReturn UIMClosePipe(USBDeviceAddress address, Endpoint *endpoint) APPLE_KEXT_OVERRIDE;
    virtual IOReturn UIMAbortPipe(USBDeviceAddress address, Endpoint *endpoint) APPLE_KEXT_OVERRIDE;
    virtual IOReturn UIMClearPipeStall(USBDeviceAddress address, Endpoint *endpoint) APPLE_KEXT_OVERRIDE;

    virtual IOReturn UIMDeviceRequest(IOUSBDevRequest *request, USBDeviceAddress address) APPLE_KEXT_OVERRIDE;

    virtual IOReturn UIMReadWrite(IOMemoryDescriptor *buffer, USBDeviceAddress address,
                                   Endpoint *endpoint, bool isWrite) APPLE_KEXT_OVERRIDE;

private:
    MinkarXHCIPort *fPort;
};

#endif /* !_MINKAR_XHCI_USB_BUS_H */
