//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <stdio.h>
#include "Defs-crt.h"

#define SPACE_EXE
#include "..\INC\SpaceOps.h"

#include "Config.h"
#include "TrapValues.h"

#define ASIZEOF(x) (sizeof(x)/sizeof((x)[0]))
#define K *1024
#define M *1024*1024

#define ZERO(x) RtlZeroMemory((x), sizeof(*(x)))

//
// General Definitions
//

#define COUNTBY(a,s)  (((a) + ((s)-1)) / (s))
#define ROUNDUP(a,s)  ((s) * COUNTBY(a,s))

//
// Definitions for Util
//

#define HASH(value,tablesize)     ((value) % (tablesize))

typedef struct hashentry{
    struct hashentry   *link;
    ULONGLONG           value;
} Hashentry;

//
// Returns an object from a hash table
//
Hashentry *
LookupHashEntry(
        Hashentry   *hashtable[],
        ULONG        tablesize,
        ULONGLONG    value,
        Hashentry ***pprevlink
    );

#define HANDLEVALUE(h) ((ULONGLONG)((ULONG)h >> 3))    // used for has values that have low order bits off

//
// Definitions for LoadKernel
//

// Structure used by LoadKernel to describe the sections of BasicOZ
typedef struct {
    ULONGLONG   VirtualAddress;
    ULONG       nPages;
    ULONG       Writable;
    ULONG       PhysicalBasepage;
} Section;

NTSTATUS LoadKernel(PWCHAR kernelname, PULONG_PTR entry, Section kernelsections[], int *nkernelsections);

//
// Definitions for Trap
//
ULONG trapvectorlimits[NTRAPTYPES];

//
// Definitions for Memory
//
typedef struct mapping {
    ULONG_PTR   physpage;
    ULONG       readmask;
    ULONG       writemask;
} Mapping;

// these pseudo-context values can be passed to functions, but are not stored in Portal or PCB ctx fields
#define MAXCTXS     (1 << CTXBITS)
#define MAXMODES    (1 << MODEBITS)

#  define EMULCTX   (-3)
//#define IOCTX     (-2)
//#define GLOBALCTX (-1)

//
// Spaces, domains, trapvectors, cpus -- these all implicitly exist.
// We create whatever data structures we need on-demand.
//

// XXX: from WRK sources
typedef struct {
    PORT_MESSAGE     hdr;
    int              ApiNumber;  // 0 for exceptions
    NTSTATUS         ReturnedStatus;
    EXCEPTION_RECORD ExceptionRecord;
    ULONG            FirstChance;
} EXCEPTION_MESSAGE;

typedef struct {
    Hashentry           hash;                   // hashed on ID of ntcputhread
    HANDLE              ntcputhread;
// XXX:   ULONG_PTR           ntstackpointer;         // used to fakeout NT exception dispatching at startup and portal traversal //----------
    struct domain      *domain;
    ULONG               cpu;
    BOOLEAN             trappednotinterrupted;  // awaiting replymessage not suspended
    ULONG               interrupteip;           // EIP saved at point of an InterruptCPU
    EXCEPTION_MESSAGE   ntreplymessage;
} NTThreadDescr;

NTThreadDescr *LookupNTThread(HANDLE ntthreadid);

typedef struct domain {
    HANDLE          ntprocess;
    // ULONG           mode;        // XXX; do not need this
    NTThreadDescr  *cpus[MAXCPUS];
} Domain;

#define PORTALTYPEBITS 3

typedef enum {
    unknowncall     = 0,    // emulation calls don't use PCBs
    servicetrap     = 1,
    interruptcall   = 2,
    accessviolation = 3,
    raiseexception  = 4,
    pagefault       = 5
} PCBCalltype;
#define PCBCALLTYPEBITS 3


typedef struct portal {
    Hashentry   hash;                   // hashed on portalid
    ULONG       refcount;
    ULONG       type      :PORTALTYPEBITS,
                destroyed :1,
                reserved  :1,
                ctx       :CTXBITS,
                mode      :MODEBITS,
                irql      :IRQLBITS;    // only used for interrupts
    ULONG       protmask;
    ULONG_PTR   handler;
    ULONG_PTR   arg;
} Portal;

Portal  *lookupportal      (ULONG   portalid);
void     referenceportal   (Portal *portal);
void     dereferenceportal (Portal *portal);

typedef struct trapvector {
    Portal        *emulation   [NTRAP_EMULCALLS];
    Portal        *servicecall [NTRAP_SERVCALLS];
    Portal        *interrupts  [NTRAP_INTERRUPTS];
    Portal        *exceptions  [NTRAP_EXCEPTIONS];
    Portal        *faults      [NTRAP_FAULTS];
    RTL_AVL_TABLE  vadtree;
} Trapvector;

