//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#ifndef DEVICES

void
dump_Portal(
        Portal *p,
        int     emultype,
        char   *msg
    );

void
dump_Space(
        Space  *p,
        char   *msg
    );

void
dump_Domain(
        Domain *p,
        char   *msg
    );

void
dump_PCB(
        PCB   *p,
        char  *msg
    );

void
dump_CONTEXT(
        CONTEXT *p,
        char    *msg
    );

void
dump_data(
        PCHAR p,
        ULONG n,
        char *msg
    );

void
dump_CPUHardware(
        CPUHardware *p,
        char        *msg
    );

void
dump_EXCEPTION_MESSAGE(
        EXCEPTION_MESSAGE *p,
        char              *msg
    );

void
dump_PORT_MESSAGE(
        PPORT_MESSAGE p,
        char         *msg
    );

void
dump_ProcessVirtualRegions(
        HANDLE process,
        ULONG  start,
        ULONG  limit,
        char  *msg
    );

void
dump_Rootmap(
    ULONG  ctx,
    char  *msg
);

void
dump_Trapvector(
        Trapvector *t,
        char       *msg
    );

void
spacebreak(
        char        *why,
        CLIENT_ID   *pcid,
        char        *fcn,
        char        *file,
        int          line
    );

#define SPACEBREAK(why, pcid) spacebreak((why), (pcid), __FUNCTION__, __FILE__, __LINE__)

extern char *EMULStrings[];

#endif // !DEVICES


// DEBUG MACROS
extern char *TRACE_fmt;
extern char *STRACE_fmt;

#define MASH(a,b) (((a)<<16) | (b))
#define TRACE                   printf(TRACE_fmt,  __FUNCTION__, __LINE__, __FILE__);
#define STRACE(m,v0,v1,v2,v3)   printf(STRACE_fmt, __FUNCTION__, __LINE__, __FILE__, (m), (v0), (v1), (v2), (v3));

