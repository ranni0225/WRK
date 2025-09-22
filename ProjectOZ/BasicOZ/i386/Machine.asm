;;
;; Copyright (c) Microsoft Corporation. All rights reserved. 
;; 
;; You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
;; If you do not agree to the terms, do not use the code.
;;

TITLE   "BasicOZ machine-level code"

include ..\INC\TrapValues.inc
include Config.inc
include Syscalls.inc

.686
;;.model flat,c

;
; Machine-level code for BasicOZ
;
;;.data
_DATA   SEGMENT PAGE PUBLIC 'DATA'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING
        ORG     $ + 64*1024-4        ; aligned 4KB stack
        mainstacktop        DWORD   0

; variables containing bootstrap vectors
        PUBLIC C _pemulationvector
        PUBLIC C _pservicevector
        PUBLIC C _plibraryvector
        _pemulationvector   DWORD   0
        _pservicevector     DWORD   0
        _plibraryvector     DWORD   0

; variables for dynamic stack allocation
; stacks are linked by integers not pointers, so that a sizable sequence number can be used in 32-bit Compare&Swap operations
; the stacks themselves are pointed by at by pstackvector[], but the pstackvector[] entries are not updated except when
; an allocated stack is deleted
;
; the stacks are linked through an index stored at the beginning of each stack region
; stacks are size DYNSTACK_ALLOCSIZE

        _stackhead          DWORD   DYNSTACK_EMPTYINDEX ; sequence [20b] / index [12b] (empty)
        _dynamicstackhint   DWORD   0                   ; where to start searching for an available index
        _dynamicstackuninit DWORD   1                   ; no dynamic stacks have been allocated
        _pstackvector       DWORD   0
        ORG     $ + 4*(MAXDYNAMICSTACKS-2)
                            DWORD   0                   ; need to allocate something or space from ORG is released since at end of segment

_DATA   ENDS
        EXTERN  _boot:proc
;;.code
_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

        SUBTTL "BasicOZ boot"
        PUBLIC C _basicoz
_basicoz:
        ; establish system pointers
        mov     eax,                EMULATIONVECTOR
        mov     eax,                [eax]
        mov     _pemulationvector,  eax

        mov     eax,                SERVICEVECTOR
        mov     eax,                [eax]
        mov     _pservicevector,    eax

        mov     eax,                LIBRARYVECTOR
        mov     eax,                [eax]
        mov     _plibraryvector,    eax

        ; setup the boot stack
        lea     esp,                mainstacktop
        mov     ebp,                esp

        xor     eax,eax

        call    _boot
@@:
        mov     eax,                _pemulationvector
        jmp     dword ptr [eax+4*TRAPEMUL_Resume]
        jmp     @B
        ;notreached

; Macro for dynamic allocation of a stack for this domain and then calling a C function
; If the pool of dynamic stacks is exhausted, we trap (will wait and then retry)
; N.B. This is only a macro so we can save a register, otherwise handler could be passed in to a routine.

DynamicInvoke macro  handler
    EXTERNDEF  C dispatch_&handler&:PROC
    EXTERNDEF  C &handler&:PROC
    ;; available registers:  eax, esp, ebp
    ;; parameter registers:  ebx, ecx, edx, edi, esi
    ;; set ebp, esp to stack when done, with parameters pushed onto stack
dispatch_&handler& LABEL PROC
    mov         eax,                _stackhead              ;; head sequence/index
    mov         ebp,                eax                     ;; temporarily stash [_stackhead]
    and         eax,                DYNSTACK_INDEXMASK      ;; extract index
    cmp         eax,                DYNSTACK_EMPTYINDEX
    jnz         @F
    ;; XXX: how do we get to our sleep parameters without clobbering the original parameter registers??
    lea         eax,                _stackhead              ;; &_stackhead is pattern for wakeup
    mov         esp,                _pservicevector         ;;
    jmp         dword ptr [esp+4*BASICSERV_NOSTACK_SLEEP]   ;;
    ;; XXX: how to we really get back here????????????????????????????????????????????????????????????????????????????????????????
    jmp         dispatch_&handler&                          ;; restart after trap returns
