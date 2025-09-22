//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

//
// trap types
//
#define TRAPTYPE_EMULATION    0      // emulated SPACE hardware instructions, like resume
         // N.B.: TRAPEMUL_* must match Bootstrap.asm
#define     TRAPEMUL_Noop             0
#define     TRAPEMUL_MapMemory        1
#define     TRAPEMUL_MapIO            2
#define     TRAPEMUL_MapTrap          3
#define     TRAPEMUL_CreatePortal     4
#define     TRAPEMUL_DestroyPortal    5
#define     TRAPEMUL_CleanCtx         6
#define     TRAPEMUL_Resume           7
#define     TRAPEMUL_Suspend          8
#define     TRAPEMUL_Unsuspend        9
#define     TRAPEMUL_PopCaller       10
#define     TRAPEMUL_DiscardToken    11
#define     TRAPEMUL_StartCPU        12
#define     TRAPEMUL_InterruptCPU    13
#define     TRAPEMUL_AccessDevice    14
#define     TRAPEMUL_ManageIRQL      15
#define     TRAPEMUL_HaltCPU         16
#define     TRAPEMUL_GetSPACEParams  17
#define     TRAPEMUL_Break           18
#define     NTRAP_EMULCALLS          19

#define TRAPTYPE_SERVICETRAP  1      // implement service call
#define     NTRAP_SERVCALLS         256

#define TRAPTYPE_INTERRUPT    2      // device interrupts
#define     NTRAP_INTERRUPTS         64

#define TRAPTYPE_EXCEPTION    3      // exceptions use intel trap numbers
#define     TRAPEXCEPT_DIVIDE         0
#define     TRAPEXCEPT_DEBUG          1
#define     TRAPEXCEPT_BREAKPOINT     3
#define     TRAPEXCEPT_INVALIDOPCODE  4
#define     TRAPEXCEPT_ILLEGALOP      5
#define     TRAPEXCEPT_ACCESSDENIED   6
#define     NTRAP_EXCEPTIONS          7

#define TRAPTYPE_FAULT        4      // pagefaults and protection faults
#define     TRAPFAULT_ACCESS          1
#define     TRAPFAULT_STACK           2 // N.B.: mapped to TRAPFAULT_ACCESS
#define     TRAPFAULT_PROTECTION      3 // N.B.: mapped to TRAPFAULT_ACCESS
#define     NTRAP_FAULTS              4

#define NTRAPTYPES            5      // Number of valid traptypes

