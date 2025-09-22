;;
;; Copyright (c) Microsoft Corporation. All rights reserved. 
;; 
;; You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
;; If you do not agree to the terms, do not use the code.
;;

TITLE   "SPACE NT process bootstrap executable"

include ..\INC\TrapValues.inc

.686
.model flat,c

defemulcall macro icall
        DWORD           emul_&icall&
endm

defservcall macro icall
        DWORD           OZ_&icall&
endm

;
; assumes a stack is setup
; services not using a stack have to be invoked more directly
;
SPACECall macro  emulcall,icall,N,indirflag,vectoroutflag
    if emulcall gt 0
        emul_&icall& LABEL PROC
    else
        OZ_&icall&   LABEL PROC
    endif

    ;; get argument pointer
    if indirflag gt 0
        mov  eax,  4[esp]
    else
        lea  eax,  4[esp]
    endif

        ;; save registers -- just in case
        push ebx
        push edi
        push esi

        ;; pass 0-5 parameters via the registers
    if N gt 0
            mov  ebx,   [eax]
     if N gt 1
            mov  ecx,  4[eax]
      if N gt 2
            mov  edx,  8[eax]
       if N gt 3
            mov  edi, 12[eax]
        if N gt 4
            mov  esi, 16[eax]
         if N gt 5
            .ERR too many parameters for emulation call
         endif
        endif
       endif
      endif
     endif
    endif

    if emulcall gt 0
        ;; emulation call
        BYTE            0f0h, 0fh, 0bh      ;; lock ud2 - is the illegal instruction for emulation calls
        BYTE            TRAPEMUL_&icall     ;; call index
    else
        BYTE            0fh, 0bh            ;; ud2 - is the illegal instruction for service calls
        WORD            icall               ;; call index
    endif

    if vectoroutflag gt 0
        ;; return parameters
        mov  eax,   16[esp]                 ;; 4 + the C we pushed
        mov    [eax],  ebx
        mov   4[eax],  ecx
        mov   8[eax],  edx
        mov  12[eax],  edi
        mov  16[eax],  esi
        xor  eax,      eax
    endif

        ;; restore registers
        pop  esi
        pop  edi
        pop  ebx

        ;; return (return values are in eax/edx -- except if vectoroutflag)
        ret
endm


;
; initial trap to SPACE for NT threads
; implementing CPUs in domains
;
.code
        PAGE
        SUBTTL "SPACE 'CPU' bootstrap code"
        EXTERNDEF  C bootspacecpu:PROC

; pointers stored Hat known absolute address so we don't have to link
; Linked at:  71001000
        DWORD   emulationvector
        DWORD   servicevector
        DWORD   libraryvector

; must match TrapTypes.h
emulationvector:
        defemulcall  Noop           ;  0
        defemulcall  MapMemory      ;  1
        defemulcall  MapIO          ;  2
        defemulcall  MapTrap        ;  3
        defemulcall  CreatePortal   ;  4
        defemulcall  DestroyPortal  ;  5
        defemulcall  CleanCtx       ;  6
        defemulcall  Resume         ;  7
        defemulcall  Suspend        ;  8
        defemulcall  Unsuspend      ;  9
        defemulcall  PopCaller      ; 10
        defemulcall  DiscardToken   ; 11
        defemulcall  StartCPU       ; 12
        defemulcall  InterruptCPU   ; 13
        defemulcall  AccessDevice   ; 14
        defemulcall  ManageIrql     ; 15
        defemulcall  HaltCPU        ; 16
        defemulcall  GetSPACEParams ; 17
        defemulcall  SPACEBreak     ; 18

; must match SystemLib.h
        extern  StudyImage:PROC     ;   SPACEStatus StudyImage(void *imagebase, ulong imagesize, Section imagesections[], int *nimagesections)
libraryvector:
        DWORD   StudyImage          ;  0

;
; bootstrap call -- immediately invokes Resume instruction, passing the NT stack pointer as result
;
bootspacecpu:
        xor             ebx, ebx            ; set result to 0
        ; fall through to Resume

