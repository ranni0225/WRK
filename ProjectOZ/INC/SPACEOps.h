//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

// Some basic types
typedef unsigned long      ulong;
typedef unsigned short     ushort;
typedef unsigned char      uchar;
typedef unsigned __int64   ulong64;
typedef int                boolean;


typedef ulong64 SPACETOKEN;

// space context/modes
#define KERNELCTX           0
#define KERNELMODE          0
#define USERMODE            1

// SPACE paging macros
#define NTSPACE_PAGEBITS 16
#define NTSPACE_PAGESIZE  (1 << NTSPACE_PAGEBITS)
#define PAGE2ADDR(p) ((ulong)((p) << NTSPACE_PAGEBITS))
#define ADDR2PAGE(a) ((ulong)((a) >> NTSPACE_PAGEBITS))

#define IOCTX     (-2)
#define GLOBALCTX (-1)

// IO Definitions
#define IRQLBITS     6
#define MAXDEVICES  32      // bitmask must fit in unsigned long

// Definitions for read/write masks
#define BITMASK(x) ((ulong) (1 << (x)))

#define NOACCESS            ((ulong)  0)
#define ALLACCESS           ((ulong) -1)
#define ACCESSMASK(x)       BITMASK(x)
#define ACCESSOK(mask,mode) ((mask) & ACCESSMASK(mode))

//
// HW Emulation calls
//

// used with GetSPACEParams()
// no longer than 5 ulongs
typedef struct {
        ulong       vpage_base;
        ulong       vpage_limit;
        ulong       maxctxs;
        uchar       maxmodes;
        uchar       maxirqls;
        uchar       maxdevices;
        uchar       physbasepage;   // small number of pages used to boot BasicOZ
        ulong       nphysmempages;
} SPACEParams;
typedef union {
    SPACEParams params;
    ulong           registers[5];
} XSPACEParams;



#ifdef SPACE_EXE
#   define  EMULCPU     ulong cpu,
#   define  EMULCPUx    ulong cpu
#   define  EMULRETURN  NTSTATUS
#   define  E(n)        n
#else
#   define  EMULCPU
#   define  EMULCPUx
#   define  EMULRETURN  void
#   define  E(n)        SPACE_ ## n
void SPACE_Noop();
void SPACE_GetSPACEParams(SPACEParams *p);
#endif


struct regargs {
    ulong args[5];
};

EMULRETURN E(MapMemory) (EMULCPU  ulong ctx, ulong vpage, ulong ppage, ulong readmask, ulong writemask);
typedef union {
    struct {
        ulong ctx;
        ulong vpage;
        ulong ppage;
        ulong readmask;
        ulong writemask;
    };
    struct regargs;
} XMapMemory;
#define NTSPACE_INVALIDPAGE (0xffffffff)

EMULRETURN E(CleanCtx) (EMULCPU  ulong ctx);

ulong E(CreatePortal)       (EMULCPU  uchar type, uchar mode, uchar irql, ulong ctx, ulong handler, ulong protmask);
typedef union {
    struct {
        uchar type;
        uchar mode;
        uchar irql;
        uchar spare;
        ulong ctx;
        ulong handler;
        ulong protmask;
    };
    struct regargs;
} XCreatePortal;

typedef enum portaltype {
    emulationcall = 0,  // deliberate by program, only certain registers saved/restored
    servicecall   = 1,  // deliberate by program, only certain registers saved/restored
    interrupt     = 2,  // IRQL raised, lowered upon return, interrupted instruction resumed, all registers saved/restored
    exception     = 3,  // these are fatal, limited parameters, chain destroyed, cannot be resumed [analysis introduces much complexity]
    fault         = 4,  // parameters explain fault, faulting instruction resumed, all registers saved/restored [used only for accessfault]
    vaddr         = 5,  // pagefaults, based on virtual address range.  otherwise same as fault
} Portaltype;

EMULRETURN E(DestroyPortal) (EMULCPU  ulong portalid);

EMULRETURN E(MapTrap)       (EMULCPU  ulong ctx, uchar traptype, uchar global, ulong indexbase, ulong indexlimit, ulong portalid);
typedef union {
    struct {
        ulong  ctx;
        uchar  traptype;
        uchar  global;
        ushort spare;
        ulong  indexbase;
        ulong  indexlimit;
        ulong  portalid;
    };
    struct regargs;
} XMapTrap;

// traptypes must agree with Portaltypes and PCBCalltype (for now -- otherwise need a function for cross-checking them)
typedef enum {
    trapemul   = 0,
    trapserv   = 1,
    trapintr   = 2,
    trapexcept = 3,
    trapfault  = 4,
    trapvaddr  = 5
} Traptype;

SPACETOKEN  E(Suspend)      (EMULCPUx);
EMULRETURN  E(Resume)       (EMULCPU  ulong64 result);
EMULRETURN  E(Unsuspend)    (EMULCPU  SPACETOKEN token);
EMULRETURN  E(DiscardToken) (EMULCPU  SPACETOKEN token);
EMULRETURN  E(PopCaller)    (EMULCPUx);

EMULRETURN  E(StartCPU)     (EMULCPU  ulong boot_cpu, ulong boot_ctx, ulong boot_mode, ulong boot_pc, ulong boot_arg);
typedef union {
    struct {
        ulong boot_cpu;
        ulong boot_ctx;
        ulong boot_mode;
        ulong boot_pc;
        ulong boot_arg;
    };
    struct regargs;
} XStartCPU;

EMULRETURN  E(HaltCPU)      (EMULCPU  ulong reason);

EMULRETURN  E(MapIO)        (EMULCPU  uchar deviceid, uchar writable, ulong iospacepage, ulong physpage);
typedef union {
    struct {
        uchar  deviceid;
        uchar  writable;
        ushort spare;
        ulong  iospacepage;
        ulong  physpage;
    };
    struct regargs;
} XMapIO;

ulong64     E(AccessDevice) (EMULCPU  ushort deviceid, uchar op, uchar deviceregister, ulong value, ulong valuex);
typedef union {
    struct {
        ushort deviceid;
        uchar  op;
        uchar  deviceregister;
        ulong  value;
        ulong  valuex;
    };
    struct regargs;
} XAccessDevice;

void  E(InterruptCPU)       (EMULCPU  ushort targetcpu, uchar interrupt, uchar irql);
typedef union {
    struct {
        ushort targetcpu;
        uchar  interrupt;
        uchar  irql;
    };
    struct regargs;
} XInterruptCPU;


typedef enum {
    raiseirql, setirql, lowerirql
} IRQLOps;

IRQLOps E(ManageIRQL)       (EMULCPU  IRQLOps irqlop, uchar irql);
typedef union {
    struct {
        uchar  irqlop;
        uchar  irql;
        ushort spare;
    };
    struct regargs;
} XManageIRQL;

void E(SPACEBreak)          (EMULCPU  ulong breakvalue, char breakmsg[]);       // breakmsg limited to 16 characters because we pack in registers
typedef union {
    struct {
        ulong  breakvalue;
        uchar  breakmsg[16];
    };
    struct regargs;
} XSPACEBreak;

