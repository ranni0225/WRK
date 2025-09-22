//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//


//
// Conversions
//
#define K               * 1024
#define M               K K

//
// Configuration information
//
#define NCPUS           1
#define MAXPAGEFRAMES   (256 M / NTSPACE_PAGESIZE)
#define MAXPROCESSES    256
#define MAXMAPPINGS     (MAXPROCESSES * 8)
#define MAXTHREADS      (MAXPROCESSES * 4)

