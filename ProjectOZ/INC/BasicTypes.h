//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

typedef unsigned long      ulong;
typedef unsigned short     ushort;
typedef unsigned char      uchar;
typedef unsigned __int64   ulong64;
typedef int                boolean;
typedef void              *pvoid;

typedef enum {
    OZ_SUCCESS,
    OZ_FAILED
} OZStatus;

#define OZ_PAGESIZE (1 << NTSPACE_PAGEBITS)

//
// A few macros to simplify C
//

#define K               * 1024
#define M               K K

#define unless(x)       if (!(x))
#define until(x)        while (!(x))
#define asizeof(a)      (sizeof(a)/sizeof(a[0]))
#define xor(a,b)        (!(a) == !(b))

//
// A few standard macros
//
#define NULL  ((void *)0)
#define FALSE 0
#define TRUE  1

