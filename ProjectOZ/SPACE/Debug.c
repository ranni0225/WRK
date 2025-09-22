//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include "Defs.h"

#define TERSE

char *TRACE_fmt  = "TRACE: %s(): line %4d in %s\n";
char *STRACE_fmt = "TRACE: %s(): line %4d in %s <%s> [%08x %08x %08x %08x]\n";

char *portaltypestrings[] = {
    "emulationcall", // 0
    "servicecall",   // 1
    "interrupt",     // 2
    "exception",     // 3
    "fault",         // 4
    "vaddr"          // 5
};

#define PORTALTYPESTRING(t) (((t) < ASIZEOF(portaltypestrings))? portaltypestrings[(t)] : "<unknown>")

char *EMULStrings[] = {
    "Noop",           //  0
    "MapMemory",      //  1
    "MapIO",          //  2
    "MapTrap",        //  3
    "CreatePortal",   //  4
    "DestroyPortal",  //  5
    "CleanCtx",       //  6
    "Resume",         //  7
    "Suspend",        //  8
    "Unsuspend",      //  9
    "PopCaller",      // 10
    "DiscardToken",   // 11
    "StartCPU",       // 12
    "InterruptCPU",   // 13
    "AccessDevice",   // 14
    "ManageIRQL",     // 15
    "HaltCPU",        // 16
    "GetSPACEParams"  // 17
};

void
dump_Portal(
        Portal *p,
        int     emultype,
        char   *msg
    )
{
    if (p == NULL) {
        printf("dump_Portal(NULL): %s\n", msg);
        return;
    }

#ifdef TERSE
    printf("Portal <%x>  [%s]:  [%8x] => %08x Hash refcount=%x destroyed=%x type=%x <%s> %s\n",
           p, msg, (ULONG)p->hash.value, p->hash.link, p->refcount, p->destroyed, p->type, PORTALTYPESTRING(p->type),
           (emultype<0)? "<unknown>" : EMULStrings[emultype]);
    printf("  xxx=%x ctx/mode/irql=%x/%x/%x  protmask=%x  handler/arg=%x/%x\n",
           p->reserved, p->ctx, p->mode, p->irql, p->protmask, p->handler, p->arg);
#else
    printf("Portal <%x>  [%s]\n",   p, msg);

    printf("  [%8x] => %08x  Hash\n", (ULONG)p->hash.value, p->hash.link);
    printf("  %8x  refcount\n",     p->refcount);
    printf("  %8x  type <%s>\n",    p->type, PORTALTYPESTRING(p->type));
    printf("  %8x  destroyed\n",    p->destroyed);
    printf("  %8x  reserved\n",     p->reserved);
    printf("  %8x  ctx\n",          p->ctx);
    printf("  %8x  mode\n",         p->mode);
    printf("  %8x  irql\n",         p->irql);
    printf("  %8x  protmask\n",     p->protmask);
    printf("  %8x  handler\n",      p->handler);
    printf("  %8x  arg\n",          p->arg);

    printf("+++\n");
#endif
}

void
dump_Domain(
        Domain *p,
        char   *msg
    )
{
    EXCEPTION_MESSAGE *xm;
    EXCEPTION_RECORD  *xr;
    NTThreadDescr     *td;
    int i;

    if (p == NULL) {
        printf("dump_Domain(NULL): %s\n", msg);
        return;
    }

    printf("Domain [%s]:  ntprocess=%x  NTTheadDescr[]\n", msg, p->ntprocess);

    for (i = 0;  i < MAXCPUS;  i++) {
        td = p->cpus[i];
        if (td) {
            xm = &td->ntreplymessage;
            xr = &td->ntreplymessage.ExceptionRecord;
            printf(" %2d: NTthread cid/handle: %4d./%4x  cpu: %2x  trappednotinterrupted: %c  interrupteip: %8x  " "\n"//ntstack: %8x\n"
                   "               ntreplymsg: <msgid: %08x type: %08x pid/tid: %07d./%07d.  ReturnedStatus: %08x>\n"
                   "                     xrec: (code:  %08x addr: %08x args:    %08x/%08x>\n",
                    i, (ULONG)td->hash.value, td->ntcputhread, td->cpu, (td->trappednotinterrupted? 'T' : 'F'), td->interrupteip, //td->ntstackpointer,
                    xm->hdr.MessageId, xm->hdr.u2.s2.Type, xm->hdr.ClientId.UniqueProcess, xm->hdr.ClientId.UniqueThread, xm->ReturnedStatus,
                    xr->ExceptionCode, xr->ExceptionAddress, xr->ExceptionInformation[0], xr->ExceptionInformation[1]);
        }
    }
    printf("\n");
}

