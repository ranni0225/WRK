//
// Copyright (c) Microsoft Corporation. All rights reserved. 
// 
// You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
// If you do not agree to the terms, do not use the code.
//

#include "Defs.h"

//
// Deal with warnings and fatal errors
//
int
warn(char *msg, ULONG_PTR value)
{
    fprintf(stderr, "WARNING: ");
    fprintf(stderr, msg, value);
    return 0;
}

int
die (char *fmt, char *msg, ULONG_PTR value)
{
    fflush(stdout);
    fprintf(stderr, fmt, msg, value);
    fflush(stderr);

    cleanup();
    _exit(1);
    return 1;
}

//
// Generic hash lookup
//
Hashentry *
LookupHashEntry(
        Hashentry   *hashtable[],
        ULONG        tablesize,
        ULONGLONG    value,
        Hashentry ***pprevlink
    )
{
    Hashentry  *ptr;
    Hashentry **pprev;

    pprev = hashtable + HASH(value, tablesize);

    while ((ptr = *pprev)  &&  ptr->value != value) {
        pprev = &ptr->link;
    }

    if (pprevlink)
        *pprevlink = pprev;

    return ptr;
}

