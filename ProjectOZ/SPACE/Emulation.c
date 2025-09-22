//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include "Defs.h"

void
UnexpectedMessage(
        PORT_MESSAGE        *msg,
        EXCEPTION_MESSAGE   *xmsg,
        BOOLEAN              fatal
    )
{
    NTSTATUS s;

    printf("Unexpected message: type=%d ClientId=%d/%d (%x/%x)",
            msg->u2.s2.Type, msg->ClientId.UniqueProcess, msg->ClientId.UniqueThread,
                             msg->ClientId.UniqueProcess, msg->ClientId.UniqueThread);
    if (xmsg)
        printf("  Api %d ReturnedStatus %x FirstChance %d\n", xmsg->ApiNumber, xmsg->ReturnedStatus, xmsg->FirstChance);
    printf("\n");

    if (fatal)
        die("Unexpected message\n", NULL, 0);

    s = NtReplyPort(spaceglobals.spaceport, xmsg? (PPORT_MESSAGE)xmsg : msg);
    FATALFAIL(s, "UnexpectedExceptionMessage: NtReplyPort");
}


void EmulateTrap(ULONG cpu, ULONG xcode, ULONG arg0, ULONG arg1);

typedef enum {
    EMULRETURNING,
    EMULCOMPLETED,
    EMULILLEGAL,
    EMULDENIED,
    EMULINTERRUPTPENDING
} EMULRESULT;

EMULRESULT EmulateHardware(NTThreadDescr *ntthreaddescr, ULONG trapdecodeiwhole);

//
//
//
UCHAR msgbuffer[256];

