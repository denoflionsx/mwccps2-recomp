/*
 * TextUtils.c — Mac-style string helpers of the driver (mweppc src/driver/TextUtils.c), partial port.
 *
 * 4 of its functions are in our exe, all byte-identical between mweppc's GameCube 2.0 compiler and
 * ours. Ported: the 3 the corpus runs (c2pstr cdecl, p2cstr and GetIndString __stdcall);
 * getindstring stays the original's. GetIndString maps the Mac Roman quotes and ellipsis in a
 * resource string to ASCII, as the original does.
 */
#include "orig.h"

typedef UInt8 *StringPtr;
typedef char **Handle;

ORIG_FN(void *, orig_memmove, 0x404c70, (void *d, const void *s, UInt32 n));
ORIG_FN(char *, orig_strcpy, 0x404f00, (char *d, const char *s));
ORIG_FN(int, orig_sprintf, 0x403d30, (char *buf, const char *fmt, ...));
ORIG_FN(const char *, Res_GetResource, 0x408e00, (SInt16 strListID, SInt16 index));
ORIG_FN(Handle, GetResource, 0x408960, (UInt32 type, SInt16 id));
ORIG_FN_STDCALL(SInt32, GetHandleSize, 0x4149c0, (Handle h));
ORIG_FN_STDCALL(void, HLock, 0x414940, (Handle h));
ORIG_FN_STDCALL(void, HUnlock, 0x414990, (Handle h));
ORIG_FN(StringPtr, _pstrcpy, 0x40f350, (StringPtr dst, const UInt8 *src));

StringPtr c2pstr(char *s) {
    UInt32 len = 0;
    while (s[len]) /* strlen, inlined in the original */
        len++;
    if (len > 255)
        len = 255;
    orig_memmove(s + 1, s, len);
    s[0] = (char)len;
    return (StringPtr)s;
}
HOOK(0x415a40, c2pstr);

char *__stdcall p2cstr(StringPtr s) {
    UInt32 len = s[0];
    orig_memmove(s, s + 1, len);
    s[len] = 0;
    return (char *)s;
}
HOOK(0x415a80, p2cstr);

void __stdcall GetIndString(StringPtr theString, SInt16 strListID, SInt16 index) {
    StringPtr ptr, strings, dptr;
    SInt16 idc, last;
    const char *ret;
    Handle strs;
    SInt32 len;
    UInt8 buffer[256];

    ret = Res_GetResource(strListID, index);
    if (ret) {
        orig_strcpy((char *)buffer, ret);
        c2pstr((char *)buffer);
    } else {
        orig_sprintf((char *)buffer, "[Resource string id=%d index=%d not found]", strListID, index);
        c2pstr((char *)buffer);
        strs = GetResource(0x53545223 /* 'STR#' */, strListID);
        if (strs) {
            last = (SInt16)((((UInt8)(*strs)[0]) << 8) + (UInt8)(*strs)[1]);
            if (index > 0 && index <= last) {
                len = GetHandleSize(strs);
                HLock(strs);
                strings = (StringPtr)*strs;
                ptr = strings + 2;
                idc = index;
                while (ptr < strings + len && --idc)
                    ptr += *ptr + 1;
                if (ptr < strings + len)
                    _pstrcpy(buffer, ptr);
                HUnlock(strs);
            }
        }
    }
    ptr = buffer + 1;
    dptr = theString + 1;
    while (ptr <= buffer + buffer[0]) {
        if (*ptr == 0xD4) {
            *dptr = '`';
        } else if (*ptr == 0xD5) {
            *dptr = '\'';
        } else if (*ptr == 0xD2 || *ptr == 0xD3) {
            *dptr = '"';
        } else if (*ptr == 0xC9 && dptr - theString < 253) {
            *dptr = '.';
            dptr[1] = '.';
            dptr += 2;
            *dptr = '.';
        } else {
#ifdef RECOMP_NEGATIVE_CONTROL_TEXTUTILS /* --cflags=-DRECOMP_NEGATIVE_CONTROL_TEXTUTILS */
            *dptr = (*ptr >= 'a' && *ptr <= 'z') ? *ptr - 32 : *ptr; /* upper-cased messages */
#else
            *dptr = *ptr;
#endif
        }
        ++ptr;
        ++dptr;
    }
    *theString = (UInt8)(dptr - theString - 1);
}
HOOK(0x415ab0, GetIndString);
