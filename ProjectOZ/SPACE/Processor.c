//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include "Defs.h"

//
// read the faulting instruction and decode it
// e.g. for IA32 use combinations of lock prefix and ud2 and trailing data  (0xf0 and 0x0f 0x0b and 0xXXXX)
// [but could also use combinations involving other prefix groups, e.g. 0x66 and 0x67]
//      f0 0f 0b xx  -- HW emulation op xx
//      0f 0b xx xx  -- Service Call op xxxx
//

ULONG
getinstruction(
        HANDLE  process,
        PVOID   pc
    )
{
    TrapDecode  ti;
    ULONG       faultinstr = 0;
    ULONG       readsize = 0;
    NTSTATUS    s;

    ti.iwhole = 0;

    s = NtReadVirtualMemory(process, pc, &faultinstr, sizeof(faultinstr), &readsize);
    FATALFAIL(s, "NtReadVirtualMemory fault instruction");

           if ((faultinstr & 0xffffff) == 0x0b0ff0) {
        ti.skiplength = 4;
        ti.hwemulation = 1;
        ti.operation = (faultinstr >> 24);

    } else if ((faultinstr & 0xffff)   == 0x0b0f) {
        ti.skiplength = 4;
        ti.servicecall = 1;
        ti.operation = (faultinstr >> 16);

    } else {
        ti.illegalop = 1;
    }

    return  ti.iwhole;
}

VOID  setselectors(
        CONTEXT *pctx
    )
{
    if (!pctx->SegDs && (pctx->ContextFlags & CONTEXT_SEGMENTS) == CONTEXT_SEGMENTS) {
        pctx->SegGs = 0;
        pctx->SegFs = KGDT_R3_TEB   | 3;
        pctx->SegEs = KGDT_R3_DATA  | 3;
        pctx->SegDs = KGDT_R3_DATA  | 3;
    }

    if (!pctx->SegCs && (pctx->ContextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL) {
        pctx->SegSs = KGDT_R3_DATA  | 3;
        pctx->SegCs = KGDT_R3_CODE  | 3;
    }
}

void
get5args(
        ULONG     *args5,
        CONTEXT   *ctx
    )
{
    args5[0] = ctx->Ebx;
    args5[1] = ctx->Ecx;
    args5[2] = ctx->Edx;
    args5[3] = ctx->Edi;
    args5[4] = ctx->Esi;
//STRACE("[Ebx Ecx Edx Edi xEsi]", args5[0], args5[1], args5[2], args5[3])
}

void
ret5args(
        CONTEXT   *ctx,
        ULONG     *args5
    )
{
    ctx->Ebx = args5[0];
    ctx->Ecx = args5[1];
    ctx->Edx = args5[2];
    ctx->Edi = args5[3];
    ctx->Esi = args5[4];
//STRACE("[Ebx Ecx Edx Edi xEsi]", args5[0], args5[1], args5[2], args5[3])
}

void
set4args(
        CONTEXT   *ctx,
        ULONG     *args5
    )
{
    ctx->Ebx = args5[0];
    ctx->Ecx = args5[1];
    ctx->Edx = args5[2];
    ctx->Edi = args5[3];
//STRACE("[Ebx Ecx Edx Edi]", args5[0], args5[1], args5[2], args5[3])
}

void
retresultarg(
        CONTEXT   *ctx,
        ULONGLONG  resultarg
    )
{
    ULARGE_INTEGER x;
    x.QuadPart = resultarg;

    ctx->Eax = x.LowPart;
    ctx->Edx = x.HighPart;
//STRACE("[Eax Edx Eip Esp]", ctx->Eax, ctx->Edx, ctx->Eip, ctx->Esp)
}

void
retresultarg2(
        CONTEXT   *ctx,
        ULONG      resultarg_low,
        ULONG      resultarg_high
    )
{
    ctx->Eax = resultarg_low;
    ctx->Edx = resultarg_high;
//STRACE("[Eax Edx Eip Esp]", ctx->Eax, ctx->Edx, ctx->Eip, ctx->Esp)
}

ULONGLONG
makequad(ULONG a, ULONG b)
{
    ULARGE_INTEGER x;
    x.LowPart  = a;
    x.HighPart = b;
    return x.QuadPart;
}

ULONG
splitquad(PULONG ph, ULONGLONG v)
{
    ULARGE_INTEGER x;
    x.QuadPart = v;
    *ph = x.HighPart;
    return x.LowPart;
}