@@:
    ;; ebp is [_stackhead]
    ;; eax is head index isolated from ebp
    mov         esp,                _pstackvector[4*eax]    ;; base of the allocated stack, address of 'next' index
    mov         eax,                ebp
    and         eax,                DYNSTACK_SEQUENCEMASK   ;; extract sequence from head sequence/index
    add         eax,                DYNSTACK_SEQUENCEINCR   ;; increment sequence
    or          eax,                [esp]                   ;; insert index of next stack
    mov         esp,                eax
    mov         eax,                ebp                     ;; retrieve [_stackhead] into EAX for cmpxchg
    ;; ebp remembers the allocated sequence/index
    ;; esp is new head sequence/index
    ;; eax is old head sequence/index
    lock cmpxchg dword ptr _stackhead, esp                  ;; update the head if eax == [_stackhead]
    jne         dispatch_&handler&                          ;; if failed, restart at the top
    ;; esp now scratch
    ;; eax is old head sequence/index
    and         eax,                DYNSTACK_INDEXMASK      ;; extract index
    mov         esp,                _pstackvector[4*eax]
    add         esp,                DYNSTACK_ALLOCSIZE      ;; points just off the allocatedstack
    ;; remember our stack index on the stack
    and         ebp,                DYNSTACK_INDEXMASK
    push        ebp
    ;; push parameters
    push        esi
    push        edi
    push        edx
    push        ecx
    push        ebx
    ;; clear the registers (for debugging sanity)
    xor         eax,                eax
    mov         ebp,                eax
    mov         esi,                eax
    mov         edi,                eax
    mov         edx,                eax
    mov         ecx,                eax
    mov         ebx,                eax
    call        &handler&

    ; free the stack and RESUME
    jmp         trapResume
    endm

        EXTERNDEF C trapResume:Proc
trapResume LABEL PROC
    ; esp points into the allocated stack or is NULL if there was no stack
    ; eax/edx contain the return values -- so must preserve them
    ; all other registers are scratch
    mov         esi,                _pemulationvector
    or          esp,                esp                     ; if ESP is NULL just trap to emulated resume instruction
    jnz         @F
    jmp         dword ptr [esi+4*TRAPEMUL_Resume]
@@:
    ; get our stack index
    and         esp,                NOT DYNSTACK_BASEMASK
    mov         ebx,                (DYNSTACK_ALLOCSIZE-4)[esp]

    ; ASSERT: ebx < MAXDYNAMICSTACKS && _pstackvector[ebx] == esp
    mov         esi,                _pservicevector
    cmp         ebx,                MAXDYNAMICSTACKS
    jb          @F
    jmp         dword ptr [esi+4*BASICSERV_NOSTACK_BUGCHECK] ; with ESP at stack allocation, EBX stack index
@@:
    cmp         esp,                _pstackvector[4*ebx]
    je          @F
    jmp         dword ptr [esi+4*BASICSERV_NOSTACK_BUGCHECK] ; with ESP at stack allocation, EBX stack index
@@:

    ; esp points at the base of the stack allocation
    ; ebx is the stack index
    ; eax/edx are the return values
    mov         ebp,                eax                     ; stash eax, as we will need the register for cmpxchg
    mov         eax,                _stackhead              ; [_stackhead]
