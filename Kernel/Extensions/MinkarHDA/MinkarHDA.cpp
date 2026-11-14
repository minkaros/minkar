/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * MinkarHDA: Intel High Definition Audio controllers (PCI class 0x0403).
 * It resets the controller, runs the command rings (CORB and RIRB) and
 * finds the codecs on the link: each codec's vendor, revision and function
 * groups are in the registry (HDACodecs). Playback comes on top of it.
 */

#include "MinkarHDA.h"
#include <IOKit/IOLib.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>

#define super IOService
#define LOG(fmt, ...) IOLog("MinkarHDA: " fmt "\n", ##__VA_ARGS__)

OSDefineMetaClassAndStructors(MinkarHDA, IOService);

static inline void
barrier(void)
{
    __asm__ volatile("mfence" ::: "memory");
}

/* ---- Registers --------------------------------------------------------- */

/* BAR0, uncached; from the BAR itself when the platform made no
   IODeviceMemory for it (a 64-bit BAR continues in BAR1). */
bool
MinkarHDA::mapRegisters()
{
    const UInt8 reg = kIOPCIConfigBaseAddress0;

    fMap = fPCI->mapDeviceMemoryWithRegister(reg, kIOMapInhibitCache);
    if (fMap == NULL) {
        uint32_t lo = fPCI->configRead32(reg);
        uint64_t base = lo & ~0xFULL;

        if ((lo & 1) != 0)
            return false;
        if ((lo & 0x6) == 0x4)
            base |= (uint64_t)fPCI->configRead32(reg + 4) << 32;
        if (base == 0)
            return false;
        fDesc = IOMemoryDescriptor::withPhysicalAddress(base, 0x4000, kIODirectionNone);
        if (fDesc == NULL)
            return false;
        fMap = fDesc->map(kIOMapAnywhere | kIOMapInhibitCache);
        if (fMap == NULL)
            return false;
    }
    fRegs = (volatile uint8_t *)fMap->getVirtualAddress();
    return true;
}

/* ---- Controller -------------------------------------------------------- */

/* Takes the link through reset; the codecs that answer set their bit in
   STATESTS. */
bool
MinkarHDA::resetController()
{
    write32(HDA_GCTL, read32(HDA_GCTL) & ~HDA_GCTL_CRST);
    for (int i = 0; i < 1000 && (read32(HDA_GCTL) & HDA_GCTL_CRST) != 0; i++)
        IODelay(100);
    if ((read32(HDA_GCTL) & HDA_GCTL_CRST) != 0)
        return false;
    IODelay(200);
    write32(HDA_GCTL, read32(HDA_GCTL) | HDA_GCTL_CRST);
    for (int i = 0; i < 1000 && (read32(HDA_GCTL) & HDA_GCTL_CRST) == 0; i++)
        IODelay(100);
    if ((read32(HDA_GCTL) & HDA_GCTL_CRST) == 0)
        return false;
    /* The codecs ask for an address within 25 frames (521 us) of the
       reset; give them a millisecond. */
    IODelay(1000);
    fCodecMask = read16(HDA_STATESTS) & 0x7FFF;
    write16(HDA_STATESTS, fCodecMask);
    return true;
}

static uint16_t
ringEntries(uint8_t sizeRegister, uint8_t *choice)
{
    if (sizeRegister & HDA_RINGSIZE_CAP256) {
        *choice = HDA_RINGSIZE_256;
        return 256;
    }
    if (sizeRegister & HDA_RINGSIZE_CAP16) {
        *choice = HDA_RINGSIZE_16;
        return 16;
    }
    *choice = HDA_RINGSIZE_2;
    return 2;
}

/* The command ring (CORB) and the response ring (RIRB), in one page below
   4 GB: the CORB's 1 KB, then the RIRB's 2 KB. */
