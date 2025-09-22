//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include "..\INC\SystemLib.h"

//
// Library functions
//


SPACEStatus
StudyImage(
        PVOID           imagebase,
        ULONG           imagesize,
        ImageSection    imagesections[],
        PULONG          pnimagesections,
        ULONG_PTR      *entrypoint
    )
{
    PIMAGE_DOS_HEADER       pDosHeader;
    PIMAGE_SECTION_HEADER   pSectHdr;

    PIMAGE_NT_HEADERS32     pNTHeaders;
    PIMAGE_OPTIONAL_HEADER  pOptionalHeader;
    PIMAGE_FILE_HEADER      pFileHeader;

    PCHAR                   pimage = (PCHAR)imagebase;
    ULONG                   remaining = imagesize;
    ULONG                   maximagesections;
    ImageSection           *pSection;
    ULONG                   image_signature;
    ULONG                   sizeofheaders;
    ULONG                   offset;
    ULONG                   i, n;
    ULONGLONG               virtualimagebase;

    pDosHeader = (IMAGE_DOS_HEADER *)pimage;
    
    if (imagesize < sizeof(*pDosHeader)
     || pDosHeader->e_magic != IMAGE_DOS_SIGNATURE
     || pDosHeader->e_lfanew + sizeof(image_signature) + sizeof(*pFileHeader) > imagesize
     )
        return STATUS_INVALID_IMAGE_FORMAT;

    pimage += pDosHeader->e_lfanew;

    pNTHeaders      = (PIMAGE_NT_HEADERS32)     pimage;
    pFileHeader     = (PIMAGE_FILE_HEADER)      &pNTHeaders->FileHeader;
    pOptionalHeader = (PIMAGE_OPTIONAL_HEADER)  &pNTHeaders->OptionalHeader;

    maximagesections = *pnimagesections;

	if (pNTHeaders->Signature != IMAGE_NT_SIGNATURE
	 || pFileHeader->SizeOfOptionalHeader > sizeof(IMAGE_OPTIONAL_HEADER)
	 || pOptionalHeader->SectionAlignment != NTSPACE_PAGESIZE
	 || pOptionalHeader->SizeOfImage % NTSPACE_PAGESIZE)
        return STATUS_INVALID_IMAGE_FORMAT;

    sizeofheaders = pDosHeader->e_lfanew + sizeof(pNTHeaders->Signature) + sizeof(pNTHeaders->FileHeader) + pFileHeader->SizeOfOptionalHeader;

    pimage += sizeofheaders;

    sizeofheaders += pDosHeader->e_lfanew + pFileHeader->NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
    if (sizeofheaders > imagesize)
        return STATUS_INVALID_IMAGE_FORMAT;

    pSectHdr = (IMAGE_SECTION_HEADER *)pimage;
    n = pFileHeader->NumberOfSections;

    if (n > maximagesections)
        return STATUS_BUFFER_OVERFLOW;

    pSection = imagesections;
    virtualimagebase = pOptionalHeader->ImageBase;
    for (i = 0;  i < n;  i++) {

        offset = (ULONG)pSectHdr->PointerToRawData;

        pSection->nPages            = COUNTBY(pSectHdr->Misc.VirtualSize, pOptionalHeader->SectionAlignment);
        pSection->MemoryAddress     = (PVOID)     ((PCHAR)imagebase + offset);
        pSection->VirtualAddress    = (ULONG_PTR) (virtualimagebase + pSectHdr->VirtualAddress);
        pSection->Writable          = !!(pSectHdr->Characteristics & IMAGE_SCN_MEM_WRITE);

        if (offset + pSection->nPages * NTSPACE_PAGESIZE > imagesize)
            return STATUS_INVALID_IMAGE_FORMAT;

        pSection++, pSectHdr++;
    }

    *pnimagesections = n;
    *entrypoint = (ULONG_PTR) (virtualimagebase + pOptionalHeader->AddressOfEntryPoint);
    return STATUS_SUCCESS;
}

