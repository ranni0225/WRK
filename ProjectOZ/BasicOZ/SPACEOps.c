//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include "BasicOZ.h"

//
// HW Emulation calls
//
typedef ulong64 EMULscalar0  ();                            // common scalar cases
typedef ulong64 EMULscalar1  (ulong   arg0);
typedef ulong64 EMULscalar1x (ulong64 arg0);
typedef ulong64 EMULscalar2  (ulong   arg0,  ulong arg1);
typedef ulong64 EMULscalar5  (ulong   arg0,  ulong arg1,   ulong arg2,   ulong arg3,   ulong arg4);

typedef ulong   EMULvector   (ulong *args);                 // vector case
typedef void    EMULbidir    (ulong *args);                 // up to 5 ulongs in, 5 ulongs out

typedef union {
    ulong64 all;
    ulong   part[2];
} Split64;

EMULscalar0     trapNoop;
void
SPACE_Noop()
{
    (void) trapNoop();
}

EMULvector     trapMapMemory;
void
SPACE_MapMemory(
        ulong ctx,
        ulong vpage,
        ulong ppage,
        ulong readmask,
        ulong writemask
    )
{
    XMapMemory u = { ctx, vpage, ppage, readmask, writemask };
    (void) trapMapMemory(u.args);
}

EMULscalar1    trapCleanCtx;
void
SPACE_CleanCtx(
        ulong ctx
    )
{
    (void) trapCleanCtx(ctx);
}

EMULvector     trapCreatePortal;
ulong
SPACE_CreatePortal(
        uchar type,
        uchar mode,
        uchar irql,
        // fill
        ulong ctx,
        ulong handler,
        ulong protmask
    )
{
    XCreatePortal u = { type, mode, irql, 0, ctx, handler, protmask };
    return trapCreatePortal(u.args);
}

EMULscalar1x   trapDestroyPortal;
void
SPACE_DestroyPortal(
        ulong portalid
    )
{
    (void) trapDestroyPortal(portalid);
}

EMULvector     trapMapTrap;
void
SPACE_MapTrap(
        ulong  ctx,
        uchar  traptype,
        uchar  global,
        // fill
        ulong  indexbase,
        ulong  indexlimit,
        ulong  portalid
    )
{
    XMapTrap u = { ctx, traptype, global, 0, indexbase, indexlimit, portalid };
    (void) trapMapTrap(u.args);
}

EMULscalar0    trapSuspend;
SPACETOKEN
SPACE_Suspend()
{
    return  trapSuspend();
}

EMULscalar1x   trapResume;
void
SPACE_Resume      (ulong64 result)
{
    (void) trapResume(result);
}

EMULscalar2    trapUnsuspend;
void
SPACE_Unsuspend(
        SPACETOKEN token
    )
{
    Split64 x;
    x.all  = token;
    (void) trapUnsuspend(x.part[1], x.part[0]);     // note order
}

EMULscalar2    trapDiscardToken;
void
SPACE_DiscardToken(
        SPACETOKEN token
    )
{
    Split64 x;
    x.all  = token;
    (void) trapDiscardToken(x.part[1], x.part[0]);     // note order
}

EMULscalar0    trapPopCaller;
void
SPACE_PopCaller()
{
    (void) trapPopCaller();
}

EMULvector     trapStartCPU;
void
SPACE_StartCPU(
        ulong boot_cpu,
        ulong boot_ctx,
        ulong boot_mode,
        ulong boot_pc,
        ulong boot_arg
    )
{
    XStartCPU u = { boot_cpu, boot_ctx, boot_mode, boot_pc, boot_arg };
    (void) trapStartCPU(u.args);
}

EMULscalar1    trapHaltCPU;
void
SPACE_HaltCPU(
       ulong reason
   )
{
    (void) trapHaltCPU(reason);
}

EMULvector     trapMapIO;
void
SPACE_MapIO(
        uchar deviceid,
        uchar writable,
        // fill
        ulong iospacepage,
        ulong physpage
    )
{
    XMapIO u = { deviceid, writable, 0, iospacepage, physpage };
    (void) trapMapIO(u.args);
}

EMULvector     trapAccessDevice;
ulong64
SPACE_AccessDevice(
        ushort deviceid,
        uchar op,
        uchar deviceregister,
        ulong value,
        ulong valuex
    )
{
    XAccessDevice u = { deviceid, op, deviceregister, value, valuex };
    return trapAccessDevice(u.args);
}

EMULvector     trapInterruptCPU;
void
SPACE_InterruptCPU(
        ushort targetcpu,
        uchar interrupt,
        uchar irql
    )
{
    XInterruptCPU u = { targetcpu, interrupt, irql };
    (void) trapInterruptCPU(u.args);
}

EMULvector     trapManageIRQL;
IRQLOps
SPACE_ManageIRQL(
        IRQLOps irqlop,
        uchar irql
        //fill
    )
{
    XManageIRQL u = { irqlop, irql, 0 };
    return (IRQLOps) trapManageIRQL(u.args);
}

EMULbidir      trapGetSPACEParams;
void
SPACE_GetSPACEParams(
        SPACEParams *p
    )
{
    trapGetSPACEParams((ulong *)p);
}

EMULvector     trapSPACEBreak;
void
SPACE_SPACEBreak(
        ulong   breakvalue,
        char   *breakmsg
    )
{
    XSPACEBreak u = { 0 };

    char *p       = breakmsg;
    char *q       = u.breakmsg;
    int n         = sizeof(u.breakmsg);

    u.breakvalue    = breakvalue;
    while (n-- > 0  &&  (*q++ = *p++))
        {}

    (void) trapSPACEBreak(u.args);
}

