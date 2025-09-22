//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include "Defs.h"

// The global trapvector preempts the per-space trapvectors
Trapvector  GlobalTrapVector;

ULONG trapvectorlimits[NTRAPTYPES] = {
    NTRAP_EMULCALLS,
    NTRAP_SERVCALLS,
    NTRAP_INTERRUPTS,
    NTRAP_EXCEPTIONS,
    NTRAP_FAULTS
};

VOID     InitVADTree (Trapvector *pTrapvector);
Portal **InsertVAD   (Trapvector *pTrapvector, ULONG VirtualPageBase, ULONG VirtualPageLimit);
Portal **LookupVAD   (Trapvector *pTrapvector, ULONG VirtualPage);
BOOLEAN  DeleteVAD   (Trapvector *pTrapvector, ULONG VirtualPage);

Trapvector *
instantiatetrapvector()
{
    Trapvector *trapvector = malloc(sizeof(*trapvector));
    trapvector || FATALFAIL(STATUS_NO_MEMORY, "MapTrap");
    ZERO(trapvector);

    InitVADTree(trapvector);
    return trapvector;
}

//
//
//
NTSTATUS
MapTrap(
        ULONG       cpu,
        ULONG       ctx,
        UCHAR       traptype,
        UCHAR       global,
        ULONG       indexbase,
        ULONG       indexlimit,
        ULONG       portalid
    )
{
    Trapvector  *trapvector;
    Portal     **pportal;
    Portal      *portal;
    ULONG        i;

//STRACE("[ctx, G/traptype/portalid, indexbase, indexlimit]", ctx, (global<<31)|MASH(traptype,portalid), indexbase, indexlimit)
    portal = NULL;

    if (traptype != trapvaddr && traptype >= NTRAPTYPES)
        return STATUS_ILLEGAL_INSTRUCTION;

    // validate the trapvector limits
    if (traptype != trapvaddr) {
        if (indexlimit == 0)
            indexlimit = indexbase;

        if (indexbase > indexlimit  ||  indexlimit > trapvectorlimits[traptype])
            return STATUS_ILLEGAL_INSTRUCTION;

    } else if (indexbase < spaceparams.vpage_base || indexlimit && spaceparams.vpage_limit < indexlimit) {
            return STATUS_ILLEGAL_INSTRUCTION;
    }


    // validate that the ctx matches the traptype
    switch (ctx) {
    case   IOCTX:
        return STATUS_ILLEGAL_INSTRUCTION;

    case   EMULCTX:
        if (traptype != trapemul)
            return STATUS_ILLEGAL_INSTRUCTION;
        break;

    case   GLOBALCTX:
        if (traptype == trapemul)
            return STATUS_ILLEGAL_INSTRUCTION;
        if (!global)
            return STATUS_ILLEGAL_INSTRUCTION;
        break;

    default:    // a space ctx
        if (traptype == trapemul)
            return STATUS_ILLEGAL_INSTRUCTION;
        if (global)
            return STATUS_ILLEGAL_INSTRUCTION;
        break;
    }

    // lookup the portalid
    if (portalid) {
        portal = lookupportal(portalid);
        if (portal == NULL)
            return STATUS_ILLEGAL_INSTRUCTION;
    }

    if (global) {
        trapvector = &GlobalTrapVector;
    } else {
        if (spaces[ctx] == NULL)
            instantiatespace(ctx);
        trapvector = spaces[ctx]->trapvector;

        if (trapvector == NULL) {
            if (!portal) return STATUS_SUCCESS;
            trapvector = spaces[ctx]->trapvector = instantiatetrapvector();
        }
    }

    if (portal && traptype != portal->type)
        return STATUS_ILLEGAL_INSTRUCTION;

    switch (traptype) {
    case trapemul:
        // for emulation 'traps' the portal only controls access
        pportal = trapvector->emulation;
        break;

    case trapserv:
        pportal = trapvector->servicecall;
        break;

    case trapintr:
        pportal = trapvector->interrupts;
        break;

    case trapexcept:
        pportal = trapvector->exceptions;
        break;

    case trapfault:
        pportal = trapvector->faults;
        indexbase = indexlimit = TRAPFAULT_ACCESS;
        break;

    case trapvaddr:
        if (indexbase >= indexlimit)
            return STATUS_ILLEGAL_INSTRUCTION;

        pportal = InsertVAD(trapvector, indexbase, indexlimit);
        if (!pportal)
            return STATUS_ILLEGAL_INSTRUCTION;
        if (*pportal)
            dereferenceportal(*pportal);
        referenceportal(portal);
        *pportal = portal;
        return STATUS_SUCCESS;

    default:
        // should not be reachable since we already checked
        return STATUS_ILLEGAL_INSTRUCTION;
    }

    i = indexbase;
    do {
        if (pportal[i])
            dereferenceportal(pportal[i]);

        referenceportal(portal);
        pportal[i] = portal;
    } while (indexlimit && ++i < indexlimit);

    return STATUS_SUCCESS;
}


