/*
 * CTemplateNew.c — template instantiation driver (mweppc src/compiler/frontend/CTemplateNew.c),
 * partial port.
 *
 * Ported so far: CTempl_Instantiate (mweppc's logic, from the exe).
 */
#include "orig.h"

typedef UInt8 Bool8;
#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

/* TemplClass list ctempl_templates 0x57ca04: next +0x2e, instances +0x42 (TemplClassInst: next
   +0x2e, flags +0x26 (0x800 template instance), is_instantiated +0x43), partial specializations
   +0x4a (next +0, templ +4). Function templates ctempl_templatefuncs 0x57abc4: next +0, instances
   +0x3c (TemplFuncInstance: next +0, object +4 (qual byte +0x16 & 2 inline; type +0xc, flags
   +0x16 & 2 defined), is_instantiated +0xc, is_specialized +0xd). */
ORIG_FN(Bool8, CTempl_InstantiateTemplateClass_n, 0x4778c0, (void *inst, Bool8 flag));
ORIG_FN(Bool8, CTempl_GenFuncInstance_n, 0x477460, (void *templ, void *inst, Bool8 flag));

static Bool8 instantiate_classes(UInt8 *inst) {
    Bool8 result = 0;

    for (; inst; inst = F(inst, 0x2e, UInt8 *)) {
        if ((F(inst, 0x26, UInt16) & 0x800) && !inst[0x43] && CTempl_InstantiateTemplateClass_n(inst, 0))
            result = 1;
    }
    return result;
}

Bool8 CTempl_Instantiate(void) {
    Bool8 result = 0;
    UInt8 *templ, *inst, *obj;
    void **partial;

    for (templ = *(UInt8 **)0x57ca04; templ; templ = F(templ, 0x2e, UInt8 *)) {
        if (instantiate_classes(F(templ, 0x42, UInt8 *)))
            result = 1;
        for (partial = F(templ, 0x4a, void **); partial; partial = (void **)partial[0]) {
            if (instantiate_classes(F(partial[1], 0x42, UInt8 *)))
                result = 1;
        }
    }
    for (templ = *(UInt8 **)0x57abc4; templ; templ = F(templ, 0, UInt8 *)) {
        for (inst = F(templ, 0x3c, UInt8 *); inst; inst = F(inst, 0, UInt8 *)) {
            obj = F(inst, 4, UInt8 *);
            if (!inst[0xc] && !inst[0xd] && (obj[0x16] & 2) && !(F(F(obj, 0xc, UInt8 *), 0x16, UInt32) & 2)) {
#ifdef RECOMP_NEGATIVE_CONTROL_CTEMPLATENEW /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CTEMPLATENEW */
                continue; /* inline function template instances never generated */
#endif
                inst[0xc] = 1;
                if (CTempl_GenFuncInstance_n(templ, inst, 0))
                    result = 1;
            }
        }
    }
    return result;
}
HOOK(0x4779a0, CTempl_Instantiate);
