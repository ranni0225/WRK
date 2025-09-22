//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include "Defs.h"

void
InterruptCPU(
        ULONG       cpu,
        USHORT      targetcpu,
        UCHAR       interrupt,
        UCHAR       irql
    )
{
    NTSTATUS s;
    InterruptMsg    im;

//STRACE("[cpu, targetcpu, interrupt, irql]", cpu, targetcpu, interrupt, irql)
    im.hdr.u1.s1.DataLength       = sizeof(InterruptMsg) - sizeof(PORT_MESSAGE);
    im.hdr.u1.s1.TotalLength      = sizeof(InterruptMsg);
    im.hdr.u2.s2.Type             = LPC_DATAGRAM;
    im.hdr.u2.s2.DataInfoOffset   = 0;

    im.interrupt = interrupt;
    im.irql      = irql;
    im.cpu       = targetcpu;

    s = NtRequestPort( spaceglobals.spaceport, (PPORT_MESSAGE)&im );
	FATALFAIL(s, "NtRequestPort");
}

ULONG
ManageIRQL(
        ULONG      cpu,
        IRQLOps    irqlop,
        UCHAR      irql
        )
{
    CPUHardware    *pcpu        = cpuhardware + cpu;
    InterruptItem  *item;
    InterruptItem **plink;
    InterruptItem **savedplink;
    UCHAR           oirql       = pcpu->irql;
    UCHAR           highestirql;
    IRQLOps         op = irqlop;

//STRACE("[cpu, irqlop, newirql, oldirql]", cpu, irqlop, irql, oirql)
    switch(op) {
    case raiseirql:
        if (irql < oirql) irql = oirql;     // will set irql to at least oirql
        break;
    case setirql:
        break;
    case lowerirql:
        if (irql > oirql) irql = oirql;     // will set irql to no more than oirql
        break;
    }

    pcpu->irql = irql;

    // if the IRQL did not go lower, then none of the pending interrupts
    // will fire, and we can just return
    if (irql >= oirql)
        return oirql;

    // search for the highest-priority queued interrupt deepest in queue.  if we find one that 
    // can be interrupted we will prevent any code running before the interrupt is delivered
    plink = &pcpu->queuehead;
    savedplink = NULL;
    highestirql = 0;
    while ((item = *plink)) {
        if (item->irql >= highestirql) {
            highestirql = item->irql;
            savedplink = plink;
        }
        plink = &item->link;
    }

    // if we found one, dequeue and set it as the pending interrupt on the current CPU
    if (savedplink) {
        item = *savedplink;
        *savedplink = item->link;
        item->link = NULL;
        pcpu->pendinginterrupt = item;
    }

    return oirql;
}

