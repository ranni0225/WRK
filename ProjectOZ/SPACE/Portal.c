//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include "Defs.h"

Hashentry *TokenHash[1023];
SPACETOKEN mastertoken = 1;

ULONGLONG   makequad    (ULONG a,   ULONG b);
ULONG       splitquad   (PULONG ph, ULONGLONG v);

Hashentry *PortalHash[4095];
ULONGLONG  nextportalid = 1;

SPACETOKEN
Suspend(
        ULONG       cpu
    )
{
    Hashentry **prevlink;
    PCB        *pcb         = cpuhardware[cpu].currentchain;
    ULONG       ownctx      = cpuhardware[cpu].ctx;
    ULONG       ownmode     = cpuhardware[cpu].mode;

    cpuhardware[cpu].currentchain = NULL;

    if (!pcb) return 0;

    pcb->hash.value = mastertoken++;
    prevlink = TokenHash + HASH(pcb->hash.value, ASIZEOF(TokenHash));

    pcb->hash.link = *prevlink;
    *prevlink = &pcb->hash;

    pcb->tokenallocated = 1;
    pcb->tokenowner_ctx   = ownctx;
    pcb->tokenowner_mode  = ownmode;

STRACE("[cpu, token, ownctx, ownmode]", cpu, (ULONG)pcb->hash.value, ownctx, ownmode)
    return pcb->hash.value;
}

NTSTATUS
Unsuspend(
        ULONG       cpu,
        SPACETOKEN  token
    )
{
    Hashentry  **prevlink = NULL;
    Hashentry   *ptr;
    PCB         *pcb = NULL;
    PCB         *chainpcb;

    // Find the chain for the token and make it the current one
    // If we already had a chain, we discard it
    // TODO: should it be an exception to find the current chain is not empty?

    ptr = LookupHashEntry(TokenHash, ASIZEOF(TokenHash), token, &prevlink);

    if (ptr == NULL)
        return STATUS_ILLEGAL_INSTRUCTION;

    // remove the pcb from the tokenhash
    *prevlink = pcb->hash.link;

    // discard the current chain
    chainpcb = cpuhardware[cpu].currentchain;
    while (chainpcb) {
        PCB *tpcb = chainpcb;
        chainpcb = chainpcb->chainlink;
        free(tpcb);
    }

    // make the token chain the new current chain
    cpuhardware[cpu].currentchain = pcb;

//STRACE("[cpu, token, newpcb, neweip]", cpu, (ULONG)token, pcb, pcb->ntcontext.Eip)
    return STATUS_SUCCESS;
}


NTSTATUS
Resume(
        ULONG       cpu,
        ULONGLONG   result
    )
{
    CPUHardware   *pcpu           = cpuhardware + cpu;
    NTThreadDescr *ntthreaddescr;
    PCB           *pcb            = NULL;

    NTSTATUS     s;

    pcb = pcpu->currentchain;

    // check for errors
    if (pcb == NULL) {
        return STATUS_ILLEGAL_INSTRUCTION;
    }

    // pop the pcb off the chain
    pcpu->currentchain = pcb->chainlink;

    // set the IRQL, if changing
    // if any pending interrupts are deliverable, pcpu->pendinginterrupt will be set
    if (pcb->oldirql != pcpu->irql) {
        (void) ManageIRQL(cpu, setirql, (UCHAR)pcb->oldirql);
    }

    // return the result, if appropriate
    if (pcb->returnargflag) {
        retresultarg(&pcb->ntcontext, result);
    }

    // find the NT thread for the current CPU in the domain we are returning to
    // it won't exist if nothing has ever run there before
    instantiatentthreaddescr(pcb->oldctx, pcb->oldmode, cpu);

    ntthreaddescr = spaces[pcb->oldctx]->domains[pcb->oldmode]->cpus[cpu];
    if (ntthreaddescr->ntcputhread == NULL) {
        // Starting the CPU will push pcb onto the chain and then resume it
        StartNTCPUThread(cpu, ntthreaddescr, pcb);
        return STATUS_SUCCESS;
    }

    // restore context from PCB onto an existing ntcputhread
    s = NtSetContextThread( ntthreaddescr->ntcputhread, &pcb->ntcontext );
    FATALFAIL(s, "NtSetContextThread failed");

    // Cannot reply to the port if there is a pending interrupt, as we can't let the CPU run
    if (!pcpu->pendinginterrupt) {
        // reply to the last message
        s = NtReplyPort( spaceglobals.spaceport,
                         (PPORT_MESSAGE)&spaces[pcb->oldctx]->domains[pcb->oldmode]->cpus[cpu]->ntreplymessage );
        FATALFAIL(s, "NtReplyPort failed");

    // free the pcb
    free(pcb);
}

    return STATUS_SUCCESS;
}


