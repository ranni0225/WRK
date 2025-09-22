//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include "Defs.h"

int
main(int ac, char *argv[])
{
    LARGE_INTEGER       physmemsize;
    OBJECT_ATTRIBUTES   obja;
    NTSTATUS            s;
    Section            *sect;
    ULONG               i, n;
    ULONG               kportalid, uportalid;
    ULONG_PTR           entrypoint;

    printf("Welcome to SPACE!\n");

    // Set global configuration parameters
    spaceparams.vpage_base    = MINIMUM_VPAGE;
    spaceparams.vpage_limit   = MAXIMUM_VPAGE;
    spaceparams.maxctxs       = MAXCTXS;
    spaceparams.maxmodes      = MAXMODES;
    spaceparams.maxirqls      = MAXIRQLS;
    spaceparams.maxdevices    = MAXDEVICES;
    

    // Create the Physical Memory
    physmemsize.QuadPart = ROUNDUP(PHYSMEMSIZE, NTSPACE_PAGESIZE);
    spaceparams.nphysmempages = (ULONG_PTR) physmemsize.QuadPart / NTSPACE_PAGESIZE;

    s = NtCreateSection( &spaceglobals.physmemsection, SECTION_ALL_ACCESS, NULL,
                         &physmemsize, PAGE_EXECUTE_READWRITE, SEC_COMMIT, NULL);
    FATALFAIL(s, "CreatePhysicalMemory");

    // Load BasicOZ kernel into physical memory
    nkernelsections = ASIZEOF(kernelsections);
    s = LoadKernel(BASICOZKERNEL, &entrypoint, kernelsections, &nkernelsections);
    FATALFAIL(s, "LoadKernel");

    for (sect = kernelsections, n = nkernelsections;  n;  sect++, n--) {
        ULONG vpage = ADDR2PAGE(sect->VirtualAddress);
        if (vpage < spaceparams.vpage_base || spaceparams.vpage_limit <= vpage)
            BUGCHECK("Bad virtual mappings for kernel executable");
    }

    // Create the shared exception port
    InitializeObjectAttributes(&obja, NULL, 0, NULL, NULL);
	s = NtCreatePort(&spaceglobals.spaceport, &obja, 0, 256, 4096 * 16);
    FATALFAIL(s, "Create Exception Port");

    // Start CPU 0 in space 0 at domain 0 
    // It will trap right away, and we'll pickup the message as soon as we enter SPACEEmulation()
    s = StartCPU(0, 0, KERNELCTX, KERNELMODE, entrypoint, 0);
    FATALFAIL(s, "StartCPU 0");

    // Map the physical pages into kernel space/domain for CPU 0
    for (sect = kernelsections, n = nkernelsections;  n;  sect++, n--) {
        ULONG readmask = ACCESSMASK(0);
        ULONG writemask = sect->Writable? readmask : NOACCESS;

        for (i = 0;  i < sect->nPages;  i++) {
            s = MapMemory(0, KERNELCTX, ADDR2PAGE(sect->VirtualAddress) + i, sect->PhysicalBasepage + i, readmask, writemask);
            FATALFAIL(s, "Map Kernel Memory");
        }
    }

    // Map in the emulation portals.  Only the type and ACCESSMASK matters in the portal.
    kportalid = CreatePortal(0, emulationcall, 0, 0, EMULCTX, 0, ACCESSMASK(KERNELMODE));
    uportalid = CreatePortal(0, emulationcall, 0, 0, EMULCTX, 0, ACCESSMASK(KERNELMODE) | ACCESSMASK(USERMODE));
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_Noop,           0, uportalid);  FATALFAIL(s, "Map trapemul Noop");
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_MapMemory,      0, kportalid);  FATALFAIL(s, "Map trapemul MapMemory");
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_MapIO,          0, kportalid);  FATALFAIL(s, "Map trapemul MapIO");
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_MapTrap,        0, kportalid);  FATALFAIL(s, "Map trapemul MapTrap");
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_CreatePortal,   0, kportalid);  FATALFAIL(s, "Map trapemul CreatePortal");
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_DestroyPortal,  0, kportalid);  FATALFAIL(s, "Map trapemul DestroyPortal");
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_CleanCtx,       0, kportalid);  FATALFAIL(s, "Map trapemul CleanCtx");
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_Resume,         0, uportalid);  FATALFAIL(s, "Map trapemul Resume");
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_Suspend,        0, uportalid);  FATALFAIL(s, "Map trapemul Suspend");
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_Unsuspend,      0, uportalid);  FATALFAIL(s, "Map trapemul Unsuspend");
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_PopCaller,      0, uportalid);  FATALFAIL(s, "Map trapemul PopCaller");
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_DiscardToken,   0, uportalid);  FATALFAIL(s, "Map trapemul DiscardToken");
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_StartCPU,       0, kportalid);  FATALFAIL(s, "Map trapemul StartCPU");
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_InterruptCPU,   0, kportalid);  FATALFAIL(s, "Map trapemul InterruptCPU");
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_AccessDevice,   0, kportalid);  FATALFAIL(s, "Map trapemul AccessDevice");
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_ManageIRQL,     0, kportalid);  FATALFAIL(s, "Map trapemul ManageIRQL");
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_HaltCPU,        0, kportalid);  FATALFAIL(s, "Map trapemul HaltCPU");
    s = MapTrap(0, EMULCTX, trapemul, 1, TRAPEMUL_GetSPACEParams, 0, uportalid);  FATALFAIL(s, "Map trapemul GetSPACEParams");

    // Destroy the temporary portal (it won't actually be deleted because the references are held by the traps)
    (void) DestroyPortal(0, kportalid);
    (void) DestroyPortal(0, uportalid);

    // TODO: specify the actual handler to boot the system
    kportalid = CreatePortal(0, exception, KERNELMODE, 0, KERNELCTX, 0, ACCESSMASK(KERNELMODE) | ACCESSMASK(USERMODE));
    s = MapTrap(0, GLOBALCTX, trapexcept, 1, TRAPEXCEPT_ILLEGALOP, 0, kportalid);  FATALFAIL(s, "Map trapexcept ILLEGALOP");
    (void) DestroyPortal(0, kportalid);

    // Register the device emulations
    RegisterAllDevices();

    // Start SPACE Emulation loop
    SPACEEmulation();

    // Cleanup & Exit
    cleanup();
    printf("SPACE has shutdown.\n");
    return 0;
}