Portal *TrapToPortal(
        ULONG       ctx,
        ULONG       mode,
        Traptype    traptype,
        ULONG       trapindex
    )
{
    Trapvector  *glob = &GlobalTrapVector;
    Trapvector  *loc  = spaces[ctx]->trapvector;
    Portal      *defportal;
    Portal      *portal;
    Portal     **pportal;

//STRACE(" [ctx/mode loc traptype trapindex]", MASH(ctx,mode), loc, traptype, trapindex)
    if (!loc)
        loc  = spaces[ctx]->trapvector = instantiatetrapvector();

    defportal = loc->exceptions[TRAPEXCEPT_ILLEGALOP];
    if (defportal == NULL)
        defportal = glob->exceptions[TRAPEXCEPT_ILLEGALOP];

    defportal || BUGCHECK("LookupPortal no illegalop portal");

    portal = NULL;

    switch (traptype) {
    case trapemul:
        if (trapindex >= trapvectorlimits[traptype])
            break;

        portal = loc->emulation[trapindex];
        if (portal == NULL)
            portal = glob->emulation[trapindex];
        break;

    case trapserv:
        if (trapindex >= trapvectorlimits[traptype])
            break;

        portal = loc->servicecall[trapindex];
        if (portal == NULL)
            portal = glob->servicecall[trapindex];
        break;

    case trapintr:
        if (trapindex >= trapvectorlimits[traptype])
            break;

        portal = loc->servicecall[trapindex];
        if (portal == NULL)
            portal = glob->servicecall[trapindex];
        break;

    case trapexcept:
        if (trapindex >= trapvectorlimits[traptype])
            break;

        portal = loc->exceptions[trapindex];
        if (portal == NULL)
            portal = glob->exceptions[trapindex];
        break;

    case trapvaddr:
        if (trapindex < spaceparams.vpage_base || spaceparams.vpage_limit < trapindex)
            break;

        portal = NULL;
        pportal = LookupVAD(loc, trapindex);
        if (!pportal)
            pportal = LookupVAD(glob, trapindex);
        if (pportal) {
            portal = *pportal;
        }
        if (portal)
            break;

        // fall through ...

    case trapfault:
        if (trapindex >= trapvectorlimits[traptype])
            break;

        // simplify by mapping all faults into TRAPFAULT_ACCESS
        portal = loc->faults[TRAPFAULT_ACCESS];
        if (!portal)
            portal = glob->faults[TRAPFAULT_ACCESS];
        break;

    default:
        BUGCHECK("LookupPortal bad calltype");
        //notreached
        return NULL;
    }

    if (!portal) {
        return defportal;
    }

    if (ACCESSOK(portal->protmask, mode)) {
        return portal;
    }
    
    portal = loc->exceptions[TRAPEXCEPT_ACCESSDENIED];
    if (portal == NULL)
        portal = glob->exceptions[TRAPEXCEPT_ACCESSDENIED];

    if (portal == NULL)
        portal = defportal;

dump_Portal(portal, -1, "ACCESSDENIED");
    return portal;
}

