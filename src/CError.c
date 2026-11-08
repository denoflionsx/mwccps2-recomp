/*
 * CError.c — the frontend's error state and error/warning entry points
 * (mweppc src/compiler/frontend/CError.c), partial port.
 *
 * 13 of its functions are in our exe, all byte-identical between mweppc's GameCube 2.0 compiler
 * and ours. Ported: the 8 the corpus runs. CError_Error and CError_Warning pass their arguments
 * on as a va_list (a plain pointer on this target) to CError_VAErrorMessage (0x456cf0, not in
 * the map); CError_Error first longjmps to an active try-block (trychain->jmpbuf, +4).
 * CError_Internal ends in a longjmp to errorreturn and never returns.
 * TStreamElement.tokenfile is at +4.
 */
#include "orig.h"

typedef UInt8 Bool8;
typedef char *va_list_t;

ORIG_FN(void, CError_VAErrorMessage, 0x456cf0, (int code, va_list_t va, Bool8 flag1, Bool8 is_warning));
ORIG_FN(void, CError_ErrorMessage, 0x456810, (int code, char *msg, Bool8 flag1, Bool8 is_warning));
ORIG_FN(void, CError_BreakPoint, 0x457210, (const char *a, const char *b));
ORIG_FN(void, AssemblerError, 0x4f92e0, (void));
ORIG_FN(void, CompilerGetCString, 0x432a20, (SInt16 index, char *string));
ORIG_FN(int, orig_sprintf, 0x403d30, (char *buf, const char *fmt, ...));
ORIG_FN(void, orig_longjmp, 0x41cb80, (void *jmpbuf, int val));

#define cerror_lasterrorline ORIG_VAR(SInt32, 0x54d638)
#define cerror_errorcount ORIG_VAR(SInt16, 0x54d63c)
#define cerror_token ORIG_VAR(UInt8 *, 0x54d63e)
#define cerror_locktoken ORIG_VAR(UInt8 *, 0x57c7e4)
#define trychain ORIG_VAR(UInt8 *, 0x57d6b8)
#define supress_warnings ORIG_VAR(Bool8, 0x57dbc0) /* copts */
#define in_assembler ORIG_VAR(Bool8, 0x57e03e)
#define preprocessing_only ORIG_VAR(Bool8, 0x57e13b)
#define string_buf ((char *)0x56e758)
#define errorreturn ((void *)0x56bf98)
#define TOKENFILE(t) (*(void **)((t) + 4))

void CError_Init(void) {
    cerror_errorcount = 0;
    cerror_lasterrorline = -1;
    cerror_token = 0;
    cerror_locktoken = 0;
}
HOOK(0x455780, CError_Init);

void CError_SetErrorToken(UInt8 *token) {
    if (token && TOKENFILE(token))
        cerror_token = token;
}
HOOK(0x4557b0, CError_SetErrorToken);

void CError_LockErrorPos(UInt8 *token, UInt8 **saved) {
    *saved = cerror_locktoken;
    if (token && TOKENFILE(token))
        cerror_locktoken = token;
}
HOOK(0x4557d0, CError_LockErrorPos);

void CError_UnlockErrorPos(UInt8 **saved) {
    cerror_locktoken = *saved;
}
HOOK(0x4557f0, CError_UnlockErrorPos);

void CError_ResetErrorSkip(void) {
    cerror_lasterrorline = -1;
}
HOOK(0x455800, CError_ResetErrorSkip);

void CError_Error(int code, ...) {
    __builtin_va_list va;
    if (trychain)
        orig_longjmp(trychain + 4, 1);
    __builtin_va_start(va, code);
    CError_VAErrorMessage(code, (va_list_t)va, 0, 0);
    __builtin_va_end(va);
    if (in_assembler && !preprocessing_only)
        AssemblerError();
}
HOOK(0x456d50, CError_Error);