@@:
    mov         ecx,                eax  
    and         ecx,                DYNSTACK_INDEXMASK      ; extract head index
    mov         [esp],              ecx                     ; put old index into the next link in the stack we are freeing
    mov         ecx,                eax
    and         ecx,                DYNSTACK_SEQUENCEMASK   ; extract sequence
    add         ecx,                DYNSTACK_SEQUENCEINCR   ; increment sequence
    or          ecx,                ebx                     ; insert new index

    lock cmpxchg dword ptr _stackhead, ecx                  ; update the head to point at freed stack if eax == [_stackhead]
    jne         @B                                          ; if _stackhead changed since we read it, retry (eax has new _stackhead)

    ; if we freed the only stack and it isn't the first stack, let somebody wakeup and run on it now
    ; ebp/edx are the return values

    mov         esi,                _pservicevector
    and         eax,                DYNSTACK_INDEXMASK      ; extract previous head index
    cmp         eax,                DYNSTACK_EMPTYINDEX
    jne         @F
    lea         ecx,                _stackhead              ;; &_stackhead is pattern for wakeup (passed in ecx)
    mov         eax,                ebp                     ;; restore EAX value  (EDX value wasn't changed)
    jmp         dword ptr [esi+4*BASICSERV_NOSTACK_WAKEUP]  ;; must preserve EAX/EDX (the return values) and call Resume for us
@@:
    ; eax gets the return value
    ; callee must preserve edi, as it has our return value
    mov         eax,                ebp                     ;; restore EAX value  (EDX value wasn't changed)
    mov         esi,                _pemulationvector



    jmp         dword ptr [esi+4*TRAPEMUL_Resume]
    ;notreached

;
; routine to add a dynamic stacks
; argument is the base of a stack allocation of size DYNSTACK_ALLOCSIZE and must be aligned
; returns 1 if successful, or 0 if there already were MAXDYNAMICSTACKS
;
    EXTERNDEF C AddDynamicStack:Proc
AddDynamicStack LABEL PROC
    ; scratch registers:  eax ecx edx
    ; preserve registers: ebx esi edi (and ebp/esp of course)
    ; find empty location in pstackvector

    mov         ecx,                _dynamicstackhint       ; index hint
@@:
    ; search for index, starting at the hint
    ; (the stack indices may be sparse, if we start reclaiming stacks as we will likely pick whatever was at the front of the list)
    mov         eax,                _pstackvector[4*ecx]
    or          eax,                eax
    jz          @F                                          ; found first unused index
    inc         ecx
    cmp         ecx,                MAXDYNAMICSTACKS
    jl          @B

    ; return failure
    xor         eax,                eax
    ret
@@:
    ; save a new hint
    mov         eax,                ecx
    inc         eax
    mov         _dynamicstackhint,  eax

    ; write stack allocation to vector
    mov         edx,                4[esp]                  ; get stack allocation
    mov         _pstackvector[4*ecx],  edx
    
    ; insert the new stack at the head
    ;   ecx is the index of the new stack
    ;   edx is the location where we write the next index (i.e. the base of the stack allocation)
    ;   eax is scratch
    ;   save ebx and esi so we can use them too
    push        ebx
    push        esi
    mov         eax,                _stackhead              ; [_stackhead]
@@:
    mov         ebx,                eax  
    and         ebx,                DYNSTACK_INDEXMASK      ; extract head index
    mov         [edx],              ebx                     ; put old head index into the next link in the stack we are freeing
    mov         esi,                eax
    and         esi,                DYNSTACK_SEQUENCEMASK   ; extract sequence
    add         esi,                DYNSTACK_SEQUENCEINCR   ; increment sequence
    or          esi,                ecx                     ; insert new index

    lock cmpxchg dword ptr _stackhead, esi                  ; update the head to point at freed stack if eax == [_stackhead]
    jne         @B                                          ; if _stackhead changed since we read it, retry (eax has new _stackhead]

    ; if this is the first initialization, or there was already a free stack, we are done
    mov         esi,                _dynamicstackuninit
    or          esi,                esi
    jnz         @F
    cmp         ebx,                DYNSTACK_EMPTYINDEX
    jne         @F

    ; else let somebody wakeup and run on the stack we just added  (our caller should beware!)
    ; restore ebx and esi
    pop         esi
    pop         ebx

    lea         ecx,                _stackhead              ;; &_stackhead is pattern for wakeup (passed in ecx)
    mov         eax,                _pservicevector
    jmp         dword ptr [eax+4*BASICSERV_NOSTACK_WAKEUP]  ;; must return EAX=0 to our caller when it eventually calls Resume to return the CPU

@@:
    ; set _dynamicstackuninit = FALSE
    xor         ebx,                 ebx
    mov         _dynamicstackuninit, ebx                    ;; we have now initialized the first stack

    ; restore ebx and esi and return success
    pop         esi
    pop         ebx
    mov         eax,                1
    ret

;
; Handle the rest of the hardware emulation calls
;

EmulationCall macro  name
    EXTERNDEF C trap&name:PROC
trap&name& LABEL PROC
    mov         eax,                _pemulationvector
    mov         eax,                [eax+4*TRAPEMUL_&name]
    jmp         eax
    endm

    EmulationCall Noop
    EmulationCall MapMemory
    EmulationCall MapIO
    EmulationCall MapTrap
    EmulationCall CreatePortal
    EmulationCall DestroyPortal
    EmulationCall CleanCtx
    ; Special:    Resume            ; trapResume has to release the stack, so it is handcoded above
    EmulationCall Suspend
    EmulationCall Unsuspend
    EmulationCall PopCaller
    EmulationCall DiscardToken
    EmulationCall StartCPU
    EmulationCall InterruptCPU
    EmulationCall AccessDevice
    EmulationCall ManageIRQL
    EmulationCall HaltCPU
    EmulationCall GetSPACEParams
    EmulationCall SPACEBreak

;
; Define the dispatch entries here
;
; System calls implemented by BasicOZ are defined in the included file
;


SystemCall macro  name,index
    EXTERNDEF C sys_&name:PROC
sys_&name& LABEL PROC
    mov         eax,                _pservicevector
    mov         eax,                [eax+4*index]
    jmp         eax
    endm

include BasicOZServices.inc

_TEXT   ENDS

; End of file
    end