void
cleantrapvector(
        Trapvector *trapvector
    )
{
    PVOID ptr;

    if (trapvector == NULL)
        return;

    while (! RtlIsGenericTableEmptyAvl(&trapvector->vadtree)) {
        ptr = RtlGetElementGenericTableAvl(&trapvector->vadtree, 0);
        (void) RtlDeleteElementGenericTableAvl(&trapvector->vadtree, ptr);
    }

    free(trapvector);
}

//
// VADTree routines for mapping Virtual Pages to Portals
//

typedef struct {
    ULONG       VirtualPageBase;
    ULONG       VirtualPageLimit;
    Portal     *Portal;
} VADNode;

RTL_GENERIC_COMPARE_RESULTS
NTAPI
VADCompare(
        PRTL_AVL_TABLE  pVadTable,
        PVOID           pFirst,
        PVOID           pSecond
    )
{
    VADNode *pFirstVad  = pFirst;
    VADNode *pSecondVad = pSecond;

    if (pFirstVad->VirtualPageLimit <= pSecondVad->VirtualPageBase)
        return GenericLessThan;
    if (pSecondVad->VirtualPageLimit <= pFirstVad->VirtualPageBase)
        return GenericGreaterThan;

    // else ranges overlap
    return GenericEqual;
}

PVOID
NTAPI
VADAlloc(
        struct _RTL_AVL_TABLE *Table,
        CLONG ByteSize
    )
{
    VADNode *v = malloc(ByteSize);

    if (v) {
        v->VirtualPageBase  = 0;
        v->VirtualPageLimit = 0;
        v->Portal           = NULL;
    }
    return v;
}

VOID
NTAPI
VADFree(
        struct _RTL_AVL_TABLE *Table,
        PVOID Buffer
    )
{
    free(Buffer);
}

VOID
InitVADTree(
        Trapvector *pTrapvector
    )
{
   RtlInitializeGenericTableAvl (&pTrapvector->vadtree, VADCompare, VADAlloc, VADFree, NULL);
}


Portal **
InsertVAD(
        Trapvector *pTrapvector,
        ULONG        VirtualPageBase,
        ULONG        VirtualPageLimit
    )
{
    BOOLEAN newflag;
    VADNode v;
    VADNode *p;

    v.VirtualPageBase  = VirtualPageBase;
    v.VirtualPageLimit = VirtualPageLimit;
    v.Portal           = NULL;

    p = RtlInsertElementGenericTableAvl (&pTrapvector->vadtree, &v, sizeof(v), &newflag);

    // if not a new element, then we had a conflict with an existing node
    // but if successful -- return the Portal in the allocated node, not our stack buffer!
    return newflag? &p->Portal  :  NULL;
}

Portal **
LookupVAD(
        Trapvector *pTrapvector,
        ULONG        VirtualPage
    )
{
    VADNode v;
    VADNode *p;

    v.VirtualPageBase   = VirtualPage;
    v.VirtualPageLimit  = VirtualPage + 1;
    v.Portal            = NULL;

    p = RtlLookupElementGenericTableAvl (&pTrapvector->vadtree, &v);

//STRACE("[trapvector vpage p p->Portal]", pTrapvector, VirtualPage, p, (p? &p->Portal : NULL))
    return p? &p->Portal : NULL;
}

BOOLEAN
DeleteVAD(
        Trapvector *pTrapvector,
        ULONG        VirtualPage
    )
{
    VADNode v;

    v.VirtualPageBase   = VirtualPage;
    v.VirtualPageLimit  = VirtualPage + 1;
    v.Portal            = 0;

    return RtlDeleteElementGenericTableAvl (&pTrapvector->vadtree, &v);
}