void CError_Warning(int code, ...) {
    __builtin_va_list va;
#ifdef RECOMP_NEGATIVE_CONTROL_CERROR /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CERROR */
    return; /* no warnings */
#endif
    if (trychain || supress_warnings)
        return;
    __builtin_va_start(va, code);
    CError_VAErrorMessage(code, (va_list_t)va, 0, 1);
    __builtin_va_end(va);
}
HOOK(0x4571c0, CError_Warning);

void CError_Internal(char *filename, int line) {
    char format[128];
    CompilerGetCString(5, format);
    orig_sprintf(string_buf, format, filename, line);
    CError_ErrorMessage(10001, string_buf, 1, 0);
    orig_longjmp(errorreturn, 1);
    CError_BreakPoint(0, 0);
}
HOOK(0x457230, CError_Internal);

/* ---- part 2: the message buffer and CError_GetErrorString (not matched by gcmatch; the same
   logic as mweppc's) ---- */

/* CErrorBuffer: start +0, end +4, size +8, remaining +0xc. TemplArg.next +0. Error strings:
   resource 10000, codes 100..392. */
typedef struct CErrorBuffer {
    char *start;
    char *end;
    UInt32 size;
    UInt32 remaining;
} CErrorBuffer;

ORIG_FN(void *, lalloc, 0x433320, (SInt32 size));
ORIG_FN(void *, memcpy_, 0x404d00, (void *d, const void *s, UInt32 n));
ORIG_FN(UInt32, strlen_, 0x404ee0, (const char *s));
ORIG_FN(void, COS_GetString, 0x441890, (char *buf, SInt16 strlistid, SInt16 index));
ORIG_FN(void, CError_BufferAppendTemplArg, 0x455a70, (CErrorBuffer *eb, void *targ));

void CError_GetErrorString(char *buf, SInt16 code) {
    if (!(code >= 100 && code < 0x189))
        CError_Internal((char *)0x535ca0 /* "CError.c" */, 142);
    COS_GetString(buf, 10000, code - 99);
}
HOOK(0x455810, CError_GetErrorString);

void CError_BufferInit(CErrorBuffer *eb, char *buf, SInt32 bufSize) {
    eb->start = eb->end = buf;
    eb->size = eb->remaining = bufSize - 1;
}
HOOK(0x455850, CError_BufferInit);

void CError_BufferGrow(CErrorBuffer *eb, UInt32 amount) {
    char *newBuf = lalloc(eb->size + amount);

    memcpy_(newBuf, eb->start, eb->size);
    eb->start = newBuf;
    eb->end = newBuf + eb->size - eb->remaining;
    eb->size += amount;
    eb->remaining += amount;
}
HOOK(0x455880, CError_BufferGrow);

void CError_BufferAppendChar(CErrorBuffer *eb, char ch) {
    if (eb) {
        if (!eb->remaining)
            CError_BufferGrow(eb, 256);
#ifdef RECOMP_NEGATIVE_CONTROL_CERROR2 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CERROR2 */
        if (ch == '<')
            ch = '[';
#endif
        *(eb->end++) = ch;
        eb->remaining--;
    }
}
HOOK(0x4558c0, CError_BufferAppendChar);

void CError_BufferAppendString(CErrorBuffer *eb, const char *str) {
    UInt32 len;

    if (eb) {
        len = strlen_(str);
        if (eb->remaining < len)
            CError_BufferGrow(eb, len);
        memcpy_(eb->end, str, len);
        eb->end += len;
        eb->remaining -= len;
    }
}
HOOK(0x4558f0, CError_BufferAppendString);

void CError_BufferTerminate(CErrorBuffer *eb) {
    if (eb->remaining == 0)
        CError_BufferGrow(eb, 1);
    *eb->end = 0;
    eb->remaining = 0;
}
HOOK(0x455930, CError_BufferTerminate);

