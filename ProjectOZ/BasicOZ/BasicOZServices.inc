;;
;; Copyright (c) Microsoft Corporation. All rights reserved. 
;; 
;; You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
;; If you do not agree to the terms, do not use the code.
;;

; included in Machine.asm

; external declarations of C routines
    EXTERN C foo:PROC
;   EXTERN C ConsolePrint:PROC
;   EXTERN C ConsoleGetline:PROC

; public declarations of asm dispatch routines
    PUBLIC C dispatch_foo
;   PUBLIC C dispatch_ConsolePrint
;   PUBLIC C dispatch_ConsoleGetline

; define dynamic stubs for server side
    DynamicInvoke   foo
;   DynamicInvoke   ConsolePrint
;   DynamicInvoke   ConsoleGetline

; define stubs for client side
    SystemCall      foo,                BASICSERV_foo
;   SystemCall      ConsolePrint,       BASICSERV_ConsolePrint
;   SystemCall      ConsoleGetline,     BASICSERV_ConsoleGetline


