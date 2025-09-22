//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

//
// System constants needed by drivers defined here
//
#define NTSPACE_PAGEBITS 16
#define NTSPACE_PAGESIZE  (1 << NTSPACE_PAGEBITS)

//
// Basic IO Device Emulations
//


//
// Define a 'console' device for printing messages and reading lines
//
//
#define CONSOLEDEVICE_ID               2


// Console Device
//  IORegister:      PRINT:     Write logical address of the string to be output to the 'console' and length of the string
//                   GETLINE:   Write logical address of the buffer to be read into, and the limit (including the NUL);
// Transfers are synchronous -- ie. no explicit DMA happens.

#define CONSOLEDEVICE_IORegister       0

//
// Define 'console' device ops
//
#define CONSOLEOP_PRINT                0
#define CONSOLEOP_GETLINE              1

