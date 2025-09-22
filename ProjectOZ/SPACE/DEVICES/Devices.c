//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#define DEVICES

#include "Devices.h"

#include "..\INC\DeviceDefs.h"

//
// Define a 'console' device for outputing messages
//
//
#define CONSOLEDEVICE_IRQL         2
#define CONSOLEDEVICE_INTERRUPT    2

struct {
    HANDLE           consolehandle;
} consoledevice;


ULONGLONG
ConsoledeviceAccess(
        ULONG cpu,              // ignored.  device accessible on every CPU
        int op,
        ULONG deviceregister,
        ULONG value,            // address of buffer in iospace
        ULONG valuex            // character count
    )
{
    WCHAR               outbuf[2048];
    WCHAR              *wp;
    ULONG               outbytes;
    int                 n;

//STRACE("[deviceregister, value, valuex, x]", deviceregister, value, valuex, 0)
    outbytes = sizeof(outbuf) - sizeof(outbuf[0]);
    valuex *= sizeof(WCHAR);
    if (outbytes > valuex) {
        outbytes = valuex;
    }

    switch (deviceregister) {
    case CONSOLEDEVICE_IORegister:
        switch (op) {
        case CONSOLEOP_PRINT:
            // synchronously copy from logical address to our device buffer, for outbytes
            (void) StartDMA(cpu, CONSOLEDEVICE_ID, (PCHAR)outbuf, (ULONG_PTR)value, outbytes, FALSE, NULL, NULL);
            outbuf[outbytes / sizeof(outbuf[0])] = 0;
            // write out our device buffer
            printf("%ws", outbuf);
            break;

        case CONSOLEOP_GETLINE:
            printf("INPUT> ");
            wp = fgetws(outbuf, outbytes/sizeof(WCHAR), stdin);
            if (wp == NULL)
                return 0;
            n = 1;  while (*wp++)  n++; // count characters, including nul

            // pseudo-DMA back to 'physical memory'
            // synchronously copy string in our device buffer to the logical address in 'physical' memory
            (void) StartDMA(cpu, CONSOLEDEVICE_ID, (PCHAR)outbuf, (ULONG_PTR)value, n*sizeof(WCHAR), TRUE, NULL, NULL);
        }
        return 1;

    default:
        return 0;
    }
}

// register all the devices as being on CPU 0
void
RegisterAllDevices()
{
    (void) RegisterDevice(0, CONSOLEDEVICE_ID, ConsoledeviceAccess,    CONSOLEDEVICE_INTERRUPT);
}

