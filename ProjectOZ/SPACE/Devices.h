//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include <nt.h>
#include <ntrtl.h>

#include "Defs-crt.h"
#include "Config.h"

//
// Definitions for IO Device Emulation
//

typedef ULONGLONG (*DEVICEACCESS)        (ULONG cpu, int op, ULONG deviceregister, ULONG value, ULONG valuex);
typedef VOID      (*DMACOMPLETECALLBACK) (ULONG cpu, ULONG deviceid, ULONG bytecount, PVOID callbackcontext);

NTSTATUS    StartDMA        (ULONG cpu, ULONG deviceid, PCHAR devicememory, ULONG_PTR base, ULONG bytecount,
                             BOOLEAN writememory, DMACOMPLETECALLBACK callback, PVOID callbackcontext);

void        InterruptCPU    (ULONG cpu, USHORT targetcpu, UCHAR interrupt, UCHAR irql);

NTSTATUS    RegisterDevice  (ULONG cpu, ULONG deviceid, DEVICEACCESS deviceroutine, ULONG interrupt);

