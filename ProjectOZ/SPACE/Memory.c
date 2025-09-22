//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include "Defs.h"

#define NTSPACE_TOPMASK ((1 << NTSPACE_TOPBITS) - 1)
#define NTSPACE_MIDMASK ((1 << NTSPACE_MIDBITS) - 1)

#define TOPVADBITS(va) (((va) >> (NTSPACE_PAGEBITS + NTSPACE_MIDBITS)) &  NTSPACE_TOPMASK)
#define MIDVADBITS(va) (((va) >> (NTSPACE_PAGEBITS                  )) &  NTSPACE_MIDMASK)
#define BOTVADBITS(va) (((va)                                        ) & (NTSPACE_PAGESIZE-1))

#define NTSPACE_MAPDIRENTRIES (1 << NTSPACE_TOPBITS)
#define NTSPACE_MAPTABENTRIES (1 << NTSPACE_MIDBITS)

Mapping *
VPageToMap(
        ULONG      cpu,
        Space     *space,
        ULONG_PTR  vpage
    )
{
    ULONG_PTR va    = PAGE2ADDR(vpage);
    ULONG     rooti = TOPVADBITS(va);

    Mapping **rootmap;
    Mapping  *pmap;

    if (space == NULL)
        return NULL;
    rootmap = space->rootmap;
    if (rootmap == NULL) {
        ULONG sz = NTSPACE_MAPDIRENTRIES * sizeof(Mapping *);
        rootmap = malloc(sz);
        rootmap != NULL  || FATALFAIL(STATUS_NO_MEMORY, "Allocating rootmap");

        RtlZeroMemory(rootmap, sz);
        space->rootmap = rootmap;
    }

    pmap = rootmap[rooti];

    if (pmap == NULL) {
        ULONG sz = NTSPACE_MAPTABENTRIES * sizeof(Mapping);
        pmap = malloc(sz);
        pmap != NULL  || FATALFAIL(STATUS_NO_MEMORY, "Allocating pagemap");


        RtlZeroMemory(pmap, sz);
        rootmap[rooti] = pmap;
    }

    return &pmap[MIDVADBITS(va)];
}

void
instantiatespace(
        ULONG ctx
    )
{
    if (spaces[ctx] == NULL) {
        spaces[ctx] = malloc(sizeof(Space));
        spaces[ctx] || FATALFAIL(STATUS_NO_MEMORY, "instantiate space");
        ZERO(spaces[ctx]);

        if (ctx > spaceglobals.highestctx)
            spaceglobals.highestctx = ctx;
    }

}

void
instantiatedomain(
        ULONG ctx,
        ULONG mode
    )
{
    instantiatespace(ctx);

    if (spaces[ctx]->domains[mode] == NULL) {
        spaces[ctx]->domains[mode] = malloc(sizeof(Domain));
        spaces[ctx]->domains[mode] || FATALFAIL(STATUS_NO_MEMORY, "instantiate domain");
        ZERO(spaces[ctx]->domains[mode]);
    }
}

void instantiatentthreaddescr(
        ULONG ctx,
        ULONG mode,
        ULONG cpu
    )
{
    NTThreadDescr **d;

    instantiatedomain(ctx,mode);
    d = &spaces[ctx]->domains[mode]->cpus[cpu];

    if (*d)  return;

    *d = malloc(sizeof(**d));
    *d || FATALFAIL(STATUS_NO_MEMORY, "instantiate ntthreaddescr");
    ZERO(*d);

    (*d)->domain = spaces[ctx]->domains[mode];
    (*d)->cpu = cpu;
}

NTSTATUS
CleanCtx(
        ULONG xcpu,
        ULONG ctx
    )
{
    Space      *space;
    Domain     *domain;
    Mapping   **pmap;
    Mapping   **epmap;
    ULONG       mode;
    ULONG       cpu;

    if (ctx == IOCTX) {
        space = iospace;
        iospace = NULL;
    } else if (ctx < MAXCTXS) {
        space = spaces[ctx];
        spaces[ctx] = NULL;
    } else {
        return STATUS_ILLEGAL_INSTRUCTION;
    }


    if (space == NULL)
        return STATUS_SUCCESS;

    //
    // free the domain structures, terminating the corresponding NT Process (and thus the CPU threads)
    // make sure the handles get closed
    //
    for (mode = 0;  mode < MAXMODES;  mode++) {
        domain = space->domains[mode];
        if (domain == NULL)
            continue;

        space->domains[mode] = NULL;
        for (cpu = 0;  cpu < MAXCPUS;  cpu++) {
            if (domain->cpus[cpu] && domain->cpus[cpu]->ntcputhread) {
                NtClose(domain->cpus[cpu]->ntcputhread);
            }
        }

        if (domain->ntprocess) {
            NtTerminateProcess(domain->ntprocess, STATUS_SUCCESS);
            NtClose(domain->ntprocess);
        }

        free(domain);
    }

    //
    // free the map tables and then the map directory
    //
    if (space->rootmap) {
        epmap = space->rootmap + NTSPACE_MAPDIRENTRIES;
        for (pmap = space->rootmap;  pmap < epmap;  pmap++) {
            if (pmap) {
                free(*pmap);
                *pmap = NULL;
            }
        }
        free(space->rootmap);
        space->rootmap = NULL;
    }

    //
    // clean the trapvector
    //
    cleantrapvector(space->trapvector);
    space->trapvector = NULL;

    //
    // free the space structure itself
    //
    free(space);

    return STATUS_SUCCESS;
}