NTSTATUS
DiscardToken(
        ULONG       cpu,
        SPACETOKEN  token
    )
{
    Hashentry   *ptr;
    Hashentry  **prevlink  = NULL;
    PCB         *pcb;

    // lookup the token
    ptr = LookupHashEntry(TokenHash, ASIZEOF(TokenHash), token, &prevlink);
    if (ptr == NULL) {
        return STATUS_ILLEGAL_INSTRUCTION;
    }

    pcb = CONTAINING_RECORD(ptr, PCB, hash);

STRACE("[cpu, token, pcb, pcbeip]", cpu, (ULONG)token, pcb, pcb->ntcontext.Eip)
    // remove the pcb from the tokenhash
    *prevlink = pcb->hash.link;

    // free all the PCBs in the chain
    while (pcb) {
        PCB *tpcb = pcb;
        pcb = pcb->chainlink;
        free(tpcb);
    }

    return STATUS_SUCCESS;
}

//
// Pop the top PCB from the current chain
//
NTSTATUS
PopCaller(
        ULONG    cpu
    )
{
    PCB *pcb = cpuhardware[cpu].currentchain;
    if (pcb == NULL) {
        return STATUS_ILLEGAL_INSTRUCTION;
    }
    cpuhardware[cpu].currentchain = pcb->chainlink;
//STRACE("[cpu, pcb, pcbeip, x]", cpu, pcb, pcb->ntcontext.Eip, 0)
    free(pcb);

    return STATUS_SUCCESS;
}

Portal *
allocateportal()
{
    Hashentry **prevlink;
    Portal     *portal;

    portal = malloc(sizeof(*portal));
    portal || FATALFAIL(STATUS_NO_MEMORY, "instantiateportal");
    ZERO(portal);

    portal->hash.value = nextportalid++;
    prevlink = PortalHash + HASH(portal->hash.value, ASIZEOF(PortalHash));

    portal->hash.link = *prevlink;
    *prevlink = &portal->hash;

    return portal;
}

void
referenceportal(
        Portal *portal
    )
{
    portal->refcount++;
}

void
dereferenceportal(
        Portal *portal
    )
{
    if (portal->refcount == 0)
        BUGCHECK("Under-referenced portal");

    --portal->refcount;
    if (portal->refcount == 0 && portal->destroyed) {
        free(portal);
    }
}


ULONG
CreatePortal(
        ULONG       cpu,
        UCHAR       type,
        UCHAR       mode,
        UCHAR       irql,
        ULONG       ctx,
        ULONG       handler,
        ULONG       protmask
    )
{

    Portal *portal = allocateportal();

    portal->type	    = type;
	portal->mode	    = mode;
	portal->irql	    = irql;
	portal->ctx	        = ctx;
	portal->handler	    = handler;
	portal->protmask    = protmask;

    if (portal->hash.value != (ULONG)portal->hash.value) BUGCHECK("portalid overflow");

//STRACE("[irql/type, ctx/mode, handler, portalid]", MASH(irql,type), MASH(ctx,mode), handler, (ULONG)portal->hash.value)
    return (ULONG)portal->hash.value;
}

Portal *
lookupportal(
        ULONG       portalid
    )
{
    Hashentry   *ptr = LookupHashEntry(PortalHash, ASIZEOF(PortalHash), portalid, NULL);
    return ptr?  CONTAINING_RECORD(ptr, Portal, hash)  :  NULL;
}

NTSTATUS
DestroyPortal(
        ULONG    cpu,
        ULONG    portalid
    )
{
    Hashentry  **prevlink  =  NULL;
    Hashentry   *ptr = LookupHashEntry(PortalHash, ASIZEOF(PortalHash), portalid, &prevlink);
    Portal      *portal;

//STRACE("[portalid, ptr, x, x]", portalid, ptr, 0, 0)
    if (ptr == NULL)
        return STATUS_INVALID_HANDLE;

    portal = CONTAINING_RECORD(ptr, Portal, hash);

    // remove the portal from the portalhash
    *prevlink = portal->hash.link;

    // mark as destroyed, and free if refcount is 0
    portal->destroyed = 1;
    if (portal->refcount == 0) {
        free(portal);
    }

    return STATUS_SUCCESS;
}