void
dump_Space(
        Space  *p,
        char   *msg
    )
{
    int i, n;

    if (p == NULL) {
        printf("dump_Space(NULL): %s\n", msg);
        return;
    }

    printf("Space [%s]:\n", msg);
    printf("  map:\t\t%x\t- dump with: dm ctxid\n", p->rootmap);
    printf("  trapvector:\t%x\t- dump with: dt ctxid\n", p->trapvector);

    for (n = 0, i = 0;  i < MAXMODES;  i++) if (p->domains[i]) n++;

    printf("  domains:\t<%x>\t- dump with: dd ctxid mode\n", n);
    for (i = 0;  i < MAXMODES;  i++) {
        if (p->domains[i])
            printf(" %2d: \t\t%x\t- dump with: dd ctxid %d\n", i, p->domains[i]);
    }
    printf("\n");
}

char *pcbcalltypestrings[] = {
    "unknowncall",     // 0,    // emulation calls don't use PCBs
    "servicetra",      // 1,
    "interruptcall",   // 2,
    "accessviolation", // 3,
    "raiseexception",  // 4,
    "pagefault"        // 5
};

#define PCBCALLTYPESTRING(t) (((t) < ASIZEOF(pcbcalltypestrings))? pcbcalltypestrings[(t)] : "<unknown>")

void
dump_PCB(
        PCB   *p,
        char  *msg
    )
{
#ifdef TERSE
    printf("PCB <%x>  [%s]  [%8x] => %08x Hash\n", p, msg, (ULONG)p->hash.value, p->hash.link);
    printf("  old ctx/mode/irql=%x/%x/%x  chain=%x  type=%x <%s> retarg=%x token: alloc=%x owner ctx/mode=%x/%x\n",
        p->oldctx, p->oldmode, p->oldirql, p->chainlink, p->pcbcalltype, PCBCALLTYPESTRING(p->pcbcalltype),
        p->returnargflag, p->tokenallocated, p->tokenowner_ctx, p->tokenowner_mode);
    dump_CONTEXT(&p->ntcontext, "pcb->ntcontext");
#else
    printf("PCB <%x>  [%s]\n",   p, msg);

    printf("  [%8x] => %08x  Hash\n", (ULONG)p->hash.value, p->hash.link);
    printf("  %8x  chainlink\n",        p->chainlink);
    printf("  %8x  type <%s>\n",        p->pcbcalltype,        PCBCALLTYPESTRING(p->pcbcalltype));
    printf("  %8x  tokenallocated\n",   p->tokenallocated);
    printf("  %8x  returnargflag\n",    p->returnargflag);
    printf("  %8x  oldctx\n",           p->oldctx);
    printf("  %8x  oldmode\n",          p->oldmode);
    printf("  %8x  oldirql\n",          p->oldirql);
    printf("  %8x  tokenowner_ctx\n",   p->tokenowner_ctx);
    printf("  %8x  tokenowner_mode\n",  p->tokenowner_mode);
    dump_CONTEXT(&p->ntcontext, "pcb->ntcontext");
    printf("+++\n");
#endif
}

void
out_vector(
        char   *tname,
        Portal *p[],
        int     nentries
    )
{
    int i, maxvalid;

    printf("  %13s", tname);

    // don't print extra lines of 'xxx'
    maxvalid = nentries;
    while (maxvalid > 16) {           // we can print at least these many on a line
        if (p[maxvalid-1] != NULL)
            break;
        maxvalid--;
    }

    for (i = 0;  i < maxvalid;  i++) {
        if (p[i] == NULL)
            printf(" xxx");
        else
            printf(" %3x", (ULONG)p[i]->hash.value);
    }
    if (maxvalid < nentries)
        printf(" ...");
    printf("\n");
}

