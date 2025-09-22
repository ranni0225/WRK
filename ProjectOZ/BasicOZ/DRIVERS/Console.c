//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include "..\BasicOZ.h"

//
// Console driver
//
// The console 'hardware' consists of one register and two operations:
//      CONSOLEOP_PRINT   - writes a mapped UNICODE buffer to the console
//      CONSOLEOP_GETLINE - reads a NUL terminated line into a mapped UNICODE buffer, truncating if necessary
// To get a mapped buffer the physical addresses of the page(s) containing the buffer are mapped into IO space by MapIO.
// The device register is accessed by
//      (void) SPACE_AccessDevice(CONSOLEDEVICE_ID, CONSOLEOP_PRINT,   CONSOLEDEVICE_IORegister, mappedconsolebuffer, noutputchars);
// or
//      (void) SPACE_AccessDevice(CONSOLEDEVICE_ID, CONSOLEOP_GETLINE, CONSOLEDEVICE_IORegister, mappedconsolebuffer, maxinputchars) 
//
// The driver provides two system calls:
//      boolean ConsolePrint  (ushort *virtualbuffer, ulong noutputchars)
//      boolean ConsoleGetline(ushort *virtualbuffer, ulong maxinputchars)
// So all the driver needs to do is handle the mapping of the buffer into IO space for the current address space.
//
//

extern ulong mappedconsolebuffer;

boolean
ConsolePrint(
        ushort *virtualbuffer,
        ulong   noutputchars
    )
{
    ulong noutputbytes  = noutputchars * sizeof(ushort);

    (void) SPACE_AccessDevice(CONSOLEDEVICE_ID, CONSOLEOP_PRINT,   CONSOLEDEVICE_IORegister, mappedconsolebuffer, noutputchars);
    return TRUE;
}

boolean
ConsoleGetline(
        ushort *virtualbuffer,
        ulong   maxinputchars
    )
{
    ulong maxinputbytes  = maxinputchars * sizeof(ushort);

    (void) SPACE_AccessDevice(CONSOLEDEVICE_ID, CONSOLEOP_GETLINE,   CONSOLEDEVICE_IORegister, mappedconsolebuffer, maxinputchars);
    return TRUE;
}

//
// Version of ConsolePrint to print from kernel mode, as well as some print utility routines
//
void
debugprint(
        ushort *virtualbuffer,
        ulong   noutputchars
    )
{
    ulong noutputbytes  = noutputchars * sizeof(ushort);

    (void) SPACE_AccessDevice(CONSOLEDEVICE_ID, CONSOLEOP_PRINT,   CONSOLEDEVICE_IORegister, mappedconsolebuffer, noutputchars);
}

static
ushort *
tohex(
        ushort *buffer,
        ulong   value
    )
{
    int     i;

    for (i = 0;  i < 8;  i++)
        buffer[i] = L"0123456789ABCDEF"[(value>>(28-4*i)) & 0xf];
    buffer[8] = L'\0';
    return buffer;
}

void
debugprintx(ulong x)
{
    ushort buffer[9];
    debugprint(tohex(buffer, x), sizeof(buffer)-1);
}

