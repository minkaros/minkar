/*
 * Copyright (C) 2026 Zoe Knox. All rights reserved.
 * Modifications Copyright (c) 2026 The Minkar contributors
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "ACPIPlatformExpert.h"
#include "AppleAPIC.h"
#include <IOKit/IODeviceTreeSupport.h>
#include <pexpert/pexpert.h>

typedef struct {
    uint64_t baseAddress;
    uint16_t segmentGroup;
    uint8_t startBus;
    uint8_t endBus;
    uint32_t reserved;
} __attribute__((packed)) ACPI_MCFG;

typedef struct {
    uint8_t  maxBusNum;
    uint8_t  majorVersion;
    uint8_t  minorVersion;
    uint8_t  BIOSPresent;

    union {
        struct {
            uint8_t configMethod1;
            uint8_t configMethod2;
            uint8_t specialCycle1;
            uint8_t specialCycle2;
        } s;
    } u_bus;

} PCI_bus_info_t;

void
ACPIPlatformExpert::PE_Log(const char * fmt, ...)
{
    char    buffer[1024];
    va_list args;
    
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer) - 1, fmt, args);
    va_end(args);
    kprintf("[ACPI PE] %s\n", buffer);
}

void
ACPIPlatformExpert::parseAPIC(void * table, IOService * nub)
{
    unsigned ioapicIndex = 0;   /* Minkar: I/O APICs in MADT order */

    struct LAPIC_RECORD {
        uint8_t  type;
        uint8_t  length;
        uint8_t  proc_id;
        uint8_t  lapic_id;
        uint32_t flags;
    } __attribute__((packed));

    struct IOAPIC_RECORD {
        uint8_t  type;
        uint8_t  length;
        uint8_t  io_apic_id;
        uint8_t  reserved;
        uint32_t address;
        uint32_t gsi_base;
    } __attribute__((packed));

    struct ISO_RECORD {
        uint8_t  type;
        uint8_t  length;
        uint8_t  bus;
        uint8_t  source;
        uint32_t gsi;
        uint16_t flags;
    } __attribute__((packed));

    /* Processor Local x2APIC (ACPI 4.0+, MADT type 9). Firmware on hybrid parts
     * (Meteor Lake and later) describes processors this way rather than type 0. */
    struct X2APIC_RECORD {
        uint8_t  type;
        uint8_t  length;
        uint16_t reserved;
        uint32_t x2apic_id;
        uint32_t flags;
        uint32_t acpi_uid;
    } __attribute__((packed));

    char          buf[64];
    struct MADT * madt = (struct MADT *) table;
    MADT_Record * rec = madt->records;
    int           index = 0;
    OSArray     * intrOverrides = NULL;
    OSArray     * intrSources = NULL;
    
    PE_Log("parseAPIC(%p, %p) flags %p",
           table, madt->lapicAddress, madt->flags);

    intrOverrides = OSArray::withCapacity(20);
    intrSources = OSArray::withCapacity(20);
    if (!intrOverrides || !intrSources) {
        PE_Log("Failed to create interrupt arrays!");
        return; /* and panic */
    }

    cpuArray = OSArray::withCapacity(32);
    if (!cpuArray) {
        PE_Log("Failed to create CPU array!");
        intrOverrides->release();
        intrSources->release();
        return; /* and panic */
    }

    /* Firmware may list the same processor as both a type 0 and a type 9 record.
     * The convention (matching Linux) is that type 9 wins only above ID 254, so
     * we need to know whether any type 0 record exists before walking. */
    bool haveLegacyLapics = false;
    for (MADT_Record * scan = madt->records;
         (uint64_t)scan < ((uint64_t)madt + madt->length) && scan->length != 0;
         scan = (MADT_Record *) ((uint64_t)scan + scan->length)) {
        if (scan->type == 0) {
            haveLegacyLapics = true;
            break;
        }
    }

    /*
     * Minkar: an x2APIC record (type 9) is a duplicate only when an
     * enabled type 0 record has the same APIC ID. Firmware may describe
     * some processors with type 0 records and the rest with type 9 ones,
     * small IDs included: skipping every type 9 record below 255 when any
     * type 0 one exists dropped those.
     */
    auto legacyListed = [&](uint32_t apic) -> bool {
        if (!haveLegacyLapics || apic > 0xFFu)
            return false;
        for (MADT_Record * scan = madt->records;
             (uint64_t)scan < ((uint64_t)madt + madt->length) && scan->length != 0;
             scan = (MADT_Record *) ((uint64_t)scan + scan->length)) {
            struct LAPIC_RECORD * lr = (struct LAPIC_RECORD *) scan;

            if (scan->type == 0 && (lr->flags & 1u) && lr->lapic_id == apic)
                return true;
        }
        return false;
    };

    /*
     * Minkar: a processor record the kernel can use: enabled, not a
     * duplicate of a type 0 record, and an xAPIC ID (up to 255).
     */
    auto usableProcessor = [&](MADT_Record * r, uint32_t * apic, uint32_t * proc,
        bool log) -> bool {
        if (r->type == 0) {
            struct LAPIC_RECORD * lr = (struct LAPIC_RECORD *) r;

            /* A disabled processor is not there; starting it would hang. */
            if (!(lr->flags & 1u)) {
                return false;
            }
            *apic = lr->lapic_id;
            *proc = lr->proc_id;
        } else if (r->type == 9) {
            struct X2APIC_RECORD * xr = (struct X2APIC_RECORD *) r;

            if (r->length < sizeof(*xr)) {
                if (log)
                    PE_Log("Malformed MADT x2APIC record, length %d", r->length);
                return false;
            }
            /* x2APIC tables are routinely padded with disabled entries. */
            if (!(xr->flags & 1u) || xr->x2apic_id == 0xFFFFFFFFu) {
                return false;
            }
            if (legacyListed(xr->x2apic_id)) {
                return false; /* already described by a type 0 record */
            }
            *apic = xr->x2apic_id;
            *proc = xr->acpi_uid;
        } else {
            return false;
        }
        if (*apic > 0xFFu) {
            if (log)
                IOLog("ACPI: processor with APIC ID %u left out (above 255)\n", *apic);
            return false;
        }
        return true;
    };

    /*
     * Minkar: count the processors before the first one is published.
     * Each ACPICPU starts as soon as its nub is registered, and the first
     * takes ncpus as the number to wait for: counted as the nubs were made,
     * PC 2 (a Xeon E5-2697 v2, 24 threads) started 2 and the kernel panicked
     * ("Should have 12 Cores, but only found 2").
     */
    uint32_t total = 0, created = 0;
    for (MADT_Record * scan = madt->records;
         (uint64_t)scan < ((uint64_t)madt + madt->length) && scan->length != 0;
         scan = (MADT_Record *) ((uint64_t)scan + scan->length)) {
        uint32_t apic, proc;

        if (usableProcessor(scan, &apic, &proc, false))
            total++;
    }
    ncpus = total < 64 ? total : 64;
    PE_Log("MADT: %u usable processor(s)", total);

    /*
     * Minkar: madt_pause=<seconds> lists every processor record and waits,
     * to photograph a PC's processor table (no serial port to read).
     */
    uint32_t madtPause = 0;
    if (PE_parse_boot_argn("madt_pause", &madtPause, sizeof(madtPause)) && madtPause) {
        unsigned shown = 0;

        for (MADT_Record * scan = madt->records;
             (uint64_t)scan < ((uint64_t)madt + madt->length) && scan->length != 0;
             scan = (MADT_Record *) ((uint64_t)scan + scan->length)) {
            if (scan->type == 0) {
                struct LAPIC_RECORD * lr = (struct LAPIC_RECORD *) scan;

                kprintf("MADT LAPIC uid %u apic %u flags 0x%x%s", lr->proc_id, lr->lapic_id,
                    lr->flags, (++shown % 3) ? "   " : "\n");
            } else if (scan->type == 9 && scan->length >= sizeof(struct X2APIC_RECORD)) {
                struct X2APIC_RECORD * xr = (struct X2APIC_RECORD *) scan;

                kprintf("MADT x2APIC uid %u apic %u flags 0x%x%s", xr->acpi_uid, xr->x2apic_id,
                    xr->flags, (++shown % 3) ? "   " : "\n");
            }
        }
        kprintf("\nMADT: %u processor record(s), %u usable; waiting %u s\n", shown, total,
            madtPause);
        IOSleep(madtPause * 1000);
    }

    while ((uint64_t)rec < ((uint64_t)madt + madt->length)) {
        if (rec->length == 0) {
            rec = (MADT_Record *) ((uint64_t)rec + sizeof(MADT_Record));
        }

        switch (rec->type) {
            case 0:   /* Local APIC */
            case 9: { /* Local x2APIC */
                uint32_t apic_id, proc_id;

                if (!usableProcessor(rec, &apic_id, &proc_id, true)) {
                    break;
                }
                /*
                 * Minkar: the kernel takes 64 processors (MAX_CPUS); a
                 * bigger machine runs on its first 64.
                 */
                if (created >= 64) {
                    if (created++ == 64)
                        IOLog("ACPI: more than 64 processors; the kernel takes "
                            "the first 64\n");
                    break;
                }
                created++;
                OSDictionary * dict = OSDictionary::withCapacity(7);

                dict->setObject("device_type", OSString::withCString("processor"));
                dict->setObject("compatible", OSString::withCString("processor"));
                dict->setObject("apic-id", OSNumber::withNumber(apic_id, 32));
                dict->setObject("processor-id", OSNumber::withNumber(proc_id, 32));
                dict->setObject("cpu-number", OSNumber::withNumber(index, 32));

                sprintf(buf, "cpu@%d", index++);
                dict->setObject("name", OSString::withCString(buf));
                dict->setObject("location", OSNumber::withNumber(madt->lapicAddress, 32));
                IOService * aNub = createNub(dict);
                if (aNub) {
                    cpuArray->setObject(aNub);
                    aNub->attach(nub);
                    aNub->registerService();
                    aNub->release();
                }
                dict->release();
                break;
            }
            case 1: { /* I/O APIC */
                struct IOAPIC_RECORD *apic = (struct IOAPIC_RECORD *) rec;
                OSDictionary * dict = OSDictionary::withCapacity(7);

                dict->setObject("device_type", OSString::withCString("io-apic"));

                dict->setObject("Destination APIC ID",
                                OSNumber::withNumber(0ULL, 32)); // LAPIC 0
                dict->setObject("Base Vector Number",
                                OSNumber::withNumber(apic->gsi_base + 0x40, 32));
                dict->setObject("Physical Address",
                                OSNumber::withNumber(apic->address, 32));

                /*
                 * Minkar: named by its place in the MADT, not its APIC ID.
                 * The platform expert gives every device "io-apic@0" and
                 * looks for that name when it dispatches; QEMU's I/O APIC
                 * has ID 0, a PC's often 2 (the reference H270N), and there
                 * every driver that registered an interrupt waited forever
                 * for a controller that never appeared.
                 */
                sprintf(buf, "io-apic@%u", ioapicIndex++);
                dict->setObject("IOAPIC ID",
                                OSNumber::withNumber(apic->io_apic_id, 32));
                dict->setObject("InterruptControllerName", OSSymbol::withCString(buf));
                dict->setObject("name", OSString::withCString(buf));
                dict->setObject("compatible", OSString::withCString("io-apic"));

                IOService * aNub = createNub(dict);
                if (aNub) {
                    setupAPIC(aNub);
                    aNub->attach(nub);
                    aNub->registerService();
                    aNub->release();
                }
                dict->release();
                break;
            }
            case 2: { /* Interrupt Source Override */
                struct ISO_RECORD *iso = (struct ISO_RECORD *) rec;
                OSDictionary * dict = OSDictionary::withCapacity(4);
                dict->setObject("bus", OSNumber::withNumber(iso->bus, 32));
                dict->setObject("source", OSNumber::withNumber(iso->source, 32));
                dict->setObject("gsi", OSNumber::withNumber(iso->gsi, 32));
                dict->setObject("flags", OSNumber::withNumber(iso->flags, 32));
                intrOverrides->setObject(dict);
                dict->release();
                break;
            }
            case 4: { /* LAPIC NMI */
                uint32_t v = *(uint32_t *) ((uint64_t)rec + sizeof(MADT_Record));
                uint32_t acpi_id = (v & 0xff000000) >> 24;
                uint32_t flags = (v & 0x00ffff00) >> 8;
                uint32_t lint = (v & 0xff);
                OSDictionary * dict = OSDictionary::withCapacity(3);
                dict->setObject("acpi-id", OSNumber::withNumber(acpi_id, 32));
                dict->setObject("flags", OSNumber::withNumber(flags, 32));
                dict->setObject("lint", OSNumber::withNumber(lint, 32));
                intrSources->setObject(dict);
                dict->release();
                break;
            }
        }
        
        rec = (MADT_Record *) ((uint64_t)rec + rec->length);
    }

    this->setProperty("interrupt-overrides", intrOverrides);
    this->setProperty("interrupt-sources", intrSources);
}