;
; Resume
; Handling is special, because we don't return, and our stack is already deleted 
; ebx already contains the result value
;
emul_Resume:
        BYTE 0f0h, 0fh, 0bh                 ; lock ud2 - is the illegal instruction for emulation calls
        BYTE TRAPEMUL_Resume                ; Resume()
        ;; not reached

;
; emit emulation calls - calling conventions must agree with definitions in SPACEOps.h, SPACEOps.c, and Bootstrap.asm
; third  parameter is 0 if a scalar call, 1 if a vector call, 2 if vector in/out call
;
;SPACECall: emulcall, icall,    N,indirflag,vectoroutflag   ; description
    SPACECall	1, Noop,			0, 0, 0 ; void  Noop()
    SPACECall	1, MapMemory,		5, 1, 0 ; void  MapMemory(ulong ctx, ulong vpage, ulong ppage, ulong readmask, ulong writemask)
    SPACECall	1, MapIO,			3, 1, 0 ; void  MapIO(ushort deviceid, uchar npages, uchar writable, ulong lbasepage, ulong ppage[])
    SPACECall	1, MapTrap,		    5, 1, 0 ; void  MapTrap(ulong ctx, uchar traptype, uchar global, 
                                            ;                     ulong indexbase, ulong indexlimit, ulong portalid)
    SPACECall	1, CreatePortal,    4, 1, 0 ; ulong portalid = CreatePortal(uchar type, uchar suspendflag, uchar mode, uchar irql,
                                            ;                               ulong ctx, ulong handler, ulong protmask)
    SPACECall	1, DestroyPortal,	1, 0, 0 ; void  DestroyPortal(ulong portalid)
    SPACECall	1, CleanCtx,		1, 0, 0 ; void  CleanCtx(ulong ctx)
;   SPACECall	1, Resume,			1, 0, 0 ; void  Resume(ulong result) -- cannot use SPACEcall because OZ already deleted stack
    SPACECall	1, Suspend,		    0, 1, 0 ; SPACETOKEN Suspend() - returns token
    SPACECall	1, Unsuspend,		2, 0, 0 ; void  Unsuspend(SPACETOKEN token)
    SPACECall	1, PopCaller,		0, 0, 0 ; void  PopCaller()
    SPACECall	1, DiscardToken,	2, 0, 0 ; void  DiscardToken(SPACETOKEN token)
    SPACECall	1, StartCPU,		5, 1, 0 ; void  StartCPU(ulong boot_cpu, ulong boot_ctx, ulong boot_mode, ulong boot_pc, ulong boot_arg)
    SPACECall	1, InterruptCPU,	1, 1, 0 ; void  InterruptCPU(uchar targetcpu, uchar interrupt, uchar irql)
    SPACECall	1, AccessDevice,	3, 1, 0 ; ulong AccessDevice(ushort deviceid, uchar op, uchar deviceregister, ulong value, ulong valuex)
    SPACECall	1, ManageIrql,		1, 1, 0 ; ulong ManageIrql(IrqlOps op, uchar irql)
    SPACECall	1, HaltCPU,		    0, 0, 0 ; void  HaltCPU()
    SPACECall	1, GetSPACEParams,	5, 1, 1 ; void  GetSPACEParams(...)
    SPACECall	1, SPACEBreak,	    5, 1, 1 ; void  SPACEBreak(ulong breakvalue, uchar breakmsg[16])


; the table of service calls for BasicOZ:
; emit actual service calls
        icall = 0
        while icall lt TRAPSERV_MAXCALLS
            ; XXX: nothing indirect or vectorout (so far), but could conditionally operate on the icall encodings if we wanted to add this
            SPACECall	0, %(icall), 5, 0, 0 ; ulong64 ServiceCall(arg0, arg1, arg2, arg3, arg4)
            icall = (icall+1)
        endm

; emit vector
servicevector:
        icall = 0
        while icall lt TRAPSERV_MAXCALLS
            defservcall %(icall)
            icall = (icall+1)
        endm

; end of file
    end