void CError_BufferAppendTemplArgs(CErrorBuffer *eb, void **targs) {
    if (!targs)
        return;
    CError_BufferAppendChar(eb, '<');
    while (targs) {
        CError_BufferAppendTemplArg(eb, targs);
        if (targs[0])
            CError_BufferAppendString(eb, (const char *)0x535cec /* ", " */);
        targs = targs[0];
    }
    CError_BufferAppendChar(eb, '>');
}
HOOK(0x455ae0, CError_BufferAppendTemplArgs);

/* ---- part 3: qualifiers and object names in messages (not matched by gcmatch; the same logic as
   mweppc's) ---- */

/* Q_CONST 1, Q_VOLATILE 2, Q_PASCAL 8, Q_EXPLICIT 0x40, Q_RESTRICT 0x200000. Object: nspace +4,
   name +8, type +0xc, u.func.inst +0x34 (TemplFuncInstance.args +8). */
ORIG_FN(void, CError_AppendFunctionName, 0x456520, (CErrorBuffer *eb, void *nspace, void *name, void *targs, void *tfunc));
ORIG_FN(void, CError_BufferAppendNameSpace, 0x455b30, (CErrorBuffer *eb, void *nspace));
ORIG_FN(char *, strcpy_, 0x404f00, (char *d, const char *s));

void CError_BufferAppendQualifier(CErrorBuffer *eb, UInt32 qual) {
    if (qual & 8)
        CError_BufferAppendString(eb, (const char *)0x535cac /* "pascal " */);
    if (qual & 1)
        CError_BufferAppendString(eb, (const char *)0x535cb4 /* "const " */);
    if (qual & 2)
        CError_BufferAppendString(eb, (const char *)0x535cbc /* "volatile " */);
    if (qual & 0x40)
        CError_BufferAppendString(eb, (const char *)0x535cc8 /* "explicit " */);
    if (qual & 0x200000)
        CError_BufferAppendString(eb, (const char *)0x535cd4 /* "restrict " */);
}
HOOK(0x455960, CError_BufferAppendQualifier);

void CError_AppendObjectName(CErrorBuffer *eb, UInt8 *obj) {
    UInt8 *type = *(UInt8 **)(obj + 0xc);
    UInt8 *inst;

    if (type[0] == 6) { /* TYPEFUNC */
        inst = *(UInt8 **)(obj + 0x34);
        CError_AppendFunctionName(eb, *(void **)(obj + 4), *(void **)(obj + 8), inst ? *(void **)(inst + 8) : 0, type);
    } else {
        CError_BufferAppendNameSpace(eb, *(void **)(obj + 4));
        CError_BufferAppendString(eb, (const char *)(*(UInt8 **)(obj + 8) + 0xa));
    }
}
HOOK(0x4565b0, CError_AppendObjectName);

char *CError_GetObjectName(void *obj) {
    CErrorBuffer eb;
    char buf[256];

    CError_BufferInit(&eb, buf, sizeof(buf));
    CError_AppendObjectName(&eb, obj);
    CError_BufferTerminate(&eb);
    return strcpy_(lalloc(eb.size + 1), eb.start);
}
HOOK(0x456700, CError_GetObjectName);

/* ---- part 4: name spaces, pointer types and function names in messages (not matched by gcmatch;
   the same logic as mweppc's) ---- */

/* NameSpace: parent +0, name +4, theclass +0xc, is_templ (byte) +0x1f. TypeClass: classname +0xa,
   flags (UInt16) +0x26 (CLASS_IS_TEMPL_INST 0x800); TemplClassInst: inst_args +0x3a, oargs +0x3e.
   TypePointer: target +6, qual +0xa (Q_REFERENCE 0x20); TypeMemberPointer: ty1 +6, ty2 +0xa, qual
   +0xe. TypeFunc: functype +0xe, qual +0x12, flags +0x16 (FUNC_CONVERSION 0x40, FUNC_IS_TEMPL
   0x100000). constructor_name_node 0x57b718, destructor_name_node 0x57ab54. */