void
dump_Trapvector(
        Trapvector *t,
        char       *msg
    )
{
    struct copy_of_private_VADNode_from_trap_c {
        ULONG       VirtualPageBase;
        ULONG       VirtualPageLimit;
        Portal     *Portal;
    } *ptr;

    if (t == NULL) {
        printf("dump_Trapvector(NULL): %s\n", msg);
        return;
    }

    printf("Trapvector <%x>: %s\n", t, msg);

    out_vector("emulation:",     t->emulation,   NTRAP_EMULCALLS);
    out_vector("servicecall:",   t->servicecall, NTRAP_SERVCALLS);
    out_vector("interrupts:",    t->interrupts,  NTRAP_INTERRUPTS);
    out_vector("exceptions:",    t->exceptions,  NTRAP_EXCEPTIONS);
    out_vector("faults:",        t->faults,      NTRAP_FAULTS);

    for (ptr = RtlEnumerateGenericTableAvl(&t->vadtree, TRUE);  ptr != NULL;  ptr = RtlEnumerateGenericTableAvl(&t->vadtree, FALSE)) {
            printf("  %08x->%08x:  %4x\n", ptr->VirtualPageBase, ptr->VirtualPageLimit,
                                           ptr->Portal? ((ULONG)ptr->Portal->hash.value) : 0xdead);
    }
}

void
dump_CONTEXT(
        CONTEXT *p,
        char    *msg
    )
{
#ifdef TERSE
    printf("CONTEXT <%x> %s: ctxflags=%x segs Cs/Ss=%x/%x Ds=%x\n", p, msg, p->ContextFlags, p->SegCs, p->SegSs, p->SegDs);
    printf("  Ebx=%x  Ecx=%X  Edx=%x  Edi=%x  Esi=%x  Eax=%x  Eip=%x Ebp/Esp=%x/%x EF=%x\n",
            p->Ebx, p->Ecx, p->Edx, p->Edi, p->Esi, p->Eax, p->Eip, p->Ebp, p->Esp, p->EFlags);
#else
    printf("CONTEXT <%x>  [%s]\n",   p, msg);
    printf("  %8x  ContextFlags\n", p->ContextFlags);
    printf("  %8x  SegGs\n",        p->SegGs);
    printf("  %8x  SegFs\n",        p->SegFs);
    printf("  %8x  SegEs\n",        p->SegEs);
    printf("  %8x  SegDs\n",        p->SegDs);
    printf("\n");
    printf("  %8x  SegCs\n",        p->SegCs);
    printf("  %8x  SegSs\n",        p->SegSs);
    printf("\n");
    printf("  %8x  Edi\n",          p->Edi);
    printf("  %8x  Esi\n",          p->Esi);
    printf("  %8x  Ebx\n",          p->Ebx);
    printf("  %8x  Edx\n",          p->Edx);
    printf("  %8x  Ecx\n",          p->Ecx);
    printf("  %8x  Eax\n",          p->Eax);
    printf("\n");
    printf("  %8x  Eip\n",          p->Eip);
    printf("  %8x  Ebp\n",          p->Ebp);
    printf("  %8x  Esp\n",          p->Esp);
    printf("  %8x  EFlags\n",       p->EFlags);
    printf("\n");
    printf("+++\n");
#endif
}

void
dump_data(
        PCHAR p,
        ULONG n,
        char *msg
    )
{
    ULONG i, x;

    x = 0;
    printf("  %s\n", msg);
    for (i = 0;  i < n;  i++) {
        if (x == 0)
            printf("    ");
        printf(" %0x", p[i]);
        if (x == 15) {
            printf("\n");
            x = 0;
        } else {
            x++;
        }
    }

    printf("\n");
}

#define DUMP_DATA(base, offset, count) dump_data( (offset) + (PCHAR)(base), (count), "DATA");


char *CPUStateStrings[] = {
    "Uninitialized",    // 0,
    "Running",          // 1,
    "Halted"            // 2,
};

