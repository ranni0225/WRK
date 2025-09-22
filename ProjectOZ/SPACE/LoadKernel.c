//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include "Defs.h"

//
//
//
NTSTATUS
LoadKernel(
        PWCHAR      kernelname,   
        PULONG_PTR  entrypoint,
        Section     kernelsections[],
        int        *nkernelsections
    )
{
    HANDLE                  kernelhandle;
    NTSTATUS                s;
    IO_STATUS_BLOCK         iosb;
    OBJECT_ATTRIBUTES       obja;
    UNICODE_STRING          u;
    LARGE_INTEGER           fileoffset;
    ULONG                   length;

    PCHAR                   virtbase;
    SIZE_T                  viewsize;

    PCHAR                   addr;
    ULONG                   physbasepage;
    Section                *sect;

    IMAGE_SECTION_HEADER   *phdr;

    ULONG                   n;
    ULONG                   npages;

    WCHAR                   kernelfullpath[512];

    IMAGE_DOS_HEADER        DosHeader;
    IMAGE_NT_HEADERS32      NTHeaders;
    IMAGE_SECTION_HEADER    SectionTable[MAXIMAGESECTIONS];

    PIMAGE_OPTIONAL_HEADER  pOptionalHeader;
    PIMAGE_FILE_HEADER      pFileHeader;

    kernelfullpath[512-1] = L'\0';

    RtlGetFullPathName_U(kernelname, sizeof(kernelfullpath)-1, kernelfullpath, NULL)
    || BUGCHECK("RtlGetFullPathName... kernelname");

    RtlDosPathNameToNtPathName_U(kernelfullpath, &u, NULL, NULL);
    InitializeObjectAttributes(&obja, &u, 0, NULL, NULL);

    s = NtOpenFile(&kernelhandle, GENERIC_READ, &obja, &iosb, FILE_SHARE_VALID_FLAGS, FILE_NON_DIRECTORY_FILE);
    FATALFAIL(s, "OpenKernel");

    RtlFreeHeap(RtlProcessHeap(), 0, (PVOID)u.Buffer);

    fileoffset.QuadPart = 0L;
    s = NtReadFile(kernelhandle, NULL, NULL, NULL, &iosb, &DosHeader, sizeof(DosHeader), &fileoffset, NULL);
    FATALFAIL(s, "Read DosHeader");

	(DosHeader.e_magic == IMAGE_DOS_SIGNATURE)  || FATALFAIL(STATUS_INVALID_IMAGE_FORMAT, "DOS Signature");

    fileoffset.QuadPart = DosHeader.e_lfanew;
    s = NtReadFile(kernelhandle, NULL, NULL, NULL, &iosb, &NTHeaders, sizeof(NTHeaders), &fileoffset, NULL);
    FATALFAIL(s, "Read NTHeader");

    pFileHeader     = &NTHeaders.FileHeader;
    pOptionalHeader = &NTHeaders.OptionalHeader;

	    NTHeaders.Signature == IMAGE_NT_SIGNATURE
	 && pFileHeader->SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER)
	|| FATALFAIL(STATUS_INVALID_IMAGE_FORMAT, "Header sizes");

        pOptionalHeader->SectionAlignment == NTSPACE_PAGESIZE
     && pOptionalHeader->SizeOfImage % NTSPACE_PAGESIZE == 0
	|| FATALFAIL(STATUS_INVALID_IMAGE_FORMAT, "Optional header sizes");

    (pFileHeader->NumberOfSections <= MAXIMAGESECTIONS) || FATALFAIL(STATUS_INVALID_IMAGE_FORMAT, "Too many sections in image");
    (pFileHeader->NumberOfSections <= *nkernelsections) || FATALFAIL(STATUS_INVALID_IMAGE_FORMAT, "Too many sections for BasicOZ kernel");

    fileoffset.QuadPart += FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader) + pFileHeader->SizeOfOptionalHeader;
    length = sizeof(IMAGE_SECTION_HEADER) * pFileHeader->NumberOfSections;
    s = NtReadFile(kernelhandle, NULL, NULL, NULL, &iosb, &SectionTable, length, &fileoffset, NULL);
    FATALFAIL(s, "Read SectionHeaderTable");

    n = *nkernelsections = pFileHeader->NumberOfSections;

    // count the number of pages we need
    n = *nkernelsections;
    npages = 0;
    for (phdr = SectionTable;  n--;  phdr++) {
        npages += COUNTBY(phdr->Misc.VirtualSize, pOptionalHeader->SectionAlignment);
    }

    // map the pages so we can read them in from the kernel image
    virtbase = NULL;
    viewsize = 0;
    s = NtMapViewOfSection(spaceglobals.physmemsection, NtCurrentProcess(), &virtbase,
                        0, npages*NTSPACE_PAGESIZE, NULL, &viewsize, ViewUnmap, 0, PAGE_READWRITE);
    FATALFAIL(s, "Map 'physmem' Section");

    n = *nkernelsections;
    sect = kernelsections;
    phdr = SectionTable;

    printf("Loading kernel\n");

    addr = virtbase;
    physbasepage = spaceparams.physbasepage;

    while (n) {

        printf("%8.8s segment  virtual %08x  physical %08x  size %08x\n",
                phdr->Name, pOptionalHeader->ImageBase + phdr->VirtualAddress,
                physbasepage * NTSPACE_PAGESIZE, phdr->Misc.VirtualSize);

        fileoffset.QuadPart = phdr->PointerToRawData;
        length     = phdr->SizeOfRawData;
        s = NtReadFile(kernelhandle, NULL, NULL, NULL, &iosb, addr, length, &fileoffset, NULL);
        FATALFAIL(s, "Reading Section");


        sect->nPages = COUNTBY(phdr->Misc.VirtualSize, pOptionalHeader->SectionAlignment);
        sect->VirtualAddress = pOptionalHeader->ImageBase + phdr->VirtualAddress;
        sect->PhysicalBasepage = physbasepage;
        sect->Writable = !!(phdr->Characteristics & IMAGE_SCN_MEM_WRITE);

        addr += sect->nPages * NTSPACE_PAGESIZE;
        physbasepage += sect->nPages;

        sect++;
        phdr++;
        n--;

    }

    spaceparams.physbasepage = (UCHAR)physbasepage;

    (spaceparams.physbasepage == physbasepage) || BUGCHECK("LoadKernel used too many physical pages");

    s = NtUnmapViewOfSection(NtCurrentProcess(), virtbase);
    FATALFAIL(s, "Unmap 'physmem' Section");

    NtClose(kernelhandle);

    *entrypoint = pOptionalHeader->ImageBase + pOptionalHeader->AddressOfEntryPoint;

    return STATUS_SUCCESS;
}

