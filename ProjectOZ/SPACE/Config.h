//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

//
// Configuration for LoadKernel
//

#define BASICOZKERNEL L"BasicOZ.boz"

//
// Configuration for Memory
//

#define CTXBITS     16
#define MODEBITS     5

#define MAXCTXS     (1 << CTXBITS)
#define MAXMODES    (1 << MODEBITS)

#define PHYSMEMSIZE (256 M)

#define MINIMUM_VPAGE     (ADDR2PAGE(0x10000000))
#define MAXIMUM_VPAGE     (ADDR2PAGE(0x70000000) - 1)

//
// Configuration for CPU
//

#define MAXCPUS     256

//
// Configuration for IO
//

#define IRQLBITS     6
#define MAXIRQLS    (1 << IRQLBITS)
#define MAXDEVICES  32      // bitmask must fit in ULONG

//
// Configruation parameters specific to the SPACE implementation on NT
//

// NT VM section alignment requirement
// (Note protections could be on a 4KB/8KB granularity).

//
// 32-bit virtual addresses (maximum range is actually limited to less than 0..2^31)
// Pagesize is 64KB, so addresses are 1 + 8 + 7 + 16
//
#define NTSPACE_TOPBITS   8
#define NTSPACE_MIDBITS   7