#define CPUSTATESTRING(t) (((t) < ASIZEOF(CPUStateStrings))? CPUStateStrings[(t)] : "<unknown>")

void
dump_CPUHardware(
        CPUHardware *p,
        char        *msg
    )
{
#ifdef TERSE
    printf("CPUHardware <%x>  [%s]  cpustate=%x <%s>  ctx/mode/irql=%x/%x/%x chain=%x intq=%x pend=%x\n",
            p, msg, p->cpustate, CPUSTATESTRING(p->cpustate), p->ctx, p->mode, p->irql,
            p->currentchain, p->queuehead, p->pendinginterrupt);
    if (p->currentchain)
        dump_PCB(p->currentchain, "currentchain");
#else
    printf("CPUHardware <%x>  [%s]\n",   p, msg);

    printf("  %8x  cpustate <%s>\n",    p->cpustate,        CPUSTATESTRING(p->cpustate));
    printf("  %8x  ctx\n",              p->ctx);
    printf("  %8x  mode\n",             p->mode);
    printf("  %8x  irql\n",             p->irql);
    printf("  %8x  currentchain\n",     p->currentchain);
    printf("  %8x  queuehead\n",        p->queuehead);
    printf("  %8x  pendinginterrupt\n", p->pendinginterrupt);

    if (p->currentchain)
        dump_PCB(p->currentchain, "currentchain");
    printf("+++\n");
#endif
}

void
dump_PORT_MESSAGE(
        PPORT_MESSAGE p,
        char         *msg
    )
{
    printf("PORT_MESSAGE <%x>  [%s]\n",   p, msg);
    printf("  %8x Length\n",                    p->u1.Length);
    printf("  %8x Type\n",                      p->u2.s2.Type);
    printf("  %8x DataInfoOffset\n",            p->u2.s2.DataInfoOffset);
    printf("  %8x ClientId.UniqueProcess\n",    p->ClientId.UniqueProcess);
    printf("  %8x ClientId.UniqueThread\n",     p->ClientId.UniqueThread);
    printf("  %8x MessageId\n",                 p->MessageId);
    printf("x %8x CallbackId\n",                p->CallbackId);
    DUMP_DATA(p, sizeof(PORT_MESSAGE), 32);
    printf("+++\n");
}

void
dump_EXCEPTION_MESSAGE(
        EXCEPTION_MESSAGE *p,
        char              *msg
    )
{
    EXCEPTION_RECORD  *x = &p->ExceptionRecord;

#ifdef TERSE
    printf("  xrec: code/addr=%x  info=%x/%x\n",
            x->ExceptionCode, x->ExceptionAddress, x->ExceptionInformation[0], x->ExceptionInformation[1]);
#else
    dump_PORT_MESSAGE(&p->hdr, msg);
    printf("  %8x ApiNumber\n",                 p->ApiNumber);
    printf("  %8x ReturnedSTatus\n",            p->ReturnedStatus);
    printf("  %8x &ExceptionRecord\n",         &p->ExceptionRecord);

    printf("     %8x .code\n",                  x->ExceptionCode);
    printf("     %8x .address\n",               x->ExceptionAddress);
    printf("     %8x .information[0]\n",        x->ExceptionInformation[0]);
    printf("     %8x .information[1]\n",        x->ExceptionInformation[1]);

    printf("  %8x FirstChance\n",               p->FirstChance);
    printf("+++\n");
#endif
}

//
// see VPageToMap -- this must match that function
//
#define NTSPACE_TOPMASK ((1 << NTSPACE_TOPBITS) - 1)
#define NTSPACE_MIDMASK ((1 << NTSPACE_MIDBITS) - 1)

#define TOPVADBITS(va) (((va) >> (NTSPACE_PAGEBITS + NTSPACE_MIDBITS)) &  NTSPACE_TOPMASK)
#define MIDVADBITS(va) (((va) >> (NTSPACE_PAGEBITS                  )) &  NTSPACE_MIDMASK)

#define NTSPACE_MAPDIRENTRIES (1 << NTSPACE_TOPBITS)
#define NTSPACE_MAPTABENTRIES (1 << NTSPACE_MIDBITS)