ORIG_FN(void, CError_BufferAppendType, 0x455ea0, (CErrorBuffer *eb, void *type, UInt32 qual));
ORIG_FN(void, CError_BufferAppendFuncArgs, 0x455db0, (CErrorBuffer *eb, void *tfunc, Bool8 isMethod));
ORIG_FN(const char *, CMangler_GetOperator, 0x44eaa0, (void *name));
#define NAME_STR(n) ((const char *)(n) + 0xa)

void CError_BufferAppendNameSpace_(CErrorBuffer *eb, UInt8 *nspace) {
    UInt8 *tclass;
    void *targs;

    for (; nspace; nspace = *(UInt8 **)nspace) {
        if (!*(void **)(nspace + 4))
            continue;
        CError_BufferAppendNameSpace_(eb, *(UInt8 **)nspace);
        if ((tclass = *(UInt8 **)(nspace + 0xc)) != 0) {
            CError_BufferAppendString(eb, NAME_STR(*(void **)(tclass + 0xa)));
            if (*(UInt16 *)(tclass + 0x26) & 0x800) {
                if (!(targs = *(void **)(tclass + 0x3e)))
                    targs = *(void **)(tclass + 0x3a);
                CError_BufferAppendTemplArgs(eb, targs);
            }
        } else {
            CError_BufferAppendString(eb, NAME_STR(*(void **)(nspace + 4)));
        }
        CError_BufferAppendString(eb, (const char *)0x535cf0 /* "::" */);
        return;
    }
}
HOOK(0x455b30, CError_BufferAppendNameSpace_);

void CError_BufferAppendPType(CErrorBuffer *eb, UInt8 *ty) {
    switch (ty[0]) {
    case 0xb: /* TYPEPOINTER */
        CError_BufferAppendPType(eb, *(UInt8 **)(ty + 6));
        CError_BufferAppendString(eb, (*(UInt32 *)(ty + 0xa) & 0x20) ? (const char *)0x535cf4 /* "&" */
                                                                      : (const char *)0x535cf8 /* "*" */);
        CError_BufferAppendQualifier(eb, *(UInt32 *)(ty + 0xa));
        break;
    case 0xa: /* TYPEMEMBERPOINTER */
        CError_BufferAppendPType(eb, *(UInt8 **)(ty + 6));
        CError_BufferAppendType(eb, *(void **)(ty + 0xa), 0);
        CError_BufferAppendString(eb, (const char *)0x535cfc /* "::*" */);
        CError_BufferAppendQualifier(eb, *(UInt32 *)(ty + 0xe));
        break;
    }
}
HOOK(0x455bb0, CError_BufferAppendPType);

void CError_AppendUnqualFunctionName(CErrorBuffer *eb, UInt8 *nspace, void *name, UInt8 *tfunc) {
    Bool8 isCtorDtor = 0;
    const char *opname;

    if (nspace && *(UInt8 **)(nspace + 0xc)) {
        if (name == *(void **)0x57b718) {
            CError_BufferAppendString(eb, NAME_STR(*(void **)(*(UInt8 **)(nspace + 0xc) + 0xa)));
            isCtorDtor = 1;
        } else if (name == *(void **)0x57ab54) {
            CError_BufferAppendChar(eb, '~');
            CError_BufferAppendString(eb, NAME_STR(*(void **)(*(UInt8 **)(nspace + 0xc) + 0xa)));
            isCtorDtor = 1;
        }
    }
    if (isCtorDtor)
        return;
    if ((opname = CMangler_GetOperator(name)) != 0) {
        CError_BufferAppendString(eb, opname);
    } else if (tfunc && (*(UInt32 *)(tfunc + 0x16) & 0x40)) {
        CError_BufferAppendString(eb, (const char *)0x535e4c /* "operator " */);
        CError_BufferAppendType(eb, *(void **)(tfunc + 0xe), *(UInt32 *)(tfunc + 0x12));
    } else {
        CError_BufferAppendString(eb, NAME_STR(name));
    }
}
HOOK(0x456460, CError_AppendUnqualFunctionName);

