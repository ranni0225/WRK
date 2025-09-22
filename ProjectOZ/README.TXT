Copyright (c) Microsoft Corporation

Overview
--------
ProjectOZ is an experimental environment based on the SPACE abstractions for
the CPU, MMU, and trap mechanisms (search: probert bruno SPACE).  ProjectOZ
implements these abstractions using the native NTAPI of the Windows kernel,
including features specific to building user-mode operating system
personalities (aka NT subsystems).

Because there is a real OS underneath handling the hardware details rather
than a simulator, students should find it easier to explore kernels at
the algorithm and data structure level rather than worrying about so many
low-level details.

ProjectOZ is provided in source form to universities worldwide. ProjectOZ is
continuing to evolve as contributors join the project.  Information on downloading
the latest version is available at:

    http://www.msdnaacr.net/curriculum/pfv.aspx?ID=6547

There is also a community forum for discussions related to ProjectOZ:

    http://forums.microsoft.com/WindowsAcademic/ShowForum.aspx?ForumID=196&SiteID=8

The Windows Kernel and the Windows Academic Program teams can be contacted at

    compsci@microsoft.com


Architecture
------------
The SPACE abstractions are implemented in a user-mode program (SPACE.exe)
which runs as a native subsystem process under Windows.  Students run BasicOZ
on top of SPACE, using SPACE to provide the basic hardware abstractions.
By modifying BasicOZ students implement various projects which improve
the rather simple capabilities of BasicOZ.

Multiple instances of SPACE.exe can run on a single machine, effectively
implementing a multicomputer upon which students can experiment
with distributed algorithms.

SPACE.exe supports an extensible set of emulated devices.  Each device
is presented to BasicOZ as a set of registers.  The device emulations can
cause interrupts and perform DMA operations to memory

SPACE Primitives
----------------
SPACE is based on the idea that threads, virtual memory, and IPC are a bad
semantic match as the low-level abstractions in a system.  What actually exists
in the hardware is the CPU, MMU, and trap/interrupt mechanism.  Threads, virtual
memory, etc, should be higher-level abstractions built on top of fundamental
abstractions representing the hardware:

    CPU  - sequences through instructions while referencing memory.
    MMU  - maps CPU virtual addresses into physical addresses, for valid mappings
    TRAP - an interruption in the normal sequencing of a CPU due to an invalid
           memory mapping, a trap, exception, or an external interrupt.

    CONTEXT - a set of MMU mappings is represented by a CONTEXT value (e.g. a
          hardware contextid, or the root of the page table (CR3 on x86).  The
          permissible operations for each mapping are determined by the CPU MODE.
    MODE - the current CPU MODE selects how permission bits are applied from each
          mapping in the current CONTEXT (analogous to Ring0..Ring3 on x86 hardware).
    PORTAL - for each CONTEXT maps hardware TRAPs to a handler, including
          specification of a new CONTEXT and MODE as well as the handler itself
    PCB  - when a CPU traverses a PORTAL, the previous execution environment (CONTEXT,
          MODE, Program Counter, and CPU registers) are recorded in a PCB (Processor
          Control Block).  PCBs are normally chained each time a new TRAP occurs.

SPACE Operations
----------------
The basic operations implemented by SPACE.exe manipulate the SPACE primitives.
    MapMemory(ctx, virtual, phys, modeaccess)

    MapPortal(ctx, trap, pctx, pmode, ppc, modeaccess)

    Resume()
    token = Suspend()
    Unsuspend(token)

MapMemory() and MapPortal() set CONTEXT and PORTAL mappings for a specific CONTEXT.

Portal traversal is implicit in response to a TRAP (including deliberately executing
an instruction that causes a TRAP).  The interrupt execution environment is recorded
in a PCB and subsequent portal traversals chain PCBs as a stack.

Resume() restores the most recent PCB (analogous to a return-from-interrupt instruction).
Suspend() breaks the current chain by assigning a token to it and creating a new chain.
Unsuspend() restores a previous chain.

NT Facilities used to implement SPACE
-------------------------------------
Objects
    Threads 	    - NT unit of CPU scheduling
    Processes 	    - NT virtual address space container
    Sections 	    - NT sharable memory objects
    Exception port  - NT mechanism for subsystem fault handling

Functions
    MapView	        - Map process addresses to section offsets
    Wait/Reply port - Receive/Send message to port

SPACE Implementation
--------------------
CPUs are implemented using NT threads.  MMUs are built using NT processes.
Because NT threads are bound to processes, each SPACE CONTEXT/MODE is represented
as an NT process with an NT thread for each SPACE CPU.
SPACE.exe listens on the exception port for the NT processes, which allows it to
manipulate the state of the NT threads to chain PCBs, implement portal traversal
(including for memory faults) and other SPACE operations.

BasicOZ is loaded by SPACE.exe, and then communicates by causing traps which SPACE.exe
handles to implement the SPACE 'hardware' emulation.

Workloads, Instrumentation, Community
-------------------------------------
If ProjectOZ proves to be a useful environment for teaching operating systems,
then over time much of the support will come from the academic community itself.
ProjectOZ will be extended to improve the accuracy of the instrumentation available
to students, and workloads will be developed that allow student projects to be
more accurately contrasted & compared.

Using WRK, the Windows Kernel itself may be extended to provide some of the
facilities that are currently not available to ProjectOZ (such as thread-virtual
time, reference & dirty bits for the shared memory section, 64KB pages).

PREVIEW INFORMATION
-------------------
The ProjectOZ preview makes information about the NTAPI mechanisms used by ProjectOZ
available to interested faculty and our collaborators.

There is prototype code for SPACE.exe which implements the SPACE primitives.  It will
boot the skeletal BasicOZ and demonstrate how the trap, MMU, and CPU mechanisms work.

There will be a continuous stream of drops of ProjectOZ over the summer with increasing
amount of functionality in BasicOZ, as well as development of the first projects.

If you want to build and play with the code, you need the July WRK for the tools:
    set  wrk=the WRK directory
    set  projectoz=the ProjectOZ directory
    path %wrk%\tools\x86;%path%
    nmake -nologo x86=

To get on the list for notification of updates contact me directly, or if you'd rather
poll -- just check the curriculum website periodically.

    http://www.msdnaacr.net/curriculum/pfv.aspx?ID=6547

- DavePr@microsoft.com

Updated: 2006/07/14

