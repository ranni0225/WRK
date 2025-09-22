//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include "BasicOZ.h"

void
boot()
{
}

void consolePrint(ushort msg[]);

extern dispatch_foo();
ulong
foo()
{
    consolePrint(L"In foo - now returning\n");
    return 0xfeed;
}


ulong physbasepage;
ulong nphysmempages;

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

// Send
// Receive

ushort consolebuffer[512] = L"Hello Outer SPACE!\n";
ulong  mappedconsolebuffer;
ulong  maxoutputchars = sizeof(consolebuffer) / sizeof(consolebuffer[0]) - 1;
ulong  maxinputchars  = sizeof(consolebuffer) / sizeof(consolebuffer[0]) - 1;

ulong
mapconsolebuffer()
{
    ulong page0, page1;

    // XXX: reserving iopage 0/1 for our consolebuffer
    // XXX: assuming we know the mapping of the kernel pages -- which we really need to (somehow) get when we are booted
    // XXX: maybe by accessing our own image??
    // XXX: for now we happen to know that:  60010000 is physical page 0
    page0 = ADDR2PAGE((ulong)consolebuffer - 0x60010000);
    page1 = ADDR2PAGE((ulong)consolebuffer - 0x60010000 + sizeof(consolebuffer));

    SPACE_MapIO(CONSOLEDEVICE_ID, 1, 0, page0);
    if (page1 != page0)
        SPACE_MapIO(CONSOLEDEVICE_ID, 1, 1, page1);

    return 0 + ((NTSPACE_PAGESIZE-1) & (ulong)consolebuffer);   // logical map is iospace 0, so just add offset of consolebuffer
}

void
consolePrint(ushort msg[])
{
    ulong  n = 0;

    if (mappedconsolebuffer == 0)
            mappedconsolebuffer = mapconsolebuffer();

    n = 0;
    while (msg[n] && n < maxoutputchars) consolebuffer[n] = msg[n], n++;
    consolebuffer[n] = L'0';

    (void) SPACE_AccessDevice(CONSOLEDEVICE_ID, CONSOLEOP_PRINT, CONSOLEDEVICE_IORegister, mappedconsolebuffer, n);
}

void
consolePrintX(ulong x)
{
    ushort buffer[9];
    consolePrint(tohex(buffer, x));
}

ushort *
consoleGetline()
{
    if (mappedconsolebuffer == 0)
            mappedconsolebuffer = mapconsolebuffer();
    (void) SPACE_AccessDevice(CONSOLEDEVICE_ID, CONSOLEOP_GETLINE, CONSOLEDEVICE_IORegister, mappedconsolebuffer, maxinputchars);

    return consolebuffer;
}