void
SPACEEmulation()
{
    PPORT_MESSAGE       msg = (PPORT_MESSAGE)msgbuffer;
    NTSTATUS            s;

    for (;;) {
		s = NtReplyWaitReceivePort(spaceglobals.spaceport, NULL, NULL, msg);
        FATALFAIL(s, "NtReplyWaitReceivePort");

//------- DEBUG
if (1) {
 EXCEPTION_MESSAGE *x = (EXCEPTION_MESSAGE *)msg;
 EXCEPTION_RECORD  *r = &x->ExceptionRecord;
 NTThreadDescr     *t;
 NTSTATUS           s;
 CONTEXT            ntctx;
 int icpu = -1;

 t = LookupNTThread(msg->ClientId.UniqueThread);
 if (t) icpu = t->cpu;

 printf("\nTOP-OF-LOOP:  CPU %2d. msg type %x from CIDs %d./%d.  <X: code/addr %08x/%08x  info[] %08x/%08x>\n",
    icpu, msg->u2.s2.Type, msg->ClientId.UniqueProcess, msg->ClientId.UniqueThread,
    r->ExceptionCode, r->ExceptionAddress, r->ExceptionInformation[0], r->ExceptionInformation[1]);

 ntctx.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
 if (t && t->ntcputhread) {
   s = NtGetContextThread(t->ntcputhread, &ntctx);
   if (NT_SUCCESS(s)) {
     printf(" REGISTERS: eip=%x esp/ebp=%x/%x eax=%x ebx=%x ecx=%x edx=%x edi=%x esi=%x\n",
         ntctx.Eip, ntctx.Esp, ntctx.Ebp, ntctx.Eax, ntctx.Ebx, ntctx.Ecx, ntctx.Edx, ntctx.Edi, ntctx.Esi);
     dump_CONTEXT(&ntctx, "LPC_EXCEPTION");
   }
 }
}
//-------
        //
        // Figure out what kind of message this is, and update the CPUHardware[] state appropriately
        //
        switch (msg->u2.s2.Type) {
		case LPC_EXCEPTION:
        {
  			EXCEPTION_MESSAGE *xmsg;
            EXCEPTION_RECORD  *xrec;
            CPUHardware       *pcpu;

            NTThreadDescr     *ntthreaddescr;
            ULONG              cpu;
            ULONG_PTR          arg0, arg1;

  			xmsg = (EXCEPTION_MESSAGE *) msg;
  			xmsg->ReturnedStatus = STATUS_SUCCESS;

            xrec = &xmsg->ExceptionRecord;

            // Figure out which CPU from the thread id
            ntthreaddescr = LookupNTThread(xmsg->hdr.ClientId.UniqueThread);
            if (ntthreaddescr == NULL) {
                UnexpectedMessage(msg, xmsg, FALSE);
                continue;
            }

            cpu = ntthreaddescr->cpu;
            pcpu = cpuhardware + cpu;

            // ASSERT(spaces[pcpu->ctx]->domains[pcpu->mode]->cpus[cpu]->hash.value == (ULONGLONG)xmsg->hdr.ClientId.UniqueThread);
            if (spaces[pcpu->ctx]->domains[pcpu->mode]->cpus[cpu]->hash.value != HANDLEVALUE(xmsg->hdr.ClientId.UniqueThread))
                FATALFAIL(STATUS_UNSUCCESSFUL, "clientid check");

            ntthreaddescr->ntreplymessage = *xmsg;
            ntthreaddescr->trappednotinterrupted = 1;

            switch (xmsg->ApiNumber) {
            case 0: // DbgKmExceptionApi
                // If Interrupted, then we have already handed the cpu to an interrupt
                // Just ignore this message and when the interrupt resumes the instruction
                // that caused this message will re-execute and cause it again.
                // Restarting is idempotent, but we don't want to fail to make progress or do this excessively
                // so we only retry the first interrupt, and only if the current EIP is the same as at the time of the interrupt.
                if (ntthreaddescr->trappednotinterrupted == 0 && ntthreaddescr->interrupteip) {
                    CONTEXT ntcontext;
                    ntcontext.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;

                    pcpu->currentchain || BUGCHECK("NtGetContextThread for msg");
                    s = NtGetContextThread(ntthreaddescr->ntcputhread, &ntcontext);
                    FATALFAIL(s, "NtGetContextThread for msg");

                    (ntthreaddescr->interrupteip == pcpu->currentchain->ntcontext.Eip) || BUGCHECK("interrupteip");
                    if (ntthreaddescr->interrupteip == ntcontext.Eip) {
                        ntthreaddescr->interrupteip = 0;
                        s = NtReplyPort(spaceglobals.spaceport, (PPORT_MESSAGE)xmsg);
                        FATALFAIL(s, "SPACEEmulation: NtReplyPort");
                        continue;
                    }
                }
                ntthreaddescr->interrupteip = 0;

                switch (xrec->ExceptionCode) {
                case STATUS_INVALID_LOCK_SEQUENCE:  // SPACE Hardware Emulation
                    {
                        TrapDecode      trapdecode;

                        trapdecode.iwhole = getinstruction(ntthreaddescr->domain->ntprocess, xrec->ExceptionAddress);

                        // EmulateHardware
                        if (trapdecode.hwemulation) {
                            EMULRESULT er = EmulateHardware(ntthreaddescr, trapdecode.iwhole);
                            switch (er) {
                            case EMULCOMPLETED:
                                continue;

                            case EMULRETURNING:
                                ntthreaddescr->trappednotinterrupted  || BUGCHECK("EMULRETURNING from interrupt");
                                s = NtReplyPort(spaceglobals.spaceport, (PPORT_MESSAGE)xmsg);
                                continue;

                            case EMULINTERRUPTPENDING: {
                                    InterruptItem *item = pcpu->pendinginterrupt;
                                    pcpu->pendinginterrupt = NULL;

                                    // dispatch the pending interrupt (i.e we lowered the IRQL)
                                    EmulateTrap(cpu, STATUS_SPACE_INTERRUPT, item->interrupt, item->irql);
                                    free(item);
                                    continue;
                                }
                                continue;

                            case EMULDENIED:
                                // EmulateTrap
                                EmulateTrap(cpu, STATUS_ACCESS_DENIED, trapdecode.iwhole, 0);
                                continue;

                            case EMULILLEGAL:
                                // EmulateTrap
                                EmulateTrap(cpu, STATUS_ILLEGAL_INSTRUCTION, trapdecode.iwhole, 0);
                                continue;
                            }
                            BUGCHECK("NOT REACHABLE - HW Emulation");
                        }

                        // EmulateTrap for real illegal instruction
                        EmulateTrap(cpu, xrec->ExceptionCode, trapdecode.iwhole, 0);
                    }
                    continue;

                case STATUS_ILLEGAL_INSTRUCTION:
                    {
                        TrapDecode      trapdecode;

                        trapdecode.iwhole = getinstruction(ntthreaddescr->domain->ntprocess, xrec->ExceptionAddress);
                        if (trapdecode.servicecall)
                            EmulateTrap(cpu, xrec->ExceptionCode, trapdecode.iwhole, 0);
                        else
                            EmulateTrap(cpu, xrec->ExceptionCode, 0, 0);
                    }
                    continue;

                //
                // XXX: For future implementation of CC breakpoints
                // XXX: will skip breakpoints, so only for explicitly coded breakpoints (at present)
                //
                case STATUS_BREAKPOINT:
                {
                    CONTEXT tcontext = { 0 };
                    tcontext.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
                    s = NtGetContextThread(ntthreaddescr->ntcputhread, &tcontext);
                    FATALFAIL(s, "NtGetContextThread at breakpoint");
                    dump_CPUHardware(pcpu, "BREAKPOINT CPUHardware");
                    dump_CONTEXT(&tcontext, "BREAKPOINT tcontext");

                    SPACEBREAK("breakpoint", NULL);
                    tcontext.Eip++;     // skip explicitly coded breakpoint
                    s = NtSetContextThread(ntthreaddescr->ntcputhread, &tcontext);
                    FATALFAIL(s, "NtSetContextThread at breakpoint");
                    continue;
                }

                case STATUS_ACCESS_VIOLATION:
                    arg0 = xrec->ExceptionInformation[0];
                    arg1 = xrec->ExceptionInformation[1];
//------- DEBUG
STRACE("LPC_EXCEPTION: ACCESS_VIOLATION [xcode xaddr x0 x1]", xrec->ExceptionCode, xrec->ExceptionAddress,  arg0, arg1)
STRACE("                       current: [cpu ctx mode irql]", cpu, pcpu->ctx, pcpu->mode, pcpu->irql)
if (1) {
    CONTEXT tcontext = { 0 };
    tcontext.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
    s = NtGetContextThread(ntthreaddescr->ntcputhread, &tcontext);
    FATALFAIL(s, "NtGetContextThread at breakpoint");
    dump_CPUHardware(pcpu, "LPC_EXCEPTION CPUHardware");
    dump_CONTEXT(&tcontext, "LPC_EXCEPTION context");
}
SPACEBREAK("ACCESS VIOLATION", NULL);
//-------
                    EmulateTrap(cpu, xrec->ExceptionCode, arg0, arg1);
                    continue;

                default: 
                    UnexpectedMessage(msg, xmsg, FALSE);
                    continue;
                }
                BUGCHECK("NOT REACHABLE - xcode");
                continue;

            default:
                UnexpectedMessage(msg, xmsg, FALSE);
                continue;
            }
            BUGCHECK("NOT REACHABLE - xcode");
            continue;
        }
        BUGCHECK("NOT REACHABLE - LPC_EXCEPTION");
        continue;

		case LPC_DATAGRAM:      // used for delivering interrupts to a CPU
        {
            NTThreadDescr     *ntthreaddescr;
            InterruptMsg      *imsg             = (InterruptMsg *)msg;
            CPUHardware       *pcpu             = cpuhardware + imsg->cpu;

            ntthreaddescr = spaces[pcpu->ctx]->domains[pcpu->mode]->cpus[imsg->cpu];

            // suspend the thread
            // note that if it has already taken an exception and we just haven't seen the message
            // we'll ignore it when we do, and get it again when we re-execute the instruction later
            s = NtSuspendThread(ntthreaddescr->ntcputhread, NULL);
            FATALFAIL(s, "NtSuspendThread for msg");

            ntthreaddescr->trappednotinterrupted = 0;

            if (imsg->irql > pcpu->irql) {
                UCHAR oldirql = ManageIRQL(imsg->cpu, setirql, imsg->irql);
                EmulateTrap(imsg->cpu, STATUS_SPACE_INTERRUPT, imsg->interrupt, oldirql);
                (void) ManageIRQL(imsg->cpu, setirql, oldirql);
                !pcpu->pendinginterrupt  || BUGCHECK("pending interrupt within LPC_DATAGRAM processing");

            } else {
                InterruptItem  **queue = &pcpu->queuehead;
                InterruptItem   *item;

                // push interrupt onto CPU's queue for later dispatch
                item = malloc(sizeof(*item));
                item || FATALFAIL(STATUS_NO_MEMORY, "InterruptQueueItem");
                ZERO(item);

                item->irql = imsg->irql;
                item->interrupt = imsg->interrupt;
                item->link = *queue;
                *queue = item;
            }
        }
        continue;

        case LPC_CLIENT_DIED:
        case LPC_ERROR_EVENT:
        default:
            UnexpectedMessage(msg, NULL, FALSE);
            continue;
        }
        BUGCHECK("NOT REACHABLE - LPC_EXCEPTION");
        continue;
    }
}

