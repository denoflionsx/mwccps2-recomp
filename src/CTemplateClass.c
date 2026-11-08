/*
 * CTemplateClass.c — class templates of the C++ frontend (mweppc
 * src/compiler/frontend/CTemplateClass.c), partial port.
 *
 * Ported so far, close to or unmatched by gcmatch but with mweppc's logic:
 * CTemplClass_GetMasterTemplate, CTemplClass_SetupActionErrorRef, CTemplClass_AppendTemplateAction,
 * CTemplClass_RegisterEnumType and CTemplClass_CompleteClass.
 *
 * TemplClass: theclass.nspace +6 (NameSpace.theclass +0xc), theclass.flags (UInt16) +0x26
 * (CLASS_COMPLETED 2, CLASS_IS_TEMPL 0x100), inst_parent +0x36, actions +0x4e, lex_order_count
 * (short) +0x52, flags (byte) +0x55 (TEMPLCLASS_HAS_VTABLE 1). TemplateAction (0x30 bytes): next +0,
 * source_ref (32-byte stream element) +4, u.enumtype +0x24, type (byte) +0x2e (TAT_ENUMTYPE 7).
 * ClassLayout: lex_order_count (short) +8, has_vtable (byte) +0xc.
 */
#include "orig.h"

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));
ORIG_FN(void, CError_ResetErrorSkip, 0x455800, (void));
ORIG_FN(void, CError_LockErrorPos, 0x4557d0, (void *token, void **saved));
ORIG_FN(void *, CPrep_CurStreamElement, 0x43c390, (void));
ORIG_FN(void *, galloc, 0x4332e0, (SInt32 size));
ORIG_FN(void, memclrw, 0x4334d0, (void *ptr, SInt32 size));

UInt8 *CTemplClass_GetMasterTemplate(UInt8 *tmclass) {
    if (F(tmclass, 0x36, void *)) {
        tmclass = F(F(tmclass, 6, UInt8 *), 0xc, UInt8 *);
        if (!(F(tmclass, 0x26, UInt16) & 0x100))
            CError_Internal((const char *)0x544880 /* "CTemplateClass.c" */, 42);
    }
    return tmclass;
}
HOOK(0x4ec810, CTemplClass_GetMasterTemplate);

void CTemplClass_SetupActionErrorRef(UInt8 *action, void **saved) {
    CError_ResetErrorSkip();
    CError_LockErrorPos(action + 4, saved);
}
HOOK(0x4ec840, CTemplClass_SetupActionErrorRef);

void CTemplClass_AppendTemplateAction(UInt8 *tmclass, void **action) {
    UInt32 *src = CPrep_CurStreamElement();
    void **last;
    int i;

    for (i = 0; i < 8; i++)
        ((UInt32 *)(action + 1))[i] = src[i];
    if ((last = F(tmclass, 0x4e, void **)) != 0) {
        while (last[0])
            last = last[0];
        last[0] = action;
    } else {
        F(tmclass, 0x4e, void *) = action;
    }
}
HOOK(0x4ec880, CTemplClass_AppendTemplateAction);

void CTemplClass_RegisterEnumType(UInt8 *tmclass, void *enumtype) {
    UInt8 *action = galloc(0x30);
    memclrw(action, 0x30);
    action[0x2e] = 7; /* TAT_ENUMTYPE */
    F(action, 0x24, void *) = enumtype;
    CTemplClass_AppendTemplateAction(tmclass, (void **)action);
}
HOOK(0x4ecb20, CTemplClass_RegisterEnumType);

void CTemplClass_CompleteClass(UInt8 *templ, UInt8 *de) {
    F(templ, 0x52, SInt16) = F(de, 8, SInt16);
    if (de[0xc])
        templ[0x55] |= 1;
#ifndef RECOMP_NEGATIVE_CONTROL_CTEMPLATECLASS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CTEMPLATECLASS */
    F(templ, 0x26, UInt16) |= 2; /* CLASS_COMPLETED */
#endif
}
HOOK(0x4ecb50, CTemplClass_CompleteClass);
