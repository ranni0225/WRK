//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

boolean
sys_ConsolePrint(
        ushort *virtualbuffer,
        ulong   noutputchars
    );

boolean
sys_ConsoleGetline(
        ushort *virtualbuffer,
        ulong   maxinputchars
    );

ulong
sys_foo();