void
ACPIPlatformExpert::parseMCFG(void * table, IOService * nub)
{
    PE_Log("parseMCFG(%p, %p)", table, nub);

    ACPI_MCFG * entry = (ACPI_MCFG *) ((uint64_t)table
            + sizeof(struct XSDT) + sizeof(uint64_t) /* reserved field */);

    int count = (((struct XSDT *)table)->length
                    - sizeof(struct XSDT) - sizeof(uint64_t)) / sizeof(ACPI_MCFG);

    // FIXME: handle multiple entries

    PCI_bus_info_t businfo = {0};
    businfo.maxBusNum = entry[0].endBus;
    businfo.majorVersion = 3; // PCI 3.0.. is it true? who knows.
    businfo.u_bus.s.configMethod1 = true;
    businfo.u_bus.s.configMethod2 = true;

    OSDictionary * dict = OSDictionary::withCapacity(8);
    dict->setObject("compatible", OSString::withCString("pci"));
    dict->setObject("name", OSString::withCString("pci"));
    dict->setObject("device_type", OSString::withCString("pci"));
    dict->setObject("pci-bus-info", 
                OSData::withBytes(&businfo, sizeof(businfo)));

    IOService *aNub = createNub(dict);

    if (!aNub)
        return;

    aNub->setName("pci");
    aNub->attach(nub);

    // The PCI devices attach under it in the device tree plane too: it
    // hangs from the tree's root, at 0, so that their paths are whole
    // (IODeviceTree:/pci@0/...), as PCI0@0 on a Mac. DiskArbitration
    // names a disk's bus by that path.
    IORegistryEntry *dtRoot = IORegistryEntry::fromPath("/", gIODTPlane);
    if (dtRoot) {
        aNub->attachToParent(dtRoot, gIODTPlane);
        aNub->setLocation("0", gIODTPlane);
        dtRoot->release();
    }

    aNub->registerService();
    aNub->release();
    dict->release();
}

const char * ACPIPlatformExpert::deleteList( void )
{
    return "(\"cpus\",\"PCI0\",\"PCI1\")";
}

const char * ACPIPlatformExpert::excludeList( void )
{
    return "(\"PCI0\",\"PCI1\",\"ACPI\")";
}
