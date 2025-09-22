;;
;; Copyright (c) Microsoft Corporation. All rights reserved. 
;; 
;; You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
;; If you do not agree to the terms, do not use the code.
;;

    MAXDYNAMICSTACKS                    equ     1024            ;; must fit in 12 bits (i.e. <= 4094)

    BASEVECTOR                          equ     071001000H
    EMULATIONVECTOR                     equ     (BASEVECTOR +  0)
    SERVICEVECTOR                       equ     (BASEVECTOR +  4)
    LIBRARYVECTOR                       equ     (BASEVECTOR +  8)

    NOSTACKS_PARAM_EMPTYLIST            equ     0
    NOSTACKS_PARAM_STACKCORRUPT         equ     1
    NOSTACKS_PARAM_LISTNOLONGEREMPTY    equ     2

;; one guard page, one stack page - 64KB each
    DYNSTACK_ALLOCSIZE                  equ     (64*1024)       ;; NTSPACE_PAGESIZE
    DYNSTACK_BASEMASK                   equ     (DYNSTACK_ALLOCSIZE-1)
    DYNSTACK_INDEXMASK                  equ     0fffH
    DYNSTACK_EMPTYINDEX                 equ     DYNSTACK_INDEXMASK

    DYNSTACK_SEQUENCEMASK               equ     0fffff000H
    DYNSTACK_SEQUENCEINCR               equ     000001000H