bool
MinkarHDA::startRings()
{
    uint8_t corbSize, rirbSize;
    uint64_t phys;

    stopRings();
    fRingMem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
        kIOMemoryPhysicallyContiguous | kIODirectionInOut, PAGE_SIZE, 0x00000000FFFFF000ULL);
    if (fRingMem == NULL)
        return false;
    bzero(fRingMem->getBytesNoCopy(), PAGE_SIZE);
    phys = fRingMem->getPhysicalAddress();
    fCORB = (volatile uint32_t *)fRingMem->getBytesNoCopy();
    fRIRB = (volatile HDAResponse *)((uint8_t *)fRingMem->getBytesNoCopy() + 1024);

    fCORBEntries = ringEntries(read8(HDA_CORBSIZE), &corbSize);
    fRIRBEntries = ringEntries(read8(HDA_RIRBSIZE), &rirbSize);
    write8(HDA_CORBSIZE, (read8(HDA_CORBSIZE) & ~0x3) | corbSize);
    write8(HDA_RIRBSIZE, (read8(HDA_RIRBSIZE) & ~0x3) | rirbSize);

    write32(HDA_CORBLBASE, (uint32_t)phys);
    write32(HDA_CORBUBASE, (uint32_t)(phys >> 32));
    write32(HDA_RIRBLBASE, (uint32_t)(phys + 1024));
    write32(HDA_RIRBUBASE, (uint32_t)((phys + 1024) >> 32));

    /* The read pointer back to 0: set the reset bit, see it set, clear it,
       see it clear (some controllers never show it set). */
    write16(HDA_CORBRP, HDA_CORBRP_RST);
    for (int i = 0; i < 100 && (read16(HDA_CORBRP) & HDA_CORBRP_RST) == 0; i++)
        IODelay(10);
    write16(HDA_CORBRP, 0);
    for (int i = 0; i < 100 && (read16(HDA_CORBRP) & HDA_CORBRP_RST) != 0; i++)
        IODelay(10);
    write16(HDA_CORBWP, 0);
    write16(HDA_RIRBWP, HDA_RIRBWP_RST);
    fRIRBRead = 0;
    /* An interrupt count of 0 stops some controllers' RIRB. */
    write16(HDA_RINTCNT, 1);

    write8(HDA_CORBCTL, HDA_CORBCTL_RUN);
    /* The response interrupt on: QEMU counts responses against RINTCNT and
       takes no command after that until the status it sets is cleared;
       without the interrupt it never sets it. Nothing reaches the CPU
       while INTCTL's global enable is off. */
    write8(HDA_RIRBCTL, HDA_RIRBCTL_RUN | HDA_RIRBCTL_INT);
    for (int i = 0; i < 100 && (read8(HDA_CORBCTL) & HDA_CORBCTL_RUN) == 0; i++)
        IODelay(10);
    return (read8(HDA_CORBCTL) & HDA_CORBCTL_RUN) != 0;
}

void
MinkarHDA::stopRings()
{
    if (fRegs == NULL)
        return;
    write8(HDA_CORBCTL, 0);
    write8(HDA_RIRBCTL, 0);
    for (int i = 0; i < 100 && ((read8(HDA_CORBCTL) | read8(HDA_RIRBCTL)) & 0x2) != 0; i++)
        IODelay(10);
}

/* ---- Commands ---------------------------------------------------------- */

/* One verb at a time: it goes in the CORB, and its answer is the next
   solicited entry of the RIRB (unsolicited ones are skipped for now). */
bool
MinkarHDA::sendCommand(uint32_t command, uint32_t *response)
{
    bool answered = false;

    if (fCORB == NULL)
        return false;
    IOLockLock(fCommandLock);
    uint16_t wp = (uint16_t)((read16(HDA_CORBWP) & 0xFF) + 1) % fCORBEntries;
    fCORB[wp] = command;
    barrier();
    /* The response count (RINTCNT) reached stops some controllers (QEMU's
       among them) from taking commands until the status is cleared. */
    write8(HDA_RIRBSTS, HDA_RIRBSTS_INTFL | HDA_RIRBSTS_OIS);
    write16(HDA_CORBWP, wp);

    /* A codec answers within a frame (21 us); allow 10 ms. */
    for (int i = 0; i < 1000 && !answered; i++) {
        uint16_t rirbWP = read16(HDA_RIRBWP) & 0xFF;

        while (fRIRBRead != rirbWP && !answered) {
            fRIRBRead = (uint16_t)((fRIRBRead + 1) % fRIRBEntries);
            barrier();
            if (fRIRB[fRIRBRead].extended & HDA_RESPONSE_UNSOL)
                continue;
            if (response != NULL)
                *response = fRIRB[fRIRBRead].response;
            answered = true;
        }
        if (!answered) {
            if (read8(HDA_RIRBSTS) & HDA_RIRBSTS_INTFL)
                write8(HDA_RIRBSTS, HDA_RIRBSTS_INTFL);
            IODelay(10);
        }
    }
    write8(HDA_RIRBSTS, HDA_RIRBSTS_INTFL | HDA_RIRBSTS_OIS);
    IOLockUnlock(fCommandLock);
    return answered;
}

bool
MinkarHDA::getParameter(uint8_t cad, uint8_t nid, uint8_t param, uint32_t *value)
{
    return sendCommand(HDA_CMD12(cad, nid, HDA_VERB_GET_PARAMETER, param), value);
}

/* ---- Codecs ------------------------------------------------------------ */

static void
setNumber(OSDictionary *dict, const char *key, uint64_t value, int bits)
{
    OSNumber *n = OSNumber::withNumber(value, bits);

    if (n != NULL) {
        dict->setObject(key, n);
        n->release();
    }
}