//
// Free any resources that will not be released with our demise
//
void
cleanup()
{
    ULONG ctx;

    // kill all the processes we have created
    for (ctx = 0;  ctx <= spaceglobals.highestctx;  ctx++)
        CleanCtx(0, ctx);
}

//
// handle a trap/exception/fault/interrupt other than HW emulations
//
void
EmulateTrap(
        ULONG       cpu,
        ULONG       xcode,
        ULONG       arg0,
        ULONG       arg1
    )
{
    CPUHardware    *pcpu;
    ULONG           ctx, mode;
    PCB            *pcb;
    PCB            *temppcb;
    NTSTATUS        s;
    Portal         *portal;
    TrapDecode      trapdecode;
    NTThreadDescr  *oldntthreaddescr;
    NTThreadDescr  *newntthreaddescr;

    ULONG           oldirql;
    ULONG           newirql;
    ULONG           eipadjust       = 0;
    ULONG           faultinstr      = 0;
    ULONG           interruptcall   = 0;
    ULONG           servicetrap     = 0;
    ULONG           accessviolation = 0;
    ULONG           trapindex;

    PCBCalltype     calltype        = unknowncall;
    CONTEXT         newntcontext    = { 0 };
    ULONG           arg[5];

    arg[0] = arg[1] = arg[2] = arg[3] = arg[4] = 0;

    pcpu    = cpuhardware + cpu;
    ctx     = pcpu->ctx;
    mode    = pcpu->mode;
    oldirql = pcpu->irql;
    newirql = oldirql;

    // assertion
       spaces[ctx]
    && spaces[ctx]->domains[mode]
    && spaces[ctx]->domains[mode]->cpus[cpu]
    && spaces[ctx]->domains[mode]->cpus[cpu]->ntcputhread
    || BUGCHECK("internal EmulateTrap");

    oldntthreaddescr =  spaces[ctx]->domains[mode]->cpus[cpu];

    pcb = malloc(sizeof(*pcb));
    pcb || FATALFAIL(STATUS_NO_MEMORY, "PCB");
    ZERO(pcb);

    pcb->ntcontext.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
    s = NtGetContextThread(oldntthreaddescr->ntcputhread, &pcb->ntcontext);
    FATALFAIL(s, "NtGetContextThread for msg");

    switch (xcode) {
    case STATUS_SPACE_INTERRUPT:
        calltype          = interruptcall;
        trapindex         = arg0;                 // interrupt
        newirql           = arg1;

        (newirql > oldirql) || BUGCHECK("EmulateTrap: SPACE_INTERRUPT:  lowering IRQL");

        pcpu->irql = (UCHAR)newirql;

        // if we got here through a real interrupt, record Eip -- in case
        // we get an LPC trap message sent before we suspended the thread
        if (oldntthreaddescr->trappednotinterrupted == 0)
            oldntthreaddescr->interrupteip = pcb->ntcontext.Eip;
        portal = TrapToPortal(ctx, mode, trapintr, trapindex);
        break;

    case STATUS_ILLEGAL_INSTRUCTION:
    case STATUS_INVALID_LOCK_SEQUENCE:
        calltype          = servicetrap;
        trapdecode.iwhole = arg0;

        if (!trapdecode.servicecall) {
            portal = TrapToPortal(ctx, mode, trapexcept, TRAPEXCEPT_ILLEGALOP);
            break;
        }

        // system service request
        eipadjust         = trapdecode.skiplength;
        trapindex         = trapdecode.operation;   // system service index

        get5args(arg, &pcb->ntcontext);

        portal = TrapToPortal(ctx, mode, trapserv, trapindex);
        arg[3]            = portal->arg;
        set4args(&newntcontext, arg);
        break;

    case STATUS_ACCESS_VIOLATION:
        calltype          = accessviolation;
        eipadjust         = 0;                    // re-execute faulting instruction

        arg[0]            = arg0;                 // EXCEPTION_{READ,WRITE,EXECUTE}_FAULT
        arg[1]            = arg1;                 // faulting address

        portal = TrapToPortal(ctx, mode, trapvaddr, arg1);
        arg[2]            = portal->arg;

        set4args(&newntcontext, arg);
        break;

    case STATUS_ACCESS_DENIED:
        eipadjust         = 0;                    // re-execute faulting instruction
        portal = TrapToPortal(ctx, mode, trapexcept, TRAPEXCEPT_ACCESSDENIED);
        break;
    }

    pcb->pcbcalltype      = calltype;

    pcb->oldctx           = ctx;
    pcb->oldmode          = mode;
    pcb->oldirql          = oldirql;

    pcb->chainlink        = pcpu->currentchain;
    pcpu->currentchain    = pcb;
    pcpu->ctx             = portal->ctx;
    pcpu->mode            = (UCHAR)portal->mode;
    pcpu->irql            = (UCHAR)newirql;

    pcb->ntcontext.Eip   += eipadjust;

    portal || BUGCHECK("internal EmulateTrap with no portal");

    // get a CONTEXT structure by allocating a PCB, in case we need to start a new NT thread
    temppcb = malloc(sizeof(*temppcb));
    temppcb || FATALFAIL(STATUS_NO_MEMORY, "PCB");
    ZERO(temppcb);

    temppcb->ntcontext.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
    temppcb->ntcontext.Eip = portal->handler;

    setselectors(&temppcb->ntcontext);

    // find the NT thread for the current CPU in the domain we are returning to
    // it won't exist if nothing has ever run there before
    instantiatentthreaddescr(portal->ctx, portal->mode, cpu);
    newntthreaddescr      = spaces[portal->ctx]->domains[portal->mode]->cpus[cpu];

    if (newntthreaddescr->ntcputhread == NULL) {
        // Starting the CPU will push pcb onto the chain and then resume it
        StartNTCPUThread(cpu, newntthreaddescr, temppcb);
        return;
    }

    // restore context from temporary PCB onto an existing ntcputhread
    s = NtSetContextThread( newntthreaddescr->ntcputhread, &temppcb->ntcontext );
    FATALFAIL(s, "NtSetContextThread portal traversal");

    // free the temporary pcb
    free(temppcb);
//------- DEBUG
STRACE("Portal Traversal [portal pcb newEip trappednotinterrupted] (dumping)", portal, pcb, portal->handler, newntthreaddescr->trappednotinterrupted);
dump_CPUHardware(pcpu, "Portal Traversal: CPU context including old (PCB) CONTEXT");
dump_CONTEXT(&newntcontext, "Portal Traversal:  new CPU context");
//-------
    if (newntthreaddescr->trappednotinterrupted) {
        s = NtReplyPort(spaceglobals.spaceport, (PPORT_MESSAGE)&newntthreaddescr->ntreplymessage);
        FATALFAIL(s, "NtReplyPort portal traversal");
    } else {
        s = NtResumeThread(newntthreaddescr->ntcputhread, NULL);
        FATALFAIL(s, "NtResumeThread portal traversal");
    }
}

