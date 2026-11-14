/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef _MINKAR_HDA_DRIVER_H_
#define _MINKAR_HDA_DRIVER_H_

#include <IOKit/IOService.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>
#include "HDA.h"

#define HDA_MAX_CODECS  15

class MinkarHDA : public IOService {
    OSDeclareDefaultStructors(MinkarHDA);

public:
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    virtual void free() override;

    /* A verb to a codec and its answer; false on a timeout. */
    bool sendCommand(uint32_t command, uint32_t *response);
    bool getParameter(uint8_t cad, uint8_t nid, uint8_t param, uint32_t *value);

private:
    bool mapRegisters();
    bool resetController();
    bool startRings();
    void stopRings();
    void probeCodecs();
    void describeCodec(uint8_t cad);

    uint8_t  read8(uint32_t reg)  { return *(volatile uint8_t *)(fRegs + reg); }
    uint16_t read16(uint32_t reg) { return *(volatile uint16_t *)(fRegs + reg); }
    uint32_t read32(uint32_t reg) { return *(volatile uint32_t *)(fRegs + reg); }
    void write8(uint32_t reg, uint8_t v)   { *(volatile uint8_t *)(fRegs + reg) = v; }
    void write16(uint32_t reg, uint16_t v) { *(volatile uint16_t *)(fRegs + reg) = v; }
    void write32(uint32_t reg, uint32_t v) { *(volatile uint32_t *)(fRegs + reg) = v; }

    IOPCIDevice              *fPCI;
    IOMemoryMap              *fMap;
    IOMemoryDescriptor       *fDesc;
    volatile uint8_t         *fRegs;
    uint16_t                  fCaps;
    uint16_t                  fCodecMask;       /* STATESTS after the reset */

    IOBufferMemoryDescriptor *fRingMem;         /* CORB, then RIRB */
    volatile uint32_t        *fCORB;
    volatile HDAResponse     *fRIRB;
    uint16_t                  fCORBEntries, fRIRBEntries;
    uint16_t                  fRIRBRead;        /* last RIRB entry read */
    IOLock                   *fCommandLock;
};

#endif /* _MINKAR_HDA_DRIVER_H_ */
