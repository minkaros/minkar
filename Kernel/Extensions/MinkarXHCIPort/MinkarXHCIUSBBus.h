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
 * endpoint number), and configureInterruptInEndpoint() a single
 * interrupt-in ring. So this bus supports at most one bulk IN + one bulk
 * OUT + one interrupt IN endpoint per device, not arbitrary multi-endpoint
 * topologies. USBDeviceAddress == the device's xHCI slot ID directly (no
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
    bool fBulkConfigured[64];
    bool fIntrConfigured[64];
};

#endif /* !_MINKAR_XHCI_USB_BUS_H */