EMULRESULT
EmulateHardware(
        NTThreadDescr     *ntthreaddescr,
        ULONG              trapdecodeiwhole
        )
{
    CONTEXT         ntcontext       = { 0 };
    NTSTATUS        status;

    Portal         *portal;
    TrapDecode      trapdecode;
    ULONG           eipadjust       = 0;
    ULONG           trapindex;
    ULONG           arg[5];
    ULONGLONG       rvalue;
    ULONG           cpu             = ntthreaddescr->cpu;
    CPUHardware    *pcpu            = cpuhardware + cpu;

    arg[0] = arg[1] = arg[2] = arg[3] = arg[4] = 0;

    trapdecode.iwhole = trapdecodeiwhole;
    eipadjust = trapdecode.skiplength;
    trapindex = trapdecode.operation;   // HW emulation index

    ntcontext.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
    status = NtGetContextThread(ntthreaddescr->ntcputhread, &ntcontext);
    FATALFAIL(status, "NtGetContextThread for hw emul");

    get5args(arg, &ntcontext);
    ntcontext.Eip   += eipadjust;

    portal = TrapToPortal(pcpu->ctx, pcpu->mode, trapemul, trapindex);

    // permission was denied
    if (! ACCESSOK(portal->protmask, pcpu->mode))
        return EMULDENIED;

    status = STATUS_SUCCESS;

    switch (trapindex) {
    case TRAPEMUL_Noop:
//STRACE("TRAPEMUL_Noop", 0, 0, 0, trapindex)
        break;

    case TRAPEMUL_MapMemory:
        {   XMapMemory *u = (XMapMemory *) arg;
            status = MapMemory(cpu, u->ctx, u->vpage, u->ppage, u->readmask, u->writemask);
        }
        break;

    case TRAPEMUL_MapIO:
        {   XMapIO *u = (XMapIO *) arg;
            status = MapIO(cpu, u->deviceid, u->writable, u->iospacepage, u->physpage);
        }
        break;

    case TRAPEMUL_MapTrap:
        {   XMapTrap *u = (XMapTrap *) arg;
            status = MapTrap(cpu, u->ctx, u->traptype, u->global, u->indexbase, u->indexlimit, u->portalid);
        }
        break;

    case TRAPEMUL_CreatePortal:
        {   XCreatePortal *u = (XCreatePortal *) arg;
            rvalue = CreatePortal(cpu, u->type, u->mode, u->irql, u->ctx, u->handler, u->protmask);
        }
        retresultarg(&ntcontext, rvalue);
        break;

    case TRAPEMUL_DestroyPortal:
        status = DestroyPortal(cpu, arg[0]);
        break;

    case TRAPEMUL_CleanCtx:
        status = CleanCtx(cpu, arg[0]);
        break;

    case TRAPEMUL_Resume:
        status = Resume(cpu, makequad(arg[0], arg[1]));
        return NT_SUCCESS(status)?  EMULCOMPLETED : EMULILLEGAL;

    case TRAPEMUL_Suspend:
        rvalue = Suspend(cpu);
        retresultarg(&ntcontext, rvalue);
        break;

    case TRAPEMUL_Unsuspend:
        status = Unsuspend(cpu, makequad(arg[0], arg[1]));
        break;

    case TRAPEMUL_PopCaller:
        status = PopCaller(cpu);
        break;

    case TRAPEMUL_DiscardToken:
        status = DiscardToken(cpu, makequad(arg[0], arg[1]));
        break;

    case TRAPEMUL_StartCPU:
        {   XStartCPU *u = (XStartCPU *) arg;
            status = StartCPU(cpu, u->boot_cpu, u->boot_ctx, u->boot_mode, u->boot_pc, u->boot_arg);
        }
        break;

    case TRAPEMUL_InterruptCPU:
        {   XInterruptCPU *u = (XInterruptCPU *) arg;
            InterruptCPU(cpu, u->targetcpu, u->interrupt, u->irql);
        }
        break;

    case TRAPEMUL_AccessDevice:
        {   XAccessDevice *u = (XAccessDevice *) arg;
            rvalue = AccessDevice(cpu, u->deviceid, u->op, u->deviceregister, u->value, u->valuex);
        }
        retresultarg(&ntcontext, rvalue);
        break;

    case TRAPEMUL_ManageIRQL:
        {   XManageIRQL *u = (XManageIRQL *) arg;
            rvalue = ManageIRQL(cpu, u->irqlop, u->irql);
            if (cpuhardware[cpu].pendinginterrupt)
                EMULINTERRUPTPENDING;
        }
        retresultarg(&ntcontext, rvalue);
        break;

    case TRAPEMUL_HaltCPU:
        if (cpuhardware[cpu].currentchain)
            return EMULILLEGAL;
        printf("Processor halted: %08x\n", arg[0]);
        return EMULCOMPLETED;

    case TRAPEMUL_GetSPACEParams:
        {   XSPACEParams *u = (XSPACEParams *) arg;
            u->params = spaceparams;
            ret5args(&ntcontext, u->registers);
        }
        break;

    case TRAPEMUL_Break:
        {   XSPACEBreak *u = (XSPACEBreak *) arg;
            printf("Break: %08x: %.16s\n", u->breakvalue, u->breakmsg);
            SPACEBREAK("<break-in>", NULL);
        }
        break;

    default:
        return EMULILLEGAL;
    }

    if (!NT_SUCCESS(status))
        return EMULILLEGAL;

    // returning result
    status = NtSetContextThread(ntthreaddescr->ntcputhread, &ntcontext);
    FATALFAIL(status, "NtSetContextThread for SPACE hardware emulation");

    return EMULRETURNING;
}

