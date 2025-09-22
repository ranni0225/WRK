;;
;; Copyright (c) Microsoft Corporation. All rights reserved. 
;; 
;; You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
;; If you do not agree to the terms, do not use the code.
;;

; NOTE: Definitions must match SPACE\TrapValues.h
; NOTE: TRAPEMUL_* must match Bootstrap.asm

;
; trap types
;

; emulated SPACE hardware instructions, like resume
;
     TRAPEMUL_Noop            equ  0
     TRAPEMUL_MapMemory       equ  1
     TRAPEMUL_MapIO           equ  2
     TRAPEMUL_MapTrap         equ  3
     TRAPEMUL_CreatePortal    equ  4
     TRAPEMUL_DestroyPortal   equ  5
     TRAPEMUL_CleanCtx        equ  6
     TRAPEMUL_Resume          equ  7
     TRAPEMUL_Suspend         equ  8
     TRAPEMUL_Unsuspend       equ  9
     TRAPEMUL_PopCaller       equ 10
     TRAPEMUL_DiscardToken    equ 11
     TRAPEMUL_StartCPU        equ 12
     TRAPEMUL_InterruptCPU    equ 13
     TRAPEMUL_AccessDevice    equ 14
     TRAPEMUL_ManageIRQL      equ 15
     TRAPEMUL_HaltCPU         equ 16
     TRAPEMUL_GetSPACEParams  equ 17
     TRAPEMUL_SPACEBreak      equ 18

; implement service call
     TRAPSERV_MAXCALLS        equ 256