/* A codec's vendor, revision and function groups, in the registry. */
void
MinkarHDA::describeCodec(uint8_t cad)
{
    uint32_t vendor = 0, revision = 0, nodes = 0;
    OSDictionary *codec;
    OSArray *groups;
    OSArray *codecs;

    if (!getParameter(cad, 0, HDA_PARAM_VENDOR_ID, &vendor) ||
        !getParameter(cad, 0, HDA_PARAM_REVISION_ID, &revision) ||
        !getParameter(cad, 0, HDA_PARAM_SUB_NODE_COUNT, &nodes)) {
        LOG("codec %u does not answer", cad);
        return;
    }
    codec = OSDictionary::withCapacity(4);
    groups = OSArray::withCapacity(2);
    if (codec == NULL || groups == NULL) {
        OSSafeReleaseNULL(codec);
        OSSafeReleaseNULL(groups);
        return;
    }
    setNumber(codec, "address", cad, 8);
    setNumber(codec, "vendor-id", vendor, 32);
    setNumber(codec, "revision-id", revision, 32);

    for (uint32_t i = 0; i < HDA_SUB_NODE_TOTAL(nodes); i++) {
        uint8_t nid = (uint8_t)(HDA_SUB_NODE_START(nodes) + i);
        uint32_t type = 0, widgets = 0;
        OSDictionary *group;

        if (!getParameter(cad, nid, HDA_PARAM_FUNCTION_GROUP_TYPE, &type))
            continue;
        getParameter(cad, nid, HDA_PARAM_SUB_NODE_COUNT, &widgets);
        group = OSDictionary::withCapacity(4);
        if (group == NULL)
            continue;
        setNumber(group, "node", nid, 8);
        setNumber(group, "type", HDA_FG_TYPE(type), 8);
        setNumber(group, "first-widget", HDA_SUB_NODE_START(widgets), 8);
        setNumber(group, "widgets", HDA_SUB_NODE_TOTAL(widgets), 8);
        groups->setObject(group);
        group->release();
        LOG("codec %u: %04x:%04x rev %08x, function group %u type %u, %u widgets from %u",
            cad, vendor >> 16, vendor & 0xFFFF, revision, nid, HDA_FG_TYPE(type),
            HDA_SUB_NODE_TOTAL(widgets), HDA_SUB_NODE_START(widgets));
    }
    codec->setObject("function-groups", groups);
    groups->release();

    codecs = OSDynamicCast(OSArray, getProperty("HDACodecs"));
    codecs = codecs != NULL ? OSArray::withArray(codecs) : OSArray::withCapacity(1);
    if (codecs != NULL) {
        codecs->setObject(codec);
        setProperty("HDACodecs", codecs);
        codecs->release();
    }
    codec->release();
}

void
MinkarHDA::probeCodecs()
{
    for (uint8_t cad = 0; cad < HDA_MAX_CODECS; cad++)
        if (fCodecMask & (1U << cad))
            describeCodec(cad);
}

/* ---- IOService --------------------------------------------------------- */

bool
MinkarHDA::start(IOService *provider)
{
    fPCI = OSDynamicCast(IOPCIDevice, provider);
    if (fPCI == NULL || !super::start(provider))
        return false;
    fPCI->retain();
    fCommandLock = IOLockAlloc();
    if (fCommandLock == NULL)
        return false;
    fPCI->setMemoryEnable(true);
    fPCI->setBusMasterEnable(true);
    /* Intel controllers: no traffic class (TCSEL, at 0x44) but TC0. */
    if (fPCI->configRead16(kIOPCIConfigVendorID) == 0x8086)
        fPCI->configWrite8(0x44, fPCI->configRead8(0x44) & ~0x07);

    if (!mapRegisters()) {
        LOG("cannot map the controller's registers");
        return false;
    }
    fCaps = read16(HDA_GCAP);
    if (!resetController()) {
        LOG("the controller does not come out of reset");
        return false;
    }
    if (!startRings()) {
        LOG("the command rings do not start");
        return false;
    }
    LOG("version %u.%u, %u output, %u input and %u bidirectional streams, codecs %04x",
        read8(HDA_VMAJ), read8(HDA_VMIN), HDA_GCAP_OSS(fCaps), HDA_GCAP_ISS(fCaps),
        HDA_GCAP_BSS(fCaps), fCodecMask);
    probeCodecs();
    registerService();
    return true;
}

void
MinkarHDA::stop(IOService *provider)
{
    stopRings();
    if (fRegs != NULL)
        write32(HDA_GCTL, read32(HDA_GCTL) & ~HDA_GCTL_CRST);
    super::stop(provider);
}

void
MinkarHDA::free()
{
    OSSafeReleaseNULL(fRingMem);
    OSSafeReleaseNULL(fMap);
    OSSafeReleaseNULL(fDesc);
    OSSafeReleaseNULL(fPCI);
    if (fCommandLock != NULL) {
        IOLockFree(fCommandLock);
        fCommandLock = NULL;
    }
    super::free();
}
