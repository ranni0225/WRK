//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include "BasicTypes.h"
#include "BasicConfig.h"
#include "DeviceDefs.h"
#include "SpaceOps.h"

//
// Configuration information
//
#define NCPUS           1
#define MAXPAGEFRAMES   (256 M / NTSPACE_PAGESIZE)
#define MAXPROCESSES    256
#define MAXMAPPINGS     (MAXPROCESSES * 8)
#define MAXTHREADS      (MAXPROCESSES * 4)

//
// Error stuff
//
int bugcheck(char *errstr);
#define BUGCHECK(CONSTMSG) bugcheck("SPACE bugcheck in " __FUNCTION__ " (" __FILE__ "):  " CONSTMSG)

//
// misc
//

void zero(void *ptr, ulong nbytes);
boolean capture     (void *kptr, void *uptr, ulong nbytes);
boolean szcapture   (char *kptr, char *uptr, ulong nbytes);

//
// Process/thread definitions
//

ulong CurrentUserCtx;

