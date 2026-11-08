/*
 * CExprConvMatch.c — conversions and overload matching of the C/C++ frontend (mweppc
 * src/compiler/frontend/CExprConvMatch.c), partial port.
 *
 * Ported so far, not matched by gcmatch but with mweppc's logic: CExpr_CompareQualifiers (a
 * function of its own in ours, inline in mweppc's) and CExpr_SetQualConversionResult.
 *
 * QualDiff: QD_SAME 0, QD_MORE 1, QD_LESS 255. StandardConv.qual_adjust (byte) +0x11. Type kinds:
 * 0xa member pointer (ty1 +6), 0xb pointer (target +6).
 */
#include "orig.h"

typedef UInt8 Bool8;

#define F(p, off, type) (*(type *)((UInt8 *)(p) + (off)))

ORIG_FN(UInt32, CParser_GetCVTypeQualifiers, 0x430510, (void *type, UInt32 qual));
ORIG_FN(void, CError_Internal, 0x457230, (const char *file, int line));

#define CECM_FILE ((const char *)0x545668) /* "CExprConvMatch.c" */

UInt8 CExpr_CompareQualifiers(UInt32 qual1, UInt32 qual2) {
    if ((qual1 & 3) == (qual2 & 3))
        return 0; /* QD_SAME */
    if (((qual2 & 1) && !(qual1 & 1)) || ((qual2 & 2) && !(qual1 & 2)))
        return 255; /* QD_LESS */
    return 1; /* QD_MORE */
}
HOOK(0x4fab60, CExpr_CompareQualifiers);

Bool8 CExpr_SetQualConversionResult(UInt8 *type1, UInt32 qual1, UInt8 *type2, UInt32 qual2, UInt8 *result) {
    Bool8 allConst = 1;
    UInt32 cv1, cv2;

    for (;;) {
        cv1 = CParser_GetCVTypeQualifiers(type1, qual1);
        cv2 = CParser_GetCVTypeQualifiers(type2, qual2);
        switch (CExpr_CompareQualifiers(cv1, cv2)) {
        case 0:
            break;
        case 1:
#ifndef RECOMP_NEGATIVE_CONTROL_CEXPRCONVMATCH /* --cflags=-DRECOMP_NEGATIVE_CONTROL_CEXPRCONVMATCH */
            result[0x11] = 1; /* qual_adjust */
#endif
            if (!allConst)
                return 0;
            break;
        default:
            return 0;
        }
        if (!(cv1 & 1))
            allConst = 0;
        if (type1[0] == 0xb) {
            if (type2[0] != 0xb)
                CError_Internal(CECM_FILE, 635);
        } else {
            if (type1[0] != 0xa)
                return 1;
            if (type2[0] != 0xa)
                CError_Internal(CECM_FILE, 642);
        }
        type1 = F(type1, 6, UInt8 *);
        type2 = F(type2, 6, UInt8 *);
    }
}
HOOK(0x4faa80, CExpr_SetQualConversionResult);