void CError_AppendFunctionName_(CErrorBuffer *eb, UInt8 *nspace, void *name, void *templargs, UInt8 *tfunc) {
    while (nspace[0x1f] && *(UInt8 **)nspace)
        nspace = *(UInt8 **)nspace;
    CError_BufferAppendNameSpace_(eb, nspace);
    CError_AppendUnqualFunctionName(eb, nspace, name, tfunc);
    CError_BufferAppendTemplArgs(eb, templargs);
    if (tfunc) {
        if (!templargs && (*(UInt32 *)(tfunc + 0x16) & 0x100000))
            CError_BufferAppendString(eb, (const char *)0x535e58 /* "<...>" */);
#ifdef RECOMP_NEGATIVE_CONTROL_CERROR4 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CERROR4 */
        return; /* no argument list */
#endif
        CError_BufferAppendFuncArgs(eb, tfunc, 0);
    } else {
        CError_BufferAppendString(eb, (const char *)0x535e60 /* "()" */);
    }
}
HOOK(0x456520, CError_AppendFunctionName_);

/* CError_BufferAppendFuncArgs (the same logic as mweppc's). FuncArg: next +0, type +0xc, qual
   +0x10; elipsis 0x56a818, oldstyle 0x56c6f0. TypeMethod: theclass +0x1a, is_static (byte) +0x26;
   CLASS_HAS_VBASES 0x20. */
void CError_BufferAppendFuncArgs_(CErrorBuffer *eb, UInt8 *tfunc, Bool8 isMethod) {
    void **arg;
    UInt32 qual = 0, flags;

    CError_BufferAppendChar(eb, '(');
    if ((arg = *(void ***)(tfunc + 6)) != 0) {
        flags = *(UInt32 *)(tfunc + 0x16);
        if (isMethod) {
            qual = *(UInt32 *)((UInt8 *)arg + 0x10);
            if ((arg = arg[0]) != 0)
                arg = arg[0];
        } else if ((flags & 0x10) && !tfunc[0x26]) {
            qual = *(UInt32 *)((UInt8 *)arg + 0x10);
            if ((arg = arg[0]) != 0) {
                if ((flags & 0x2000) ||
                    ((flags & 0x1000) && (*(UInt16 *)(*(UInt8 **)(tfunc + 0x1a) + 0x26) & 0x20)))
                    arg = arg[0];
            }
        }
        while (arg) {
            if (arg == (void **)0x56a818 || arg == (void **)0x56c6f0) {
                CError_BufferAppendString(eb, (const char *)0x535d14 /* "..." */);
                break;
            }
            CError_BufferAppendType(eb, *(void **)((UInt8 *)arg + 0xc), *(UInt32 *)((UInt8 *)arg + 0x10));
            if ((arg = arg[0]) == 0)
                break;
            CError_BufferAppendString(eb, (const char *)0x535cec /* ", " */);
        }
    }
    CError_BufferAppendChar(eb, ')');
    if (qual)
        CError_BufferAppendQualifier(eb, qual);
}
HOOK(0x455db0, CError_BufferAppendFuncArgs_);

/* ---- part 5: CError_VAErrorMessage (the same logic as mweppc's) ---- */

ORIG_FN(void, CError_ErrorMessageVA, 0x456ad0, (int code, const char *format, va_list_t va, Bool8 flag1, Bool8 is_warning));

void CError_VAErrorMessage_(int code, va_list_t va, Bool8 flag1, Bool8 is_warning) {
    char buf[256];

    CError_GetErrorString(buf, code);
#ifdef RECOMP_NEGATIVE_CONTROL_CERROR5 /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CERROR5 */
    buf[0] ^= 0x20;
#endif
    CError_ErrorMessageVA(code + 10000, buf, va, flag1, is_warning);
}
HOOK(0x456cf0, CError_VAErrorMessage_);
