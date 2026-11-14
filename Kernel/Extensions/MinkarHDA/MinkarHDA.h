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
#define HDA_MAX_WIDGETS 128
#define HDA_MAX_CONNECTIONS 32
#define HDA_MAX_PATH    6
#define HDA_MAX_OUTPUTS 8

/* A widget of an audio function group. */
struct HDAWidget {
    uint8_t  nid;
    uint8_t  type;                  /* HDA_WIDGET_* */
    uint32_t caps;                  /* WIDGET_CAPS */
    uint32_t pinCaps;               /* PIN_CAPS (pins) */
    uint32_t config;                /* CONFIG_DEFAULT (pins) */
    uint32_t inAmpCaps, outAmpCaps;
    uint8_t  connections[HDA_MAX_CONNECTIONS];
    uint8_t  connectionCount;
    bool     used;                  /* on an output path */
};

/* An output: a pin, the DAC that feeds it, and the widgets between (from
   the pin to the DAC), each with the input it takes from the next. */
struct HDAOutput {
    uint8_t cad;
    uint8_t pin, dac;
    uint8_t path[HDA_MAX_PATH];
    uint8_t pathInput[HDA_MAX_PATH];  /* connection index into path[i + 1] */
    uint8_t length;
    uint8_t device;                 /* HDA_DEVICE_* of the pin */
};

/* A codec's audio function group and its widgets. */
struct HDAAudioGroup {
    uint8_t   cad, nid;
    uint8_t   firstWidget, widgetCount;
    HDAWidget widgets[HDA_MAX_WIDGETS];
};

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
    bool parseAudioGroup(uint8_t cad, uint8_t nid);
    void readConnections(HDAAudioGroup *group, HDAWidget *w);
    HDAWidget *widget(HDAAudioGroup *group, uint8_t nid);
    bool findPath(HDAAudioGroup *group, HDAOutput *out, uint8_t nid, int depth);
    void findOutputs(HDAAudioGroup *group);
    void enableOutput(HDAAudioGroup *group, const HDAOutput *out);
    void publishOutputs();
    bool verb(uint8_t cad, uint8_t nid, uint32_t verb, uint32_t payload, uint32_t *response = NULL);
    bool verb4(uint8_t cad, uint8_t nid, uint32_t verb, uint32_t payload);

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

    HDAAudioGroup            *fGroups[HDA_MAX_CODECS];
    HDAOutput                 fOutputs[HDA_MAX_OUTPUTS];
    int                       fOutputCount;
};

#endif /* _MINKAR_HDA_DRIVER_H_ */
