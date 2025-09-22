//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include "Defs.h"

Hashentry *DeviceHash[31];

//
// Record this mapping, which the DMA routine may later use to perform DMA
// Changing while a DMA operation is in progress will cause an exception
//
NTSTATUS
MapIO(
        ULONG       cpu,
        UCHAR       deviceid,
        UCHAR       writable,
        ULONG       iospacepage,
        ULONG       physpage
     )
{
    ULONG        readmask;
    ULONG        writemask;

//STRACE("[deviceid, writable, iospacepage, physpage]", deviceid, writable, iospacepage, physpage)
    if (deviceid >= MAXDEVICES)
        return STATUS_ILLEGAL_INSTRUCTION;

    if (physpage >= spaceparams.nphysmempages && physpage != NTSPACE_INVALIDPAGE)
        return STATUS_ILLEGAL_INSTRUCTION;

    if (physpage < spaceparams.nphysmempages) {
        readmask = (1 << deviceid);
        writemask = writable? readmask : 0;
    } else if (physpage == NTSPACE_INVALIDPAGE) {
        readmask = writemask = 0;
    } else {
        return STATUS_ILLEGAL_INSTRUCTION;
    }

    return MapMemory(cpu, IOCTX, iospacepage, physpage, readmask, writemask);

    return STATUS_SUCCESS;
}

ULONGLONG
AccessDevice(
        ULONG      cpu,
        USHORT     deviceid,
        UCHAR      op,
        UCHAR      deviceregister,
        ULONG      value,
        ULONG      valuex
      )
{
    Hashentry  *ptr;
    Device     *dev;

    ptr = LookupHashEntry(DeviceHash, ASIZEOF(DeviceHash), deviceid, NULL);
//STRACE("[deviceid, op, deviceregister, ptr]", deviceid, op, deviceregister, ptr)

    if (ptr == NULL)
        return STATUS_NO_SUCH_DEVICE;

    dev = CONTAINING_RECORD(ptr, Device, hash);
    dev->accessfcn || BUGCHECK("no device accessfcn");
    return (dev->accessfcn)(cpu, op, deviceregister, value, valuex);
}

//
// Hardware devices
//      DMA, IRQLs, DEVICE REGISTERS
//
NTSTATUS
RegisterDevice(
        ULONG         cpu,
        ULONG         deviceid,
        DEVICEACCESS  deviceroutine,
        ULONG         interrupt
    )
{
    Hashentry  *ptr;
    Hashentry **prevlink = NULL;
    Device     *dev;

    ptr = LookupHashEntry(DeviceHash, ASIZEOF(DeviceHash), deviceid, &prevlink);
    if (ptr) {
        return STATUS_DEVICE_ALREADY_ATTACHED;
    }

    dev = malloc(sizeof(*dev));
    if (dev == NULL) {
        return STATUS_NO_MEMORY;
    }
    ZERO(dev);

    dev->hash.value = deviceid;
    dev->hash.link = *prevlink;
    dev->accessfcn = deviceroutine;

    *prevlink = &dev->hash;

//STRACE("[deviceid, deviceroutine, interrupt, dev]", deviceid, deviceroutine, interrupt, dev)
    return STATUS_SUCCESS;
}

//
// Emulate a hardware operation to do DMA
// to/from the devicememory from/to physical memory.
//

struct DMAargs {
        ULONG               cpu;
        ULONG               deviceid;
        PCHAR               vaddr;
        PCHAR               daddr;
        UCHAR               synchronous;
        SIZE_T              count;
        ULONG               todevice;
        DMACOMPLETECALLBACK callback;
        PVOID               callbackcontext;
};

// run a background thread to do the copy to/from the devicemem to physical memory
// TODO: use a pool of threads rather than creating threads on demand
void
DMAThread(
        struct DMAargs  *argp
    )
{
    PCHAR       vaddr;
    PCHAR       daddr;
    SIZE_T      count;
    ULONG       todevice;
    PCHAR       evaddr;
    NTSTATUS    s;

    vaddr    = argp->vaddr;
    daddr    = argp->daddr;
    count    = argp->count;
    todevice = argp->todevice;
    evaddr   = vaddr + count;

    if (todevice)
        memcpy(daddr, vaddr, count);    // dst, src, len
    else
        memcpy(vaddr, daddr, count);

    for (evaddr = vaddr + count;  vaddr < evaddr;  vaddr += NTSPACE_PAGESIZE) {
        s = NtUnmapViewOfSection ( NtCurrentProcess(), (PVOID)vaddr );
        FATALFAIL(s, "Unmap DMA memory");
    }

    if (argp->callback)
        (argp->callback)(argp->cpu, argp->deviceid, count, argp->callbackcontext);
    free(argp);

    if (!argp->synchronous)
        NtTerminateThread(NtCurrentThread(), STATUS_SUCCESS);
}