typedef struct {
    struct mapping **rootmap;
    Domain          *domains[MAXMODES];
    Trapvector      *trapvector;
} Space;

Space *spaces[MAXCTXS];
Space *iospace;

void instantiatespace           (ULONG ctx);
void instantiatedomain          (ULONG ctx, ULONG mode);
void instantiatentthreaddescr   (ULONG ctx, ULONG mode, ULONG cpu);

Mapping *VPageToMap(ULONG cpu, Space *space, ULONG_PTR vpage);

//
// Definitions for Interrupt
//

typedef struct {
    PORT_MESSAGE        hdr;
    ULONG               cpu;
    UCHAR               interrupt;
    UCHAR               irql;
} InterruptMsg;

typedef struct interruptitem {
    struct interruptitem  *link;
    UCHAR                  interrupt;
    UCHAR                  irql;
} InterruptItem;


//
// Definitions for Portal
//
Portal *allocateportal();

//
// Definitions for CPU
//

typedef struct pcb {
    Hashentry   hash;
    struct pcb *chainlink;
    ULONG       pcbcalltype     :PCBCALLTYPEBITS,
                tokenallocated  :1,
                returnargflag   :1,
                oldctx          :CTXBITS,
                oldmode         :MODEBITS,
                oldirql         :IRQLBITS;    // used for interruptcall
    ULONG       tokenowner_ctx;               // used when tokenallocated
    ULONG       tokenowner_mode;
    CONTEXT     ntcontext;
} PCB;

typedef enum {
    Uninitialized = 0,  // initial state
    Running,
    Halted              // waiting for something to happen
} CPUState;

typedef struct cpuhardware {
    CPUState         cpustate;
    ULONG            ctx;
    UCHAR            mode;
    UCHAR            irql;
    PCB             *currentchain;
    InterruptItem   *queuehead;
    InterruptItem   *pendinginterrupt;
} CPUHardware;

CPUHardware cpuhardware[MAXCPUS];

void StartNTCPUThread(ULONG cpu, NTThreadDescr *ntthreaddescr, PCB *pcb);

Portal *TrapToPortal(ULONG ctx, ULONG mode, Traptype traptype, ULONG trapindex);

void cleantrapvector(Trapvector *trapvector);

//
// Definitions for Emulation
//

void SPACEEmulation();

//
// Definitions for IO
//

#include "Devices.h"

typedef struct {
    Hashentry     hash;
    DEVICEACCESS  accessfcn;
} Device;

void RegisterAllDevices();

// use a fake NT status code to signify interrupts
#define STATUS_SPACE_INTERRUPT STATUS_KERNEL_APC

//
// SPACE globals
//

typedef struct {
    HANDLE          physmemsection;
    HANDLE          spaceport;
    ULONG           highestctx;
} SPACEGlobals;

SPACEParams    spaceparams;
SPACEGlobals   spaceglobals;

#define MAXIMAGESECTIONS 4
Section kernelsections[MAXIMAGESECTIONS];
int     nkernelsections;


void cleanup();

//
// Processor-specific definitions
//

VOID  setselectors(CONTEXT *pctx);

typedef union {
    struct {
        ULONG   operation   :16,
                hwemulation :1,
                servicecall :1,
                illegalop   :1,
                skiplength  :3;
    };

    ULONG iwhole;
} TrapDecode;

ULONG getinstruction(HANDLE process, PVOID pc);

void get5args     (ULONG   *args5, CONTEXT *ctx);
void ret5args     (CONTEXT *ctx,   ULONG   *args5);
void set4args     (CONTEXT *ctx,   ULONG   *args4);

void retresultarg (CONTEXT *ctx, ULONGLONG resultarg);
void retresultarg2(CONTEXT *ctx, ULONG resultarg_low, ULONG resultarg_high);

ULONGLONG   makequad    (ULONG a,   ULONG b);
ULONG       splitquad   (PULONG ph, ULONGLONG v);

//
// Error reporting
//
int warn(char *msg, ULONG_PTR value);
int die (char *fmt, char *msg, ULONG_PTR value);

#define FATALFAIL(s,msg) (NT_SUCCESS(s) || die("Fatal error in [" __FILE__ "]  " __FUNCTION__ "(): %s <%x>\n", msg, s))
#define WARNFAIL(s,msg)  (NT_SUCCESS(s) || warn(msg, s))
#define CHKFAIL(s)       {if (! NT_SUCCESS(s)) goto FAILED;}

#define BUGCHECK(msg) die("SPACE Internal error in " __FUNCTION__ ": %s  [" __FILE__ " at %d]\n", msg, __LINE__)

#include "Debug.h"          // XXX for debugging

