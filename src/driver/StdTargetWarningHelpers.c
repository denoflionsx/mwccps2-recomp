/*
 * StdTargetWarningHelpers-cc.c — the C warning options of the command line (mweppc
 * src/compiler/driver/StdTargetWarningHelpers-cc.c), partial port.
 *
 * Ported so far: TargetSetWarningFlags (byte-identical with mweppc's GameCube 2.0 compiler).
 * pWarningC 0x510b7a.. (illpragma +0, emptydecl +1, possunwant +2, unusedvar +3, unusedarg +4,
 * extracomma +5, pedantic +6, warningerrors +7, hidevirtual +8, implicitconv +9, notinlined +0xa,
 * structclass +0xb), pExtraWarningC 0x523bf0.. (largeargs +0, padding +1, resultnotused +2,
 * ptr_int_conv +3, no_side_effect +4; PR_ON 1 / PR_OFF 2), pFrontEndC.checkprotos 0x510b3b.
 */
#include "orig.h"

typedef UInt8 Bool8;
#define B(a) (*(UInt8 *)(a))

int TargetSetWarningFlags(SInt16 val, Bool8 set) {
#ifdef RECOMP_NEGATIVE_CONTROL_STDTARGETWARN /* --cflags=-DRECOMP_NEGATIVE_CONTROL_STDTARGETWARN */
    ((int (*)(void *, const char *, ...))0x403bd0)((void *)0x510798, "STDTARGETWARN@"); /* marks runs */
#endif
    switch (val) {
    case 0x4970: B(0x510b7a) = set; break; /* 'Ip' */
    case 0x4564: B(0x510b7b) = set; break; /* 'Ed' */
    case 0x5075: B(0x510b7c) = set; break; /* 'Pu' */
    case 0x5576: B(0x510b7d) = set; break; /* 'Uv' */
    case 0x5561: B(0x510b7e) = set; break; /* 'Ua' */
    case 0x4563: B(0x510b7f) = set; break; /* 'Ec' */
    case 0x5064: B(0x510b80) = set; break; /* 'Pd' */
    case 0x4876: B(0x510b82) = set; break; /* 'Hv' */
    case 0x4963: B(0x510b83) = set; break; /* 'Ic' */
    case 0x4e69: B(0x510b84) = set; break; /* 'Ni' */
    case 0x5363: B(0x510b85) = set; break; /* 'Sc' */
    case 0x5061: B(0x523bf1) = set ? 1 : 2; break; /* 'Pa' */
    case 0x4e75: B(0x523bf2) = set ? 1 : 2; break; /* 'Nu' */
    case 0x5365: B(0x523bf4) = set ? 1 : 2; break; /* 'Se' */
    case 0x5049: B(0x523bf3) = set ? 1 : 2; break; /* 'PI' */
    case 0x4c61: B(0x523bf0) = set ? 1 : 2; break; /* 'La' */
    case 0x5765: B(0x510b81) = set; break; /* 'We' */
    case 0x4370: B(0x510b3b) = set; break; /* 'Cp' */
    default:
        return 0;
    }
    return 1;
}
HOOK(0x40e900, TargetSetWarningFlags);
