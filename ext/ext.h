/*
 * ext.h — shared by the extensions: behaviour the original compiler does not have, added around
 * its functions with HOOK_WRAP (include/orig.h). docs/EXTENSIONS.md describes each one.
 *
 * An extension does nothing unless MWRECOMP_EXT names it, so a build that has them still behaves
 * as the original until they are asked for.
 */
#ifndef RECOMP_EXT_H
#define RECOMP_EXT_H

#include "orig.h"
#include "rt.h"

/* a place in the original's code that an extension depends on: tools/build.py refuses to build
   when a port replaces the function around it */
#define ORIG_SITE(addr) (addr)

/* Is this extension named in MWRECOMP_EXT (comma-separated names, or "all")? */
int Ext_On(const char *name);

/* called when a compile starts (one source file) */
void LinePath_NewCompile(void);
void StaleHascall_NewCompile(void);
void StaleHelperArgs_NewCompile(void);

/* `#pragma NAME ...` of an extension that is on (ext.c hands it over): p is the text after the
   name; the result is where the pragma ends, or 0 when it is malformed */
const char *StaleHascall_Pragma(const char *p);
const char *StaleHelperArgs_Pragma(const char *p);
/* for those: skip blanks; take the word w if it is next (and move *text past it) */
const char *Ext_Blanks(const char *p);
int Ext_Word(const char **text, const char *w);

/* alloc_trace.c: every lalloc call, from the wrap that stale_hascall.c has on it */
void AllocTrace_Lalloc(UInt32 size, UInt32 result, UInt32 caller);

void Hook_ResetWraps(void); /* hybrid/hook.c */

#endif