NTSTATUS
StartDMA(
        ULONG       cpu,
        ULONG       deviceid,
        PCHAR       devicememory,
        ULONG_PTR   base,
        ULONG       bytecount,
        BOOLEAN     writephysmem,
        DMACOMPLETECALLBACK callback,       // if NULL, synchronously copy the data
        PVOID callbackcontext
    )
{
    ULONG_PTR       pagebase;
    ULONG           pageoffset;
    ULONG           encompasscount;
    ULONG           npages;
    NTSTATUS        s;

    PCHAR           vaddr;
    PCHAR           basevaddr;
    ULONG           pageaccess;
    SIZE_T          pagesize;
    SIZE_T          totalsize;
    LARGE_INTEGER   ilarge;
    Mapping        *map;
    struct DMAargs *argp;
    ULONG           i;

//STRACE("[deviceid, devicememory, base, W/bytecount", deviceid, devicememory, base, (writephysmem<<31)|bytecount)
    // establish the counts and addresses
    pagebase = (base & ~(NTSPACE_PAGESIZE - 1));
    pageoffset = (base & (NTSPACE_PAGESIZE - 1));
    encompasscount = bytecount + pageoffset;
    npages = COUNTBY(encompasscount, NTSPACE_PAGESIZE);
    pageaccess = writephysmem? PAGE_READWRITE : PAGE_READONLY;

    // Check that all the mappings are valid
    for (i = 0;  i < npages;  i++) {
        map = VPageToMap(cpu, iospace, ADDR2PAGE(pagebase));
        if (!map)
            return STATUS_ACCESS_VIOLATION;
    }

    // allocate the structure for the parameters to the DMA thread
    argp = malloc(sizeof(*argp));
    argp || FATALFAIL(STATUS_NO_MEMORY, "No memory for DMA args");
    ZERO(argp);

    // find a sufficiently large virtual address space region we can use
    totalsize = npages * NTSPACE_PAGESIZE;
    vaddr = NULL;
    s = NtAllocateVirtualMemory(NtCurrentProcess(), (PVOID) &vaddr, 0, &totalsize, MEM_RESERVE, pageaccess);
    FATALFAIL(s, "Allocate DMA addresses");

    basevaddr = (PCHAR) vaddr;

    // now that we know vaddr, release the memory
    s = NtFreeVirtualMemory(NtCurrentProcess(), (PVOID) &vaddr, &totalsize, MEM_RELEASE);
    FATALFAIL(s, "Free DMA addresses");

    // map the physical pages into our address space
    pagesize   = NTSPACE_PAGESIZE;
    for (i = 0;  i < npages;  i++) {
        map = VPageToMap(cpu, iospace, ADDR2PAGE(pagebase));
        ilarge.QuadPart = PAGE2ADDR(map->physpage);

        s = NtMapViewOfSection ( spaceglobals.physmemsection, NtCurrentProcess(), (PVOID) &vaddr, 0,
                        NTSPACE_PAGESIZE, &ilarge, &pagesize, ViewUnmap, 0, pageaccess);
        FATALFAIL(s, "Map DMA memory");

        vaddr += NTSPACE_PAGESIZE;
        pagebase += NTSPACE_PAGESIZE;
    }

    argp->cpu      = cpu;
    argp->deviceid = deviceid;
    argp->vaddr    = basevaddr + pageoffset;
    argp->daddr    = devicememory;
    argp->count    = bytecount;
    argp->todevice = !writephysmem;
    argp->callback = callback;
    argp->callbackcontext = callbackcontext;

    //
    // If no callback routines was specified, then do the copy synchronously
    //
    if (callback == NULL) {
        argp->synchronous = 1;
        DMAThread(argp);
        return STATUS_SUCCESS;
    }

    //
    // Use an NT thread to run the DMA
    // TODO: use a pool of threads rather than creating them each time.
    //
    s = RtlCreateUserThread(NtCurrentProcess(), NULL, FALSE, 0, 0, 0, (PVOID)DMAThread, (PVOID)argp, NULL, NULL);
    FATALFAIL(s, "NtCreateThread for DMA");

    return STATUS_SUCCESS;
}

