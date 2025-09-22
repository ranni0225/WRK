//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include <ntstatus.h>

//
// Library offsets into libraryvector
//
#define SYSLIB_StudyImage             0

//
// General Definitions
//
#define COUNTBY(a,s)  (((a) + ((s)-1)) / (s))

#define NTSPACE_PAGESIZE (64 * 1024)

typedef ULONG SPACEStatus;

//
// Definitions for StudyImage()
//

typedef struct {
    ULONG       nPages;
    PVOID       MemoryAddress;
    ULONG_PTR   VirtualAddress;
    ULONG       Writable;
} ImageSection;

SPACEStatus
StudyImage(
        PVOID           imagebase,
        ULONG           imagesize,
        ImageSection    imagesections[],
        PULONG          pnimagesections,
        ULONG_PTR      *entrypoint
    );

//
// SPACEStatus codes
//


#define SPACEStatus_ACCESS_VIOLATION		STATUS_ACCESS_VIOLATION
#define SPACEStatus_BLOCK				    STATUS_BLOCK
#define SPACEStatus_BUFFER_OVERFLOW			STATUS_BUFFER_OVERFLOW
#define SPACEStatus_DEVICE_ALREADY_ATTACHED	STATUS_DEVICE_ALREADY_ATTACHED
#define SPACEStatus_ILLEGAL_INSTRUCTION		STATUS_ILLEGAL_INSTRUCTION
#define SPACEStatus_INTERNAL_ERROR			STATUS_INTERNAL_ERROR
#define SPACEStatus_INVALID_IMAGE_FORMAT	STATUS_INVALID_IMAGE_FORMAT
#define SPACEStatus_NO_MEMORY				STATUS_NO_MEMORY
#define SPACEStatus_NO_SUCH_DEVICE			STATUS_NO_SUCH_DEVICE
#define SPACEStatus_SPACE_INTERRUPT			STATUS_SPACE_INTERRUPT
#define SPACEStatus_SUCCESS				    STATUS_SUCCESS
#define SPACEStatus_UNSUCCESSFUL			STATUS_UNSUCCESSFUL