//
// insert the specified mapping for the given context and virtual page
//
NTSTATUS
MapMemory(
        ULONG       cpu,
        ULONG       ctx,
        ULONG       vpage,
        ULONG       ppage,
        ULONG       readmask,
        ULONG       writemask
     )
{
    Space       **pspace;
    Mapping      *map;
    ULONG         bit_mask;
    int           d;

    ULONG       npages = 1;

//STRACE("[ctx/npages, vpage, ppage, masks]", MASH(ctx,npages), vpage, ppage, readmask|writemask)
    if (ppage >= spaceparams.nphysmempages && ppage != NTSPACE_INVALIDPAGE)
        return STATUS_ILLEGAL_INSTRUCTION;
    if (ppage == NTSPACE_INVALIDPAGE && (readmask|writemask))
        return STATUS_ILLEGAL_INSTRUCTION;
    if (ctx == IOCTX) {
        ;
    } else if (ctx > MAXCTXS || vpage < spaceparams.vpage_base || spaceparams.vpage_limit <= vpage) {
        return STATUS_ILLEGAL_INSTRUCTION;
    }

    // put this into the mapping tree for our space
    // based on the changes, update the view in all domains within the space
    pspace = (ctx == IOCTX)?  &iospace : &spaces[ctx];

    if (*pspace == NULL) {
        *pspace = malloc(sizeof(**pspace));
        *pspace || FATALFAIL(STATUS_NO_MEMORY, "MapMemory");
        ZERO(*pspace);
    }

    map = VPageToMap(cpu, *pspace, vpage);

    if (map == NULL)
        return STATUS_ILLEGAL_INSTRUCTION;

    // if ppage isn't changing, only affect is on domains which are getting different access to the page
    // otherwise ppage has changed, and we must change all domains with any access
    if (map->physpage == ppage) {
        bit_mask = (map->readmask ^ readmask) | (map->writemask ^ writemask);
    } else {
        bit_mask =  map->readmask | readmask  |  map->writemask | writemask;
    }

    map->physpage   = ppage;
    map->readmask   = readmask;
    map->writemask  = writemask;

    for (d = 0;  d < MAXMODES;  d++) {
        ULONG         domainmask = BITMASK(d);
        ULONG         pageaccess;
        ULONG_PTR     vaddr;
        ULONGLONG     paddr;
        SIZE_T        pagesize   = NTSPACE_PAGESIZE;
        Domain       *domain     = (*pspace)->domains[d];
        NTSTATUS      s;
        ULONG         i;

        if ((bit_mask & domainmask) == 0)
            continue;

        if (domain == NULL  || domain->ntprocess == NULL)
            continue;

        if (ppage == NTSPACE_INVALIDPAGE) {
            for (i = 0;  i < npages;  i++) {
                vaddr = PAGE2ADDR(vpage+i);
                s = NtUnmapViewOfSection ( domain->ntprocess, (PVOID)vaddr );
                if (!NT_SUCCESS(s)) return STATUS_ILLEGAL_INSTRUCTION;
            }
        } else {
            if (writemask & domainmask)
                pageaccess = PAGE_READWRITE;
            else if (readmask & domainmask)
                pageaccess = PAGE_READONLY;
            else
                pageaccess = PAGE_NOACCESS;

            // need multiple views, not a single large view, or won't be able to later unmap a single page
            for (i = 0;  i < npages;  i++) {
                vaddr = PAGE2ADDR(vpage+i);
                paddr = PAGE2ADDR(ppage+i);
                s = NtMapViewOfSection ( spaceglobals.physmemsection, domain->ntprocess, (PVOID)&vaddr, 0,
                        NTSPACE_PAGESIZE, (PLARGE_INTEGER)&paddr, &pagesize, ViewUnmap, 0, pageaccess);
                if (!NT_SUCCESS(s)) return STATUS_ILLEGAL_INSTRUCTION;
            }
        }
    }
    return STATUS_SUCCESS;
}

