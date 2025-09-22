//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include "Defs.h"

Hashentry *NTThreadHash[511];

NTThreadDescr *LookupNTThread(
        HANDLE    ntthreadid
    )
{
    Hashentry *ptr = LookupHashEntry(NTThreadHash, ASIZEOF(NTThreadHash), HANDLEVALUE(ntthreadid), NULL);

    return ptr?  CONTAINING_RECORD(ptr, NTThreadDescr, hash)  :  NULL;
}


CONTEXT ntstartcontext;

void
StartNTCPUThread(
        ULONG           cpu,
        NTThreadDescr  *ntthreaddescr,
        PCB            *pcb
    )
{
    Hashentry                  **prevlink;
    Hashentry                   *ptr;
    Domain                      *domain;
    HANDLE                       ntcputhread = NULL;
    OBJECT_ATTRIBUTES            obja;
    INITIAL_TEB                  teb = { 0 };
    CLIENT_ID                    cid;
    NTSTATUS                     s;
    UNICODE_STRING               imgname;
    UNICODE_STRING               imgpath;
    UNICODE_STRING               dllpath;
	PRTL_USER_PROCESS_PARAMETERS processparameters;
	RTL_USER_PROCESS_INFORMATION processinformation={0};
    CPUHardware                 *pcpu = cpuhardware +cpu;

//STRACE("[cpu,ctx,mode,eip]", cpu, pcb->oldctx, pcb->oldmode, pcb->ntcontext.Eip)

    domain = spaces[pcb->oldctx]->domains[pcb->oldmode];

    if (domain->cpus[cpu]->ntcputhread) {
        BUGCHECK("CPU thread already exists");
    }

    if (domain->ntprocess == NULL) {
        // create process and first CPU thread for domain
        s = RtlDosPathNameToNtPathName_U(L"bootstrap.exe", &imgpath, NULL, NULL);
        FATALFAIL(s, "RtlDosPathnameToNtPathName_U");

        RtlInitUnicodeString(&imgname, L"bootstrap.exe");
        RtlInitUnicodeString(&dllpath, L"c:\\");
        s = RtlCreateProcessParameters(&processparameters, &imgname, &dllpath, 0, 0, 0, 0, 0, 0, 0);
        FATALFAIL(s, "RtlCreateProcessParameters");

        s = RtlCreateUserProcess(&imgpath,
                OBJ_CASE_INSENSITIVE,
                processparameters,
                NULL,
                NULL,
                NULL,
                FALSE,
                NULL,
                // in Windows XP we can provide the exception port here, and bypass the TCB privilege check
                spaceglobals.spaceport,
                &processinformation
            );
        FATALFAIL(s, "RtlCreateUserProcess");

        // We will fault in the missing pages on demand

        domain->ntprocess = processinformation.Process;
        ntcputhread = processinformation.Thread;
        cid = processinformation.ClientId;

        if (ntstartcontext.Eip == 0) {
            ntstartcontext.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL | CONTEXT_SEGMENTS;
            s = NtGetContextThread(ntcputhread, &ntstartcontext);
            FATALFAIL(s, "NtGetContextThread bootstrap");
        }

    } else {
        ntstartcontext.Eip   || BUGCHECK("StartNTCPU - ntstartcontext uninitialized");
        InitializeObjectAttributes(&obja, NULL, 0, NULL, NULL);
        s = NtCreateThread(&ntcputhread,
            THREAD_ALL_ACCESS,
            &obja,
            domain->ntprocess,
            &cid,
            &ntstartcontext,
            &teb,
            TRUE);

        FATALFAIL(s, "NtCreateThread");
    }

    domain->cpus[cpu]->ntcputhread   = ntcputhread;

    // hash ntcputhread into NTThreadHash table based on cid
    ptr = LookupHashEntry(NTThreadHash, ASIZEOF(NTThreadHash), HANDLEVALUE(cid.UniqueThread), &prevlink);
    if (ptr) BUGCHECK("StartNTCPU - CID already hashed");

    domain->cpus[cpu]->hash.value = HANDLEVALUE(cid.UniqueThread);
    domain->cpus[cpu]->hash.link = *prevlink;
    *prevlink = &domain->cpus[cpu]->hash;

    // run the thread, which should immediately call Resume
    pcpu->cpustate = Running;
    pcpu->currentchain = pcb;

    s = NtResumeThread(ntcputhread, NULL);
    FATALFAIL(s, "NtResumeThread");
}

//
//
//
NTSTATUS
StartCPU(
        ULONG       xxcpu,
        ULONG       boot_cpu,
        ULONG       boot_ctx,
        ULONG       boot_mode,
        ULONG_PTR   boot_pc,
        ULONG_PTR   boot_arg
        )
{
    CPUHardware    *pcpu;
    PCB            *pcb;

//STRACE("[cpu,ctx,mode,eip]", boot_cpu, boot_ctx, boot_mode, boot_pc)
    if (boot_cpu >= MAXCPUS) {
        return STATUS_ILLEGAL_INSTRUCTION;
    }

    pcpu = &cpuhardware[boot_cpu];

    if (pcpu->cpustate != Uninitialized && pcpu->cpustate != Halted) {
        return STATUS_ILLEGAL_INSTRUCTION;
    }

    pcb = malloc(sizeof(*pcb));
    pcb || FATALFAIL(STATUS_NO_MEMORY, "PCB in StartCPU");
    ZERO(pcb);

    pcpu->irql        = 0;

    pcb->oldctx        = boot_ctx;
    pcb->oldmode       = boot_mode;
    pcb->returnargflag = 0;
    pcb->ntcontext.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
    pcb->ntcontext.Eip = boot_pc;

    setselectors(&pcb->ntcontext);

    pcpu->currentchain = pcb;

    switch (pcpu->cpustate) {
        case Uninitialized: {
            NTThreadDescr  *ntthreaddescr;

            instantiatentthreaddescr(boot_ctx, boot_mode, boot_cpu);
            ntthreaddescr = spaces[boot_ctx]->domains[boot_mode]->cpus[boot_cpu];
            StartNTCPUThread(boot_cpu, ntthreaddescr, pcb);
            break;
        }

        case Halted:
            pcpu->cpustate = Running;
            (void) Resume(boot_cpu, 0);
            break;

        default:
            return STATUS_ILLEGAL_INSTRUCTION;
    }
    return STATUS_SUCCESS;
}