void
dump_Rootmap(
    ULONG  ctx,
    char  *msg
)
{

    Space     *space;
    ULONG      vdir, vpage;
    ULONG_PTR  virtualpage;

    Mapping  **rootmap;
    Mapping  *dmap, *pmap;

    printf("Rootmap for ctx %x: %x\n", ctx, msg);

    if (ctx == IOCTX) {
        space = iospace;
        printf("Using IOSPACE\n");
    } else if (ctx >= MAXCTXS) {
        printf("bad context: %x\n", ctx);
        return;
    } else {
        space = spaces[ctx];
    }

    if (space == NULL) {
        printf("empty ctx: %x\n", ctx);
        return;
    }

    rootmap = space->rootmap;

    if (rootmap == NULL) {
        printf("no rootmap for ctx: %x\n", ctx);
        return;
    }

    for (vdir = 0;  vdir < NTSPACE_MAPDIRENTRIES;  vdir++) {
        if (dmap = rootmap[vdir]) {
            for ( vpage = 0;  vpage < NTSPACE_MAPTABENTRIES;  vpage++) {
                pmap = dmap + vpage;
                if (pmap && (pmap->physpage || pmap->readmask || pmap->writemask)) {
                    virtualpage = vdir  * NTSPACE_MAPTABENTRIES + vpage ;
                    printf("MAP <%x>:  %08x -> %08x  [R: %08x  W: %08x]\n", pmap, virtualpage, pmap->physpage, pmap->readmask, pmap->writemask);
                }
            }
        }
    }
}

void
dump_ProcessVirtualRegions(
        HANDLE     process,
        ULONG_PTR  start,
        ULONG_PTR  limit,
        char       *msg
    )
{
    MEMORY_BASIC_INFORMATION32 m;
    NTSTATUS s;
    ULONG ptr;

    if (start == 0 && limit == 0) {
        start = PAGE2ADDR(spaceparams.vpage_base);
        limit = PAGE2ADDR(spaceparams.vpage_limit);
    }

    printf("Process Virtual Regions: start=%08x  limit=%08x  process=%x   <%s>\n", start, limit, process, msg);

    ptr = start;
    while (ptr < limit) {
        s = NtQueryVirtualMemory (process, (PVOID)ptr, MemoryBasicInformation, &m, sizeof(m), NULL);
        if (!NT_SUCCESS(s)) {
           printf("ERR: NtQueryVirtualMemory returns %x\n", s);
           if (s == STATUS_INVALID_HANDLE) {
               printf("Aborting\n");
               return;
           }
           ptr += 64*1024;

        } else if (m.State != MEM_FREE) {
           printf("   ptr=%08x:  Base=%08x AllocBase=%08x AllocProt=%-4x Region=%08x State=%4x  Prot=%4x  Type=%2x\n",
                ptr, m.BaseAddress, m.AllocationBase, m.AllocationProtect, m.RegionSize, m.State, m.Protect, m.Type);
           ptr += m.RegionSize;

        } else {
           ptr += m.RegionSize;
        }
    }
    printf("+++\n");
}

char *spacebreakhelp =
     "\nCommands:\n"
     "\tg - go (continue)\n"
     "\t! - launch ntsd debugger on OZ\n"
     "\td? id - dump data structure 'x'\n"
     "\t\tdc cpuid\t - dump CPUHardware for cpu\n"
     "\t\tdd ctxid mode\t - dump Domain for space ctxid and mode\n"
     "\t\tdh ctxid\t - dump handles for space ctxid\n"
     "\t\tdm ctxid\t - dump Map for ctxid\n"
     "\t\tdp portalid\t - dump Portal for portalid\n"
     "\t\tds ctxid\t - dump Space for space ctxid\n"
     "\t\tdt ctxid\t - dump trapvector for space ctxid (or -1 for GLOBALCTX)\n"
     "\t\tdv handleid [01] - dump NT virtual regions for process with handleid [all addresses]\n"
     "\t\tdx handleid [01]- get/dump NT thread context for thread with handleid [or by CID]\n"
     "\t\tdr handleid address\t - dump remote data from process handleid at address\n"
     "\t\tdw handleid address value\t - write remote value in process handleid at address\n"
     "\tq - quit\n" 
     "\t? - this help\n";

