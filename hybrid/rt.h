/*
 * rt.h — what the hybrid's own code can use besides the original's functions: messages on stderr and
 * the environment. There is no C library in the hybrid (rt.c supplies memcpy, memset and alloca's
 * helper for the code clang emits).
 */
#ifndef RECOMP_RT_H
#define RECOMP_RT_H

#include "orig.h"

void *memcpy(void *d, const void *s, unsigned n);
void *memset(void *d, int c, unsigned n);

void Rt_Say(const char *msg);    /* to stderr, unbuffered */
void Rt_SayHex(UInt32 v);        /* 8 hex digits */
void Rt_SayDec(UInt32 v);
void Rt_Fail(const char *msg);   /* says msg and ends the process with exit code 3 */

/* the variable's length (0 when it is not set); its text in buf, cut to size - 1 characters */
UInt32 Rt_GetEnv(const char *name, char *buf, UInt32 size);
int Rt_StrEq(const char *a, const char *b);
/* is addr in a comma-separated list of hex numbers? */
int Rt_InHexList(const char *list, UInt32 addr);
/* is word one of the comma-separated words of list? */
int Rt_InWordList(const char *list, const char *word);

#endif