BOOLEAN spacebreak_disabled = 0;

void
spacebreak(
        char        *why,
        CLIENT_ID   *pcid,
        char        *fcn,
        char        *file,
        int          line
    )
{
    BOOLEAN  continueflag;
    char     buf[128];
    char    *cmdline;

    char  cmdchar;
    char  optchar;
    ulong idparam0;
    ulong idparam1;
    ulong idparam2;

    int system(const char *command);
    

    fflush(stdout);

    if (spacebreak_disabled)
        return;

    fprintf(stderr, "SPACEBREAK: %s() at %d in %s: %s\n", fcn, line, file, why);

    if (pcid)
        fprintf(stderr, "To start debugger on BasicOZ:  (thread %d), process:  ntsd -p %d\n", pcid->UniqueThread, pcid->UniqueProcess);

    continueflag = 0;
    while (!continueflag) {
         fprintf(stderr, "debug cmd> ");

         cmdline = fgets(buf, sizeof(buf)-1, stdin);
         if (cmdline == NULL) {
             continueflag = 1;
             continue;
         }

         cmdchar   = '$';     // noop
         optchar   =  0;
         idparam0  =  0;
         idparam1  =  0;
         idparam2  =  0;
         sscanf(cmdline, "%c%c %x %x %x\n", &cmdchar, &optchar, &idparam0, &idparam1, &idparam2);

         switch (cmdchar) {
         case 'g':
             if (optchar == '$') {
                 fprintf(stderr, "\ncontinuing with no more breaks.\n\n");
                 spacebreak_disabled = 1;
             } else {
                 fprintf(stderr, "\ncontinuing...\n");
             }
             continueflag = 1;
             break;

         case '!':
             if (pcid) {
                 fprintf(stderr, "\nlaunching debugger ...\n");
                 sprintf(buf, "start ntsd -p %d", pcid->UniqueProcess);
                 system(buf);
             } else {
                 fprintf(stderr, "\ncannot automatically launch debugger from this point\n\n");
             }
             break;

         case 'd': {
                switch (optchar) {
                case 'm':
                    dump_Rootmap(idparam0, "dump Maps for ctx");
                    break;
                case 'c':
                    if (idparam0 < MAXCPUS)
                        dump_CPUHardware(cpuhardware+idparam0, "dump CPUHardware for id");
                    else
                        printf("Bad id\n");
                    break;
                case 'd':
                    if (idparam0 >= MAXCTXS) {
                        printf("Bad ctx: %x\n", idparam0);
                    } else if (idparam1 >= MAXMODES) {
                        printf("Bad mode: %x\n", idparam1);
                    } else if (spaces[idparam0] == NULL) {
                        printf("Space NULL for ctx %x\n", idparam0);
                    } else if (spaces[idparam0]->domains[idparam1] == NULL) {
                        printf("Domain NULL for mode %x in ctx %x\n", idparam1, idparam0);
                    } else  {
                        dump_Domain(spaces[idparam0]->domains[idparam1], "dump Domain");
                    }
                    break;
                case 'h':
                    if (idparam0 < MAXCTXS && spaces[idparam0]) {
                        ulong i;
                        for (i = 0;  i < MAXMODES;  i++)
                            if (spaces[idparam0]->domains[i])
                                printf("ctx %3x mode %2x ntprocess handle %x\n", idparam0, i, spaces[idparam0]->domains[i]->ntprocess);
                    } else
                        printf("Bad id\n");
                    break;
                case 'v': {
                    ULONG_PTR start;
                    ULONG_PTR limit;
                    char     *msg;
                    switch (idparam1) {
                        case 0:
                            start = PAGE2ADDR(spaceparams.vpage_base);
                            limit = PAGE2ADDR(spaceparams.vpage_limit);
                            msg = "dump_ProcessVirtualRegions [SPACE] for id";
                            break;
                        default:
                            start = 0;
                            limit = 0x7ffe1000;
                            msg = "dump_ProcessVirtualRegions [NTUSER] for id";
                            break;
                        }
                        dump_ProcessVirtualRegions((PVOID)idparam0, start, limit, msg);
                    }
                    break;
                case 'p':
                    dump_Portal(lookupportal(idparam0), -1, "dump Portal");
                    break;
                case 'r': {
                    ULONG       b[4];
                    ULONG       readsize;
                    NTSTATUS    s;

                    s = NtReadVirtualMemory((HANDLE)idparam0, (PVOID)idparam1, (PVOID)b, sizeof(b), &readsize);
                    if (NT_SUCCESS(s) && readsize == sizeof(b))
                        fprintf(stderr, "  <process %08x> %08x:  %08x %08x %08x %08x\n", idparam0, idparam1, b[0], b[1], b[2], b[3]);
                    else
                        fprintf(stderr, "  %08x:  pr: NtReadVirtualMemory(process %08x at %08x) size %x\n", s, idparam0, idparam1, readsize);
                    break;
                }
                case 'w': {
                    ULONG       b;
                    ULONG       iosize;
                    NTSTATUS    s;

                    s = NtReadVirtualMemory((HANDLE)idparam0, (PVOID)idparam1, &b, sizeof(b), &iosize);
                    if (!NT_SUCCESS(s) || iosize != sizeof(b)) {
                        fprintf(stderr, "  %08x:  pr: NtReadVirtualMemory(process %08x at %08x) size %x\n", s, idparam0, idparam1, iosize);
                        break;
                    }
                    s = NtWriteVirtualMemory((HANDLE)idparam0, (PVOID)idparam1, (PVOID)&idparam2, sizeof(b), &iosize);
                    if (!NT_SUCCESS(s) || iosize != sizeof(b)) {
                        fprintf(stderr, "  %08x:  pr: NtWriteVirtualMemory(process %08x at %08x) size %x\n", s, idparam0, idparam1, iosize);
                    } else {
                        fprintf(stderr, "  <process %08x> %08x:  %08x -> %08x\n", idparam0, idparam1, b, idparam2);
                    }
                    break;
                }
                case 's':
                    if (idparam0 < MAXCTXS) {
                        dump_Space(spaces[idparam0], "dump Space");
                    } else if (idparam0 == IOCTX) {
                        dump_Space(iospace, "dump IOSpace");
                    } else {
                        printf("Bad ctx\n");
                    }
                    break;
                case 't':
                    if (idparam0 < MAXCTXS && spaces[idparam0]) {
                        dump_Trapvector(spaces[idparam0]->trapvector, "Spaces[] Trapvector");
                    } else if (idparam0 == GLOBALCTX) {
                        extern Trapvector  GlobalTrapVector;
                        dump_Trapvector(&GlobalTrapVector, "dump GlobalTrapVector");
                    } else {
                        printf("Bad ctx\n");
                    }
                    break;
                case 'x':
                {
                    CONTEXT        ntcontext;
                    NTThreadDescr *ntthreaddescr;
                    NTSTATUS       s;

                    if (idparam1 == 1) {
                        // idparam0 is the CID not the handle, so look up the handle
                        ntthreaddescr = LookupNTThread((HANDLE)idparam0);
                        if (ntthreaddescr == NULL) {
                            fprintf(stderr, "\nUnknown thread CID %x\n", idparam0);
                            break;
                        }
                        idparam0 = (ULONG)ntthreaddescr->ntcputhread;
                    }
                    ntcontext.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
                    s = NtGetContextThread((HANDLE)idparam0, &ntcontext);
                    if (NT_SUCCESS(s))
                        dump_CONTEXT(&ntcontext, buf);
                    else
                        fprintf(stderr, "%08x:  px: NtGetContextThread\n", s);
                    break;
                }
                default:
                    fprintf(stderr, "\nUnknown ds specifier: %c\n", optchar);
                    fprintf(stderr, spacebreakhelp);
                }
             }
             break;

         case '?':
         default:
             fprintf(stderr, spacebreakhelp);
             break;

         case '$':
         case '\n':
             break;

         case 'q':
         case '\0':
             die("spacebreak quit", 0, 0);
             //notreached
         }
    }
}

